/*
 * SerialBridge - bridge the board's USB serial port to a coprocessor UART,
 * with the USB control lines driving the coprocessor boot and reset pins.
 *
 * Coprocessors are flashed over a UART ROM bootloader that the vendor's PC
 * tool enters by toggling the serial DTR/RTS lines as control signals wired to
 * the coprocessor. This sketch mirrors those lines onto the coprocessor boot
 * and reset pins, following the usual host flasher convention:
 *
 *   DTR -> boot (strap)     RTS -> reset (enable)
 *
 * so the coprocessor can be flashed straight through the board's USB port with
 * the vendor's usual tool.
 *
 * Note: do not use WiFi or BLE while this sketch runs; it drives the
 * coprocessor's control pins and holds its UART.
 */

#include <Arduino.h>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/ring_buffer.h>

#define BRIDGE_BUF_SIZE 4096
#define CHUNK_SIZE      64
#define DEFAULT_BAUDRATE 115200

/* Boards without a coprocessor bridge build this sketch as a no-op. */
#if DT_NODE_HAS_PROP(DT_PATH(zephyr_user), cdc_acm_serial) && \
	DT_NODE_HAS_PROP(DT_PATH(zephyr_user), coprocessor_serial) && \
	DT_NODE_HAS_PROP(DT_PATH(zephyr_user), coprocessor_boot_gpios) && \
	DT_NODE_HAS_PROP(DT_PATH(zephyr_user), coprocessor_reset_gpios)

/* The CDC-ACM port connected to the host */
static const struct device *const host_dev =
	DEVICE_DT_GET(DT_PHANDLE(DT_PATH(zephyr_user), cdc_acm_serial));

/* The UART connected to the coprocessor. */
static const struct device *const coproc_dev =
	DEVICE_DT_GET(DT_PHANDLE(DT_PATH(zephyr_user), coprocessor_serial));

/* The coprocessor boot strap and reset lines */
static const struct gpio_dt_spec boot_gpio =
	GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), coprocessor_boot_gpios);

static const struct gpio_dt_spec reset_gpio =
	GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), coprocessor_reset_gpios);

RING_BUF_DECLARE(host_to_coproc, BRIDGE_BUF_SIZE);
RING_BUF_DECLARE(coproc_to_host, BRIDGE_BUF_SIZE);

struct bridge_port {
	const struct device *uart; /* This side of the bridge */
	const struct device *peer; /* The other side */
	struct ring_buf *rx;       /* Bytes received here, on their way to peer */
	struct ring_buf *tx;       /* Bytes waiting to go out of this side */
	volatile bool rx_paused;
};

static struct bridge_port host = {
	.uart = host_dev,
	.peer = coproc_dev,
	.rx = &host_to_coproc,
	.tx = &coproc_to_host,
	.rx_paused = false,
};

static struct bridge_port coproc = {
	.uart = coproc_dev,
	.peer = host_dev,
	.rx = &coproc_to_host,
	.tx = &host_to_coproc,
	.rx_paused = false,
};

/*
 * One handler for both ends: drain the RX FIFO into the peer's
 * TX ring, and refill this side's TX FIFO from our own TX ring.
 */
static void uart_irq_callback(const struct device *dev, void *user_data) {
	uint8_t chunk[CHUNK_SIZE];
	struct bridge_port *port = static_cast<struct bridge_port *>(user_data);

	uart_irq_update(dev);

	while (uart_irq_rx_ready(dev)) {
		uint32_t space = ring_buf_space_get(port->rx);

		if (space == 0) {
			/* 
       * Stop draining the FIFO instead of dropping bytes.
       * loop() re-enables RX once the ring has room again.
			 */
			uart_irq_rx_disable(dev);
			port->rx_paused = true;
			break;
		}

		int len = uart_fifo_read(dev, chunk, MIN(space, sizeof(chunk)));
		if (len <= 0) {
			break;
		}

		ring_buf_put(port->rx, chunk, len);
		uart_irq_tx_enable(port->peer);
	}

	while (uart_irq_tx_ready(dev)) {
		uint8_t *data;
		uint32_t len = ring_buf_get_claim(port->tx, &data, CHUNK_SIZE);

		if (len == 0) {
			ring_buf_get_finish(port->tx, 0);
			uart_irq_tx_disable(dev);
			break;
		}

		int sent = uart_fifo_fill(dev, data, len);
		ring_buf_get_finish(port->tx, sent > 0 ? sent : 0);
		if (sent <= 0) {
			break;
		}
	}
}

static void bridge_resume(struct bridge_port *port) {
	if (port->rx_paused && ring_buf_space_get(port->rx) >= CHUNK_SIZE) {
		port->rx_paused = false;
		uart_irq_rx_enable(port->uart);
	}
}

/*
 * Re/start one side of the bridge. If configure is true, the UART
 * is first set to the specified baudrate, 8N1 and no flow control.
 * The CDC-ACM side line settings come from the USB host.
 */
static void bridge_port_start(struct bridge_port *port, bool configure, uint32_t baudrate) {
	uart_irq_rx_disable(port->uart);
	uart_irq_tx_disable(port->uart);

	if (configure) {
		struct uart_config cfg = {
			.baudrate = baudrate,
			.parity = UART_CFG_PARITY_NONE,
			.stop_bits = UART_CFG_STOP_BITS_1,
			.data_bits = UART_CFG_DATA_BITS_8,
			.flow_ctrl = UART_CFG_FLOW_CTRL_NONE,
		};

		uart_configure(port->uart, &cfg);
	}

	uart_irq_callback_user_data_set(port->uart, uart_irq_callback, port);
	port->rx_paused = false;
	uart_irq_rx_enable(port->uart);
}

/*
 * Follow the host: DTR drives boot and RTS drives reset, with the pin flags
 * setting the polarity, and the line coding sets the coprocessor's baud rate.
 */
static void host_lines_update(void) {
	struct uart_config cfg;
	uint32_t dtr = 0, rts = 0, baudrate = 0;

	(void)uart_line_ctrl_get(host_dev, UART_LINE_CTRL_DTR, &dtr);
	(void)uart_line_ctrl_get(host_dev, UART_LINE_CTRL_RTS, &rts);

	gpio_pin_set_dt(&boot_gpio, (int)dtr);
	gpio_pin_set_dt(&reset_gpio, (int)rts);

	if (uart_line_ctrl_get(host_dev, UART_LINE_CTRL_BAUD_RATE, &baudrate) == 0 &&
		baudrate != 0 && uart_config_get(coproc_dev, &cfg) == 0 && cfg.baudrate != baudrate) {
		bridge_port_start(&coproc, true, baudrate);
	}
}

void setup() {
	/*
   * The flashing tool drives the control GPIOs (boot/reset)
   * sequence using the UART's control lines (DTR/RTS).
	 */
	gpio_pin_configure_dt(&boot_gpio, GPIO_OUTPUT_INACTIVE);
	gpio_pin_configure_dt(&reset_gpio, GPIO_OUTPUT_INACTIVE);

	bridge_port_start(&host, false, 0);
	bridge_port_start(&coproc, true, DEFAULT_BAUDRATE);
}

void loop() {
	bridge_resume(&host);
	bridge_resume(&coproc);
	host_lines_update();
	k_sleep(K_USEC(250));
}

#else

void setup() {
}

void loop() {
}

#endif
