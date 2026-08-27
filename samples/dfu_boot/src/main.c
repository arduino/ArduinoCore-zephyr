/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/usb/class/usbd_dfu.h>
#include <zephyr/dfu/mcuboot.h>
#include <zephyr/drivers/hwinfo.h>
#include <zephyr/sys/sys_io.h>
#include <zephyr/drivers/timer/system_timer.h>
#include <cmsis_core.h>
#include <zephyr/irq.h>
#include <zephyr/logging/log.h>
#include <dfu_cookie.h>
#include <pfswap.h>
#include <dfu_usb_detach.h>
#include "dfu_images.h"
#ifdef CONFIG_APP_BFM_SELFTEST
#include "bfm_selftest.h"
#endif
#ifdef CONFIG_APP_BFM_AB_TEST
#include "bfm_ab_test.h"
#endif
LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

struct arm_vector_table {
    uint32_t msp;
    uint32_t reset;
};

/* DFU_DOUBLE_RESET_MAGIC and the cookie's storage now come from
 * <dfu_cookie.h>, shared with the loader/sketch side. */

/*
 * How long a normal (non-double-tap) boot holds the window open before
 * disarming. This only bounds the *maximum* gap between the two taps; the
 * minimum is set by how early the cookie gets armed - see
 * dfu_double_reset_arm() below.
 *
 * main() blocks for this on *every* normal boot before chainloading, and it
 * dwarfs everything else dfu_boot does (reaching main() takes well under 5 ms,
 * and the app-present check microseconds), so it is effectively this
 * bootloader's entire contribution to time-to-sketch. 350 ms covers a human
 * double-tap, which is ~100-350 ms; it no longer has to absorb the tens of
 * milliseconds of "dead zone" slack the old main()-time arming needed, because
 * the cookie is now armed from the very first entry in the init table.
 *
 * Raise it if double-taps start getting missed - that is the trade being made
 * here, since a gap longer than this falls through to the chainload instead.
 */
#define DFU_DOUBLE_RESET_WINDOW_MS 350U
#define DFU_MODE_SWITCH_DELAY_MS   250U
/*
 * Reboot into the freshly written image once DFU traffic goes quiet, so the
 * arduino-cli recipe needs no host-issued reset: dfu-util's `--reset` returns
 * the raw libusb NOT_FOUND from libusb_reset_device() and exits 251, which
 * arduino-cli reports as a failed upload.
 *
 * Rescheduled from dfu_activity_notify() on every transfer, not just on
 * completion - see src/dfu_images.c.
 */
#define DFU_IDLE_REBOOT_MS         3000U
#define APP_EMPTY_WORD             0xFFFFFFFFU

/*
 * Where the next boot stage lives: boot_partition, i.e. MCUboot.
 *
 * Derived from devicetree rather than hardcoded. It used to be a literal
 * 0x0C040000; when the partition table moved to make room for the A/B slots the
 * literal kept pointing into what had become the middle of the loader image, the
 * vector-table check failed, and the board sat in DFU mode with nothing
 * obviously wrong.
 *
 * Deliberately NOT slot0_partition, which is the other thing it could plausibly
 * be. Those were the same node until MCUboot became the second stage. They have
 * to differ now: dfu_boot must jump to mcuboot, while slot0_partition stays the
 * loader so DFU alt 0 keeps addressing what boards.txt expects to write there.
 */
#define APP_BOOT_BASE \
	(DT_REG_ADDR(DT_GPARENT(DT_NODELABEL(boot_partition))) + \
	 DT_REG_ADDR(DT_NODELABEL(boot_partition)))
#define APP_VECTOR_TABLE_ADDR      ((uint32_t)APP_BOOT_BASE)

/*
 * The cookie itself lives in the DFU_COOKIE reserved-memory region (see
 * <dfu_cookie.h>), not in this image's .noinit, so that a running loader or
 * sketch can arm it too - that is what makes a USB DFU detach request land
 * here in DFU mode instead of chainloading straight past it.
 */
static bool dfu_double_reset_latched;

/*
 * Everything between the reset itself and the moment this cookie gets armed
 * is a dead zone: a second physical reset landing in it finds an unarmed
 * cookie and goes undetected, so the user has to deliberately wait before
 * tapping again. Keeping that zone as short as possible is the whole game,
 * so the arm/check runs from PRE_KERNEL_1 priority 0 - the earliest hook the
 * kernel offers, right after the C runtime is up (BSS zeroed, .data copied;
 * the DFU_COOKIE region is outside both, which is what lets the cookie
 * survive) and well before console/UART/USB/driver init.
 *
 * Doing this from main() instead - as this sample originally did, even with
 * the check hoisted to the very first statement - leaves tens of
 * milliseconds of dead zone: driver and console bring-up, plus every boot
 * log line, since CONFIG_LOG_MODE_IMMEDIATE makes each one a *blocking*
 * UART write. That is why this sample's README used to tell users the second
 * tap "shall be done slightly later than the usual double-tap interval, due
 * to Zephyr boot timing"; with the arm moved here, an ordinary fast
 * double-tap works.
 */
static int dfu_double_reset_arm(void)
{
	dfu_double_reset_latched = (DFU_COOKIE == DFU_DOUBLE_RESET_MAGIC);

	/* Arm for the next boot (or disarm, if this *is* the second tap). The
	 * window is held open, and the cookie finally cleared, back in main()
	 * once the kernel can actually wait. */
	DFU_COOKIE = dfu_double_reset_latched ? 0U : DFU_DOUBLE_RESET_MAGIC;

	return 0;
}
SYS_INIT(dfu_double_reset_arm, PRE_KERNEL_1, 0);

static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET_OR(DT_ALIAS(led0), gpios, {0});
static bool led0_ready;
static bool dfu_download_completed;
static struct usbd_context *dfu_pending_ctx;
static void dfu_mode_work_handler(struct k_work *work);
static void dfu_reboot_work_handler(struct k_work *work);
static void schedule_dfu_mode(struct usbd_context *const ctx);
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

/*
 * led0 is a plain GPIO (no PWM timer wired to it), so a smooth breathing fade
 * is faked with software PWM: toggle the pin at a fixed carrier with a duty
 * cycle that ramps up and back down.
 *
 * The waits are k_usleep(), not k_busy_wait(). Busy-waiting works -- the usbd
 * thread (coop prio -8) and the system workqueue (-1) both preempt main (prio
 * 0), so DFU is unaffected either way -- but it pegs the core at 100% for the
 * whole, indefinite DFU residency and never lets the idle thread reach WFI.
 * Sleeping costs ~500 context switches/s instead, i.e. well under 1% CPU.
 *
 * STEPS is tick-aligned deliberately: CONFIG_SYS_CLOCK_TICKS_PER_SEC=10000
 * gives a 100 us tick and CARRIER_US / STEPS == 100 us, so every sleep is a
 * whole number of ticks. Finer steps than the tick would just round, distorting
 * the dim end of the ramp.
 */
#define DFU_BREATH_STEPS      40U   /* brightness levels per ramp (100 us each) */
#define DFU_BREATH_CARRIER_US 4000U /* soft-PWM carrier period (250 Hz) */
#define DFU_BREATH_REPS       3U    /* carrier periods per step, so 12 ms each */

static FUNC_NORETURN void breathe_led_forever(void)
{
	if (!led0_ready) {
		while (true) {
			k_sleep(K_FOREVER);
		}
	}

	while (true) {
		/*
		 * Triangle ramp: phase 0..STEPS fades in, STEPS..2*STEPS fades
		 * back out, so one full cycle is
		 * 2 * STEPS * REPS * CARRIER_US = 80 * 3 * 4 ms ~= 0.96 s.
		 */
		for (uint32_t phase = 0; phase < 2U * DFU_BREATH_STEPS; phase++) {
			uint32_t level = (phase < DFU_BREATH_STEPS)
						 ? phase
						 : (2U * DFU_BREATH_STEPS - phase);
			uint32_t on_us = (DFU_BREATH_CARRIER_US * level) / DFU_BREATH_STEPS;
			uint32_t off_us = DFU_BREATH_CARRIER_US - on_us;

			for (uint32_t rep = 0; rep < DFU_BREATH_REPS; rep++) {
				if (on_us > 0U) {
					(void)gpio_pin_set_dt(&led0, 1);
					k_usleep(on_us);
				}
				if (off_us > 0U) {
					(void)gpio_pin_set_dt(&led0, 0);
					k_usleep(off_us);
				}
			}
		}
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

	if (!dfu_download_completed) {
		/* Traffic without a completed download: nothing new to boot. */
		return;
	}

	LOG_INF("Resetting MCU after DFU transfer");
	dfu_usb_soft_disconnect();
	(void)irq_lock();
	NVIC_SystemReset();
	CODE_UNREACHABLE;
}

void dfu_activity_notify(void)
{
	/* Push the reboot out on every transfer, so it can only fire once the
	 * host has finished with us. */
	(void)k_work_reschedule(&dfu_reboot_work, K_MSEC(DFU_IDLE_REBOOT_MS));
}

/*
 * Undo the PFM panel swap the Boot ROM performs alongside the BFM one.
 *
 * The register sequence and the reasoning live in the variant's pfswap.h,
 * shared with extra/mcuboot/hooks rather than copied: both bootloaders in this
 * tree need it, and a correction to undocumented SoC behaviour landing in only
 * one of them is exactly the kind of drift that ends in a board that will not
 * boot.
 *
 * This is safe here and nowhere later: changing PFSWAP requires that nothing is
 * accessing either PFM panel (DS 31.2.17.4). This image lives entirely in BFM,
 * so the first PFM access in the whole program is app_image_present() below.
 */
static void normalize_pfswap(void)
{
	switch (pfm_normalize_swap()) {
	case PFM_SWAP_NORMALIZED:
		LOG_INF("PFSWAP normalized");
		break;
	case PFM_SWAP_LOCKED:
		LOG_ERR("PFSWAP set and locked; PFM addresses stay shifted");
		break;
	case PFM_SWAP_FAILED:
		LOG_ERR("PFSWAP would not clear");
		break;
	case PFM_SWAP_ALREADY_NORMAL:
		break;
	}
}

static bool app_image_present(uint32_t vector_addr)
{
    uint32_t initial_msp = sys_read32(vector_addr);
    uint32_t reset_vector = sys_read32(vector_addr + sizeof(uint32_t));
    uint32_t reset_pc = reset_vector & ~BIT(0);

    uintptr_t slot_base = APP_BOOT_BASE;
    uintptr_t slot_end = slot_base + DT_REG_SIZE(DT_NODELABEL(boot_partition));

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

	/*
	 * 1b. SysTick is a dedicated core exception, not part of the NVIC
	 * IRQ array cleared above, so it survives the loop untouched. If left
	 * running, the loader's own kernel init -- which assumes SysTick is in
	 * its power-on-reset state, not "already configured and counting by a
	 * previous image" -- can end up with a broken/mismatched tick source
	 * (observed on hardware: the loader's own uptime clock never advances
	 * after chainloading). Disable it and clear any pending SysTick
	 * exception before jumping, so the loader gets a clean slate.
	 *
	 * ICSR must be written by ASSIGNMENT, never read-modify-write: it
	 * holds write-1-to-set bits (PENDSTSET b26, PENDSVSET b28, NMIPENDSET
	 * b31), so a `|=` reads those status bits and writes them straight
	 * back, *setting* the very exceptions it means to clear. That leaves
	 * the loader starting with a spurious pending SysTick/PendSV and
	 * hanging as soon as it unmasks interrupts. On Armv8-M ICSR.STTNS is
	 * R/W and must be preserved -- this is the same idiom Zephyr's own
	 * cortex_m_systick.c uses. PendSV is cleared too: dfu_boot is a
	 * threaded Zephyr app, so a context switch may well be pending at the
	 * moment we jump.
	 *
	 * SysTick->VAL is deliberately not touched: the incoming image's
	 * sys_clock_driver_init() sets LOAD and VAL itself.
	 */
	sys_clock_disable();
#ifdef SCB_ICSR_STTNS_Msk
	SCB->ICSR = (SCB->ICSR & SCB_ICSR_STTNS_Msk) |
		    SCB_ICSR_PENDSTCLR_Msk | SCB_ICSR_PENDSVCLR_Msk;
#else
	SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk | SCB_ICSR_PENDSVCLR_Msk;
#endif

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
		LOG_INF("DFU download completed, arming reboot");
		dfu_download_completed = true;
		(void)k_work_reschedule(&dfu_reboot_work, K_MSEC(DFU_IDLE_REBOOT_MS));
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
	/* Before anything reads PFM -- see the comment on the function. */
	normalize_pfswap();

	setup_led();

#ifdef CONFIG_APP_BFM_SELFTEST
	/*
	 * Before the double-reset window, so it runs on every boot including
	 * the normal one that chainloads the loader a few lines below.
	 */
	bfm_selftest();
#endif

#ifdef CONFIG_APP_BFM_AB_TEST
	bfm_ab_test();
#endif

	/* Latched at PRE_KERNEL_1 by dfu_double_reset_arm(). */
	if (dfu_double_reset_latched) {
		LOG_INF("Double reset detected, entering DFU mode");
	} else {
		/* Hold the window open for a second tap, then disarm. */
		k_msleep(DFU_DOUBLE_RESET_WINDOW_MS);
		DFU_COOKIE = 0U;
		LOG_INF("No double reset detected, exiting USB DFU sample boot...");

		if (app_image_present(APP_VECTOR_TABLE_ADDR)) {
			LOG_INF("Loader image present at 0x%08x", APP_VECTOR_TABLE_ADDR);
			jump_to_application(APP_VECTOR_TABLE_ADDR);
		}

		LOG_WRN("No valid loader image at 0x%08x", APP_VECTOR_TABLE_ADDR);
	}

	/*
	 * Both remaining paths stay resident in DFU mode. No USB context is set
	 * up here: switch_to_dfu_mode() only ever disables/shuts down whatever
	 * it is handed before building `dfu_usbd` itself, so passing NULL just
	 * skips a teardown of a context that was never created.
	 */
	schedule_dfu_mode(NULL);
	breathe_led_forever();

	return 0;
}
