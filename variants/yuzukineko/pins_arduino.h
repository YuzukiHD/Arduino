/* SPDX-License-Identifier: Apache-2.0 */
#ifndef Pins_Arduino_h
#define Pins_Arduino_h

/*
 * YuzukiNeko variant (board yuzukineko) with the LCD pipeline
 * switched off, so that PB0..PB3 and PD0..PD22 are free.
 *
 * A pin number is bank * 32 + pin: use the names PA0..PF31. Analog inputs are
 * the GPADC channels A0..A11.
 */
#include "pins_gpio.h"

#define ARDUINO_YUZUKINEKO 1

#define A0 200
#define A1 201
#define A2 202
#define A3 203
#define A4 204
#define A5 205
#define A6 206
#define A7 207
#define A8 208
#define A9 209
#define A10 210
#define A11 211

/* no user LED is documented on the board: wire an LED to PA0 or redefine this */
#ifndef LED_BUILTIN
#define LED_BUILTIN PA0
#endif

/*
 * Serial = UART3 (console, PE8 TX / PE9 RX). Serial1 UART5, Serial2 UART2, Serial3 UART1,
 * Serial4 UART0, Serial5 UART4: pins are routed by begin(baud, config, rx, tx) or setPins(rx, tx)
 * and default to the first pair of the table in HardwareSerial.cpp, e.g. Serial1 = PE5 RX / PE4 TX.
 * Serial2..Serial4 default onto PF0..PF5, the pins of the SD card.
 */

#define HAVE_WIRE 1 /* i2c1 */
#define WIRE_DEV_NODE DT_NODELABEL(i2c1)
#define PIN_WIRE_SCL PE0
#define PIN_WIRE_SDA PE1
#define SCL PIN_WIRE_SCL
#define SDA PIN_WIRE_SDA

#define HAVE_SPI 1 /* spi0 */
#define SPI_DEV_NODE DT_NODELABEL(spi0)
#define PIN_SPI_SCK PC0
#define PIN_SPI_SS PC1
#define PIN_SPI_MISO PC2
#define PIN_SPI_MOSI PC4
#define SCK PIN_SPI_SCK
#define SS PIN_SPI_SS
#define MISO PIN_SPI_MISO
#define MOSI PIN_SPI_MOSI

/* analogWrite() pins, the index is the PWM channel */
#ifdef __cplusplus
static const int variant_pwm_pins[] = {PD6, PD7, PD8, PB3};
/* mux function of each of them for the PWM output, routed by analogWrite() when first used */
static const unsigned char variant_pwm_mux[] = {6, 6, 6, 7};
#endif

#endif
