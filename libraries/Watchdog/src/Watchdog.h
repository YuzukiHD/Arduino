/* SPDX-License-Identifier: Apache-2.0 */
#ifndef Watchdog_h
#define Watchdog_h

#include <stdint.h>
#include "Arduino.h"

/*
 * Hardware watchdog: resets the SoC when reset() is not called within the
 * timeout. The hardware periods are 1 to 6 s, 8, 10, 12, 14 and 16 s; begin()
 * picks the first one that is not shorter than the request.
 *
 *   Watchdog.begin(4000);
 *   void loop() { ...; Watchdog.reset(); }
 */
class WatchdogClass {
public:
	/* start the watchdog; false when the timeout is above 16 s or the hardware is not available */
	bool begin(uint32_t timeoutMs = 4000);
	/* feed it */
	void reset();
	/* stop it */
	void end();
	bool active() const { return _active; }
	/* the period in use, 0 when stopped */
	uint32_t timeoutMs() const { return _timeoutMs; }
	/* the hardware period that begin() would use for this request, 0 when too long */
	static uint32_t roundTimeout(uint32_t timeoutMs);

private:
	bool _active = false;
	uint32_t _timeoutMs = 0;
};

extern WatchdogClass Watchdog;

#endif
