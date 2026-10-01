/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MP4_h
#define MP4_h

#include <stddef.h>
#include <stdint.h>
#include "Arduino.h"

/*
 * MP4 / ISO base media file demultiplexer: the sample tables of the first
 * H.264 and the first AAC track are read into memory and the packets of a
 * track are walked in decoding order. Nothing is decoded here: feed the
 * packets to VideoDecoder / AACDecoder.
 *
 *   MP4File f;
 *   f.open("/SD:/clip.mp4");               // or open(buffer, size) / open(readFn, ctx, size)
 *   MP4Packet p;
 *   while (f.nextAudio(p)) { f.readAudio(p, buf); ... }
 *
 * Packet times are in microseconds with the edit list start already removed.
 */

struct MP4Packet {
	uint64_t offset = 0; /* position in the file */
	uint32_t size = 0;
	int64_t ptsUs = 0;
	int64_t dtsUs = 0;
	uint32_t index = 0;
	bool key = false; /* sync sample (every audio packet) */
};

/* reads len bytes at offset, returns 0 or a negative errno */
typedef int (*MP4ReadFn)(void *ctx, uint64_t offset, void *buf, size_t len);

struct MP4Impl;

class MP4File {
public:
	MP4File() {}
	~MP4File() { close(); }
	MP4File(const MP4File &) = delete;
	MP4File &operator=(const MP4File &) = delete;

	/* a file on the SD card ("/SD:/dir/name.mp4"), the card is mounted on first use */
	bool open(const char *path);
	/* a file in memory (not copied, has to stay valid) */
	bool open(const uint8_t *data, size_t size);
	/* any source */
	bool open(MP4ReadFn read, void *ctx, uint64_t size);
	void close();
	bool isOpen() const { return _impl != nullptr; }

	bool hasVideo() const;
	bool hasAudio() const;
	uint16_t width() const;
	uint16_t height() const;
	uint32_t videoFrames() const;
	uint32_t videoDurationMs() const;
	uint32_t maxVideoPacket() const;
	/* avcC record (SPS/PPS) of the video track */
	const uint8_t *videoExtra(size_t *len = nullptr) const;
	uint32_t audioSampleRate() const;
	uint16_t audioChannels() const;
	uint32_t audioFrames() const; /* number of AAC packets */
	uint32_t audioDurationMs() const;
	uint32_t maxAudioPacket() const;
	/* AudioSpecificConfig of the audio track, for AACDecoder::begin() */
	const uint8_t *audioExtra(size_t *len = nullptr) const;
	/* samples (of 1024 frames) to skip at the start, the encoder delay */
	uint32_t audioSkipFrames() const;

	void rewindVideo();
	void rewindAudio();
	bool nextVideo(MP4Packet &p);
	bool nextAudio(MP4Packet &p);
	/* position the video cursor on the last key frame at or before frame `index` */
	bool seekVideo(uint32_t index);

	/* read the data of a packet, buf has to hold p.size bytes; returns 0 or a negative errno */
	int readVideo(const MP4Packet &p, void *buf);
	int readAudio(const MP4Packet &p, void *buf);

	/*
	 * The video packet as an Annex B byte stream (start codes), with the SPS/PPS in
	 * front of key frames, as VideoDecoder::feed() wants it. `buf` needs
	 * maxVideoPacket() + 1024 bytes; returns the size or a negative errno.
	 */
	int readVideoAnnexB(const MP4Packet &p, uint8_t *buf);

private:
	MP4Impl *_impl = nullptr;
};

#endif
