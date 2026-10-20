ENTRY(_start)
SECTIONS {
 . = 0x40010000;
 .text : { *(.text*) }
 .rodata : { *(.rodata*) }
 .data : { *(.data*) }
 .tdata : ALIGN(64) { *(.tdata*) }
 .tbss : { *(.tbss*) }
 __tls_image_start = LOADADDR(.tdata);
 __tls_image_end = LOADADDR(.tdata) + SIZEOF(.tdata);
 .bss : { *(.bss*) *(COMMON) }
 /DISCARD/ : { *(.ARM.exidx*) *(.comment) *(.note*) }
}
