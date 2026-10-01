/* SPDX-License-Identifier: Apache-2.0 */
#include <errno.h>
#include <zephyr/audio/codec.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>

#include "Arduino.h"
#include "Audio.h"

static const struct device *const codec_i2s = DEVICE_DT_GET(DT_NODELABEL(audio_codec));
static const struct device *const analog = DEVICE_DT_GET(DT_NODELABEL(codec_analog));

AudioClass Audio;

static int16_t *sine_table()
{
	static int16_t *t;
	if (!t) {
		t = (int16_t *)malloc(1024 * sizeof(int16_t));
		if (t) {
			for (int i = 0; i < 1024; i++) {
				t[i] = (int16_t)(32767.0 * sin(2.0 * PI * i / 1024.0));
			}
		}
	}
	return t;
}

static audio_property_value_t pv_vol(int v)
{
	audio_property_value_t p;
	p.vol = v;
	return p;
}

static audio_property_value_t pv_mute(bool m)
{
	audio_property_value_t p;
	p.mute = m;
	return p;
}

bool AudioClass::begin(uint32_t sampleRate, uint8_t channels, bool microphone)
{
	if (_e.isOpen()) {
		return false;
	}
	if (channels > 2 || (channels == 0 && !microphone)) {
		return false;
	}
	if (!device_is_ready(codec_i2s) || !device_is_ready(analog)) {
		return false;
	}
	/* the hardware plays mono as a stereo pair, the engine always streams stereo */
	if (_e.open(codec_i2s, sampleRate, 16, channels ? 2 : 0, microphone ? 1 : 0) != 0) {
		return false;
	}
	_outCh = channels;
	_pendLen = 0;
	if (microphone) {
		audio_codec_route_input(analog, AUDIO_CHANNEL_ALL, INPUT_MIC);
	}
	if (channels) {
		setVolume(_vol);
		speaker(true);
	}
	return true;
}

void AudioClass::end()
{
	if (_outOn) {
		_e.flush();
		speaker(false);
	}
	_e.close();
	_outCh = 0;
}

void AudioClass::speaker(bool on)
{
	if (on == _outOn) {
		return;
	}
	if (on) {
		audio_codec_start_output(analog);
	} else {
		audio_codec_stop_output(analog);
	}
	_outOn = on;
}

void AudioClass::setVolume(uint8_t percent)
{
	if (percent > 100) {
		percent = 100;
	}
	_vol = percent;
	audio_codec_set_property(analog, AUDIO_PROPERTY_OUTPUT_VOLUME, AUDIO_CHANNEL_ALL,
				 pv_vol(percent * 255 / 100));
}

void AudioClass::mute(bool m)
{
	_muted = m;
	audio_codec_set_property(analog, AUDIO_PROPERTY_OUTPUT_MUTE, AUDIO_CHANNEL_ALL, pv_mute(m));
}

void AudioClass::setInputGain(uint8_t percent)
{
	if (percent > 100) {
		percent = 100;
	}
	audio_codec_set_property(analog, AUDIO_PROPERTY_INPUT_VOLUME, AUDIO_CHANNEL_ALL,
				 pv_vol(percent * 255 / 100));
}

void AudioClass::muteInput(bool m)
{
	audio_codec_set_property(analog, AUDIO_PROPERTY_INPUT_MUTE, AUDIO_CHANNEL_ALL, pv_mute(m));
}

void AudioClass::inputSource(InputSource s)
{
	audio_codec_route_input(analog, AUDIO_CHANNEL_ALL, (uint32_t)s);
}

/* mono samples are doubled into stereo frames in small pieces */
size_t AudioClass::writeFrames16(const int16_t *s, size_t count, bool block)
{
	if (_outCh == 2) {
		return _e.write(s, count * 2, block) / 2;
	}
	int16_t tmp[256];
	size_t done = 0;
	while (done < count) {
		size_t n = count - done < 128 ? count - done : 128;
		for (size_t i = 0; i < n; i++) {
			tmp[2 * i] = tmp[2 * i + 1] = s[done + i];
		}
		size_t took = _e.write(tmp, n * 4, block) / 4;
		done += took;
		if (took < n) {
			break;
		}
	}
	return done;
}

size_t AudioClass::writeSamples(const int16_t *samples, size_t count, bool block)
{
	if (!_outCh) {
		return 0;
	}
	if (_outCh == 2) {
		count &= ~(size_t)1; /* whole frames */
	}
	return writeFrames16(samples, count, block);
}

size_t AudioClass::write(uint8_t b)
{
	return write(&b, 1);
}

size_t AudioClass::write(const uint8_t *buf, size_t n)
{
	if (!_outCh) {
		return 0;
	}
	const size_t fb = _outCh * 2; /* bytes of a frame of the caller's stream */
	size_t used = 0;
	int16_t tmp[64];

	/* complete a frame left over from the last call */
	while (_pendLen && _pendLen < fb && used < n) {
		_pend[_pendLen++] = buf[used++];
	}
	if (_pendLen == fb) {
		memcpy(tmp, _pend, fb);
		if (writeSamples(tmp, _outCh, true) != _outCh) {
			return used;
		}
		_pendLen = 0;
	}
	while (n - used >= fb) {
		size_t frames = (n - used) / fb;
		size_t chunk = sizeof(tmp) / fb;
		if (frames < chunk) {
			chunk = frames;
		}
		memcpy(tmp, buf + used, chunk * fb);
		size_t samples = chunk * _outCh;
		if (writeSamples(tmp, samples, true) != samples) {
			break;
		}
		used += chunk * fb;
	}
	while (used < n && _pendLen < fb && n - used < fb) {
		_pend[_pendLen++] = buf[used++];
	}
	return used;
}

void AudioClass::playTone(uint32_t frequency, uint32_t durationMs, int16_t amplitude)
{
	int16_t *t = sine_table();
	if (!t || !_outCh || !frequency) {
		return;
	}
	uint32_t rate = _e.rate();
	uint32_t step = (uint32_t)(((uint64_t)frequency << 32) / rate);
	uint32_t phase = 0;
	uint64_t frames = (uint64_t)durationMs * rate / 1000;
	int16_t buf[256];
	while (frames) {
		size_t n = frames < 128 ? (size_t)frames : 128;
		for (size_t i = 0; i < n; i++) {
			int16_t v = (int16_t)(((int32_t)t[phase >> 22] * amplitude) >> 15);
			buf[2 * i] = buf[2 * i + 1] = v;
			phase += step;
		}
		if (_e.write(buf, n * 4, true) != n * 4) {
			break;
		}
		frames -= n;
	}
}

void AudioClass::playSilence(uint32_t durationMs)
{
	if (!_outCh) {
		return;
	}
	int16_t buf[256] = {0};
	uint64_t frames = (uint64_t)durationMs * _e.rate() / 1000;
	while (frames) {
		size_t n = frames < 128 ? (size_t)frames : 128;
		if (_e.write(buf, n * 4, true) != n * 4) {
			break;
		}
		frames -= n;
	}
}

int AudioClass::read()
{
	uint8_t b;
	return _e.read(&b, 1, 0) == 0 ? -1 : b;
}

int AudioClass::peek()
{
	uint8_t b;
	return _e.peek(&b, 1) == 0 ? -1 : b;
}

size_t AudioClass::readSamples(int16_t *dst, size_t count, uint32_t timeoutMs)
{
	return _e.read(dst, count * 2, timeoutMs) / 2;
}
