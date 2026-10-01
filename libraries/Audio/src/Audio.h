/* SPDX-License-Identifier: Apache-2.0 */
#ifndef Audio_h
#define Audio_h

#include <stdint.h>
#include "Arduino.h"
#include "AudioEngine.h"

/*
 * The on-chip codec. Output is 16-bit PCM; the stream keeps running (with
 * silence) between writes. The class is a Stream: write() plays raw
 * little-endian 16-bit PCM (interleaved when stereo), read() returns the raw
 * 16-bit mono PCM captured by the microphone input.
 *
 *   Audio.begin(48000);              // stereo output
 *   Audio.playTone(440, 500);
 *   Audio.beginRecord(16000);        // microphone only
 *
 * Output and input share the rate: begin(rate, channels, true) opens both.
 * Rates of the 48 kHz family (8000, 16000, 24000, 32000, 48000, ...) and of
 * the 44.1 kHz family (11025, 22050, 44100) cannot be used at the same time by
 * different audio blocks (codec, I2S, SPDIF): they share one PLL.
 */
class AudioClass : public Stream {
public:
	enum InputSource { INPUT_MIC = 1, INPUT_FM = 2, INPUT_LINE = 3 };

	/* channels: 1 or 2 (a mono stream is played on both sides), 0 = input only */
	bool begin(uint32_t sampleRate = 48000, uint8_t channels = 2, bool microphone = false);
	bool beginRecord(uint32_t sampleRate = 16000) { return begin(sampleRate, 0, true); }
	void end();
	bool active() const { return _e.isOpen(); }
	uint32_t sampleRate() const { return _e.rate(); }

	/* ---- output ---- */
	size_t write(uint8_t b) override;
	size_t write(const uint8_t *buf, size_t n) override;
	using Print::write;
	/* samples are int16 values (frames * channels of them); returns the samples taken */
	size_t writeSamples(const int16_t *samples, size_t count, bool block = true);
	int availableForWrite() override { return (int)_e.writeSpace() / (_outCh == 1 ? 2 : 1); }
	/* waits until everything written has been played */
	void flush() override { _e.flush(); }
	void playTone(uint32_t frequency, uint32_t durationMs, int16_t amplitude = 12000);
	void playSilence(uint32_t durationMs);

	/* headphone driver and speaker amplifier (PE10) on or off */
	void speaker(bool on);
	/* 0..100 */
	void setVolume(uint8_t percent);
	void mute(bool m);

	/* ---- microphone ---- */
	int available() override { return (int)_e.readAvailable(); }
	int read() override;
	int peek() override;
	/* waits up to timeoutMs for count samples, returns how many arrived */
	size_t readSamples(int16_t *dst, size_t count, uint32_t timeoutMs = 1000);
	/* peak of the last 20 ms: 0..32767 and 0..100 */
	int level() { return _e.rxPeak(); }
	int levelPercent() { return (int)((int32_t)_e.rxPeak() * 100 / 32767); }
	void setInputGain(uint8_t percent);
	void muteInput(bool m);
	void inputSource(InputSource s);
	void dropInput() { _e.dropInput(); }
	uint32_t inputOverruns() const { return _e.rxOverruns(); }

private:
	AudioEngine _e;
	uint8_t _outCh = 0;
	bool _outOn = false;
	uint8_t _pend[4];
	uint8_t _pendLen = 0;
	uint8_t _vol = 70;
	bool _muted = false;
	size_t writeFrames16(const int16_t *s, size_t count, bool block);
};

extern AudioClass Audio;

#endif
