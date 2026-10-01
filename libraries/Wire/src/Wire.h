/* SPDX-License-Identifier: Apache-2.0 */
#ifndef TwoWire_h
#define TwoWire_h

#include <stdint.h>
#include "Arduino.h"

#define BUFFER_LENGTH 128

class TwoWire : public Stream {
public:
	/* dev is a `const struct device *` */
	explicit TwoWire(const void *dev) : _dev(dev) {}

	void begin();
	void begin(uint8_t address) { begin(); }
	void end();
	void setClock(uint32_t hz);

	void beginTransmission(uint8_t address);
	void beginTransmission(int address) { beginTransmission((uint8_t)address); }
	/* 0 ok, 1 too long, 2 NACK on address, 3 NACK on data, 4 other, 5 timeout */
	uint8_t endTransmission(bool sendStop = true);
	uint8_t requestFrom(uint8_t address, uint8_t quantity, uint8_t sendStop = true);
	uint8_t requestFrom(int address, int quantity) { return requestFrom((uint8_t)address, (uint8_t)quantity, (uint8_t) true); }
	uint8_t requestFrom(int address, int quantity, int sendStop)
	{
		return requestFrom((uint8_t)address, (uint8_t)quantity, (uint8_t)sendStop);
	}

	size_t write(uint8_t data) override;
	size_t write(const uint8_t *data, size_t n) override;
	using Print::write;
	int available(void) override { return _rxLen - _rxPos; }
	int read(void) override { return _rxPos < _rxLen ? _rx[_rxPos++] : -1; }
	int peek(void) override { return _rxPos < _rxLen ? _rx[_rxPos] : -1; }
	void flush(void) override {}

	/* peripheral mode is not implemented; the handlers are stored and never called */
	void onReceive(void (*f)(int)) { _onReceive = f; }
	void onRequest(void (*f)(void)) { _onRequest = f; }

private:
	const void *_dev;
	bool _begun = false;
	uint8_t _addr = 0;
	uint8_t _tx[BUFFER_LENGTH];
	uint8_t _txLen = 0;
	uint8_t _rx[BUFFER_LENGTH];
	uint8_t _rxLen = 0, _rxPos = 0;
	void (*_onReceive)(int) = nullptr;
	void (*_onRequest)(void) = nullptr;
};

#ifdef HAVE_WIRE
extern TwoWire Wire;
#endif

#endif
