/* SPDX-License-Identifier: Apache-2.0 */
#ifndef DBI_h
#define DBI_h

#include <stdint.h>
#include <stddef.h>
#include "Arduino.h"

#ifdef ARDUINO_LIB_DISPLAY
#error "Display and DBI use the same pins (PD0..PD5): a sketch cannot include both"
#endif
#define ARDUINO_LIB_DBI 1

/*
 * 4-wire serial interface to a display controller (ST77xx/ILI9xxx style): commands go with the
 * D/C line low, parameters and pixels with it high. The serial clock is set in the devicetree
 * (5 MHz); the panel reset line is PD4 (active low).
 */
class DBIClass {
public:
	/** SPI mode 0..3 of the panel */
	bool begin(uint8_t spiMode = 0);

	/** Pulses the panel reset line and waits for it to settle */
	int reset(uint32_t holdMs = 20, uint32_t settleMs = 120);

	/** Sends a command byte followed by parameter bytes */
	int command(uint8_t cmd, const uint8_t *params = nullptr, size_t len = 0);
	template <size_t N> int command(uint8_t cmd, const uint8_t (&params)[N]) { return command(cmd, params, N); }
	/** Streams data bytes (D/C high), e.g. pixels after a memory write command */
	int writeData(const void *buf, size_t len);

	/* MIPI DCS commands every controller understands */
	int softReset() { int r = command(0x01); delay(120); return r; }
	int sleepOut() { int r = command(0x11); delay(120); return r; }
	int displayOn() { return command(0x29); }
	int displayOff() { return command(0x28); }
	int invert(bool on) { return command(on ? 0x21 : 0x20); }
	/** Column/row window of the following pixel write */
	int setWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
	/** Sets the window and writes RGB565 pixels (w*h); swapBytes puts the high byte first on the wire */
	int writeRGB565(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *pix, bool swapBytes = true);

private:
	uint8_t _mode = 0;
	bool _begun = false;
};

extern DBIClass DBI;

#endif
