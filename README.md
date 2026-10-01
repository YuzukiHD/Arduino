# Arduino support for YuzukiHD boards

Arduino support for YuzukiHD boards: write `.ino` sketches against the Arduino API and upload them from the
Arduino IDE or arduino-cli. Underneath runs Zephyr, in a prebuilt image that the sketch is linked against
with plain gcc, so using it needs only the Arduino IDE (or arduino-cli), a RISC-V GCC and `xfel`.

Supported boards: the Allwinner F101 based boards (the F101 EVB, YuzukiNeko). More boards and SoCs are added as
a `cores/<soc>` / `variants/<board>` pair.

```
cores/f101/       Arduino.h, Print/Stream/String, HardwareSerial (headers) and the sketch entry
src/core/         implementation of the core, part of the prebuilt core
libraries/<n>/    public headers of the libraries (src/), f101.conf + f101.overlay of their hardware
libsrc/<n>/       implementation of the libraries, part of the prebuilt core
variants/evb/     pin names, devicetree overlay, Kconfig fragment
prebuilt/         core.elf/.bin/.flash, the loader (boot.img), headers and linker script for sketches
tools/            upload.sh, upload.cmd (used by the IDE), build-core (rebuilds prebuilt/, maintainers)
boards.txt, platform.txt, programmers.txt   the Arduino platform
examples/         one folder per example sketch
```

## Install

Arduino IDE 2 / arduino-cli (Windows and Linux x86_64): add this to **Additional boards manager URLs**
(File > Preferences) and install **YuzukiHD Boards** from the Boards Manager:

    https://github.com/YuzukiHD/Arduino/releases/latest/download/package_yuzukihd_index.json

It installs the platform, a RISC-V GCC (xPack, cut down to what a sketch needs) and `xfel`. Linux needs
`libusb-1.0` for `xfel` and a udev rule for the board in FEL mode (USB id 1f3a:efe8). Windows needs a WinUSB
driver for the board in FEL mode (Zadig). macOS is not packaged yet.

## Build the release (maintainers)

GitHub Actions (`.github/workflows/build.yml`) does it: it builds the core with the xPack GCC, packages it, installs the
package with arduino-cli and compiles every example, and a tag `v<platform version>` publishes the files as a release.
The same by hand: `CROSS_COMPILE=<xPack>/bin/riscv-none-elf- tools/build-core` then `tools/package --out dist`.

## How it runs

```
SPI NOR   0x000000  loader (SyterKit, 3 copies at 64 KiB intervals): brings up the PSRAM
          0x030000  core header page + core image (Zephyr, drivers, libraries; linked at 0x40000000)
          0x500000  sketch image (linked for the window at 0x40e00000)
```

At power up the BROM starts the loader, which copies the core and the sketch into PSRAM and starts
the core; the core calls `setup()` and `loop()` of the sketch. The sketch is compiled and linked by
plain gcc against `prebuilt/core/core.elf` (`platform.txt`); the headers it needs are in `prebuilt/`.

**Upload** (IDE: "Upload Using Programmer" with `xfel`, which needs no serial port): hold the FEL key,
power the board, upload. `xfel` writes loader, core and sketch to the flash and starts the loader. A board
that runs from flash does not enter FEL without the key. The loader is an application of SyterKit
(`boards/yuzukineko/app_sram/arduino-boot`, GPL-2.0).

**Rebuild the core** (maintainers, needs the west workspace this directory lives in): `tools/build-core`.

## What is supported

| Arduino API | Implementation |
|---|---|
| `pinMode/digitalWrite/digitalRead` | Zephyr GPIO, pin number = bank*32 + pin, names `PA0`..`PF31` |
| `attachInterrupt/detachInterrupt` | GPIO callbacks (`RISING/FALLING/CHANGE/ONHIGH/ONLOW`) |
| `analogRead(A0..A11)` | GPADC, 12 bit scaled by `analogReadResolution()` (default 10) |
| `analogWrite`, `tone`, `analogWriteFrequency` | PWM0, pins `PD6 PD7 PD8 PB3` (500 Hz default) |
| `Serial` | console UART3 (PE8/PE9 by default) with an RX ring buffer, `begin(baud, SERIAL_8N1...)` |
| `Serial1..Serial5` | UART5, UART2, UART1, UART0, UART4 |
| `setPins(rx, tx)`, `begin(baud, cfg, rx, tx)` | pins are routed at run time (no devicetree edit): UART0 PF4/PF2, UART1 PF1/PF0 or PB1/PB0, UART2 PF5/PF4, UART3 PE9/PE8 or PE1/PE0, UART4 PE3/PE2, UART5 PE5/PE4 (RX/TX). Other pins are refused. The first pair is the default (Serial2..Serial4 default onto the SD card pins PF0..PF5) |
| `Wire` | I2C1 on PE0 (SCL) / PE1 (SDA), controller mode only |
| `SPI` | SPI0 on PC0 (SCK) / PC4 (MOSI) / PC2 (MISO); chip select is up to the sketch |
| `millis/micros/delay/delayMicroseconds` | kernel clock, 24 MHz cycle counter |
| `String`, `Print`, `Stream`, `F()`, `printf` | own implementation, `Serial.printf()` supported |
| math | toolchain libm (`sin`, `pow`, ...) |

`setup()` and `loop()` run in Zephyr's main thread (16 KiB stack); `new`/`malloc`
use all RAM above the image.

## Libraries for the on-chip peripherals

Every library is part of the prebuilt core; a sketch just includes it. Devices that own pins or power
are started by the library in `begin()`, not at boot.

| Library | Hardware | Notes |
|---|---|---|
| `Display` | DE + TCON + RGB panel | ARGB8888 buffer, drawing/text, `show()`, video plane (`showYuv`), backlight. Uses PD0..PD22, PE6/7, PB0..PB3 once `Display.begin()` ran (LVGL is not part of the core: point the LVGL Arduino library at `Display.buffer()`) |
| `G2D` | 2D engine | fill, blit (convert/scale), blend; rotate/flip must keep format and size |
| `DBI` | MIPI DBI (PD0..PD5) | exclusive with `Display`: both use PD0..PD5, including both is an `#error` |
| `Audio` | on-chip codec | PCM out, mic in, tone, volume, speaker PA (PE10) |
| `I2S`, `SPDIF` | I2S0, OWA | I2S0 has no pins on the EVB (internal loopback); S/PDIF RX DMA unresolved |
| `SD` | SMHC0 (PF0..PF6) | Arduino SD API, `File` is a `Stream`; also the `USBDrive` volume |
| `USBDevice` | CherryUSB device | `USBSerial` (CDC ACM) |
| `USBHost` | CherryUSB host | `USBHostMSC` (+FAT mount as `USBDrive`), `USBHostSerial`; one USB role per sketch |
| `VideoDecoder` | VE | hardware JPEG/PNG, H.264 stream |
| `MP4`, `AACDecoder`, `MP4Player` | demuxer, AAC, full player | player needs Display, SD and codec |
| `Watchdog` | WDT | `begin(ms)`, `reset()`, `end()` |

Status on the EVB (serial-verifiable results only; picture and sound are not judged):
verified `I2S` loopback (180k frames, 0 mismatches), `SD` (mkdir/read/seek/rename/remove),
`Watchdog` (CPU reset), `G2D` (all calls return 0), `VideoDecoder` JPEG (320x240 in 4 ms).
Built but not run yet: `Display`, `DBI`, `Audio`, `SPDIF`, `USBDevice`, `USBHost`, `MP4*`, H.264, PNG.

## The EVB variant

Pins that the EVB's LCD and backlight use (PD0..PD22, PB0..PB3) are only claimed once `Display` (or `DBI`)
is started; PWM and UART pins are routed when first used. The EVB has no documented user LED:
`LED_BUILTIN` is `PA0`, wire an LED there. PE2 is the USB VBUS switch and PF0..PF5 the SD card, avoid them.

A new board is a new directory in `variants/<name>/` with `pins_arduino.h`, `<name>.overlay` and
`<name>.conf`.

## Limits

- `Wire.endTransmission(false)` (repeated start) still ends with a STOP; there is no peripheral mode.
- `Serial.print` is synchronous (polled TX) and the shell/log are disabled so that `Serial` owns the console UART.
- `analogWrite` on a pin without PWM does a digital 50 % threshold.
- No `Servo`, `EEPROM`, `WiFi` libraries yet.

## Verified on the EVB

`SelfTest` (String, timing, math, GPIO, ADC, PWM, Wire, SPI, Serial RX echo), `AnalogSerial`, `DisplayBasics`
on the single core, flash boot at power up, `upload.cmd` from Windows with xfel; earlier on a per-peripheral
core: `I2S` loopback, `SD`, `Watchdog`, `G2D`, `VideoDecoder` JPEG. Not run on the current core:
`DBI`, `G2D`, `VideoDecoder`, `Audio`, `SPDIF`, `USBDevice`, `USBHost`, `MP4*`, H.264, PNG.
