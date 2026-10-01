/* SPDX-License-Identifier: Apache-2.0 */
#ifndef SPDIF_h
#define SPDIF_h

#include <stdint.h>
#include "Arduino.h"
#include "AudioEngine.h"

/*
 * S/PDIF (IEC 60958) transmitter, stereo, 16 or 24 bit (24 bit samples are
 * 32-bit words). Rates: 22050, 24000, 32000, 44100, 48000, 88200, 96000,
 * 176400, 192000. The output pin is PE6.
 *
 * The receiver is not usable: with the internal loopback the RX FIFO sees the
 * frames but the RX DMA request never arrives. begin(rate, bits, true, true)
 * opens it anyway for experiments; reads will not return data.
 */
class SPDIFClass : public Print {
public:
	bool begin(uint32_t sampleRate = 48000, uint8_t bitsPerSample = 16, bool rx = false,
		   bool loopback = false);
	void end();
	bool active() const { return _e.isOpen(); }

	size_t write(uint8_t b) override { return write(&b, 1); }
	/* raw PCM bytes, whole frames (left, right) */
	size_t write(const uint8_t *buf, size_t n) override;
	using Print::write;
	int availableForWrite() override { return (int)_e.writeSpace(); }
	void flush() override { _e.flush(); }

	/* experimental */
	size_t readBytes(uint8_t *dst, size_t n, uint32_t timeoutMs = 100) { return _e.read(dst, n, timeoutMs); }
	int available() { return (int)_e.readAvailable(); }

private:
	AudioEngine _e;
};

extern SPDIFClass SPDIF;

#endif
