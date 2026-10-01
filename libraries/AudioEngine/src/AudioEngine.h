/* SPDX-License-Identifier: Apache-2.0 */
#ifndef AudioEngine_h
#define AudioEngine_h

#include <stddef.h>
#include <stdint.h>

/*
 * Streams PCM through one I2S-API device (on-chip codec, I2S0, S/PDIF): a
 * ring buffer on each direction, a feeder thread that cuts the TX ring into
 * 20 ms blocks (silence when it runs dry, so the stream never stops) and a
 * reader thread that collects the RX blocks.
 *
 * It keeps Zephyr out of the header: `dev` is a `const struct device *`.
 */
struct AudioEngineImpl;

class AudioEngine {
public:
	enum Format { FORMAT_I2S = 0, FORMAT_LEFT_JUSTIFIED = 1, FORMAT_RIGHT_JUSTIFIED = 2 };

	AudioEngine();
	~AudioEngine();

	/*
	 * bits: 16, 24 or 32. A sample of 24 or 32 bits takes 4 bytes. txChannels
	 * / rxChannels: 0 leaves the direction off. Returns 0 or a negative errno.
	 */
	int open(const void *dev, uint32_t rate, uint8_t bits, uint8_t txChannels,
		 uint8_t rxChannels, bool loopback = false, Format format = FORMAT_I2S,
		 size_t ringBytes = 32768);
	void close();
	bool isOpen() const;

	uint32_t rate() const;
	uint8_t sampleBytes() const;
	uint8_t txFrameBytes() const;
	uint8_t rxFrameBytes() const;

	/* TX: whole frames only; returns the bytes taken (block = wait for room) */
	size_t write(const void *data, size_t bytes, bool block);
	size_t writeSpace() const;
	/* wait until everything written has been played */
	void flush();

	/* RX: waits up to timeoutMs for data, returns the bytes copied */
	size_t read(void *dst, size_t bytes, uint32_t timeoutMs);
	size_t readAvailable() const;
	size_t peek(void *dst, size_t bytes) const;
	void dropInput();
	/* peak of the last received block, 0..32767 (top 16 bits of wider samples) */
	int32_t rxPeak() const;

	uint32_t txBlocksPlayed() const;
	uint32_t rxOverruns() const;
	uint32_t txRecoveries() const;
	uint32_t rxRecoveries() const;

private:
	AudioEngineImpl *d;
	AudioEngine(const AudioEngine &);
	AudioEngine &operator=(const AudioEngine &);
};

#endif
