/* SPDX-License-Identifier: Apache-2.0 */
#include <zephyr/kernel.h>

#include "Arduino.h"

int main(void)
{
	init();
	initVariant();
	setup();
	for (;;) {
		loop();
		serialEventRun();
		yield();
	}
	return 0;
}
