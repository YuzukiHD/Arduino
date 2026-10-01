/* SPDX-License-Identifier: Apache-2.0 */
#include <pvmp4audiodecoder_api.h>
#include <stdlib.h>
#include <string.h>

#include "Arduino.h"
#include "AACDecoder.h"

struct AACImpl {
	tPVMP4AudioDecoderExternal ext;
	void *mem;
	/* the decoder writes the extra channel pair of HE-AAC behind the first one */
	int16_t pcm[1024 * 2 * 2];
};

bool AACDecoder::begin(const uint8_t *asc, size_t len)
{
	end();
	if (!asc || !len) {
		return false;
	}
	AACImpl *i = (AACImpl *)calloc(1, sizeof(AACImpl));
	if (!i) {
		return false;
	}
	i->mem = malloc(PVMP4AudioDecoderGetMemRequirements());
	i->ext.desiredChannels = 2;
	i->ext.outputFormat = OUTPUTFORMAT_16PCM_INTERLEAVED;
	i->ext.aacPlusEnabled = false;
	i->ext.pOutputBuffer = i->pcm;
	i->ext.pOutputBuffer_plus = i->pcm + 1024 * 2;
	if (!i->mem || PVMP4AudioDecoderInitLibrary(&i->ext, i->mem) != 0) {
		free(i->mem);
		free(i);
		return false;
	}
	i->ext.pInputBuffer = (UChar *)asc;
	i->ext.inputBufferCurrentLength = len;
	i->ext.inputBufferUsedLength = 0;
	i->ext.remainderBits = 0;
	if (PVMP4AudioDecoderConfig(&i->ext, i->mem) != MP4AUDEC_SUCCESS) {
		free(i->mem);
		free(i);
		return false;
	}
	_impl = i;
	return true;
}

void AACDecoder::end()
{
	if (_impl) {
		free(_impl->mem);
		free(_impl);
		_impl = nullptr;
	}
}

int AACDecoder::decode(const uint8_t *packet, size_t len, int16_t *pcm)
{
	if (!_impl) {
		return -1;
	}
	_impl->ext.pInputBuffer = (UChar *)packet;
	_impl->ext.inputBufferCurrentLength = len;
	_impl->ext.inputBufferUsedLength = 0;
	_impl->ext.remainderBits = 0;
	int ret = PVMP4AudioDecodeFrame(&_impl->ext, _impl->mem);
	if (ret != MP4AUDEC_SUCCESS) {
		return -ret;
	}
	int frames = _impl->ext.frameLength > 0 ? _impl->ext.frameLength : FRAME_SAMPLES;
	if (frames > FRAME_SAMPLES) {
		frames = FRAME_SAMPLES;
	}
	memcpy(pcm, _impl->pcm, (size_t)frames * 2 * sizeof(int16_t));
	return frames;
}

uint32_t AACDecoder::sampleRate() const
{
	return _impl ? (uint32_t)_impl->ext.samplingRate : 0;
}

int AACDecoder::channels() const
{
	return _impl ? _impl->ext.encodedChannels : 0;
}
