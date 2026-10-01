/* SPDX-License-Identifier: Apache-2.0 */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/mipi_dbi.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/kernel.h>

#include "Arduino.h"
#include "DBI.h"

DBIClass DBI;

static const struct device *const dbi_dev = DEVICE_DT_GET(DT_NODELABEL(dbi));

static struct mipi_dbi_config dbi_cfg(uint8_t spi_mode)
{
	struct mipi_dbi_config c = {};
	c.mode = MIPI_DBI_MODE_SPI_4WIRE;
	c.config.operation = SPI_WORD_SET(8) | SPI_OP_MODE_MASTER | SPI_TRANSFER_MSB |
			     ((spi_mode & 1) ? SPI_MODE_CPHA : 0) | ((spi_mode & 2) ? SPI_MODE_CPOL : 0);
	return c;
}

bool DBIClass::begin(uint8_t spiMode)
{
	_mode = spiMode & 3;
	(void)device_init(dbi_dev); /* not initialized at boot: it owns PD0..PD5 */
	_begun = device_is_ready(dbi_dev);
	return _begun;
}

int DBIClass::reset(uint32_t holdMs, uint32_t settleMs)
{
	if (!_begun) return -ENODEV;
	int r = mipi_dbi_reset(dbi_dev, holdMs);
	k_msleep(settleMs);
	return r;
}

int DBIClass::command(uint8_t cmd, const uint8_t *params, size_t len)
{
	if (!_begun) return -ENODEV;
	struct mipi_dbi_config c = dbi_cfg(_mode);
	return mipi_dbi_command_write(dbi_dev, &c, cmd, params, len);
}

int DBIClass::writeData(const void *buf, size_t len)
{
	if (!_begun) return -ENODEV;
	struct mipi_dbi_config c = dbi_cfg(_mode);
	struct display_buffer_descriptor desc = {};
	desc.buf_size = len;
	desc.width = len;
	desc.height = 1;
	desc.pitch = len;
	return mipi_dbi_write_display(dbi_dev, &c, (const uint8_t *)buf, &desc, PIXEL_FORMAT_RGB_565);
}

int DBIClass::setWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
	uint8_t col[4] = {(uint8_t)(x0 >> 8), (uint8_t)x0, (uint8_t)(x1 >> 8), (uint8_t)x1};
	uint8_t row[4] = {(uint8_t)(y0 >> 8), (uint8_t)y0, (uint8_t)(y1 >> 8), (uint8_t)y1};
	int r = command(0x2A, col, 4);
	return r ? r : command(0x2B, row, 4);
}

int DBIClass::writeRGB565(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *pix, bool swapBytes)
{
	int r = setWindow(x, y, x + w - 1, y + h - 1);
	if (r) return r;
	r = command(0x2C);
	if (r) return r;
	size_t n = (size_t)w * h;
	if (!swapBytes) return writeData(pix, n * 2);
	/* stream in chunks through a swapped copy */
	static uint8_t chunk[2048] __attribute__((aligned(64)));
	const uint8_t *src = (const uint8_t *)pix;
	size_t left = n * 2;
	while (left) {
		size_t c = left > sizeof(chunk) ? sizeof(chunk) : left;
		for (size_t i = 0; i < c; i += 2) {
			chunk[i] = src[i + 1];
			chunk[i + 1] = src[i];
		}
		r = writeData(chunk, c);
		if (r) return r;
		src += c;
		left -= c;
	}
	return 0;
}
