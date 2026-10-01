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
	/* dev is a `const struct device *` (kept opaque so Arduino.h stays free of Zephyr) */
	explicit HardwareSerial(const void *dev) : _dev(dev) {}

	void begin(unsigned long baud) { begin(baud, SERIAL_8N1); }
	void begin(unsigned long baud, uint16_t config);
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
	volatile uint16_t _head = 0, _tail = 0;
	uint8_t _rx[SERIAL_RX_BUFFER_SIZE];
	bool _started = false;
};

#if defined(HAVE_SERIAL0)
extern HardwareSerial Serial;
#endif
#if defined(HAVE_SERIAL1)
extern HardwareSerial Serial1;
#endif

extern void serialEventRun(void);

#endif
