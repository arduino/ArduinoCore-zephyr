/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Emulated ST M95M01 SPI EEPROM (arduino,emul-m95m01), the EEPROM of the SPI
 * test of the CI. The chip select is a GPIO driven by the sketch: the
 * emulator follows its level, a command starts at each falling edge.
 */

#define DT_DRV_COMPAT arduino_emul_m95m01

#include <string.h>

#include <zephyr/device.h>
#include <zephyr/drivers/emul.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/spi_emul.h>
#include <zephyr/sys/util.h>

#define M95M01_SIZE      (128 * 1024)
#define M95M01_PAGE_SIZE 256

#define CMD_WRSR  0x01
#define CMD_WRITE 0x02
#define CMD_READ  0x03
#define CMD_WRDI  0x04
#define CMD_RDSR  0x05
#define CMD_WREN  0x06

#define STATUS_WEL BIT(1)

/* bytes of a READ or WRITE before the data: command and 3 address bytes */
#define ADDR_END 4

/* returned when the EEPROM does not drive its output */
#define IDLE_BYTE 0xff

struct emul_m95m01_cfg {
	struct gpio_dt_spec cs;
};

struct emul_m95m01_data {
	uint8_t mem[M95M01_SIZE];
	uint8_t status;
	/* chip select level, and bytes since the falling edge */
	bool cs_high;
	uint32_t index;
	uint8_t cmd;
	uint32_t addr;
	struct gpio_callback cs_cb;
};

/* one byte of a command; returns the byte to send back */
static uint8_t emul_m95m01_byte(struct emul_m95m01_data *data, uint8_t in) {
	uint32_t index = data->index++;
	uint8_t out = IDLE_BYTE;

	if (data->cs_high) {
		return IDLE_BYTE;
	}

	if (index == 0) {
		data->cmd = in;
		data->addr = 0;
		switch (in) {
		case CMD_WREN:
			data->status |= STATUS_WEL;
			break;
		case CMD_WRDI:
			data->status &= ~STATUS_WEL;
			break;
		default:
			break;
		}
		return IDLE_BYTE;
	}

	switch (data->cmd) {
	case CMD_RDSR:
		/* the status, again and again (writes are never in progress) */
		out = data->status;
		break;
	case CMD_READ:
	case CMD_WRITE:
		if (index < ADDR_END) {
			/* 17-bit address: A16 is bit 0 of the first byte */
			data->addr = (data->addr << 8) | (index == 1 ? (in & 0x01) : in);
			break;
		}
		if (data->cmd == CMD_READ) {
			out = data->mem[data->addr];
			data->addr = (data->addr + 1) % M95M01_SIZE;
		} else if ((data->status & STATUS_WEL) != 0) {
			/* writes wrap within the page */
			uint32_t page = data->addr & ~(M95M01_PAGE_SIZE - 1);
			uint32_t offset = (data->addr + index - ADDR_END) % M95M01_PAGE_SIZE;

			data->mem[page + offset] = in;
		}
		break;
	default:
		/* WRSR is accepted and ignored, other commands are unknown */
		break;
	}

	return out;
}

/* the byte of a buffer at a position of the stream: words are sent most
 * significant byte first, and are in the host order in the buffer
 */
static uint8_t *emul_m95m01_buf_byte(const struct spi_buf_set *bufs, size_t pos,
									 size_t word_bytes) {
	if (bufs == NULL) {
		return NULL;
	}
	for (size_t i = 0; i < bufs->count; i++) {
		const struct spi_buf *buf = &bufs->buffers[i];

		if (pos < buf->len) {
			size_t word = pos / word_bytes;
			size_t byte = pos % word_bytes;
			size_t phys = word * word_bytes;

			phys += IS_ENABLED(CONFIG_LITTLE_ENDIAN) ? word_bytes - 1 - byte : byte;
			return buf->buf == NULL ? NULL : (uint8_t *)buf->buf + phys;
		}
		pos -= buf->len;
	}

	return NULL;
}

static size_t emul_m95m01_buf_len(const struct spi_buf_set *bufs) {
	size_t len = 0;

	for (size_t i = 0; bufs != NULL && i < bufs->count; i++) {
		len += bufs->buffers[i].len;
	}

	return len;
}

static int emul_m95m01_io(const struct emul *target, const struct spi_config *config,
						  const struct spi_buf_set *tx_bufs, const struct spi_buf_set *rx_bufs) {
	struct emul_m95m01_data *data = target->data;
	size_t word_bytes = DIV_ROUND_UP(SPI_WORD_SIZE_GET(config->operation), 8);
	size_t len = MAX(emul_m95m01_buf_len(tx_bufs), emul_m95m01_buf_len(rx_bufs));

	for (size_t pos = 0; pos < len; pos++) {
		uint8_t *tx = emul_m95m01_buf_byte(tx_bufs, pos, word_bytes);
		uint8_t *rx = emul_m95m01_buf_byte(rx_bufs, pos, word_bytes);
		uint8_t out = emul_m95m01_byte(data, tx == NULL ? 0x00 : *tx);

		if (rx != NULL) {
			*rx = out;
		}
	}

	return 0;
}

static void emul_m95m01_cs_handler(const struct device *port, struct gpio_callback *cb,
								   gpio_port_pins_t pins) {
	struct emul_m95m01_data *data = CONTAINER_OF(cb, struct emul_m95m01_data, cs_cb);
	int level = gpio_emul_output_get(port, __builtin_ctz(cb->pin_mask));

	ARG_UNUSED(pins);

	if (level < 0 || (level != 0) == data->cs_high) {
		return;
	}
	data->cs_high = (level != 0);
	if (data->cs_high) {
		/* a write ends at the rising edge, and disables writes */
		if (data->index > 0 && (data->cmd == CMD_WRITE || data->cmd == CMD_WRSR)) {
			data->status &= ~STATUS_WEL;
		}
	} else {
		data->index = 0;
	}
}

static const struct spi_emul_api emul_m95m01_api = {
	.io = emul_m95m01_io,
};

static int emul_m95m01_init(const struct emul *target, const struct device *parent) {
	const struct emul_m95m01_cfg *cfg = target->cfg;
	struct emul_m95m01_data *data = target->data;

	ARG_UNUSED(parent);

	memset(data->mem, 0xff, sizeof(data->mem));
	data->status = 0;
	data->cs_high = true;
	data->index = 0;

	if (!gpio_is_ready_dt(&cfg->cs)) {
		return -ENODEV;
	}
	gpio_init_callback(&data->cs_cb, emul_m95m01_cs_handler, BIT(cfg->cs.pin));

	return gpio_add_callback(cfg->cs.port, &data->cs_cb);
}

/* the emulator framework refers to a device on the node: define one, with no API */
#define EMUL_M95M01_DEFINE(n)                                                                      \
	static const struct emul_m95m01_cfg emul_m95m01_cfg_##n = {                                    \
		.cs = GPIO_DT_SPEC_INST_GET(n, cs_monitor_gpios),                                          \
	};                                                                                             \
	static struct emul_m95m01_data emul_m95m01_data_##n;                                           \
	DEVICE_DT_INST_DEFINE(n, NULL, NULL, NULL, NULL, POST_KERNEL, CONFIG_SPI_INIT_PRIORITY, NULL); \
	EMUL_DT_INST_DEFINE(n, emul_m95m01_init, &emul_m95m01_data_##n, &emul_m95m01_cfg_##n,          \
						&emul_m95m01_api, NULL)

DT_INST_FOREACH_STATUS_OKAY(EMUL_M95M01_DEFINE)
