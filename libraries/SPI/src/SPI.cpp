/* SPDX-License-Identifier: Apache-2.0 */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/spi.h>

#include "SPI.h"

#if defined(SPI_DEV_NODE) && DT_NODE_HAS_STATUS(SPI_DEV_NODE, okay)
SPIClass SPI(DEVICE_DT_GET(SPI_DEV_NODE));
#endif

void SPIClass::begin()
{
	const struct device *d = (const struct device *)_dev;
	_begun = d && device_is_ready(d);
}

void SPIClass::end()
{
	_begun = false;
}

static int xfer(const void *dev, const SPISettings &s, const void *tx, void *rx, size_t n)
{
	struct spi_config cfg = {};
	cfg.frequency = s.clock;
	cfg.operation = SPI_WORD_SET(8) | SPI_OP_MODE_MASTER |
			(s.bitOrder == MSBFIRST ? 0 : SPI_TRANSFER_LSB) |
			((s.dataMode & 1) ? SPI_MODE_CPHA : 0) |
			((s.dataMode & 2) ? SPI_MODE_CPOL : 0);
	struct spi_buf txb = {.buf = (void *)tx, .len = n};
	struct spi_buf rxb = {.buf = rx, .len = n};
	struct spi_buf_set txs = {.buffers = &txb, .count = 1};
	struct spi_buf_set rxs = {.buffers = &rxb, .count = 1};
	return spi_transceive((const struct device *)dev, &cfg, tx ? &txs : NULL, rx ? &rxs : NULL);
}

void SPIClass::transfer(const void *tx, void *rx, size_t count)
{
	if (_begun && count) {
		xfer(_dev, _s, tx, rx, count);
	}
}

void SPIClass::transfer(void *buf, size_t count)
{
	transfer(buf, buf, count);
}

uint8_t SPIClass::transfer(uint8_t data)
{
	uint8_t rx = 0;
	transfer(&data, &rx, 1);
	return rx;
}

uint16_t SPIClass::transfer16(uint16_t data)
{
	uint8_t b[2];
	if (_s.bitOrder == MSBFIRST) {
		b[0] = data >> 8; b[1] = data;
	} else {
		b[0] = data; b[1] = data >> 8;
	}
	transfer(b, 2);
	return _s.bitOrder == MSBFIRST ? (b[0] << 8) | b[1] : (b[1] << 8) | b[0];
}
