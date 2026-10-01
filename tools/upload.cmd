@echo off
rem Write a sketch to the SPI NOR flash of the board and start it:
rem   upload.cmd <xfel.exe> <boot.img> <loader_fel.bin> <core.flash> <sketch.bin>
rem The board has to be in FEL mode (hold the FEL key while powering it). Afterwards the board boots
rem from the flash by itself (the loader copies the core and the sketch into PSRAM).
"%~1" version || (echo no board in FEL mode: hold the FEL key and power cycle the board & exit /b 1)
"%~1" spinor write 0x0 "%~2" || exit /b 1
"%~1" spinor write 0x30000 "%~4" || exit /b 1
"%~1" spinor write 0x500000 "%~5" || exit /b 1
"%~1" write 0x20000 "%~3" || exit /b 1
"%~1" exec 0x20000
