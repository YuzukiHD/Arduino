/* SPDX-License-Identifier: Apache-2.0 */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/display/display_sunxi.h>
#include <zephyr/cache.h>

#include "Arduino.h"
#include "Display.h"
#include "font5x7.h"

#if !defined(CONFIG_DISPLAY_SUNXI_ARGB8888)
#error "the Display library needs CONFIG_DISPLAY_SUNXI_ARGB8888"
#endif

DisplayClass Display;

static inline const struct device *dev_of(void *p)
{
	return static_cast<const struct device *>(p);
}

bool DisplayClass::begin(uint8_t brightness)
{
	if (_buf) {
		return true;
	}
	const struct device *d = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
	(void)device_init(d); /* the display is not brought up at boot (-EALREADY when it is) */
	if (!device_is_ready(d)) {
		return false;
	}
	struct display_capabilities caps;
	display_get_capabilities(d, &caps);
	_w = caps.x_resolution;
	_h = caps.y_resolution;
	_buf = (uint32_t *)aligned_alloc(64, (size_t)_w * _h * 4);
	if (!_buf) {
		return false;
	}
	_dev = (void *)d;
	memset(_buf, 0, (size_t)_w * _h * 4);
	show();
	display_set_brightness(d, brightness);
	display_blanking_off(d);
	return true;
}

void DisplayClass::end()
{
	if (_dev) {
		display_blanking_on(dev_of(_dev));
	}
	free(_buf);
	_buf = nullptr;
	_dev = nullptr;
}

/* ---- drawing ---- */

void DisplayClass::drawPixel(int x, int y, uint32_t c)
{
	if (_buf && x >= 0 && y >= 0 && x < _w && y < _h) {
		_buf[(size_t)y * _w + x] = c;
	}
}

uint32_t DisplayClass::getPixel(int x, int y) const
{
	return (_buf && x >= 0 && y >= 0 && x < _w && y < _h) ? _buf[(size_t)y * _w + x] : 0;
}

void DisplayClass::hspan(int x0, int x1, int y, uint32_t c)
{
	if (y < 0 || y >= _h) return;
	if (x0 < 0) x0 = 0;
	if (x1 >= _w) x1 = _w - 1;
	uint32_t *p = _buf + (size_t)y * _w;
	for (int x = x0; x <= x1; x++) p[x] = c;
}

void DisplayClass::fillScreen(uint32_t c)
{
	if (!_buf) return;
	for (size_t i = 0, n = (size_t)_w * _h; i < n; i++) _buf[i] = c;
}

void DisplayClass::fillRect(int x, int y, int w, int h, uint32_t c)
{
	if (!_buf) return;
	for (int j = 0; j < h; j++) hspan(x, x + w - 1, y + j, c);
}

void DisplayClass::drawFastHLine(int x, int y, int w, uint32_t c) { hspan(x, x + w - 1, y, c); }

void DisplayClass::drawFastVLine(int x, int y, int h, uint32_t c)
{
	for (int j = 0; j < h; j++) drawPixel(x, y + j, c);
}

void DisplayClass::drawRect(int x, int y, int w, int h, uint32_t c)
{
	if (w <= 0 || h <= 0) return;
	drawFastHLine(x, y, w, c);
	drawFastHLine(x, y + h - 1, w, c);
	drawFastVLine(x, y, h, c);
	drawFastVLine(x + w - 1, y, h, c);
}

void DisplayClass::drawLine(int x0, int y0, int x1, int y1, uint32_t c)
{
	int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
	int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
	int err = dx + dy;
	for (;;) {
		drawPixel(x0, y0, c);
		if (x0 == x1 && y0 == y1) break;
		int e2 = 2 * err;
		if (e2 >= dy) { err += dy; x0 += sx; }
		if (e2 <= dx) { err += dx; y0 += sy; }
	}
}

void DisplayClass::drawCircle(int cx, int cy, int r, uint32_t c)
{
	int x = r, y = 0, err = 1 - r;
	while (x >= y) {
		drawPixel(cx + x, cy + y, c); drawPixel(cx - x, cy + y, c);
		drawPixel(cx + x, cy - y, c); drawPixel(cx - x, cy - y, c);
		drawPixel(cx + y, cy + x, c); drawPixel(cx - y, cy + x, c);
		drawPixel(cx + y, cy - x, c); drawPixel(cx - y, cy - x, c);
		y++;
		if (err < 0) err += 2 * y + 1;
		else { x--; err += 2 * (y - x) + 1; }
	}
}

void DisplayClass::fillCircle(int cx, int cy, int r, uint32_t c)
{
	int x = r, y = 0, err = 1 - r;
	while (x >= y) {
		hspan(cx - x, cx + x, cy + y, c); hspan(cx - x, cx + x, cy - y, c);
		hspan(cx - y, cx + y, cy + x, c); hspan(cx - y, cx + y, cy - x, c);
		y++;
		if (err < 0) err += 2 * y + 1;
		else { x--; err += 2 * (y - x) + 1; }
	}
}

void DisplayClass::drawTriangle(int x0, int y0, int x1, int y1, int x2, int y2, uint32_t c)
{
	drawLine(x0, y0, x1, y1, c);
	drawLine(x1, y1, x2, y2, c);
	drawLine(x2, y2, x0, y0, c);
}

void DisplayClass::fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, uint32_t c)
{
	if (y0 > y1) { int t = y0; y0 = y1; y1 = t; t = x0; x0 = x1; x1 = t; }
	if (y1 > y2) { int t = y1; y1 = y2; y2 = t; t = x1; x1 = x2; x2 = t; }
	if (y0 > y1) { int t = y0; y0 = y1; y1 = t; t = x0; x0 = x1; x1 = t; }
	if (y0 == y2) {
		int a = min(x0, min(x1, x2)), b = max(x0, max(x1, x2));
		hspan(a, b, y0, c);
		return;
	}
	for (int y = y0; y <= y2; y++) {
		long xa = x0 + (long)(x2 - x0) * (y - y0) / (y2 - y0);
		long xb = y < y1 ? (y1 == y0 ? x1 : x0 + (long)(x1 - x0) * (y - y0) / (y1 - y0))
				 : (y2 == y1 ? x1 : x1 + (long)(x2 - x1) * (y - y1) / (y2 - y1));
		if (xa > xb) { long t = xa; xa = xb; xb = t; }
		hspan((int)xa, (int)xb, y, c);
	}
}

void DisplayClass::drawBitmap(int x, int y, const uint32_t *pix, int w, int h)
{
	if (!_buf) return;
	for (int j = 0; j < h; j++) {
		int yy = y + j;
		if (yy < 0 || yy >= _h) continue;
		int x0 = x < 0 ? -x : 0;
		int x1 = x + w > _w ? _w - x : w;
		if (x1 > x0) memcpy(_buf + (size_t)yy * _w + x + x0, pix + (size_t)j * w + x0, (x1 - x0) * 4);
	}
}

/* ---- text ---- */

void DisplayClass::drawChar(int x, int y, char ch, uint32_t fg, uint32_t bg, int size, bool opaque)
{
	if (ch < 0x20 || ch > 0x7E) ch = '?';
	const uint8_t *g = font5x7[ch - 0x20];
	for (int col = 0; col < 6; col++) {
		uint8_t bits = col < 5 ? g[col] : 0;
		for (int row = 0; row < 8; row++) {
			bool on = bits & (1 << row);
			if (on) fillRect(x + col * size, y + row * size, size, size, fg);
			else if (opaque) fillRect(x + col * size, y + row * size, size, size, bg);
		}
	}
}

int DisplayClass::textWidth(const char *s) const { return (int)strlen(s) * 6 * _size; }

size_t DisplayClass::write(uint8_t c)
{
	if (c == '\n') {
		_cx = 0;
		_cy += 8 * _size;
	} else if (c != '\r') {
		if (_wrap && _cx + 6 * _size > _w) {
			_cx = 0;
			_cy += 8 * _size;
		}
		drawChar(_cx, _cy, (char)c, _fg, _bg, _size, _opaque);
		_cx += 6 * _size;
	}
	return 1;
}

/* ---- screen ---- */

void DisplayClass::show(int x, int y, int w, int h)
{
	if (!_buf || !_dev) return;
	if (x < 0) { w += x; x = 0; }
	if (y < 0) { h += y; y = 0; }
	if (x + w > _w) w = _w - x;
	if (y + h > _h) h = _h - y;
	if (w <= 0 || h <= 0) return;
	struct display_buffer_descriptor desc = {};
	desc.width = w;
	desc.height = h;
	desc.pitch = _w;
	desc.buf_size = (size_t)_w * h * 4;
	display_write(dev_of(_dev), x, y, &desc, _buf + (size_t)y * _w + x);
}

void DisplayClass::show() { show(0, 0, _w, _h); }

void DisplayClass::setBrightness(uint8_t level)
{
	if (_dev) display_set_brightness(dev_of(_dev), level);
}

void DisplayClass::blank(bool on)
{
	if (!_dev) return;
	if (on) display_blanking_on(dev_of(_dev));
	else display_blanking_off(dev_of(_dev));
}

bool DisplayClass::showYuv(const void *y, const void *uv, int w, int h, int strideY, int strideUV,
			   bool nv21, bool fullRange, bool bt709, bool nonblock)
{
	if (!_dev) return false;
	struct display_sunxi_yuv img = {};
	img.y = y;
	img.uv = uv;
	img.nv21 = nv21;
	img.width = w;
	img.height = h;
	img.stride_y = strideY;
	img.stride_uv = strideUV;
	img.full_range = fullRange;
	img.bt709 = bt709;
	img.nonblock = nonblock;
	return display_sunxi_show_yuv(dev_of(_dev), &img) == 0;
}

void DisplayClass::hideYuv()
{
	if (_dev) display_sunxi_hide_yuv(dev_of(_dev));
}

void DisplayClass::cacheFlush(const void *p, size_t n)
{
	sys_cache_data_flush_range(const_cast<void *>(p), n);
}
