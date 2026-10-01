/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Compiled with every sketch (it is the only file of the core directory): the header the
 * prebuilt core looks for. The symbols below are provided by sketch.ld and core.elf.
 */
#include <stdint.h>
#include "sketch_abi.h"

void setup(void);
void loop(void);
void serialEvent(void) __attribute__((weak));
void serialEvent1(void) __attribute__((weak));

extern "C" {
extern void (*__init_array_start[])(void);
extern void (*__init_array_end[])(void);
extern char __sketch_bss_start[], __sketch_bss_end[], __sketch_image_end[];
extern char f101_core_id_value[]; /* absolute symbol, value = id of the core */
}

/* extern "C": a const object has internal linkage in C++, -u could not find it */
extern "C" __attribute__((section(".f101_header"), used))
const struct f101_sketch_header f101_sketch_header = {
	F101_SKETCH_MAGIC,
	F101_SKETCH_ABI,
	(uint32_t)(uintptr_t)f101_core_id_value,
	setup,
	loop,
	serialEvent,
	serialEvent1,
	__init_array_start,
	__init_array_end,
	(uint32_t)(uintptr_t)__sketch_bss_start,
	(uint32_t)(uintptr_t)__sketch_bss_end,
	(uint32_t)(uintptr_t)__sketch_image_end,
};
