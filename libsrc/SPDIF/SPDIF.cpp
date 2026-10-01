/* SPDX-License-Identifier: Apache-2.0 */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>

#include "Arduino.h"
#include "SPDIF.h"

static const struct device *const owa = DEVICE_DT_GET(DT_NODELABEL(owa));

SPDIFClass SPDIF;

bool SPDIFClass::begin(uint32_t sampleRate, uint8_t bitsPerSample, bool rx, bool loopback)
{
	if (bitsPerSample != 16 && bitsPerSample != 24) {
		return false;
	}
	return _e.open(owa, sampleRate, bitsPerSample, 2, rx ? 2 : 0, loopback) == 0;
}

void SPDIFClass::end()
{
	_e.close();
}

size_t SPDIFClass::write(const uint8_t *buf, size_t n)
{
	return _e.write(buf, n, true);
}
