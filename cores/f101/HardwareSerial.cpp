/* SPDX-License-Identifier: Apache-2.0 */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/irq.h>

#include "Arduino.h"

#define DEV_SERIAL0 DT_CHOSEN(zephyr_console)

#define DEV_UART_OKAY(n) DT_NODE_HAS_STATUS(DT_NODELABEL(uart##n), okay)
#define DEV_IS_CONSOLE(n) DT_SAME_NODE(DT_NODELABEL(uart##n), DEV_SERIAL0)

HardwareSerial Serial(DEVICE_DT_GET(DEV_SERIAL0));

/* Serial1 is the first enabled UART that is not the console */
#if DEV_UART_OKAY(5) && !DEV_IS_CONSOLE(5)
#define DEV_SERIAL1 DT_NODELABEL(uart5)
#elif DEV_UART_OKAY(2) && !DEV_IS_CONSOLE(2)
#define DEV_SERIAL1 DT_NODELABEL(uart2)
#elif DEV_UART_OKAY(1) && !DEV_IS_CONSOLE(1)
#define DEV_SERIAL1 DT_NODELABEL(uart1)
#endif
#ifdef DEV_SERIAL1
HardwareSerial Serial1(DEVICE_DT_GET(DEV_SERIAL1));
#endif

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
#ifdef DEV_SERIAL1
	if (serialEvent1 && Serial1.available()) {
		serialEvent1();
	}
#endif
}
