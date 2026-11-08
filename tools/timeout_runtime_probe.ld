ENTRY(timeoutRuntimeProbe)
SECTIONS {
 .text 0x100010 : { *(.text*) *(.rodata*) }
 .data 0x200800 : { *(.data*) }
 .bss 0x200800 : { *(.bss*) *(COMMON) }
 /DISCARD/ : { *(.comment) *(.eh_frame*) }
 ASSERT(SIZEOF(.data) == 0, "Probe globals must be zero initialized")
 ASSERT(ADDR(.bss) + SIZEOF(.bss) <= 0x202000, "Probe state exceeds reserved SRAM")
 ASSERT(ADDR(.text) + SIZEOF(.text) < 0x200000, "Probe exceeds ROM area")
}
