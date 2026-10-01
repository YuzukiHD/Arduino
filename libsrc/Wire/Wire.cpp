/* SPDX-License-Identifier: Apache-2.0 */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/i2c.h>
#include <errno.h>

#include "Wire.h"

#if defined(WIRE_DEV_NODE) && DT_NODE_HAS_STATUS(WIRE_DEV_NODE, okay)
TwoWire Wire(DEVICE_DT_GET(WIRE_DEV_NODE));
#endif

void TwoWire::begin()
{
	const struct device *d = (const struct device *)_dev;
	if (!d || !device_is_ready(d)) {
		return;
	}
	i2c_configure(d, I2C_MODE_CONTROLLER | I2C_SPEED_SET(I2C_SPEED_STANDARD));
	_begun = true;
}

void TwoWire::end()
{
	_begun = false;
}

void TwoWire::setClock(uint32_t hz)
{
	const struct device *d = (const struct device *)_dev;
	if (!d) {
		return;
	}
	uint32_t speed = hz >= 1000000 ? I2C_SPEED_FAST_PLUS : hz >= 400000 ? I2C_SPEED_FAST
									     : I2C_SPEED_STANDARD;
	i2c_configure(d, I2C_MODE_CONTROLLER | I2C_SPEED_SET(speed));
}

void TwoWire::beginTransmission(uint8_t address)
{
	_addr = address;
	_txLen = 0;
}

size_t TwoWire::write(uint8_t data)
{
	if (_txLen >= BUFFER_LENGTH) {
		return 0;
	}
	_tx[_txLen++] = data;
	return 1;
}

size_t TwoWire::write(const uint8_t *data, size_t n)
{
	size_t i = 0;
	while (i < n && write(data[i])) {
		i++;
	}
	return i;
}

uint8_t TwoWire::endTransmission(bool sendStop)
{
	(void)sendStop; /* the driver always ends a write with a STOP */
	const struct device *d = (const struct device *)_dev;
	if (!d || !_begun) {
		return 4;
	}
	int r = i2c_write(d, _tx, _txLen, _addr);
	_txLen = 0;
	switch (r) {
	case 0: return 0;
	case -ETIMEDOUT: return 5;
	case -ENXIO:
	case -EIO:
	case -ENODEV: return 2;
	default: return 4;
	}
}

uint8_t TwoWire::requestFrom(uint8_t address, uint8_t quantity, uint8_t sendStop)
{
	(void)sendStop;
	const struct device *d = (const struct device *)_dev;
	_rxLen = _rxPos = 0;
	if (!d || !_begun) {
		return 0;
	}
	if (quantity > BUFFER_LENGTH) {
		quantity = BUFFER_LENGTH;
	}
	if (i2c_read(d, _rx, quantity, address) != 0) {
		return 0;
	}
	_rxLen = quantity;
	return quantity;
}
