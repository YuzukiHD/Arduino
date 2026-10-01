/* SPDX-License-Identifier: Apache-2.0 */
#ifndef Arduino_LVGL_h
#define Arduino_LVGL_h

/*
 * LVGL runs on the display of the board; its objects are created with the lv_*
 * functions. Zephyr registers the display with LVGL before setup() runs.
 *
 *   void setup() { LVGL.begin(); lv_obj_t *l = lv_label_create(lv_screen_active()); ... }
 *   void loop()  { LVGL.handle(); }
 *
 * The Arduino helper macros (min, max, abs, round, ...) are hidden while the LVGL
 * headers are read.
 */
#pragma push_macro("min")
#pragma push_macro("max")
#pragma push_macro("abs")
#pragma push_macro("round")
#pragma push_macro("constrain")
#pragma push_macro("sq")
#pragma push_macro("radians")
#pragma push_macro("degrees")
#pragma push_macro("word")
#pragma push_macro("bitRead")
#pragma push_macro("bitSet")
#pragma push_macro("bitClear")
#pragma push_macro("bitWrite")
#pragma push_macro("PI")
#pragma push_macro("HALF_PI")
#pragma push_macro("TWO_PI")
#pragma push_macro("DEFAULT")
#pragma push_macro("DEG_TO_RAD")
#pragma push_macro("RAD_TO_DEG")
#pragma push_macro("EULER")
#pragma push_macro("HIGH")
#pragma push_macro("LOW")
#pragma push_macro("INPUT")
#pragma push_macro("OUTPUT")
#pragma push_macro("DEC")
#pragma push_macro("HEX")
#pragma push_macro("OCT")
#pragma push_macro("BIN")
#undef min
#undef max
#undef abs
#undef round
#undef constrain
#undef sq
#undef radians
#undef degrees
#undef word
#undef bitRead
#undef bitSet
#undef bitClear
#undef bitWrite
#undef PI
#undef HALF_PI
#undef TWO_PI
#undef DEFAULT
#undef DEG_TO_RAD
#undef RAD_TO_DEG
#undef EULER
#undef HIGH
#undef LOW
#undef INPUT
#undef OUTPUT
#undef DEC
#undef HEX
#undef OCT
#undef BIN
#include <lvgl.h>
#pragma pop_macro("min")
#pragma pop_macro("max")
#pragma pop_macro("abs")
#pragma pop_macro("round")
#pragma pop_macro("constrain")
#pragma pop_macro("sq")
#pragma pop_macro("radians")
#pragma pop_macro("degrees")
#pragma pop_macro("word")
#pragma pop_macro("bitRead")
#pragma pop_macro("bitSet")
#pragma pop_macro("bitClear")
#pragma pop_macro("bitWrite")
#pragma pop_macro("PI")
#pragma pop_macro("HALF_PI")
#pragma pop_macro("TWO_PI")
#pragma pop_macro("DEFAULT")
#pragma pop_macro("DEG_TO_RAD")
#pragma pop_macro("RAD_TO_DEG")
#pragma pop_macro("EULER")
#pragma pop_macro("HIGH")
#pragma pop_macro("LOW")
#pragma pop_macro("INPUT")
#pragma pop_macro("OUTPUT")
#pragma pop_macro("DEC")
#pragma pop_macro("HEX")
#pragma pop_macro("OCT")
#pragma pop_macro("BIN")

class LVGLClass {
public:
	/** Checks the display, switches it on. Returns false when it is not ready. */
	bool begin(uint8_t brightness = 255);
	/** Runs the LVGL timers (call it from loop()); sleeps at most maxSleepMs. Returns the time LVGL wants to wait. */
	uint32_t handle(uint32_t maxSleepMs = 5);
};

extern LVGLClass LVGL;

#endif
