ENTRY(nativeCadenceBoot)
SECTIONS {
 .text 0x100010 : { *(.start) *(.text*) *(.rodata*) }
 .data : { *(.data*) }
 .bss : { *(.bss*) *(COMMON) }
 /DISCARD/ : { *(.comment) *(.eh_frame*) }
 ASSERT(SIZEOF(.data)==0 && SIZEOF(.bss)==0,"No native static writable allocations admitted")
 ASSERT(ADDR(.text)+SIZEOF(.text)<0x110000,"Platform must fit one ROM page")
}
