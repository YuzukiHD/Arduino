/* SPDX-License-Identifier: Apache-2.0 */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/display.h>
#include <zephyr/kernel.h>
#include <lvgl.h>

#include "LVGL.h"

LVGLClass LVGL;

bool LVGLClass::begin(uint8_t brightness)
{
	const struct device *d = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
	if (!device_is_ready(d)) {
		return false;
	}
	display_set_brightness(d, brightness);
	lv_timer_handler();
	display_blanking_off(d);
	return true;
}

uint32_t LVGLClass::handle(uint32_t maxSleepMs)
{
	uint32_t ms = lv_timer_handler();
	k_msleep(ms < maxSleepMs ? ms : maxSleepMs);
	return ms;
}
