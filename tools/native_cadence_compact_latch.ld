SECTIONS {
 . = 0x101400;
 .text : { *(.text*) *(.rodata*) }
 /DISCARD/ : { *(.comment*) *(.note*) }
}
