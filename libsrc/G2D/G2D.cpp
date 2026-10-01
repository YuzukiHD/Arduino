/* SPDX-License-Identifier: Apache-2.0 */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/g2d.h>

#include "Arduino.h"
#include "G2D.h"

G2DClass G2D;

static const struct device *dev_of(void *p)
{
	return static_cast<const struct device *>(p);
}

bool G2DClass::begin()
{
	if (_dev) {
		return true;
	}
	const struct device *d = DEVICE_DT_GET(DT_NODELABEL(g2d));
	if (!device_is_ready(d)) {
		return false;
	}
	struct g2d_capabilities caps;
	if (g2d_get_capabilities(d, &caps) == 0) {
		_maxW = caps.max_width;
		_maxH = caps.max_height;
	}
	_dev = (void *)d;
	return true;
}

static struct g2d_surface conv(const G2DSurface &s)
{
	struct g2d_surface o = {};
	o.format = (enum g2d_format)s.format; /* same order */
	o.width = s.width;
	o.height = s.height;
	for (int i = 0; i < 3; i++) {
		o.plane[i] = s.plane[i];
		o.pitch[i] = s.pitch[i];
	}
	return o;
}

static struct g2d_rect conv(const G2DRect &r)
{
	struct g2d_rect o = {r.x, r.y, r.w, r.h};
	return o;
}

int G2DClass::fill(const G2DSurface &dst, const G2DRect &rect, uint32_t color)
{
	if (!_dev) return -ENODEV;
	struct g2d_surface d = conv(dst);
	struct g2d_rect r = conv(rect);
	return g2d_fill(dev_of(_dev), &d, &r, color);
}

int G2DClass::blit(const G2DSurface &src, const G2DRect &srcRect, const G2DSurface &dst,
		   const G2DRect &dstRect, G2DRotation rot, uint32_t flags)
{
	if (!_dev) return -ENODEV;
	struct g2d_surface s = conv(src), d = conv(dst);
	struct g2d_rect sr = conv(srcRect), dr = conv(dstRect);
	return g2d_blit(dev_of(_dev), &s, &sr, &d, &dr, (enum g2d_rotation)rot, flags);
}

int G2DClass::blend(const G2DSurface &fg, const G2DRect &fgRect, const G2DSurface &bg,
		    const G2DRect &bgRect, const G2DSurface &dst, const G2DRect &dstRect,
		    G2DBlendMode mode, uint8_t fgAlpha, uint32_t flags)
{
	if (!_dev) return -ENODEV;
	struct g2d_surface f = conv(fg), b = conv(bg), d = conv(dst);
	struct g2d_rect fr = conv(fgRect), br = conv(bgRect), dr = conv(dstRect);
	struct g2d_blend bl = {};
	bl.mode = (enum g2d_blend_mode)mode;
	bl.fg_alpha_mode = fgAlpha == 255 ? G2D_ALPHA_PIXEL : G2D_ALPHA_MIXED;
	bl.fg_alpha = fgAlpha;
	bl.bg_alpha_mode = G2D_ALPHA_PIXEL;
	bl.bg_alpha = 255;
	return g2d_blend(dev_of(_dev), &f, &fr, &b, &br, &d, &dr, &bl, flags);
}
