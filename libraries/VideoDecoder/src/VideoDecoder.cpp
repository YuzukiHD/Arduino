/* SPDX-License-Identifier: Apache-2.0 */
#include <errno.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/display/display_sunxi.h>
#include <zephyr/drivers/vdec.h>

#include "Arduino.h"
#include "VideoDecoder.h"

VideoDecoderClass VideoDecoder;

static const struct device *const ve = DEVICE_DT_GET(DT_NODELABEL(ve));
static const struct device *const disp = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

static_assert(VideoDecoderClass::AGAIN == -EAGAIN, "AGAIN");
static_assert(VideoDecoderClass::BUSY == -EBUSY, "BUSY");
static_assert(VideoDecoderClass::NODATA == -ENODATA, "NODATA");

static void to_public(const struct vdec_frame &f, VideoFrame &out)
{
	out.format = (uint8_t)f.format;
	out.width = f.width;
	out.height = f.height;
	out.plane[0] = f.plane[0];
	out.plane[1] = f.plane[1];
	out.stride[0] = f.stride[0];
	out.stride[1] = f.stride[1];
	out.pts = f.pts;
	out._priv = f.priv;
}

static void to_driver(const VideoFrame &f, struct vdec_frame &out)
{
	out.format = (enum vdec_format)f.format;
	out.width = f.width;
	out.height = f.height;
	out.plane[0] = f.plane[0];
	out.plane[1] = f.plane[1];
	out.stride[0] = f.stride[0];
	out.stride[1] = f.stride[1];
	out.pts = f.pts;
	out.priv = f._priv;
}

bool VideoDecoderClass::begin()
{
	return device_is_ready(ve);
}

static int decode(enum vdec_codec codec, const void *data, size_t len, enum vdec_format fmt,
		  VideoFrame &frame)
{
	struct vdec_frame f = {};
	int ret = vdec_decode_image(ve, codec, data, len, fmt, &f);

	if (ret == 0) {
		to_public(f, frame);
	}
	return ret;
}

int VideoDecoderClass::decodeJPEG(const void *data, size_t len, VideoFrame &frame,
				  VideoFrame::Format format)
{
	return decode(VDEC_CODEC_JPEG, data, len, (enum vdec_format)format, frame);
}

int VideoDecoderClass::decodePNG(const void *data, size_t len, VideoFrame &frame)
{
	return decode(VDEC_CODEC_PNG, data, len, VDEC_FORMAT_RGBA8888, frame);
}

void VideoDecoderClass::release(VideoFrame &frame)
{
	if (frame._priv == nullptr) {
		return;
	}
	struct vdec_frame f = {};

	to_driver(frame, f);
	vdec_frame_release(ve, &f);
	frame = VideoFrame();
}

bool VideoDecoderClass::openH264(size_t bufferSize, VideoFrame::Format format)
{
	struct vdec_stream_config cfg = {};
	struct vdec_stream *s = nullptr;

	if (_stream) {
		return false;
	}
	cfg.codec = VDEC_CODEC_H264;
	cfg.format = (enum vdec_format)format;
	cfg.buffer_size = bufferSize;
	if (vdec_stream_open(ve, &cfg, &s) != 0) {
		return false;
	}
	_stream = s;
	return true;
}

int VideoDecoderClass::feed(const void *data, size_t len, int64_t pts, size_t *consumed)
{
	size_t used = 0;

	if (!_stream) {
		return -EINVAL;
	}
	int ret = vdec_stream_feed(ve, (struct vdec_stream *)_stream, data, len, pts, &used);
	if (consumed) {
		*consumed = used;
	}
	return ret;
}

int VideoDecoderClass::getFrame(VideoFrame &frame)
{
	struct vdec_frame f = {};

	if (!_stream) {
		return -EINVAL;
	}
	int ret = vdec_stream_get_frame(ve, (struct vdec_stream *)_stream, &f);
	if (ret == 0) {
		to_public(f, frame);
	}
	return ret;
}

int VideoDecoderClass::flush()
{
	return _stream ? vdec_stream_flush(ve, (struct vdec_stream *)_stream) : -EINVAL;
}

void VideoDecoderClass::closeStream()
{
	if (_stream) {
		vdec_stream_close(ve, (struct vdec_stream *)_stream);
		_stream = nullptr;
	}
}

int VideoDecoderClass::show(const VideoFrame &frame, bool fullRange, bool nonblock)
{
	if (frame.format != VideoFrame::NV12 && frame.format != VideoFrame::NV21) {
		return -ENOTSUP;
	}
	struct display_sunxi_yuv yuv = {};

	yuv.y = frame.plane[0];
	yuv.uv = frame.plane[1];
	yuv.nv21 = frame.format == VideoFrame::NV21;
	yuv.width = frame.width;
	yuv.height = frame.height;
	yuv.stride_y = frame.stride[0];
	yuv.stride_uv = frame.stride[1];
	yuv.full_range = fullRange;
	yuv.bt709 = frame.height > 576;
	yuv.nonblock = nonblock;
	return display_sunxi_show_yuv(disp, &yuv);
}

void VideoDecoderClass::hide()
{
	display_sunxi_hide_yuv(disp);
}

static inline uint8_t clamp8(int v)
{
	return v < 0 ? 0 : v > 255 ? 255 : (uint8_t)v;
}

/* BT.601 full range, the usual matrix of JPEG */
static inline void yuv_to_rgb(int y, int u, int v, uint8_t &r, uint8_t &g, uint8_t &b)
{
	u -= 128;
	v -= 128;
	r = clamp8(y + ((91881 * v) >> 16));
	g = clamp8(y - ((22554 * u + 46802 * v) >> 16));
	b = clamp8(y + ((116130 * u) >> 16));
}

template <typename F> static void convert(const VideoFrame &fr, F put)
{
	for (uint16_t row = 0; row < fr.height; row++) {
		if (fr.format == VideoFrame::RGBA8888) {
			/* bytes in memory: A, B, G, R */
			const uint8_t *p = fr.plane[0] + (size_t)row * fr.stride[0];
			for (uint16_t col = 0; col < fr.width; col++, p += 4) {
				put(row, col, p[3], p[2], p[1]);
			}
			continue;
		}
		const uint8_t *yp = fr.plane[0] + (size_t)row * fr.stride[0];
		const uint8_t *cp = fr.plane[1] + (size_t)(row / 2) * fr.stride[1];
		bool nv21 = fr.format == VideoFrame::NV21;
		for (uint16_t col = 0; col < fr.width; col++) {
			int u = cp[(col & ~1) + (nv21 ? 1 : 0)];
			int v = cp[(col & ~1) + (nv21 ? 0 : 1)];
			uint8_t r, g, b;
			yuv_to_rgb(yp[col], u, v, r, g, b);
			put(row, col, r, g, b);
		}
	}
}

void VideoDecoderClass::toRGB565(const VideoFrame &fr, uint16_t *out, size_t stride)
{
	convert(fr, [&](uint16_t row, uint16_t col, uint8_t r, uint8_t g, uint8_t b) {
		out[row * stride + col] = (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
	});
}

void VideoDecoderClass::toARGB8888(const VideoFrame &fr, uint32_t *out, size_t stride)
{
	convert(fr, [&](uint16_t row, uint16_t col, uint8_t r, uint8_t g, uint8_t b) {
		out[row * stride + col] = 0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
	});
}
