/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Emulated ATECC608 secure element (arduino,emul-atecc608): the commands the
 * secure element libraries need to start and to read data. No key or
 * signature is emulated.
 *
 * Protocol: a write starts with a word address, 0x03 for a command packet
 * (count, opcode, param1, param2 (LE), data, CRC16 (LE); count includes
 * itself and the CRC), 0x02 for idle, 0x01 for sleep. A read returns the
 * response packet of the last command (count, data, CRC16), or the wake-up
 * status (0x11) when there is none: the wake-up pulse is a write to address
 * 0, that no emulator sees, so the device is always awake.
 */

#define DT_DRV_COMPAT arduino_emul_atecc608

#include <string.h>

#include <zephyr/device.h>
#include <zephyr/drivers/emul.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/i2c_emul.h>
#include <zephyr/sys/util.h>

#define WORD_ADDR_RESET   0x00
#define WORD_ADDR_SLEEP   0x01
#define WORD_ADDR_IDLE    0x02
#define WORD_ADDR_COMMAND 0x03

#define OPCODE_READ   0x02
#define OPCODE_RANDOM 0x1b
#define OPCODE_INFO   0x30

#define STATUS_AFTER_WAKE 0x11
#define STATUS_CRC_ERROR  0xff
#define STATUS_EXEC_ERROR 0x0f

#define ZONE_CONFIG  0x00
#define ZONE_MASK    0x03
#define ZONE_32BYTES 0x80

#define CONFIG_SIZE 128
#define MAX_DATA    32
/* count, data, CRC16 */
#define MAX_PACKET  (1 + MAX_DATA + 2)

/* configuration zone: serial number 01 23 4e 6a .. 8c 2d 1f 4a ee, revision
 * 0x6002 (ATECC608A), configuration and data zones locked (bytes 86-87 = 0)
 */
static const uint8_t atecc608_config[CONFIG_SIZE] = {
	[0] = 0x01,  [1] = 0x23,  [2] = 0x4e,  [3] = 0x6a,  [4] = 0x00,
	[5] = 0x00,  [6] = 0x60,  [7] = 0x02,  [8] = 0x8c,  [9] = 0x2d,
	[10] = 0x1f, [11] = 0x4a, [12] = 0xee, [86] = 0x00, [87] = 0x00,
};

static const uint8_t atecc608_revision[4] = {0x00, 0x00, 0x60, 0x02};

struct emul_atecc608_data {
	uint8_t response[MAX_PACKET];
	size_t response_len;
	size_t response_pos;
};

static uint16_t atecc608_crc16(const uint8_t *data, size_t len) {
	uint16_t crc = 0;

	for (size_t i = 0; i < len; i++) {
		for (uint8_t shift = 0x01; shift != 0; shift <<= 1) {
			uint8_t data_bit = (data[i] & shift) ? 1 : 0;
			uint8_t crc_bit = crc >> 15;

			crc <<= 1;
			if (data_bit != crc_bit) {
				crc ^= 0x8005;
			}
		}
	}

	return crc;
}

static void atecc608_respond(struct emul_atecc608_data *data, const uint8_t *payload, size_t len) {
	uint16_t crc;

	data->response[0] = len + 3;
	memcpy(&data->response[1], payload, len);
	crc = atecc608_crc16(data->response, len + 1);
	data->response[len + 1] = crc & 0xff;
	data->response[len + 2] = crc >> 8;
	data->response_len = len + 3;
	data->response_pos = 0;
}

static void atecc608_respond_status(struct emul_atecc608_data *data, uint8_t status) {
	atecc608_respond(data, &status, 1);
}

static void atecc608_command(struct emul_atecc608_data *data, const uint8_t *pkt, size_t len) {
	uint8_t count;
	uint8_t opcode;
	uint8_t param1;
	uint16_t param2;
	uint16_t crc;

	if (len < 7 || pkt[0] < 7 || pkt[0] > len) {
		atecc608_respond_status(data, STATUS_CRC_ERROR);
		return;
	}
	count = pkt[0];
	crc = pkt[count - 2] | (pkt[count - 1] << 8);
	if (crc != atecc608_crc16(pkt, count - 2)) {
		atecc608_respond_status(data, STATUS_CRC_ERROR);
		return;
	}
	opcode = pkt[1];
	param1 = pkt[2];
	param2 = pkt[3] | (pkt[4] << 8);

	switch (opcode) {
	case OPCODE_INFO:
		atecc608_respond(data, atecc608_revision, sizeof(atecc608_revision));
		break;
	case OPCODE_RANDOM: {
		uint8_t random[MAX_DATA];

		/* fixed bytes: the sketches can only check their format */
		for (size_t i = 0; i < sizeof(random); i++) {
			random[i] = 0x5a ^ (i * 0x1d);
		}
		atecc608_respond(data, random, sizeof(random));
		break;
	}
	case OPCODE_READ: {
		/* configuration zone only: 4 bytes at word param2, or the 32-byte
		 * block of param2
		 */
		size_t size = (param1 & ZONE_32BYTES) ? 32 : 4;
		size_t offset = (size == 32) ? ((param2 >> 3) & 0x03) * 32 : (param2 & 0x1f) * 4;

		if ((param1 & ZONE_MASK) != ZONE_CONFIG || offset + size > CONFIG_SIZE) {
			atecc608_respond_status(data, STATUS_EXEC_ERROR);
			break;
		}
		atecc608_respond(data, &atecc608_config[offset], size);
		break;
	}
	default:
		atecc608_respond_status(data, STATUS_EXEC_ERROR);
		break;
	}
}

static int emul_atecc608_transfer(const struct emul *target, struct i2c_msg *msgs, int num_msgs,
								  int addr) {
	struct emul_atecc608_data *data = target->data;

	ARG_UNUSED(addr);

	for (int i = 0; i < num_msgs; i++) {
		struct i2c_msg *msg = &msgs[i];

		if ((msg->flags & I2C_MSG_RW_MASK) == I2C_MSG_READ) {
			if (data->response_pos >= data->response_len) {
				/* no response: the status after the wake-up */
				atecc608_respond_status(data, STATUS_AFTER_WAKE);
			}
			for (uint32_t j = 0; j < msg->len; j++) {
				msg->buf[j] = data->response_pos < data->response_len ?
								  data->response[data->response_pos++] :
								  0xff;
			}
			continue;
		}

		if (msg->len == 0) {
			continue;
		}
		switch (msg->buf[0]) {
		case WORD_ADDR_COMMAND:
			atecc608_command(data, &msg->buf[1], msg->len - 1);
			break;
		case WORD_ADDR_RESET:
		case WORD_ADDR_SLEEP:
		case WORD_ADDR_IDLE:
		default:
			data->response_len = 0;
			data->response_pos = 0;
			break;
		}
	}

	return 0;
}

static const struct i2c_emul_api emul_atecc608_api = {
	.transfer = emul_atecc608_transfer,
};

static int emul_atecc608_init(const struct emul *target, const struct device *parent) {
	struct emul_atecc608_data *data = target->data;

	ARG_UNUSED(parent);

	memset(data, 0, sizeof(*data));

	return 0;
}

/* the emulator framework refers to a device on the node: define one, with no API */
#define EMUL_ATECC608_DEFINE(n)                                                                    \
	static struct emul_atecc608_data emul_atecc608_data_##n;                                       \
	DEVICE_DT_INST_DEFINE(n, NULL, NULL, NULL, NULL, POST_KERNEL, CONFIG_I2C_INIT_PRIORITY, NULL); \
	EMUL_DT_INST_DEFINE(n, emul_atecc608_init, &emul_atecc608_data_##n, NULL, &emul_atecc608_api,  \
						NULL)

DT_INST_FOREACH_STATUS_OKAY(EMUL_ATECC608_DEFINE)
