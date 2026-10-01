/* SPDX-License-Identifier: Apache-2.0 */
#ifndef G2D_h
#define G2D_h

#include <stdint.h>
#include "Arduino.h"

enum class G2DFormat : uint8_t {
	ARGB8888, ABGR8888, RGBA8888, BGRA8888, XRGB8888, XBGR8888,
	RGB888, BGR888, RGB565, BGR565, ARGB4444, ARGB1555,
	/* source only */
	NV12, NV21, I420, YUYV,
};

enum class G2DRotation : uint8_t { Rot0, Rot90, Rot180, Rot270 };

/** Porter-Duff modes; the foreground is the "source" */
enum class G2DBlendMode : uint8_t {
	Clear, Src, Dst, SrcOver, DstOver, SrcIn, DstIn, SrcOut, DstOut, SrcAtop, DstAtop, Xor,
};

struct G2DRect {
	uint16_t x, y, w, h;
};

/** A buffer of pixels. Planar/semi-planar YUV use plane[1] (and plane[2] for I420). */
struct G2DSurface {
	G2DFormat format = G2DFormat::ARGB8888;
	uint16_t width = 0, height = 0;
	void *plane[3] = {nullptr, nullptr, nullptr};
	uint32_t pitch[3] = {0, 0, 0}; /* bytes per row of each plane, 0 = tightly packed */

	/** Surface over a tightly packed buffer of one plane (packed RGB formats) */
	static G2DSurface of(G2DFormat f, void *buf, uint16_t w, uint16_t h, uint32_t pitchBytes = 0)
	{
		G2DSurface s;
		s.format = f;
		s.width = w;
		s.height = h;
		s.plane[0] = buf;
		s.pitch[0] = pitchBytes;
		return s;
	}
	/** NV12/NV21 over a luma plane followed by a chroma plane */
	static G2DSurface nv12(void *y, void *uv, uint16_t w, uint16_t h, uint32_t pitchY, uint32_t pitchUV,
			       bool nv21 = false)
	{
		G2DSurface s = of(nv21 ? G2DFormat::NV21 : G2DFormat::NV12, y, w, h, pitchY);
		s.plane[1] = uv;
		s.pitch[1] = pitchUV;
		return s;
	}
	/** Surface over the drawing buffer of Display (or any object with buffer()/width()/height()) */
	template <class D> static G2DSurface fromDisplay(D &d)
	{
		return of(G2DFormat::ARGB8888, d.buffer(), d.width(), d.height(), d.stride() * 4);
	}
};

class G2DClass {
public:
	/* flags for blit() and blend() */
	static constexpr uint32_t FlipH = 1u << 0;
	static constexpr uint32_t FlipV = 1u << 1;
	static constexpr uint32_t SrcPremultiplied = 1u << 2;
	static constexpr uint32_t DstPremultiplied = 1u << 3;
	static constexpr uint32_t NoCacheOps = 1u << 4;
	static constexpr uint32_t YuvFullRange = 1u << 5;
	static constexpr uint32_t YuvBt709 = 1u << 6;

	/** true when the accelerator is ready */
	bool begin();

	/** All calls wait for completion and return 0 or a negative errno. */

	/** Fill a rectangle with a 0xAARRGGBB colour */
	int fill(const G2DSurface &dst, const G2DRect &rect, uint32_t color);
	/** Copy with format conversion; different rectangle sizes scale, rotation 90/270 swaps them */
	int blit(const G2DSurface &src, const G2DRect &srcRect, const G2DSurface &dst, const G2DRect &dstRect,
		 G2DRotation rot = G2DRotation::Rot0, uint32_t flags = 0);
	/** Compose fg over bg into dst (dst may be bg). fgAlpha < 255 is multiplied with the pixel alpha. */
	int blend(const G2DSurface &fg, const G2DRect &fgRect, const G2DSurface &bg, const G2DRect &bgRect,
		  const G2DSurface &dst, const G2DRect &dstRect, G2DBlendMode mode = G2DBlendMode::SrcOver,
		  uint8_t fgAlpha = 255, uint32_t flags = 0);

	/** Size limit of one operation */
	int maxWidth() const { return _maxW; }
	int maxHeight() const { return _maxH; }

private:
	void *_dev = nullptr;
	int _maxW = 0, _maxH = 0;
};

extern G2DClass G2D;

#endif
