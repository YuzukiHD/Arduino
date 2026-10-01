/* SPDX-License-Identifier: Apache-2.0 */
#ifndef Display_h
#define Display_h

#include <stdint.h>
#include "Arduino.h"

#ifdef ARDUINO_LIB_DBI
#error "Display and DBI use the same pins (PD0..PD5): a sketch cannot include both"
#endif
#define ARDUINO_LIB_DISPLAY 1

/*
 * The LCD of the EVB. All drawing goes to an ARGB8888 buffer in RAM (0xAARRGGBB,
 * `stride()` pixels per row); show() copies it to the frame buffer plane that the
 * display engine scans out. Alpha 0 pixels let a video picture (showYuv) shine through.
 */
class DisplayClass : public Print {
public:
	/** Starts the panel and the backlight. Returns false when the display is not ready. */
	bool begin(uint8_t brightness = 255);
	void end();

	int width() const { return _w; }
	int height() const { return _h; }
	/** ARGB8888 drawing buffer and its row length in pixels (= width) */
	uint32_t *buffer() { return _buf; }
	int stride() const { return _w; }

	static constexpr uint32_t color(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255)
	{
		return ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
	}

	/* drawing, clipped to the screen */
	void drawPixel(int x, int y, uint32_t c);
	uint32_t getPixel(int x, int y) const;
	void fillScreen(uint32_t c);
	void fillRect(int x, int y, int w, int h, uint32_t c);
	void drawRect(int x, int y, int w, int h, uint32_t c);
	void drawFastHLine(int x, int y, int w, uint32_t c);
	void drawFastVLine(int x, int y, int h, uint32_t c);
	void drawLine(int x0, int y0, int x1, int y1, uint32_t c);
	void drawCircle(int cx, int cy, int r, uint32_t c);
	void fillCircle(int cx, int cy, int r, uint32_t c);
	void drawTriangle(int x0, int y0, int x1, int y1, int x2, int y2, uint32_t c);
	void fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, uint32_t c);
	/** copies ARGB8888 pixels (w*h, tightly packed) */
	void drawBitmap(int x, int y, const uint32_t *pix, int w, int h);

	/* text: 5x7 font in a 6x8 cell, scaled by setTextSize(); print() and printf() work */
	void setCursor(int x, int y) { _cx = x; _cy = y; }
	int getCursorX() const { return _cx; }
	int getCursorY() const { return _cy; }
	void setTextColor(uint32_t fg) { _fg = fg; _opaque = false; }
	void setTextColor(uint32_t fg, uint32_t bg) { _fg = fg; _bg = bg; _opaque = true; }
	void setTextSize(int s) { _size = s < 1 ? 1 : s; }
	void setTextWrap(bool w) { _wrap = w; }
	void drawChar(int x, int y, char ch, uint32_t fg, uint32_t bg, int size, bool opaque);
	int textWidth(const char *s) const;
	size_t write(uint8_t c) override;
	using Print::write;

	/** Copy the drawing buffer (or a rectangle of it) to the screen */
	void show();
	void show(int x, int y, int w, int h);

	/* panel */
	void setBrightness(uint8_t level);
	void blank(bool on);

	/*
	 * Video plane: shows an NV12/NV21 picture scaled to the screen. The memory must stay
	 * valid and unchanged until the next call or hideYuv(), data cache lines clean.
	 * The drawing buffer stays on top; pixels with alpha 0 show the picture.
	 */
	bool showYuv(const void *y, const void *uv, int w, int h, int strideY, int strideUV,
		     bool nv21 = false, bool fullRange = false, bool bt709 = false,
		     bool nonblock = false);
	void hideYuv();
	/** Writes a buffer back from the data cache (needed before the display reads it) */
	static void cacheFlush(const void *p, size_t n);

private:
	void *_dev = nullptr;
	uint32_t *_buf = nullptr;
	int _w = 0, _h = 0;
	int _cx = 0, _cy = 0, _size = 1;
	uint32_t _fg = 0xFFFFFFFF, _bg = 0xFF000000;
	bool _opaque = false, _wrap = true;
	void hspan(int x0, int x1, int y, uint32_t c);
};

extern DisplayClass Display;

#endif
