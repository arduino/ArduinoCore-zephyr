/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <Arduino.h>
#include <zephyr/drivers/can.h>
#include <zephyr/kernel.h>
#include <api/HardwareCAN.h>
#include <zephyrPinctrl.h>

namespace arduino {

enum class CanMode : uint8_t {
	Classic,
	FD,
};

/**
 * CAN FD message class supporting payloads up to 64 bytes.
 * Maintains API compatibility with CanMsg while extending data capacity.
 */
class CanFDMsg : public Printable {
public:
	static constexpr uint8_t MAX_DATA_LENGTH = 64;

	static constexpr uint32_t CAN_EFF_FLAG = 0x80000000U;
	static constexpr uint32_t CAN_SFF_MASK = 0x000007FFU;
	static constexpr uint32_t CAN_EFF_MASK = 0x1FFFFFFFU;

	CanFDMsg(uint32_t can_id, uint8_t can_data_len, uint8_t const *can_data_ptr)
		: id(can_id), data_length(MIN(can_data_len, MAX_DATA_LENGTH)), data() {
		if (data_length && can_data_ptr) {
			memcpy(data, can_data_ptr, data_length);
		}
	}

	CanFDMsg() : CanFDMsg(0, 0, nullptr) {
	}

	uint32_t getStandardId() const {
		return (id & CAN_SFF_MASK);
	}

	uint32_t getExtendedId() const {
		return (id & CAN_EFF_MASK);
	}

	bool isStandardId() const {
		return ((id & CAN_EFF_FLAG) == 0);
	}

	bool isExtendedId() const {
		return ((id & CAN_EFF_FLAG) == CAN_EFF_FLAG);
	}

	virtual size_t printTo(Print &p) const override {
		char buf[20] = {0};
		size_t len = 0;

		/* Print the header. */
		if (isStandardId()) {
			len =
				snprintf(buf, sizeof(buf), "[%03" PRIX32 "] (%d) : ", getStandardId(), data_length);
		} else {
			len =
				snprintf(buf, sizeof(buf), "[%08" PRIX32 "] (%d) : ", getExtendedId(), data_length);
		}
		size_t n = p.write(buf, len);

		/* Print the data. */
		for (size_t d = 0; d < data_length; d++) {
			len = snprintf(buf, sizeof(buf), "%02X", data[d]);
			n += p.write(buf, len);
		}

		/* Wrap up. */
		return n;
	}

	uint32_t id;
	uint8_t data_length;
	uint8_t data[MAX_DATA_LENGTH];
};

class ZephyrCAN final : public HardwareCAN {
public:
	using ReceiveCallback = void (*)(CanFDMsg const &msg, void *user_data);

	ZephyrCAN(const struct device *can_dev);

	virtual ~ZephyrCAN() {
	}

	bool begin(CanBitRate can_bitrate) override;
	bool beginFD(CanBitRate arbitration_bitrate, uint32_t data_bitrate, bool bitrate_switch = true);
	void end() override;

	int write(CanMsg const &msg) override;
	int writeFD(CanFDMsg const &msg);
	size_t available() override;
	CanMsg read() override;
	CanFDMsg readFD();

	/**
	 * Register a callback to be invoked on CAN message reception.
	 *
	 * When a callback is registered, received messages are delivered ONLY to the callback
	 * and NOT to the polling queue (available()/read()). Messages will not appear in
	 * available()/read() until clearReceiveCallback() is called.
	 *
	 * The callback is executed in a dedicated worker thread, never in ISR context.
	 *
	 * @param callback callback function, or nullptr to disable
	 * @param user_data optional user data passed to callback
	 */
	void onReceive(ReceiveCallback callback, void *user_data = nullptr);

	/**
	 * Disable callback and return to polling mode.
	 *
	 * After calling this, received messages will again be available via available()/read().
	 */
	void clearReceiveCallback();

	/**
	 * Add a receive filter for a specific CAN ID.
	 *
	 * This API configures filters and must be called before begin()/beginFD().
	 *
	 * If no filter is configured before begin(), wildcard filters are installed for
	 * both standard and extended IDs (accept-all behavior).
	 *
	 * At most one configured filter per ID type is tracked by the library:
	 * - one standard (11-bit) filter
	 * - one extended (29-bit) filter
	 * A subsequent call for the same ID type replaces the previous one.
	 *
	 * @param id CAN message ID
	 * @param mask CAN ID mask (0 = accept all, 0x7FF for 11-bit, 0x1FFFFFFF for 29-bit)
	 * @param extended true for 29-bit extended ID, false for 11-bit standard ID
	 * @return 1 on success, or negative error code
	 */
	int addReceiveFilter(uint32_t id, uint32_t mask, bool extended = false);

private:
	const struct device *_dev;
	int _filter_id_std;
	int _filter_id_ext;
	bool _std_filter_configured;
	bool _ext_filter_configured;
	struct can_filter _std_filter;
	struct can_filter _ext_filter;
	uint8_t _tx_frame_flags;
	struct k_msgq _rx_msgq;
	__aligned(4) uint8_t _rxbuf[sizeof(struct can_frame) * 32];
	ReceiveCallback _rx_user_callback;
	void *_rx_user_data;
	struct k_msgq _cb_msgq;
	__aligned(4) uint8_t _cbbuf[sizeof(struct can_frame) * 16];
	struct k_sem _cb_sem;
	struct k_thread _cb_thread;
	K_KERNEL_STACK_MEMBER(_cb_thread_stack, 1024);
	bool _cb_thread_started;

	bool _begin(CanBitRate arbitration_bitrate, CanMode mode, uint32_t data_bitrate,
				bool bitrate_switch);

	/* Template implementations for write/writeFD */
	template <typename MsgType> int _write_impl(MsgType const &msg);

	/* Template implementations for read/readFD */
	template <typename MsgType> bool _read_impl(MsgType &msg);

	static CanFDMsg _frame_to_msg(struct can_frame const &frame);
	static void _callback_thread_entry(void *p1, void *p2, void *p3);

	static void _rx_callback(const struct device *dev, struct can_frame *frame, void *user_data);
};

} /* namespace arduino */

using CanMsg = arduino::CanMsg;
using CanFDMsg = arduino::CanFDMsg;

/* ---- Global object declarations ------------------------------------------ */

#if DT_NODE_HAS_PROP(DT_PATH(zephyr_user), cans) && (DT_PROP_LEN(DT_PATH(zephyr_user), cans) > 1)

#define ARDUINO_CAN_DEFINED_0 1
#define DECL_EXTERN_CAN_0(i)  extern arduino::ZephyrCAN CAN;
#define DECL_EXTERN_CAN_N(i)  extern arduino::ZephyrCAN CAN##i;
#define DECLARE_EXTERN_CAN_N(n, p, i)                                                              \
	COND_CODE_1(ARDUINO_CAN_DEFINED_##i,                                   \
		    (DECL_EXTERN_CAN_0(i)), (DECL_EXTERN_CAN_N(i)))

DT_FOREACH_PROP_ELEM(DT_PATH(zephyr_user), cans, DECLARE_EXTERN_CAN_N)

#undef DECLARE_EXTERN_CAN_N
#undef DECL_EXTERN_CAN_N
#undef DECL_EXTERN_CAN_0
#undef ARDUINO_CAN_DEFINED_0

#elif DT_NODE_HAS_PROP(DT_PATH(zephyr_user), cans)

extern arduino::ZephyrCAN CAN;

#else

#error "CAN library requires zephyr,user.cans in devicetree"

#endif /* DT_NODE_HAS_PROP(zephyr_user, cans) */
