OUTPUT_ARCH(powerpc:common)
ENTRY(ap_entry)
SECTIONS {
  . = 0x81200000;
  .text : { *(.text.ap_entry) *(.text*) }
  .rodata : { *(.rodata*) }
  .data : { *(.data*) *(.sdata*) }
  .bss : { *(.sbss*) *(.bss*) *(COMMON) }
  /DISCARD/ : { *(.comment) *(.eh_frame) }
}
