#!/bin/sh
# Write a sketch to the SPI NOR flash of the board and start it:
#   upload.sh <xfel> <boot.img> <loader_fel.bin> <core.flash> <sketch.bin>
# The board has to be in FEL mode (hold the FEL key while powering it). Afterwards the board boots
# from the flash by itself (the loader copies the core and the sketch into PSRAM).
XFEL="$1"; BOOT="$2"; LOADER="$3"; CORE="$4"; SKETCH="$5"
"$XFEL" version || { echo "no board in FEL mode: hold the FEL key and power cycle the board"; exit 1; }
"$XFEL" spinor write 0x0 "$BOOT" || exit 1
"$XFEL" spinor write 0x30000 "$CORE" || exit 1
"$XFEL" spinor write 0x500000 "$SKETCH" || exit 1
# run it now, without a power cycle
"$XFEL" write 0x20000 "$LOADER" && "$XFEL" exec 0x20000
