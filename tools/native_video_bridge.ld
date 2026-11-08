ENTRY(nativeFrameBridgeProbe)
SECTIONS {
 .text 0x100010 : { *(.text*) *(.rodata*) }
 .data 0x20f000 : { *(.data*) }
 .bss 0x20f000 : { *(.bss*) *(COMMON) }
 /DISCARD/ : { *(.comment) *(.eh_frame*) }
 ASSERT(SIZEOF(.data) == 0, "Bridge state must be initialized explicitly")
 ASSERT(ADDR(.bss) + SIZEOF(.bss) <= 0x20f800, "Bridge state exceeds SRAM reservation")
 ASSERT(ADDR(.text) + SIZEOF(.text) < 0x200000, "Bridge code exceeds ROM reservation")
}
