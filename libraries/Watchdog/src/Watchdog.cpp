/* SPDX-License-Identifier: Apache-2.0 */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/watchdog.h>

#include "Arduino.h"
#include "Watchdog.h"

WatchdogClass Watchdog;

static const struct device *const wdt = DEVICE_DT_GET(DT_NODELABEL(wdt));
static const uint32_t periods_ms[] = {1000, 2000, 3000, 4000, 5000, 6000,
				      8000, 10000, 12000, 14000, 16000};

uint32_t WatchdogClass::roundTimeout(uint32_t timeoutMs)
{
	for (size_t i = 0; i < sizeof(periods_ms) / sizeof(periods_ms[0]); i++) {
		if (timeoutMs <= periods_ms[i]) {
			return periods_ms[i];
		}
	}
	return 0;
}

bool WatchdogClass::begin(uint32_t timeoutMs)
{
	if (!device_is_ready(wdt)) {
		return false;
	}
	uint32_t period = roundTimeout(timeoutMs ? timeoutMs : 1);
	if (period == 0) {
		return false;
	}
	end();
	struct wdt_timeout_cfg cfg = {};
	cfg.window.min = 0;
	cfg.window.max = period;
	cfg.callback = NULL;
	cfg.flags = WDT_FLAG_RESET_SOC;
	if (wdt_install_timeout(wdt, &cfg) < 0 || wdt_setup(wdt, 0) < 0) {
		return false;
	}
	_active = true;
	_timeoutMs = period;
	return true;
}

void WatchdogClass::reset()
{
	if (_active) {
		wdt_feed(wdt, 0);
	}
}

void WatchdogClass::end()
{
	/* also stops a watchdog that the boot code left running */
	wdt_disable(wdt);
	_active = false;
	_timeoutMs = 0;
}
