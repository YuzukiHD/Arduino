/* SPDX-License-Identifier: Apache-2.0 */
#ifndef AACDecoder_h
#define AACDecoder_h

#include <stddef.h>
#include <stdint.h>
#include "Arduino.h"

/*
 * AAC-LC decoder (fixed point, software). Decodes one raw AAC packet (as stored in
 * an MP4 file, no ADTS header) to 16 bit interleaved stereo PCM; a mono stream is
 * duplicated to both channels.
 *
 *   AACDecoder dec;
 *   dec.begin(file.audioExtra(&n), n);
 *   int16_t pcm[AACDecoder::MAX_PCM_SAMPLES];
 *   int frames = dec.decode(packet, size, pcm);   // frames of 2 samples each
 */
struct AACImpl;

class AACDecoder {
public:
	static const int FRAME_SAMPLES = 1024;
	/* size of the pcm buffer of decode(), in int16_t */
	static const int MAX_PCM_SAMPLES = 1024 * 2;

	AACDecoder() {}
	~AACDecoder() { end(); }
	AACDecoder(const AACDecoder &) = delete;
	AACDecoder &operator=(const AACDecoder &) = delete;

	/* audioSpecificConfig is the extra data of the AAC track */
	bool begin(const uint8_t *audioSpecificConfig, size_t len);
	void end();

	/* returns the number of PCM frames written (1024), or a negative decoder error */
	int decode(const uint8_t *packet, size_t len, int16_t *pcm);

	uint32_t sampleRate() const;
	/* channels of the encoded stream; the output always has 2 */
	int channels() const;

private:
	AACImpl *_impl = nullptr;
};

#endif
