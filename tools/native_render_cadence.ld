SECTIONS {
 .text 0x102000 : { __render_code_begin = .; *(.text*) __render_code_end = .; *(.rodata*) }
 .data : { *(.data*) }
 .willow_far_stream 0x146000 : {
  __render_far_code_begin = .;
  *(.willow_far_stream .willow_far_stream.*)
  . = ALIGN(4);
  __render_far_code_end = .;
 }
 .bss 0xFF4000 (NOLOAD) : { __render_bss_begin = .; *(.bss*) *(COMMON) . = ALIGN(4); __render_bss_end = .; }
 /DISCARD/ : { *(.comment*) *(.eh_frame*) *(.note*) }
}
ASSERT(SIZEOF(.data)==0,"No initialized native writable data")
PROVIDE(__render_bss_limit = 0xFF5000);
ASSERT(__render_bss_end<=__render_bss_limit,"Native renderer exceeds route-proved globals")
ASSERT(ADDR(.text)+SIZEOF(.text)<=0x10E000,"Renderer overlaps palette hook reservation")
ASSERT(__render_far_code_end<=0x150000,"Far-stream auxiliary code exceeds reserved ROM")
