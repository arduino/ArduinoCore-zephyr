/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <sample_usbd.h>

#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/usb/class/usbd_dfu.h>
#include <zephyr/dfu/mcuboot.h>
#include <zephyr/drivers/hwinfo.h>
#include <zephyr/sys/sys_io.h>
#include <cmsis_core.h>
#include <zephyr/irq.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

struct arm_vector_table {
    uint32_t msp;
    uint32_t reset;
};

#define DFU_DOUBLE_RESET_MAGIC     0x44524655U
#define DFU_DOUBLE_RESET_WINDOW_MS 1000U
#define DFU_MODE_SWITCH_DELAY_MS   250U
#define DFU_REBOOT_DELAY_MS        100U
#define DFU_BLINK_PERIOD_MS        100U
#define APP_VECTOR_TABLE_ADDR      0x0C040000U
#define APP_EMPTY_WORD             0xFFFFFFFFU

static uint32_t dfu_boot_cookie __noinit;
static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET_OR(DT_ALIAS(led0), gpios, {0});
static bool led0_ready;
static bool dfu_download_completed;
static struct usbd_context *dfu_pending_ctx;
static void dfu_mode_work_handler(struct k_work *work);
static void dfu_reboot_work_handler(struct k_work *work);
static void schedule_dfu_mode(struct usbd_context *const ctx);
K_TIMER_DEFINE(dfu_window_timer, NULL, NULL);
K_WORK_DELAYABLE_DEFINE(dfu_mode_work, dfu_mode_work_handler);
K_WORK_DELAYABLE_DEFINE(dfu_reboot_work, dfu_reboot_work_handler);

USBD_DEVICE_DEFINE(dfu_usbd, DEVICE_DT_GET(DT_NODELABEL(zephyr_udc0)), 0x2341, 0x017e);

USBD_DESC_LANG_DEFINE(sample_lang);
USBD_DESC_CONFIG_DEFINE(fs_cfg_desc, "DFU FS Configuration");
USBD_DESC_CONFIG_DEFINE(hs_cfg_desc, "DFU HS Configuration");

static const uint8_t attributes =
	(IS_ENABLED(CONFIG_SAMPLE_USBD_SELF_POWERED) ? USB_SCD_SELF_POWERED : 0) |
	(IS_ENABLED(CONFIG_SAMPLE_USBD_REMOTE_WAKEUP) ? USB_SCD_REMOTE_WAKEUP : 0);
/* Full speed configuration */
USBD_CONFIGURATION_DEFINE(sample_fs_config, attributes, CONFIG_SAMPLE_USBD_MAX_POWER, &fs_cfg_desc);

/* High speed configuration */
USBD_CONFIGURATION_DEFINE(sample_hs_config, attributes, CONFIG_SAMPLE_USBD_MAX_POWER, &hs_cfg_desc);

static void switch_to_dfu_mode(struct usbd_context *const ctx);

static void setup_led(void)
{
	if (!gpio_is_ready_dt(&led0)) {
		LOG_WRN("LED not available");
		return;
	}

	if (gpio_pin_configure_dt(&led0, GPIO_OUTPUT_INACTIVE)) {
		LOG_WRN("Failed to configure LED");
		return;
	}

	led0_ready = true;
}

static void blink_led_forever(k_timeout_t period)
{
	while (true) {
		if (led0_ready) {
			(void)gpio_pin_toggle_dt(&led0);
		}

		k_sleep(period);
	}
}

static void led_on(void)
{
	if (led0_ready) {
		(void)gpio_pin_set_dt(&led0, 1);
	}
}

static void led_off(void)
{
	if (led0_ready) {
		(void)gpio_pin_set_dt(&led0, 0);
	}
}

static void dfu_mode_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);

	switch_to_dfu_mode(dfu_pending_ctx);
	dfu_pending_ctx = NULL;
}

static void schedule_dfu_mode(struct usbd_context *const ctx)
{
	dfu_pending_ctx = ctx;
	(void)k_work_reschedule(&dfu_mode_work, K_MSEC(DFU_MODE_SWITCH_DELAY_MS));
}

static void dfu_reboot_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);

	LOG_INF("Resetting MCU after DFU transfer");
	(void)irq_lock();
	NVIC_SystemReset();
	CODE_UNREACHABLE;
}

static bool app_image_present(uint32_t vector_addr)
{
    uint32_t initial_msp = sys_read32(vector_addr);
    uint32_t reset_vector = sys_read32(vector_addr + sizeof(uint32_t));
    uint32_t reset_pc = reset_vector & ~BIT(0);

    uintptr_t slot_base =
        DT_REG_ADDR(DT_GPARENT(DT_NODELABEL(slot0_partition))) +
        DT_REG_ADDR(DT_NODELABEL(slot0_partition));
    uintptr_t slot_end = slot_base + DT_REG_SIZE(DT_NODELABEL(slot0_partition));

    /* VTOR on Cortex-M needs alignment on 128-byte boundary */
    if ((vector_addr & 0x7FU) != 0U) {
        return false;
    }

    /* MSP must be valid pointer and 8-byte aligned */
    if ((initial_msp == 0U) || (initial_msp == APP_EMPTY_WORD)) {
        return false;
    }
    if ((initial_msp & 0x7U) != 0U) {
        return false;
    }

    /* Reset vector must be Thumb and point inside loader slot */
    if ((reset_vector & BIT(0)) == 0U) {
        return false;
    }
    if ((reset_pc < slot_base) || (reset_pc >= slot_end)) {
        return false;
    }

    /* First instruction at reset handler must not be erased/zero */
    uint32_t first_instr = sys_read32(reset_pc);
    if ((first_instr == 0U) || (first_instr == APP_EMPTY_WORD)) {
        return false;
    }

    return true;
}

static FUNC_NORETURN void jump_to_application(uint32_t app_addr)
{
	struct arm_vector_table *vt = (struct arm_vector_table *)app_addr;

	/* 1. Disable and clear pending state for all NVIC lines. */
    (void)irq_lock();

    for (int i = 0; i < (CONFIG_NUM_IRQS + 31) / 32; i++) {
        NVIC->ICER[i] = 0xFFFFFFFF;
        NVIC->ICPR[i] = 0xFFFFFFFF;
    }

	/* 2. Clear stack limits inherited from the previous context. */
	__set_MSPLIM(0U);
	__set_PSPLIM(0U);

	/* 3. Set CONTROL back to 0: privileged Thread mode, MSP selected. */
    __set_CONTROL(0);
    __ISB();

#if defined(CONFIG_ARM_MPU)
	/* 4. Disable MPU to avoid memory access faults when jumping to the loader/application. */
	MPU->CTRL = 0;
	__DSB();
	__ISB();
#endif

	/* 4. Relocate vector table to the target image . */
    SCB->VTOR = app_addr;
    __DSB();
    __ISB();

	/* 5. Load target MSP and branch to target Reset_Handler. */
    __set_MSP(vt->msp);
    __ISB();

    ((void (*)(void))vt->reset)();

    CODE_UNREACHABLE;
}

static void msg_cb(struct usbd_context *const usbd_ctx, const struct usbd_msg *const msg)
{
	LOG_INF("USBD message: %s", usbd_msg_type_string(msg->type));

	if (msg->type == USBD_MSG_CONFIGURATION) {
		LOG_INF("\tConfiguration value %d", msg->status);
	}

	if (usbd_can_detect_vbus(usbd_ctx)) {
		if (msg->type == USBD_MSG_VBUS_READY) {
			if (usbd_enable(usbd_ctx)) {
				LOG_ERR("Failed to enable device support");
			}
		}

		if (msg->type == USBD_MSG_VBUS_REMOVED) {
			if (usbd_disable(usbd_ctx)) {
				LOG_ERR("Failed to disable device support");
			}
		}
	}

	if (msg->type == USBD_MSG_DFU_APP_DETACH) {
		LOG_INF("DFU app detach received, switching USB device to DFU descriptors");
		schedule_dfu_mode(usbd_ctx);
	}

	if (msg->type == USBD_MSG_DFU_DOWNLOAD_COMPLETED) {
		LOG_INF("DFU download completed, waiting for USB reset");
		dfu_download_completed = true;
	}

	if ((msg->type == USBD_MSG_RESET) && dfu_download_completed) {
		LOG_INF("USB reset received after DFU download, scheduling reboot");
		dfu_download_completed = false;
		(void)k_work_reschedule(&dfu_reboot_work, K_MSEC(DFU_REBOOT_DELAY_MS));
	}
}

static void switch_to_dfu_mode(struct usbd_context *const ctx)
{
	int err;

	LOG_INF("Detach USB device");
	if (ctx != NULL) {
		usbd_disable(ctx);
		usbd_shutdown(ctx);
	}

	err = usbd_add_descriptor(&dfu_usbd, &sample_lang);
	if (err) {
		LOG_ERR("Failed to initialize language descriptor (%d)", err);
		return;
	}

	if (usbd_caps_speed(&dfu_usbd) == USBD_SPEED_HS) {
		err = usbd_add_configuration(&dfu_usbd, USBD_SPEED_HS, &sample_hs_config);
		if (err) {
			LOG_ERR("Failed to add High-Speed configuration");
			return;
		}

		err = usbd_register_class(&dfu_usbd, "dfu_dfu", USBD_SPEED_HS, 1);
		if (err) {
			LOG_ERR("Failed to add register classes");
			return;
		}

		usbd_device_set_code_triple(&dfu_usbd, USBD_SPEED_HS, 0, 0, 0);
	}

	err = usbd_add_configuration(&dfu_usbd, USBD_SPEED_FS, &sample_fs_config);
	if (err) {
		LOG_ERR("Failed to add Full-Speed configuration");
		return;
	}

	err = usbd_register_class(&dfu_usbd, "dfu_dfu", USBD_SPEED_FS, 1);
	if (err) {
		LOG_ERR("Failed to add register classes");
		return;
	}

	usbd_device_set_code_triple(&dfu_usbd, USBD_SPEED_FS, 0, 0, 0);

	err = usbd_init(&dfu_usbd);
	if (err) {
		LOG_ERR("Failed to initialize USB device support");
		return;
	}

	err = usbd_msg_register_cb(&dfu_usbd, msg_cb);
	if (err) {
		LOG_ERR("Failed to register message callback");
		return;
	}

	err = usbd_enable(&dfu_usbd);
	if (err) {
		LOG_ERR("Failed to enable USB device support");
	}
}

int main(void)
{
	setup_led();

	struct usbd_context *sample_usbd;
	int ret;

	sample_usbd = sample_usbd_init_device(msg_cb);
	if (sample_usbd == NULL) {
		LOG_ERR("Failed to initialize USB device");
		return -ENODEV;
	}

	if (!usbd_can_detect_vbus(sample_usbd)) {
		ret = usbd_enable(sample_usbd);
		if (ret) {
			LOG_ERR("Failed to enable device support");
			return ret;
		}
	}

	if (dfu_boot_cookie == DFU_DOUBLE_RESET_MAGIC) {
		dfu_boot_cookie = 0U;

		LOG_INF("Double reset detected, entering DFU mode");
		schedule_dfu_mode(sample_usbd);
		blink_led_forever(K_MSEC(DFU_BLINK_PERIOD_MS));
	}

	/* First reset: open a short window for the second reset. */
	dfu_boot_cookie = DFU_DOUBLE_RESET_MAGIC;
	k_timer_start(&dfu_window_timer, K_MSEC(DFU_DOUBLE_RESET_WINDOW_MS), K_NO_WAIT);
	(void)k_timer_status_sync(&dfu_window_timer);
	dfu_boot_cookie = 0U;
	LOG_INF("No double reset detected, exiting USB DFU sample boot...");

	if (app_image_present(APP_VECTOR_TABLE_ADDR)) {
		LOG_INF("Loader image present at 0x%08x", APP_VECTOR_TABLE_ADDR);
		jump_to_application(APP_VECTOR_TABLE_ADDR);
	}

	LOG_WRN("No valid loader image at 0x%08x", APP_VECTOR_TABLE_ADDR);
	schedule_dfu_mode(sample_usbd);
	blink_led_forever(K_MSEC(DFU_BLINK_PERIOD_MS));

	return 0;
}
