/*
 * WiFiProgrammer - bridge the board's USB serial port to a coprocessor/radio
 * module UART, to flash new firmware onto it or talk to it directly from a
 * PC terminal.
 *
 * At the module's CHIP_EN low-to-high transition (reset), it samples its
 * BOOT strap pin to decide whether to start its UART ROM bootloader (BOOT
 * high: accept new firmware over UART) or run its current application
 * firmware (BOOT low).
 *
 * By default this sketch always drives BOOT high at reset, so the module is
 * ready to be flashed as soon as the board powers up or resets: no further
 * action is needed. Set WIFI_PROGRAMMER_BOOT_BUTTON to 1 below to instead let
 * the board's USER button pick the boot mode, the same way as the original,
 * board-specific mezza_wifi_bridge sample: hold it down while resetting the
 * board to enter the bootloader, leave it released to run the module's
 * current firmware.
 *
 * This sketch relies on the generic 'zephyr,user' device-tree properties
 * 'cdc-acm-serial', 'coprocessor-serial', 'coprocessor-boot-gpios' and
 * 'coprocessor-reset-gpios'; any board that declares them is supported, the
 * same convention used by the WiFi/examples/SerialBridge sketch. The
 * coprocessor UART is intentionally not exposed as one of the board's
 * numbered Serial objects (Serial1, Serial2, ...): it is reserved for this
 * bridge so it doesn't collide with sketches that use the module for WiFi/BLE.
 *
 * Note: do not use WiFi or BLE from a sketch while this one is running; it
 * takes exclusive control of the module's BOOT/CHIP_EN pins and UART.
 */

#include <Arduino.h>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/ring_buffer.h>

/* Set to 1 to let the board's USER button pick the module's boot mode at
 * reset instead of always forcing the UART bootloader. */
#define WIFI_PROGRAMMER_BOOT_BUTTON 0

#define BRIDGE_BUF_SIZE 4096
#define CHUNK_SIZE      64
#define DEFAULT_BAUDRATE 115200

/* Boards without the required coprocessor bridge properties build this
 * sketch as a no-op. */
#if DT_NODE_HAS_PROP(DT_PATH(zephyr_user), cdc_acm_serial) && \
	DT_NODE_HAS_PROP(DT_PATH(zephyr_user), coprocessor_serial) && \
	DT_NODE_HAS_PROP(DT_PATH(zephyr_user), coprocessor_boot_gpios) && \
	DT_NODE_HAS_PROP(DT_PATH(zephyr_user), coprocessor_reset_gpios)

/* The CDC-ACM port connected to the host */
static const struct device *const host_dev =
	DEVICE_DT_GET(DT_PHANDLE(DT_PATH(zephyr_user), cdc_acm_serial));

/* The UART connected to the coprocessor/radio module. */
static const struct device *const coproc_dev =
	DEVICE_DT_GET(DT_PHANDLE(DT_PATH(zephyr_user), coprocessor_serial));

/* The module's BOOT strap and CHIP_EN/reset lines. */
static const struct gpio_dt_spec boot_gpio =
	GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), coprocessor_boot_gpios);

static const struct gpio_dt_spec reset_gpio =
	GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), coprocessor_reset_gpios);

#if WIFI_PROGRAMMER_BOOT_BUTTON
#if DT_NODE_EXISTS(DT_ALIAS(button))
/* The board's USER button, used to pick the module's boot mode. */
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(DT_ALIAS(button), gpios);
#else
#error "WIFI_PROGRAMMER_BOOT_BUTTON requires a 'button' alias in the board's device tree"
#endif
#endif

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
		uint32_t len = ring_buf_get_ptr(port->tx, &data, 0);

		if (len == 0) {
			uart_irq_tx_disable(dev);
			break;
		}

		int sent = uart_fifo_fill(dev, data, MIN(len, CHUNK_SIZE));
		if (sent > 0) {
			ring_buf_consume(port->tx, sent);
		}
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
 * Keep the coprocessor UART's baud rate in sync with the host's. Flashing
 * tools commonly renegotiate a higher speed mid-session (e.g. switching to
 * 2 Mbps once the module's bootloader handshake succeeds); without this the
 * bridge would keep talking to the module at the fixed DEFAULT_BAUDRATE and
 * every command after the switch would fail.
 */
static void host_baud_update(void) {
	struct uart_config cfg;
	uint32_t baudrate = 0;

	if (uart_line_ctrl_get(host_dev, UART_LINE_CTRL_BAUD_RATE, &baudrate) == 0 &&
		baudrate != 0 && uart_config_get(coproc_dev, &cfg) == 0 && cfg.baudrate != baudrate) {
		bridge_port_start(&coproc, true, baudrate);
	}
}

static bool coproc_enabled = false;

void setup() {
	gpio_pin_configure_dt(&boot_gpio, GPIO_OUTPUT_INACTIVE);
	/*
	 * Some radio/coprocessor drivers (e.g. this board's WiFi driver) may
	 * already have driven the reset/CHIP_EN line high while the system
	 * was booting, before this sketch had a chance to set BOOT, so the
	 * module could have already latched the wrong boot mode. Reconfiguring
	 * it here forces it back low, ready for a fresh low-to-high transition
	 * once the host has the port open, below.
	 */
	gpio_pin_configure_dt(&reset_gpio, GPIO_OUTPUT_INACTIVE);

#if WIFI_PROGRAMMER_BOOT_BUTTON
	gpio_pin_configure_dt(&button, GPIO_INPUT);
	if (gpio_pin_get_dt(&button) == 1) {
		/* USER button held: ask the module to start its UART bootloader. */
		gpio_pin_set_dt(&boot_gpio, 1);
	}
#else
	/* Always come up in the UART bootloader, ready to be flashed. */
	gpio_pin_set_dt(&boot_gpio, 1);
#endif

	bridge_port_start(&host, false, 0);
	bridge_port_start(&coproc, true, DEFAULT_BAUDRATE);
}

void loop() {
	bridge_resume(&host);
	bridge_resume(&coproc);
	host_baud_update();

	/* Release the module from reset only once the host has the USB serial
	 * port open, so none of the handshake bytes are lost or missed.
	 */
	if (!coproc_enabled && Serial) {
		gpio_pin_set_dt(&reset_gpio, 1);
		coproc_enabled = true;
	}

	k_sleep(K_USEC(250));
}

#else

void setup() {
}

void loop() {
}

#endif
