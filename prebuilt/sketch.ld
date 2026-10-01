/* Linker script of a sketch for the prebuilt core (the sketch runs from a fixed RAM window). */
OUTPUT_ARCH(riscv)
__sketch_start = 0x40E00000;
ENTRY(__sketch_start)

MEMORY
{
	SKETCH (rwx) : ORIGIN = 0x40E00000, LENGTH = 0x200000
}

SECTIONS
{
	.header : { KEEP(*(.f101_header)) } > SKETCH

	.text : {
		*(.text .text.*)
		*(.gnu.linkonce.t.*)
	} > SKETCH

	.rodata : ALIGN(4) {
		*(.rodata .rodata.* .srodata .srodata.*)
		*(.gnu.linkonce.r.*)
	} > SKETCH

	.init_array : ALIGN(4) {
		PROVIDE_HIDDEN(__init_array_start = .);
		KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*) SORT_BY_INIT_PRIORITY(.ctors.*)))
		KEEP(*(.init_array .ctors))
		PROVIDE_HIDDEN(__init_array_end = .);
	} > SKETCH

	.data : ALIGN(4) {
		*(.data .data.* .sdata .sdata.*)
		*(.gnu.linkonce.d.*)
		. = ALIGN(4);
		__sketch_image_end = .;
	} > SKETCH

	/* not in the image: the core clears it before the sketch starts */
	.bss (NOLOAD) : ALIGN(4) {
		__sketch_bss_start = .;
		*(.sbss .sbss.* .bss .bss.*)
		*(COMMON)
		. = ALIGN(4);
		__sketch_bss_end = .;
	} > SKETCH

	/DISCARD/ : {
		*(.comment) *(.riscv.attributes) *(.eh_frame*) *(.note*)
	}
}
