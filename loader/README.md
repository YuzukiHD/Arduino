# Loader

`boot.img` and `arduino_boot_fel.bin` are the first-stage loader of the boards: it runs from SRAM, brings up the
PSRAM, copies the core and the sketch from the SPI NOR flash into PSRAM and starts the core. They are binaries of
an application of [SyterKit](https://github.com/YuzukiHD/SyterKit) (GPL-2.0-or-later, with its PSRAM library),
`boards/yuzukineko/app_sram/arduino-boot`; `arduino-boot/main.c` is its source (the binaries were built with SyterKit v0.5.0, commit 7ae2d79f8, plus this application).

| file | use |
|---|---|
| `boot.img` | written to the start of the flash, three copies of the loader at 0x0, 0x10000 and 0x20000 |
| `arduino_boot_fel.bin` | the same loader as a FEL image: written to 0x20000 of the SRAM and executed to start from FEL |

Flash layout: loader 0x000000, core header page + core 0x030000, sketch 0x500000 (see `tools/upload.sh`).

Rebuild (SyterKit checkout, an XuanTie or xPack riscv GCC, no Rust):

    make O=out CROSS_COMPILE=riscv-none-elf- yuzukineko_rv32_sram_defconfig
    make O=out CROSS_COMPILE=riscv-none-elf- arduino-boot
    # out/build/yuzukineko/app_sram/arduino-boot/arduino-boot_fel.bin and _spi.bin
    # boot.img = three copies of arduino-boot_spi.bin at 0, 0x10000, 0x20000 (the rest 0xff)
