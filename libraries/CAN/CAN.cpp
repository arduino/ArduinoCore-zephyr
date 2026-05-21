/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <CAN.h>
#include <string.h>
#include <zephyr/sys/util_macro.h>

/* -------------------------------------------------------------------------- */
/*  Arduino CAN implementation                                                */
/* -------------------------------------------------------------------------- */

arduino::ZephyrCAN::ZephyrCAN(const struct device *can_dev)
	: _dev(can_dev), _filter_id_std(-1), _filter_id_ext(-1), _std_filter_configured(false),
	  _ext_filter_configured(false), _std_filter{.id = 0U, .mask = 0U, .flags = 0U},
	  _ext_filter{.id = 0U, .mask = 0U, .flags = CAN_FILTER_IDE}, _tx_frame_flags(0U),
	  _rx_user_callback(nullptr), _rx_user_data(nullptr), _cb_thread_started(false) {
	k_msgq_init(&_rx_msgq, reinterpret_cast<char *>(_rxbuf), sizeof(struct can_frame), 32U);
	k_msgq_init(&_cb_msgq, reinterpret_cast<char *>(_cbbuf), sizeof(struct can_frame), 16U);
	k_sem_init(&_cb_sem, 0U, K_SEM_MAX_LIMIT);
}

bool arduino::ZephyrCAN::begin(CanBitRate can_bitrate) {
	return _begin(can_bitrate, CanMode::Classic, static_cast<uint32_t>(can_bitrate), false);
}

bool arduino::ZephyrCAN::beginFD(CanBitRate arbitration_bitrate, uint32_t data_bitrate,
								 bool bitrate_switch) {
	return _begin(arbitration_bitrate, CanMode::FD, data_bitrate, bitrate_switch);
}

bool arduino::ZephyrCAN::_begin(CanBitRate arbitration_bitrate, CanMode mode, uint32_t data_bitrate,
								bool bitrate_switch) {
	/* Init device and re-apply DEFAULT pinctrl state so shared pins
	 * are remuxed back to CAN after other peripherals have used them.
	 */
	(void)zephyr::arduino::init_dev_apply_pinctrl(_dev);

	/* Bitrate can only be changed while the controller is stopped. */
	(void)can_stop(_dev);

	int ret;
	if (mode == CanMode::FD) {
		can_mode_t capabilities = CAN_MODE_NORMAL;
		ret = can_get_capabilities(_dev, &capabilities);
		if (ret < 0 || (capabilities & CAN_MODE_FD) == 0U) {
			return false;
		}

		ret = can_set_mode(_dev, CAN_MODE_FD);
		if (ret < 0) {
			return false;
		}
	} else {
		ret = can_set_mode(_dev, CAN_MODE_NORMAL);
		if (ret < 0) {
			return false;
		}
	}

	ret = can_set_bitrate(_dev, static_cast<uint32_t>(arbitration_bitrate));
	if (ret < 0) {
		return false;
	}

	if (mode == CanMode::FD) {
#if defined(CONFIG_CAN_FD_MODE)
		ret = can_set_bitrate_data(_dev, data_bitrate);
		if (ret < 0) {
			return false;
		}

		_tx_frame_flags = CAN_FRAME_FDF;
		if (bitrate_switch) {
			_tx_frame_flags |= CAN_FRAME_BRS;
		}
#else
		ARG_UNUSED(data_bitrate);
		ARG_UNUSED(bitrate_switch);
		return false;
#endif
	} else {
		_tx_frame_flags = 0U;
	}

	ret = can_start(_dev);
	if (ret < 0 && ret != -EALREADY) {
		return false;
	}

	if (!_cb_thread_started) {
		k_tid_t const tid = k_thread_create(
			&_cb_thread, _cb_thread_stack, K_KERNEL_STACK_SIZEOF(_cb_thread_stack),
			&_callback_thread_entry, this, nullptr, nullptr, K_PRIO_PREEMPT(10), 0, K_NO_WAIT);
		if (tid == nullptr) {
			return false;
		}
		k_thread_name_set(&_cb_thread, "arduino_can_cb");
		_cb_thread_started = true;
	}

	if (_std_filter_configured) {
		_filter_id_std = can_add_rx_filter(_dev, _rx_callback, this, &_std_filter);
	}

	if (_ext_filter_configured) {
		_filter_id_ext = can_add_rx_filter(_dev, _rx_callback, this, &_ext_filter);
	}

	if (!_std_filter_configured && !_ext_filter_configured) {
		/* No pre-configured filters: keep accept-all behavior for both ID types. */
		const struct can_filter std_filter = {
			.id = 0U,
			.mask = 0U,
			.flags = 0U,
		};
		_filter_id_std = can_add_rx_filter(_dev, _rx_callback, this, &std_filter);

		const struct can_filter ext_filter = {
			.id = 0U,
			.mask = 0U,
			.flags = CAN_FILTER_IDE,
		};
		_filter_id_ext = can_add_rx_filter(_dev, _rx_callback, this, &ext_filter);
	}

	/* Succeed if at least one filter was installed. */
	return (_filter_id_std >= 0 || _filter_id_ext >= 0);
}

void arduino::ZephyrCAN::end() {
	if (_filter_id_std >= 0) {
		can_remove_rx_filter(_dev, _filter_id_std);
		_filter_id_std = -1;
	}
	if (_filter_id_ext >= 0) {
		can_remove_rx_filter(_dev, _filter_id_ext);
		_filter_id_ext = -1;
	}
	(void)can_stop(_dev);
}

/* -------------------------------------------------------------------------- */
/* Template implementations                                                   */
/* -------------------------------------------------------------------------- */

template <typename MsgType> int arduino::ZephyrCAN::_write_impl(MsgType const &msg) {
	struct can_frame frame;

	memset(&frame, 0, sizeof(frame));

	if (msg.isExtendedId()) {
		frame.id = msg.getExtendedId();
		frame.flags = CAN_FRAME_IDE | _tx_frame_flags;
	} else {
		frame.id = msg.getStandardId();
		frame.flags = _tx_frame_flags;
	}

	frame.dlc = can_bytes_to_dlc(msg.data_length);
	memcpy(frame.data, msg.data, msg.data_length);

	int ret = can_send(_dev, &frame, K_MSEC(100), NULL, NULL);
	return (ret == 0) ? 1 : -1;
}

template <typename MsgType> bool arduino::ZephyrCAN::_read_impl(MsgType &msg) {
	struct can_frame frame;

	if (k_msgq_get(&_rx_msgq, &frame, K_NO_WAIT) != 0) {
		return false;
	}

	uint32_t id;
	if (frame.flags & CAN_FRAME_IDE) {
		id = CanExtendedId(frame.id);
	} else {
		id = CanStandardId(frame.id);
	}

	uint8_t const data_length = can_dlc_to_bytes(frame.dlc);
	msg.id = id;
	msg.data_length = MIN(data_length, MsgType::MAX_DATA_LENGTH);
	memcpy(msg.data, frame.data, msg.data_length);

	return true;
}

int arduino::ZephyrCAN::write(CanMsg const &msg) {
	return _write_impl(msg);
}

int arduino::ZephyrCAN::writeFD(CanFDMsg const &msg) {
	if (_tx_frame_flags == 0U) {
		/* FD mode not enabled */
		return -1;
	}
	return _write_impl(msg);
}

size_t arduino::ZephyrCAN::available() {
	return k_msgq_num_used_get(&_rx_msgq);
}

arduino::CanMsg arduino::ZephyrCAN::read() {
	CanMsg msg;
	_read_impl(msg);
	return msg;
}

arduino::CanFDMsg arduino::ZephyrCAN::readFD() {
	CanFDMsg msg;
	_read_impl(msg);
	return msg;
}

void arduino::ZephyrCAN::onReceive(ReceiveCallback callback, void *user_data) {
	_rx_user_callback = callback;
	_rx_user_data = user_data;
}

void arduino::ZephyrCAN::clearReceiveCallback() {
	onReceive(nullptr, nullptr);
}

int arduino::ZephyrCAN::addReceiveFilter(uint32_t id, uint32_t mask, bool extended) {
	if (!device_is_ready(_dev)) {
		return -ENODEV;
	}

	/* Filters are configured before begin()/beginFD() and installed at begin-time. */
	if (_filter_id_std >= 0 || _filter_id_ext >= 0) {
		return -EALREADY;
	}

	if (extended) {
		_ext_filter.id = id;
		_ext_filter.mask = mask;
		_ext_filter.flags = CAN_FILTER_IDE;
		_ext_filter_configured = true;
	} else {
		_std_filter.id = id;
		_std_filter.mask = mask;
		_std_filter.flags = 0U;
		_std_filter_configured = true;
	}

	return 1;
}

/* Explicit template instantiation */
template int arduino::ZephyrCAN::_write_impl<arduino::CanMsg>(arduino::CanMsg const &);
template int arduino::ZephyrCAN::_write_impl<arduino::CanFDMsg>(arduino::CanFDMsg const &);
template bool arduino::ZephyrCAN::_read_impl<arduino::CanMsg>(arduino::CanMsg &);
template bool arduino::ZephyrCAN::_read_impl<arduino::CanFDMsg>(arduino::CanFDMsg &);

/* static */ arduino::CanFDMsg arduino::ZephyrCAN::_frame_to_msg(struct can_frame const &frame) {
	uint32_t id;
	if (frame.flags & CAN_FRAME_IDE) {
		id = CanExtendedId(frame.id);
	} else {
		id = CanStandardId(frame.id);
	}

	uint8_t const data_length = can_dlc_to_bytes(frame.dlc);
	return CanFDMsg(id, MIN(data_length, CanFDMsg::MAX_DATA_LENGTH), frame.data);
}

/* static */ void arduino::ZephyrCAN::_callback_thread_entry(void *p1, void *p2, void *p3) {
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	ZephyrCAN *self = static_cast<ZephyrCAN *>(p1);
	struct can_frame frame;

	for (;;) {
		(void)k_sem_take(&self->_cb_sem, K_FOREVER);

		while (k_msgq_get(&self->_cb_msgq, &frame, K_NO_WAIT) == 0) {
			ReceiveCallback const callback = self->_rx_user_callback;
			if (callback != nullptr) {
				CanFDMsg const msg = _frame_to_msg(frame);
				callback(msg, self->_rx_user_data);
			}
		}
	}
}

/* static */ void arduino::ZephyrCAN::_rx_callback(const struct device *dev,
												   struct can_frame *frame, void *user_data) {
	ARG_UNUSED(dev);
	ZephyrCAN *self = static_cast<ZephyrCAN *>(user_data);
	/* Called from interrupt context — K_NO_WAIT is mandatory. */

	if (self->_rx_user_callback != nullptr) {
		/* Callback-exclusive mode: enqueue only to callback queue, not polling. */
		if (k_msgq_put(&self->_cb_msgq, frame, K_NO_WAIT) == 0) {
			k_sem_give(&self->_cb_sem);
		}
	} else {
		/* Polling mode: enqueue only to polling queue. */
		(void)k_msgq_put(&self->_rx_msgq, frame, K_NO_WAIT);
	}
}

/* -------------------------------------------------------------------------- */
/*  Global object definitions (Device Tree driven)                            */
/* -------------------------------------------------------------------------- */

#if DT_NODE_HAS_PROP(DT_PATH(zephyr_user), cans)

#if (DT_PROP_LEN(DT_PATH(zephyr_user), cans) > 1)

#define ARDUINO_CAN_DEFINED_0 1
#define DECL_CAN_0(n, p, i)   arduino::ZephyrCAN CAN(DEVICE_DT_GET(DT_PHANDLE_BY_IDX(n, p, i)));
#define DECL_CAN_N(n, p, i)   arduino::ZephyrCAN CAN##i(DEVICE_DT_GET(DT_PHANDLE_BY_IDX(n, p, i)));
#define DECLARE_CAN_N(n, p, i)                                                                     \
	COND_CODE_1(ARDUINO_CAN_DEFINED_##i, (DECL_CAN_0(n, p, i)), (DECL_CAN_N(n, p, i)))

DT_FOREACH_PROP_ELEM(DT_PATH(zephyr_user), cans, DECLARE_CAN_N)

#undef DECLARE_CAN_N
#undef DECL_CAN_N
#undef DECL_CAN_0
#undef ARDUINO_CAN_DEFINED_0

#elif (DT_PROP_LEN(DT_PATH(zephyr_user), cans) == 1)

arduino::ZephyrCAN CAN(DEVICE_DT_GET(DT_PHANDLE_BY_IDX(DT_PATH(zephyr_user), cans, 0)));

#endif /* DT_PROP_LEN */

#else

#error "CAN library requires zephyr,user.cans in devicetree"

#endif /* DT_NODE_HAS_PROP */
