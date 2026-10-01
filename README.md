# Arduino core for the Allwinner F101 (on Zephyr)

Write `.ino` sketches against the Arduino API; the firmware underneath is a
Zephyr application (`yuzukineko/sun252i_f101/evb`), built by `west`/CMake and
downloaded with `xfel`.

```
Arduino/
  CMakeLists.txt, prj.conf   the Zephyr application: core + libraries + sketch
  cores/f101/                Arduino.h, Print/Stream/String, HardwareSerial, wiring.cpp, main.cpp
  variants/evb/              pin names (pins_arduino.h), devicetree overlay, Kconfig fragment
  libraries/{Wire,SPI}/      I2C and SPI on top of the Zephyr drivers
  tools/f101-arduino         build / upload / run command, sketch -> C++ conversion
  boards.txt, platform.txt   Arduino IDE / arduino-cli package (experimental, see below)
  examples/                  all example sketches: core (Blink, AnalogSerial, I2CScan, StringDemo, SelfTest)
                             and one or more per peripheral library (Display*, G2DDemo, LVGLHello, Audio*, SD*, USB*, PlayVideo, ...)
```

## Build and run

`f101-arduino` sets up Zephyr, the toolchain and the Python venv of the workspace itself, `. ./env.sh` is not needed.

```bash
Arduino/tools/f101-arduino build Arduino/examples/SelfTest      # -> build/arduino/SelfTest/zephyr/zephyr.bin
Arduino/tools/f101-arduino upload build/arduino/SelfTest        # needs xfel and a board in FEL mode
Arduino/tools/f101-arduino run   Arduino/examples/Blink         # build + upload
```

`-o DIR` picks the build directory, `-p` starts from scratch, `-L DIR` adds a
library folder (third-party Arduino libraries: `<DIR>/<Name>/src/*.cpp` or the
legacy flat layout; libraries are picked from the `#include` lines, also from a
`libraries/` folder next to the sketch).

A download needs a fresh power cycle into FEL mode; with the ThunderWorkbench
use the sequence from `CLAUDE.md` with the `zephyr.bin` of the sketch.

## What is supported

| Arduino API | Implementation |
|---|---|
| `pinMode/digitalWrite/digitalRead` | Zephyr GPIO, pin number = bank*32 + pin, names `PA0`..`PF31` |
| `attachInterrupt/detachInterrupt` | GPIO callbacks (`RISING/FALLING/CHANGE/ONHIGH/ONLOW`) |
| `analogRead(A0..A11)` | GPADC, 12 bit scaled by `analogReadResolution()` (default 10) |
| `analogWrite`, `tone`, `analogWriteFrequency` | PWM0, pins `PD6 PD7 PD8 PB3` (500 Hz default) |
| `Serial` | console UART3 (PE8/PE9) with an RX ring buffer, `begin(baud, SERIAL_8N1...)` |
| `Serial1` | UART5 (PE4 TX / PE5 RX) |
| `Wire` | I2C1 on PE0 (SCL) / PE1 (SDA), controller mode only |
| `SPI` | SPI0 on PC0 (SCK) / PC4 (MOSI) / PC2 (MISO); chip select is up to the sketch |
| `millis/micros/delay/delayMicroseconds` | kernel clock, 24 MHz cycle counter |
| `String`, `Print`, `Stream`, `F()`, `printf` | own implementation, `Serial.printf()` supported |
| math | toolchain libm (`sin`, `pow`, ...) |

`setup()` and `loop()` run in Zephyr's main thread (16 KiB stack); `new`/`malloc`
use all RAM above the image.

## Libraries for the on-chip peripherals

A library is picked from the `#include` lines of the sketch. It may carry `f101.conf` (Kconfig) and
`f101.overlay` (devicetree) that are merged into the build, so the hardware it needs is switched on
only when it is used. Extra overlays/fragments: `--overlay FILE`, `--conf FILE`.

| Library | Hardware | Notes |
|---|---|---|
| `Display` | DE + TCON + RGB panel | ARGB8888 buffer, drawing/text, `show()`, video plane (`showYuv`), backlight. Claims PD0..PD22, PE6/7, PB0..PB3; `analogWrite` on PD6/7/8/PB3 becomes digital |
| `G2D` | 2D engine | fill, blit (convert/scale), blend; rotate/flip must keep format and size |
| `LVGL` | on `Display` | `LVGL.begin()`, `LVGL.handle()`, `lv_*` directly |
| `DBI` | MIPI DBI (PD0..PD5) | exclusive with `Display` (`#error` if both) |
| `Audio` | on-chip codec | PCM out, mic in, tone, volume, speaker PA (PE10) |
| `I2S`, `SPDIF` | I2S0, OWA | I2S0 has no pins on the EVB (internal loopback); `libraries/I2S/pins_PE0_PE4.overlay` for pins. S/PDIF RX DMA unresolved |
| `SD` | SMHC0 (PF0..PF6) | Arduino SD API, `File` is a `Stream`; also the `USBDrive` volume |
| `USBDevice` | CherryUSB device | `USBSerial` (CDC ACM) |
| `USBHost` | CherryUSB host | `USBHostMSC` (+FAT mount as `USBDrive`), `USBHostSerial`; one USB role per sketch |
| `VideoDecoder` | VE | hardware JPEG/PNG, H.264 stream |
| `MP4`, `AACDecoder`, `MP4Player` | demuxer, AAC, full player | player needs Display, SD and codec |
| `Watchdog` | WDT | `begin(ms)`, `reset()`, `end()` |

Status on the EVB (serial-verifiable results only; picture and sound are not judged):
verified `I2S` loopback (180k frames, 0 mismatches), `SD` (mkdir/read/seek/rename/remove),
`Watchdog` (CPU reset), `G2D` (all calls return 0), `VideoDecoder` JPEG (320x240 in 4 ms).
Built but not run yet: `Display`, `LVGL`, `DBI`, `Audio`, `SPDIF`, `USBDevice`, `USBHost`, `MP4*`, H.264, PNG.

## The EVB variant

The EVB's LCD uses PD0..PD22 and the backlight PB0..PB3, so the variant
(`variants/evb/evb.overlay`) switches the display pipeline, backlight and G2D
off and uses those pins as GPIO/PWM. A sketch that needs the display, audio,
USB or the SD card includes the matching library (below), which switches the pins back. The EVB has no
documented user LED: `LED_BUILTIN` is `PA0`, wire an LED there.
PE2 is the USB VBUS switch and PF0..PF5 the SD card, avoid them.

A new board is a new directory in `variants/<name>/` with `pins_arduino.h`,
`<name>.overlay` and `<name>.conf`, selected with `--variant` and `--board`.

## Limits

- `Wire.endTransmission(false)` (repeated start) still ends with a STOP; there is no peripheral mode.
- `Serial.print` is synchronous (polled TX) and the shell/log are disabled so that `Serial` owns the console UART.
- `analogWrite` on a pin without PWM does a digital 50 % threshold.
- No `Servo`, `EEPROM`, `WiFi` libraries yet.
- `boards.txt` / `platform.txt` make the Arduino IDE / arduino-cli call the same
  tool (the IDE recipes are no-ops and the combine step runs the real build);
  that package is **not tested** yet, the command line tool is.

## Verified on the EVB

`examples/SelfTest` on the board: String, `delay/millis/micros` (100.031 ms),
math, GPIO pull-up, ADC, PWM, Wire (NACK without a device), SPI, `printf`, and
the Serial RX path (typed characters are echoed).
