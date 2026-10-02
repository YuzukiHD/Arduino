/* SPDX-License-Identifier: Apache-2.0 */
#ifndef HardwareSerial_h
#define HardwareSerial_h

#include <stdint.h>
#include "Stream.h"

#define SERIAL_5N1 0x00
#define SERIAL_6N1 0x02
#define SERIAL_7N1 0x04
#define SERIAL_8N1 0x06
#define SERIAL_5N2 0x08
#define SERIAL_6N2 0x0A
#define SERIAL_7N2 0x0C
#define SERIAL_8N2 0x0E
#define SERIAL_5E1 0x20
#define SERIAL_6E1 0x22
#define SERIAL_7E1 0x24
#define SERIAL_8E1 0x26
#define SERIAL_5E2 0x28
#define SERIAL_6E2 0x2A
#define SERIAL_7E2 0x2C
#define SERIAL_8E2 0x2E
#define SERIAL_5O1 0x30
#define SERIAL_6O1 0x32
#define SERIAL_7O1 0x34
#define SERIAL_8O1 0x36
#define SERIAL_5O2 0x38
#define SERIAL_6O2 0x3A
#define SERIAL_7O2 0x3C
#define SERIAL_8O2 0x3E

#define SERIAL_RX_BUFFER_SIZE 256

class HardwareSerial : public Stream {
public:
	/* dev is a `const struct device *` (kept opaque so Arduino.h stays free of Zephyr),
	 * uart is the number of the hardware UART (0..5) */
	HardwareSerial(const void *dev, int uart, bool fixedPins = false)
		: _dev(dev), _uart(uart), _fixed(fixedPins) {}

	/*
	 * Choose the pins before begin(), or pass them to begin(). Only combinations the
	 * chip can route to this UART are accepted (see HardwareSerial.cpp); returns false
	 * otherwise. Without a call the first pair of the table is used.
	 * rx/tx are Arduino pin numbers (PE9, PE8, ...), -1 keeps the current one.
	 */
	bool setPins(int rx, int tx);
	int rxPin() const { return _rxPin; }
	int txPin() const { return _txPin; }
	int uartNumber() const { return _uart; }

	void begin(unsigned long baud) { begin(baud, SERIAL_8N1); }
	void begin(unsigned long baud, uint16_t config);
	void begin(unsigned long baud, uint16_t config, int rx, int tx)
	{
		if (setPins(rx, tx)) {
			begin(baud, config);
		}
	}
	void end();
	int available(void) override;
	int peek(void) override;
	int read(void) override;
	int availableForWrite(void) override { return 64; }
	void flush(void) override;
	size_t write(uint8_t c) override;
	size_t write(const uint8_t *buf, size_t n) override;
	using Print::write;
	operator bool() { return _dev != nullptr; }

	/* called from the UART interrupt */
	void _isr(void);

private:
	const void *_dev;
	int _uart;
	int _rxPin = -1, _txPin = -1;
	bool _fixed; /* the pins come from the board devicetree (the console): begin() does not route */
	volatile uint16_t _head = 0, _tail = 0;
	uint8_t _rx[SERIAL_RX_BUFFER_SIZE];
	bool _started = false;
};

/*
 * Serial  console UART (UART3, PE8/PE9 until setPins)
 * Serial1 UART5   Serial2 UART2   Serial3 UART1   Serial4 UART0   Serial5 UART4
 * A UART that the variant does not enable is not defined: using it fails at link time.
 */
extern HardwareSerial Serial;
extern HardwareSerial Serial1;
extern HardwareSerial Serial2;
extern HardwareSerial Serial3;
extern HardwareSerial Serial4;
extern HardwareSerial Serial5;

extern void serialEventRun(void);

#endif
