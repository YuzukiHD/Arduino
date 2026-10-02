/* SPDX-License-Identifier: Apache-2.0 */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/irq.h>

#include "Arduino.h"

/* UART n of the SoC -> devicetree node, and the object that owns it */
#define UART_NODE_(n) uart##n
#define UART_NODE(n) DT_NODELABEL(UART_NODE_(n))
#define UART_OKAY(n) DT_NODE_HAS_STATUS(UART_NODE(n), okay)

#define UART_OBJECT(name, n)                                                      \
	IF_ENABLED(UART_OKAY(n), (HardwareSerial name(DEVICE_DT_GET(UART_NODE(n)), n);))

/* the console UART of the board (chosen zephyr,console) is Serial, whatever its number is; its pins
 * come from the board devicetree and are not routed. The other UARTs are Serial1..Serial5. */
#define CONSOLE_UART_IS(n) DT_SAME_NODE(UART_NODE(n), DT_CHOSEN(zephyr_console))
#if CONSOLE_UART_IS(0)
#define ARDUINO_CONSOLE_UART 0
#elif CONSOLE_UART_IS(1)
#define ARDUINO_CONSOLE_UART 1
#elif CONSOLE_UART_IS(2)
#define ARDUINO_CONSOLE_UART 2
#elif CONSOLE_UART_IS(3)
#define ARDUINO_CONSOLE_UART 3
#elif CONSOLE_UART_IS(4)
#define ARDUINO_CONSOLE_UART 4
#elif CONSOLE_UART_IS(5)
#define ARDUINO_CONSOLE_UART 5
#else
#error "the console of the board is not one of UART0..UART5"
#endif
HardwareSerial Serial(DEVICE_DT_GET(UART_NODE(ARDUINO_CONSOLE_UART)), ARDUINO_CONSOLE_UART, true);
#if !CONSOLE_UART_IS(5)
UART_OBJECT(Serial1, 5)
#endif
#if !CONSOLE_UART_IS(2)
UART_OBJECT(Serial2, 2)
#endif
#if !CONSOLE_UART_IS(1)
UART_OBJECT(Serial3, 1)
#endif
#if !CONSOLE_UART_IS(0)
UART_OBJECT(Serial4, 0)
#endif
#if !CONSOLE_UART_IS(4)
UART_OBJECT(Serial5, 4)
#endif

/*
 * Pins that can carry a UART: {rx, tx, mux function}. The first entry is the default.
 * This is the set of routings known to work on the F101 boards; other pins may exist.
 */
struct uart_route {
	int16_t rx, tx;
	uint8_t mux;
};
#define P_(bank, n) ((bank) * 32 + (n))
static const struct uart_route uart_routes[6][3] = {
	/* UART0 */ {{P_(5, 4), P_(5, 2), 3}},
	/* UART1 */ {{P_(5, 1), P_(5, 0), 4}, {P_(1, 1), P_(1, 0), 4}},
	/* UART2 */ {{P_(5, 5), P_(5, 4), 6}},
	/* UART3 */ {{P_(4, 9), P_(4, 8), 6}, {P_(4, 1), P_(4, 0), 6}},
	/* UART4 */ {{P_(4, 3), P_(4, 2), 6}},
	/* UART5 */ {{P_(4, 5), P_(4, 4), 7}},
};

bool HardwareSerial::setPins(int rx, int tx)
{
	if (_uart < 0 || _uart > 5) {
		return false;
	}
	if (rx < 0) {
		rx = _rxPin;
	}
	if (tx < 0) {
		tx = _txPin;
	}
	for (const struct uart_route &r : uart_routes[_uart]) {
		if (r.mux == 0) {
			break;
		}
		if ((rx < 0 || r.rx == rx) && (tx < 0 || r.tx == tx)) {
			if (rx >= 0 && tx >= 0 && !(r.rx == rx && r.tx == tx)) {
				continue;
			}
			/* release the pins of the previous routing, then route the new ones */
			if (_rxPin >= 0 && (_rxPin != r.rx || _txPin != r.tx)) {
				pinctrl_soc_pin_t old[2] = {
					{.pinmux = ALLWINNER_PINMUX(_rxPin >> 5, _rxPin & 31, 0)},
					{.pinmux = ALLWINNER_PINMUX(_txPin >> 5, _txPin & 31, 0)},
				};
				pinctrl_configure_pins(old, 2, PINCTRL_REG_NONE);
			}
			pinctrl_soc_pin_t pins[2] = {
				{.pinmux = ALLWINNER_PINMUX(r.rx >> 5, r.rx & 31, r.mux),
				 .pull = ALLWINNER_PIO_PULL_UP},
				{.pinmux = ALLWINNER_PINMUX(r.tx >> 5, r.tx & 31, r.mux)},
			};
			if (pinctrl_configure_pins(pins, 2, PINCTRL_REG_NONE) < 0) {
				return false;
			}
			_rxPin = r.rx;
			_txPin = r.tx;
			return true;
		}
	}
	return false;
}

static void uart_cb(const struct device *dev, void *user)
{
	ARG_UNUSED(dev);
	static_cast<HardwareSerial *>(user)->_isr();
}

void HardwareSerial::_isr(void)
{
	const struct device *d = (const struct device *)_dev;
	while (uart_irq_update(d) && uart_irq_is_pending(d)) {
		if (uart_irq_rx_ready(d)) {
			uint8_t b;
			while (uart_fifo_read(d, &b, 1) == 1) {
				uint16_t next = (_head + 1) % SERIAL_RX_BUFFER_SIZE;
				if (next != _tail) {
					_rx[_head] = b;
					_head = next;
				}
			}
		}
	}
}

void HardwareSerial::begin(unsigned long baud, uint16_t config)
{
	const struct device *d = (const struct device *)_dev;
	if (!d || !device_is_ready(d)) {
		return;
	}
	if (_rxPin < 0 && !_fixed && !setPins(uart_routes[_uart][0].rx, uart_routes[_uart][0].tx)) {
		return;
	}
	struct uart_config c = {};
	c.baudrate = baud;
	switch (config & 0x30) {
	case 0x20: c.parity = UART_CFG_PARITY_EVEN; break;
	case 0x30: c.parity = UART_CFG_PARITY_ODD; break;
	default: c.parity = UART_CFG_PARITY_NONE; break;
	}
	c.stop_bits = (config & 0x08) ? UART_CFG_STOP_BITS_2 : UART_CFG_STOP_BITS_1;
	switch ((config >> 1) & 0x3) {
	case 0: c.data_bits = UART_CFG_DATA_BITS_5; break;
	case 1: c.data_bits = UART_CFG_DATA_BITS_6; break;
	case 2: c.data_bits = UART_CFG_DATA_BITS_7; break;
	default: c.data_bits = UART_CFG_DATA_BITS_8; break;
	}
	c.flow_ctrl = UART_CFG_FLOW_CTRL_NONE;
	uart_configure(d, &c);

	_head = _tail = 0;
	if (!_started) {
		uart_irq_callback_user_data_set(d, uart_cb, this);
		_started = true;
	}
	uart_irq_rx_enable(d);
}

void HardwareSerial::end()
{
	const struct device *d = (const struct device *)_dev;
	if (d && _started) {
		uart_irq_rx_disable(d);
	}
}

int HardwareSerial::available(void)
{
	return (SERIAL_RX_BUFFER_SIZE + _head - _tail) % SERIAL_RX_BUFFER_SIZE;
}

int HardwareSerial::peek(void)
{
	return _head == _tail ? -1 : _rx[_tail];
}

int HardwareSerial::read(void)
{
	if (_head == _tail) {
		return -1;
	}
	uint8_t c = _rx[_tail];
	_tail = (_tail + 1) % SERIAL_RX_BUFFER_SIZE;
	return c;
}

void HardwareSerial::flush(void)
{
	/* writes are synchronous */
}

size_t HardwareSerial::write(uint8_t c)
{
	const struct device *d = (const struct device *)_dev;
	if (!d) {
		return 0;
	}
	uart_poll_out(d, c);
	return 1;
}

size_t HardwareSerial::write(const uint8_t *buf, size_t n)
{
	for (size_t i = 0; i < n; i++) {
		write(buf[i]);
	}
	return n;
}

void serialEventRun(void)
{
	if (serialEvent && Serial.available()) {
		serialEvent();
	}
#if UART_OKAY(5)
	if (serialEvent1 && Serial1.available()) {
		serialEvent1();
	}
#endif
}
