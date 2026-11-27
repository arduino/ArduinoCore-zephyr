/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Emulated I2C device with a register file (arduino,emul-i2c-regs): the
 * sensors of the boards, emulated by their registers with constant values,
 * so that their libraries read known data.
 */

#define DT_DRV_COMPAT arduino_emul_i2c_regs

#include <string.h>

#include <zephyr/device.h>
#include <zephyr/drivers/emul.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/i2c_emul.h>

#define REG_COUNT 256

struct emul_i2c_regs_cfg {
	/* (register, value) pairs */
	const uint8_t *regs;
	size_t regs_len;
	/* (register, mask) pairs */
	const uint8_t *self_clearing;
	size_t self_clearing_len;
};

struct emul_i2c_regs_data {
	uint8_t regs[REG_COUNT];
	uint8_t self_clearing[REG_COUNT];
	uint8_t ptr;
};

static int emul_i2c_regs_transfer(const struct emul *target, struct i2c_msg *msgs, int num_msgs,
								  int addr) {
	struct emul_i2c_regs_data *data = target->data;

	ARG_UNUSED(addr);

	for (int i = 0; i < num_msgs; i++) {
		struct i2c_msg *msg = &msgs[i];

		if ((msg->flags & I2C_MSG_RW_MASK) == I2C_MSG_READ) {
			for (uint32_t j = 0; j < msg->len; j++) {
				msg->buf[j] = data->regs[data->ptr++];
			}
			continue;
		}

		/* a write: the register pointer, then the values to store (an
		 * empty write, e.g. a bus scan, changes nothing)
		 */
		for (uint32_t j = 0; j < msg->len; j++) {
			if (j == 0) {
				data->ptr = msg->buf[0];
				continue;
			}
			data->regs[data->ptr] = msg->buf[j] & ~data->self_clearing[data->ptr];
			data->ptr++;
		}
	}

	return 0;
}

static const struct i2c_emul_api emul_i2c_regs_api = {
	.transfer = emul_i2c_regs_transfer,
};

static int emul_i2c_regs_init(const struct emul *target, const struct device *parent) {
	const struct emul_i2c_regs_cfg *cfg = target->cfg;
	struct emul_i2c_regs_data *data = target->data;

	ARG_UNUSED(parent);

	memset(data, 0, sizeof(*data));
	for (size_t i = 0; i + 1 < cfg->regs_len; i += 2) {
		data->regs[cfg->regs[i]] = cfg->regs[i + 1];
	}
	for (size_t i = 0; i + 1 < cfg->self_clearing_len; i += 2) {
		data->self_clearing[cfg->self_clearing[i]] |= cfg->self_clearing[i + 1];
	}

	return 0;
}

#define EMUL_I2C_REGS_PROP(n, prop)                                                                \
	COND_CODE_1(DT_INST_NODE_HAS_PROP(n, prop),                                                    \
				((const uint8_t[])DT_INST_PROP(n, prop)), (NULL))
#define EMUL_I2C_REGS_LEN(n, prop) DT_INST_PROP_LEN_OR(n, prop, 0)

/* the emulator framework refers to a device on the node: define one, with no API */
#define EMUL_I2C_REGS_DEFINE(n)                                                                    \
	static const struct emul_i2c_regs_cfg emul_i2c_regs_cfg_##n = {                                \
		.regs = EMUL_I2C_REGS_PROP(n, regs),                                                       \
		.regs_len = EMUL_I2C_REGS_LEN(n, regs),                                                    \
		.self_clearing = EMUL_I2C_REGS_PROP(n, self_clearing_bits),                                \
		.self_clearing_len = EMUL_I2C_REGS_LEN(n, self_clearing_bits),                             \
	};                                                                                             \
	static struct emul_i2c_regs_data emul_i2c_regs_data_##n;                                       \
	DEVICE_DT_INST_DEFINE(n, NULL, NULL, NULL, NULL, POST_KERNEL, CONFIG_I2C_INIT_PRIORITY, NULL); \
	EMUL_DT_INST_DEFINE(n, emul_i2c_regs_init, &emul_i2c_regs_data_##n, &emul_i2c_regs_cfg_##n,    \
						&emul_i2c_regs_api, NULL)

DT_INST_FOREACH_STATUS_OKAY(EMUL_I2C_REGS_DEFINE)
