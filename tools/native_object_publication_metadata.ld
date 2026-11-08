SECTIONS {
 . = 0x101000;
 .text : { *(.text*) *(.rodata*) }
 /DISCARD/ : { *(.comment*) *(.note*) }
}
