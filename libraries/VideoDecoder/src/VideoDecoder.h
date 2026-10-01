/* SPDX-License-Identifier: Apache-2.0 */
#ifndef VideoDecoder_h
#define VideoDecoder_h

#include <stddef.h>
#include <stdint.h>
#include "Arduino.h"

/*
 * Hardware JPEG / PNG / H.264 decoder (video engine).
 *
 * A decoded frame lives in decoder memory and is handed out without a copy;
 * it stays valid, and for single pictures the decoder stays claimed, until
 * release(). Frames of a stream have to be released in time, the decoder holds
 * only a few of them.
 */

struct VideoFrame {
	enum Format { NV12 = 1, NV21 = 2, RGBA8888 = 3 };
	uint8_t format = 0;
	uint16_t width = 0, height = 0;
	/* first visible pixel of each plane (NV12/NV21: luma, interleaved chroma; RGBA8888: plane[0]) */
	uint8_t *plane[2] = {nullptr, nullptr};
	uint16_t stride[2] = {0, 0};
	int64_t pts = -1; /* time stamp given to feed(), -1 for none */
	void *_priv = nullptr;

	bool valid() const { return _priv != nullptr; }
};

class VideoDecoderClass {
public:
	/* result codes of getFrame() and feed(), equal to the negative errno values */
	static const int OK = 0;
	static const int AGAIN = -11;  /* needs more data / buffer full */
	static const int BUSY = -16;   /* every frame is held by the sketch: release some */
	static const int NODATA = -61; /* flushed and all frames delivered */

	bool begin();

	/* single pictures; negative errno on failure (-ENOTSUP, -EINVAL, -ENOMEM, -EIO) */
	int decodeJPEG(const void *data, size_t len, VideoFrame &frame,
		       VideoFrame::Format format = VideoFrame::NV12);
	int decodePNG(const void *data, size_t len, VideoFrame &frame);
	/* give a frame back (single pictures: the decoder can be used again) */
	void release(VideoFrame &frame);

	/* H.264 stream: Annex B byte stream, whole NAL units */
	bool openH264(size_t bufferSize = 0, VideoFrame::Format format = VideoFrame::NV12);
	/* returns OK or AGAIN (buffer full: take frames out first); `consumed` bytes were taken */
	int feed(const void *data, size_t len, int64_t pts, size_t *consumed);
	int getFrame(VideoFrame &frame);
	int flush();
	void closeStream();
	bool streamOpen() const { return _stream != nullptr; }

	/*
	 * Put an NV12/NV21 frame on the display video plane (scaled to the screen);
	 * the frame must stay valid until the next show() or hide().
	 */
	int show(const VideoFrame &frame, bool fullRange = false, bool nonblock = false);
	void hide();

	/* CPU conversion of a frame to 16 bit RGB565 / 32 bit 0xAARRGGBB (no scaling) */
	static void toRGB565(const VideoFrame &frame, uint16_t *out, size_t outStridePixels);
	static void toARGB8888(const VideoFrame &frame, uint32_t *out, size_t outStridePixels);

private:
	void *_stream = nullptr;
};

extern VideoDecoderClass VideoDecoder;

#endif
