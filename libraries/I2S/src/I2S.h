/* SPDX-License-Identifier: Apache-2.0 */
#ifndef I2S_h
#define I2S_h

#include <stdint.h>
#include "Arduino.h"
#include "AudioEngine.h"

#define I2S_PHILIPS_MODE 0
#define I2S_RIGHT_JUSTIFIED_MODE 1
#define I2S_LEFT_JUSTIFIED_MODE 2

/*
 * I2S0 as a bus master with stereo frames. begin() opens TX and RX unless
 * setDirections() says otherwise. Frames are written as left, right samples:
 * 16 bit samples take 2 bytes, 24 and 32 bit samples 4 bytes.
 *
 * As a Stream: write() takes raw PCM bytes (whole frames), read() gives bytes
 * of the received PCM. The sample helpers move one sample at a time.
 *
 * Without the pin overlay (libraries/I2S/pins_PE0_PE4.overlay) no pin is
 * connected: use setLoopback(true) to run TX into RX inside the chip.
 */
class I2SClass : public Stream {
public:
	void setLoopback(bool on) { _loop = on; }
	void setDirections(bool tx, bool rx) { _tx = tx; _rx = rx; }
	/* call before begin() */

	bool begin(int mode, long sampleRate, int bitsPerSample);
	bool begin(long sampleRate, int bitsPerSample) { return begin(I2S_PHILIPS_MODE, sampleRate, bitsPerSample); }
	void end();
	bool active() const { return _e.isOpen(); }

	size_t write(uint8_t b) override { return write(&b, 1); }
	size_t write(const uint8_t *buf, size_t n) override;
	using Print::write;
	size_t write(const void *buf, size_t n) { return write((const uint8_t *)buf, n); }
	/* one sample of the left / right channel (the frame goes out with the right one) */
	size_t writeSample(int32_t sample);
	int32_t readSample();
	int availableForWrite() override { return (int)_e.writeSpace(); }
	void flush() override { _e.flush(); }

	int available() override { return (int)_e.readAvailable(); }
	int read() override;
	int peek() override;
	int level() { return _e.rxPeak(); }
	void dropInput() { _e.dropInput(); }
	uint32_t overruns() const { return _e.rxOverruns(); }

private:
	AudioEngine _e;
	bool _loop = false, _tx = true, _rx = true;
	int32_t _left = 0;
	bool _haveLeft = false;
};

extern I2SClass I2S;

#endif
