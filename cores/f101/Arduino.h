/*
 * Arduino core for the Allwinner F101 on top of Zephyr.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * This header does not include any Zephyr header: sketches and libraries see
 * the classic Arduino namespace only (min/max/abs are macros, as in AVR core).
 */
#ifndef Arduino_h
#define Arduino_h

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

#include "pins_arduino.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ARDUINO 10819
#define ARDUINO_ARCH_F101 1
#define ARDUINO_ARCH_ZEPHYR 1

#define HIGH 0x1
#define LOW 0x0

#define INPUT 0x0
#define OUTPUT 0x1
#define INPUT_PULLUP 0x2
#define INPUT_PULLDOWN 0x3

#define PI 3.1415926535897932384626433832795
#define HALF_PI 1.5707963267948966192313216916398
#define TWO_PI 6.283185307179586476925286766559
#define DEG_TO_RAD 0.017453292519943295769236907684886
#define RAD_TO_DEG 57.295779513082320876798154814105
#define EULER 2.718281828459045235360287471352

#define LSBFIRST 0
#define MSBFIRST 1

#define CHANGE 2
#define FALLING 3
#define RISING 4
#define ONLOW 5
#define ONHIGH 6

#define DEFAULT 1
#define EXTERNAL 0

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))
#define constrain(amt, low, high) ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))
#define round(x) ((x) >= 0 ? (long)((x) + 0.5) : (long)((x) - 0.5))
#define radians(deg) ((deg) * DEG_TO_RAD)
#define degrees(rad) ((rad) * RAD_TO_DEG)
#define sq(x) ((x) * (x))

#define clockCyclesPerMicrosecond() (24UL)
#define clockCyclesToMicroseconds(a) ((a) / clockCyclesPerMicrosecond())
#define microsecondsToClockCycles(a) ((a) * clockCyclesPerMicrosecond())

#define lowByte(w) ((uint8_t)((w) & 0xff))
#define highByte(w) ((uint8_t)((w) >> 8))

#define bitRead(value, bit) (((value) >> (bit)) & 0x01)
#define bitSet(value, bit) ((value) |= (1UL << (bit)))
#define bitClear(value, bit) ((value) &= ~(1UL << (bit)))
#define bitToggle(value, bit) ((value) ^= (1UL << (bit)))
#define bitWrite(value, bit, bitvalue) ((bitvalue) ? bitSet(value, bit) : bitClear(value, bit))
#ifndef _BV
#define _BV(bit) (1UL << (bit))
#endif

#define PROGMEM
#define PSTR(s) (s)
#define pgm_read_byte(addr) (*(const unsigned char *)(addr))
#define pgm_read_word(addr) (*(const unsigned short *)(addr))
#define pgm_read_dword(addr) (*(const unsigned long *)(addr))
#define pgm_read_float(addr) (*(const float *)(addr))
#define pgm_read_ptr(addr) (*(void *const *)(addr))
#define strcpy_P(d, s) strcpy((d), (s))
#define strlen_P(s) strlen(s)
#define memcpy_P(d, s, n) memcpy((d), (s), (n))
#define sprintf_P sprintf
#define snprintf_P snprintf

typedef bool boolean;
typedef uint8_t byte;
typedef uint16_t word;
typedef int pin_size_t;
typedef unsigned int PinMode;
typedef unsigned int PinStatus;
typedef unsigned int BitOrder;

/* libm (the C library of the image has only sqrt, the rest comes from the toolchain's libm) */
double sin(double); double cos(double); double tan(double);
double asin(double); double acos(double); double atan(double); double atan2(double, double);
double sinh(double); double cosh(double); double tanh(double);
double exp(double); double log(double); double log10(double); double pow(double, double);
double floor(double); double ceil(double); double fabs(double); double fmod(double, double);
float sinf(float); float cosf(float); float tanf(float); float atan2f(float, float);
float expf(float); float logf(float); float powf(float, float);
float floorf(float); float ceilf(float); float fabsf(float); float fmodf(float, float);

void init(void);
void initVariant(void);

void pinMode(pin_size_t pin, PinMode mode);
void digitalWrite(pin_size_t pin, PinStatus val);
int digitalRead(pin_size_t pin);

int analogRead(pin_size_t pin);
void analogReference(uint8_t mode);
void analogReadResolution(int bits);
void analogWriteResolution(int bits);
void analogWrite(pin_size_t pin, int val);
void analogWriteFrequency(pin_size_t pin, uint32_t hz);

unsigned long millis(void);
unsigned long micros(void);
void delay(unsigned long ms);
void delayMicroseconds(unsigned int us);
void yield(void);

unsigned long pulseIn(pin_size_t pin, uint8_t state, unsigned long timeout);
unsigned long pulseInLong(pin_size_t pin, uint8_t state, unsigned long timeout);
void shiftOut(pin_size_t dataPin, pin_size_t clockPin, BitOrder bitOrder, uint8_t val);
uint8_t shiftIn(pin_size_t dataPin, pin_size_t clockPin, BitOrder bitOrder);

void attachInterrupt(pin_size_t pin, void (*userFunc)(void), int mode);
void detachInterrupt(pin_size_t pin);

void interrupts(void);
void noInterrupts(void);

void randomSeed(unsigned long seed);
long map(long x, long in_min, long in_max, long out_min, long out_max);
void noTone(pin_size_t pin);

#ifdef __cplusplus
} /* extern "C" */

/* C++ only */
#include "WString.h"
#include "Print.h"
#include "Stream.h"
#include "HardwareSerial.h"

#ifndef _NOP
#define _NOP() __asm__ volatile("nop")
#endif

long random(long max);
long random(long min, long max);
void tone(pin_size_t pin, unsigned int frequency, unsigned long duration = 0);

template <typename T> static inline T absT(T x) { return x < 0 ? -x : x; }
#undef abs
#define abs(x) absT(x)

uint16_t makeWord(uint16_t w);
uint16_t makeWord(uint8_t h, uint8_t l);
#define word(...) makeWord(__VA_ARGS__)

/* weak hooks the sketch may define */
void setup(void);
void loop(void);
void serialEvent(void) __attribute__((weak));
void serialEvent1(void) __attribute__((weak));

#endif /* __cplusplus */

#endif /* Arduino_h */
