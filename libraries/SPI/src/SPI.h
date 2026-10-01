/* SPDX-License-Identifier: Apache-2.0 */
#ifndef SPI_h
#define SPI_h

#include <stdint.h>
#include "Arduino.h"

#define SPI_MODE0 0x00
#define SPI_MODE1 0x01
#define SPI_MODE2 0x02
#define SPI_MODE3 0x03

class SPISettings {
public:
	SPISettings(uint32_t clock, BitOrder bitOrder, uint8_t dataMode)
		: clock(clock), bitOrder(bitOrder), dataMode(dataMode) {}
	SPISettings() : SPISettings(4000000, MSBFIRST, SPI_MODE0) {}
	uint32_t clock;
	BitOrder bitOrder;
	uint8_t dataMode;
};

class SPIClass {
public:
	/* dev is a `const struct device *` */
	explicit SPIClass(const void *dev) : _dev(dev) {}

	void begin();
	void end();
	void beginTransaction(SPISettings settings) { _s = settings; }
	void endTransaction(void) {}
	void setBitOrder(BitOrder o) { _s.bitOrder = o; }
	void setDataMode(uint8_t m) { _s.dataMode = m; }
	void setClockDivider(uint32_t div) { _s.clock = 100000000UL / (div ? div : 1); }

	uint8_t transfer(uint8_t data);
	uint16_t transfer16(uint16_t data);
	void transfer(void *buf, size_t count);
	void transfer(const void *tx, void *rx, size_t count);

private:
	const void *_dev;
	SPISettings _s;
	bool _begun = false;
};

#ifdef HAVE_SPI
extern SPIClass SPI;
#endif

#endif
