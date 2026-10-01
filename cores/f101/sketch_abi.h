/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Contract between the prebuilt core and a sketch linked against it.
 *
 * The sketch is linked by plain gcc to run from a fixed RAM window, calling the core through
 * the symbols of core.elf (--just-symbols). Its first bytes are a f101_sketch_header, which the
 * core finds at F101_SKETCH_BASE after it has booted Zephyr.
 */
#ifndef F101_SKETCH_ABI_H
#define F101_SKETCH_ABI_H

#include <stdint.h>

#define F101_SKETCH_BASE 0x40E00000u
#define F101_SKETCH_SIZE 0x00200000u
#define F101_SKETCH_MAGIC 0x31303146u /* "F101" */
#define F101_SKETCH_ABI 1u

struct f101_sketch_header {
	uint32_t magic;
	uint32_t abi;
	/* identifies the core.elf the sketch was linked against (absolute symbol of the core) */
	uint32_t core_id;
	void (*setup)(void);
	void (*loop)(void);
	void (*serial_event)(void);
	void (*serial_event1)(void);
	void (**init_array_start)(void);
	void (**init_array_end)(void);
	uint32_t bss_start;
	uint32_t bss_end;
	uint32_t image_end;
};

#endif
