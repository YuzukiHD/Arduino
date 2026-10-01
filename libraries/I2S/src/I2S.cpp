/* SPDX-License-Identifier: Apache-2.0 */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>

#include "Arduino.h"
#include "I2S.h"

static const struct device *const i2s0 = DEVICE_DT_GET(DT_NODELABEL(i2s0));

I2SClass I2S;

bool I2SClass::begin(int mode, long sampleRate, int bitsPerSample)
{
	AudioEngine::Format f = mode == I2S_LEFT_JUSTIFIED_MODE ? AudioEngine::FORMAT_LEFT_JUSTIFIED
				: mode == I2S_RIGHT_JUSTIFIED_MODE ? AudioEngine::FORMAT_RIGHT_JUSTIFIED
								   : AudioEngine::FORMAT_I2S;
	_haveLeft = false;
	return _e.open(i2s0, sampleRate, bitsPerSample, _tx ? 2 : 0, _rx ? 2 : 0, _loop, f) == 0;
}

void I2SClass::end()
{
	_e.close();
}

size_t I2SClass::write(const uint8_t *buf, size_t n)
{
	return _e.write(buf, n, true);
}

size_t I2SClass::writeSample(int32_t s)
{
	if (!_haveLeft) {
		_left = s;
		_haveLeft = true;
		return 1;
	}
	_haveLeft = false;
	uint8_t frame[8];
	if (_e.sampleBytes() == 2) {
		int16_t a = (int16_t)_left, b = (int16_t)s;
		memcpy(frame, &a, 2);
		memcpy(frame + 2, &b, 2);
		return _e.write(frame, 4, true) == 4 ? 1 : 0;
	}
	memcpy(frame, &_left, 4);
	memcpy(frame + 4, &s, 4);
	return _e.write(frame, 8, true) == 8 ? 1 : 0;
}

int32_t I2SClass::readSample()
{
	uint8_t b[4];
	size_t sb = _e.sampleBytes();
	if (_e.read(b, sb, 100) != sb) {
		return 0;
	}
	if (sb == 2) {
		int16_t v;
		memcpy(&v, b, 2);
		return v;
	}
	int32_t v;
	memcpy(&v, b, 4);
	return v;
}

int I2SClass::read()
{
	uint8_t b;
	return _e.read(&b, 1, 0) == 0 ? -1 : b;
}

int I2SClass::peek()
{
	uint8_t b;
	return _e.peek(&b, 1) == 0 ? -1 : b;
}
