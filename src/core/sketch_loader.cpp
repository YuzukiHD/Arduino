/* SPDX-License-Identifier: Apache-2.0 */
/*
 * main() of the prebuilt core: boots, then runs the sketch that was loaded into the sketch
 * window (cores/f101/sketch_abi.h).
 */
#include <zephyr/cache.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <string.h>

#include "Arduino.h"
#include "sketch_abi.h"

extern "C" char f101_core_id_value[]; /* absolute symbol, set by the link of the core */

static void idle_forever(void)
{
	for (;;) {
		k_sleep(K_SECONDS(1));
	}
}

int main(void)
{
	init();
	initVariant();

	const struct f101_sketch_header *h = (const struct f101_sketch_header *)F101_SKETCH_BASE;

	/* the sketch was written by the downloader: make sure the CPU does not see stale lines */
	sys_cache_data_flush_and_invd_all();
	sys_cache_instr_invd_all();

	if (h->magic != F101_SKETCH_MAGIC) {
		printk("F101 Arduino core: no sketch loaded\n");
		idle_forever();
	}
	if (h->abi != F101_SKETCH_ABI || h->core_id != (uint32_t)(uintptr_t)f101_core_id_value) {
		printk("F101 Arduino core: the sketch was built for another core (id %08x, core %08x)\n",
		       (unsigned int)h->core_id, (unsigned int)(uintptr_t)f101_core_id_value);
		idle_forever();
	}

	memset((void *)(uintptr_t)h->bss_start, 0, h->bss_end - h->bss_start);
	for (void (**f)(void) = h->init_array_start; f < h->init_array_end; f++) {
		(*f)();
	}

	h->setup();
	for (;;) {
		h->loop();
		if (h->serial_event && Serial.available()) {
			h->serial_event();
		}
		if (h->serial_event1 && Serial1.available()) {
			h->serial_event1();
		}
		yield();
	}
	return 0;
}
