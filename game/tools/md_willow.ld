OUTPUT_ARCH(m68k)
SEARCH_DIR(.)
/*GROUP(-lbcc -lc -lgcc)*/
__DYNAMIC  =  0;

/*
 * Setup the memory map of the SEGA Genesis.
 * stack grows down from high memory.
 *
 * The memory map look like this:
 * +--------------------+ <- low memory
 * | .text              |
 * |        _etext      |
 * |        ctor list   | the ctor and dtor lists are for
 * |        dtor list   | C++ support
 * +--------------------+
 * .                    .
 * .                    .
 * .                    .
 * +--------------------+ <- 0xE0FF0000
 * | .data              | initialized data goes here
 * |        _data       |
 * |        _edata      |
 * +--------------------+
 * | .bss               |
 * |        _bstart     | start of bss, cleared by crt0
 * |        _bend       | start of heap, used by sbrk()
 * +--------------------+
 * .                    .
 * .                    .
 * .                    .
 * |        __stack     | top of stack
 * +--------------------+ <- 0xE1000000
 */
MEMORY
{
	rom (rx) : ORIGIN = 0x00000000, LENGTH = 0xE0000000
	ram : ORIGIN = 0xE0FF0000, LENGTH = 0x00010000
}

/*
 * allocate the stack to be at the top of memory, since the stack
 * grows down
 */

PROVIDE (__stack = 0xE1000000);


SECTIONS
{
  .text 0x00000000:
  {
    KEEP(*(.text.keepboot)) *(.text.*) *(.text)
    . = ALIGN(0x4);
     __CTOR_LIST__ = .;
    LONG((__CTOR_END__ - __CTOR_LIST__) / 4 - 2)
    *(.ctors)
    LONG(0)
    __CTOR_END__ = .;
    __DTOR_LIST__ = .;
    LONG((__DTOR_END__ - __DTOR_LIST__) / 4 - 2)
    *(.dtors)
     LONG(0)
    __DTOR_END__ = .;

    *(.rodata .rodata.*)
    *(.gcc_except_table .gcc_except_table.*)

    . = ALIGN(0x4);
    __INIT_SECTION__ = . ;
    *(.init)
    SHORT (0x4E75)	/* rts */

    __FINI_SECTION__ = . ;
    *(.fini)
    SHORT (0x4E75)	/* rts */

    _etext = .;
    *(.lit)

    *(.rodata_bin)
    *(.rodata_binf)
    . = ALIGN(0x4);
    __stage_data = .;       /* willow: stages 2-5's data from here (may run past 0x300000) */
    KEEP(*(.stagedata))
    __stage_data_end = .;
  } > rom
  /* The common and stage 1-5 data, and the initialised data image after
   * them, must end below 0x400000, where stage 6's banks are stored. */
  ASSERT(__stage_data_end + SIZEOF(.data) <= 0x400000, "willow: banks 0-7 are full (stage 1-5 data reaches stage 6's stored banks)")
  ASSERT(__stage_data <= 0x300000, "willow: the common data runs into slot 6 (stage 6's banks)")
  _stext = SIZEOF (.text);

  /* Stage 6 and the ending (willow): linked at 0x300000 (cartridge slots 6
   * and 7), stored from 0x400000 (banks 8 and 9 of the SSF2 mapper; mapped
   * in while they play: src/bank.c). Everything else must not be used from
   * 0x300000 up while they are mapped (the stage 1-5 data sits there). */
  .s6 0x00300000 : AT (0x00400000)
  {
    KEEP(*(.s6 .s6.*))
  } > rom

  .data 0xE0FF0000 :
  AT ( ADDR (.text) + SIZEOF (.text) )
  {
    *(.got.plt) *(.got)
    *(.shdata)
    *(.data .data.*)
    *(.ramprog .ramprog.*)
    _edata = .;
  } > ram
  _sdata = SIZEOF (.data);

  .bss 0xE0FF0000 + SIZEOF (.data) :
  {
    _start = . ;
    *(.shbss)
    *(.bss .bss.*)
    *(COMMON)
    _bend = . ;
  } > ram

  .stab 0 (NOLOAD) :
  {
    *(.stab)
  }

  .stabstr 0 (NOLOAD) :
  {
    *(.stabstr)
  }

  .eh_frame 0 (NOLOAD) :
  {
    *(.eh_frame)
  }
}

/* willow: stages 2-5's sprite files identical to an earlier stage's are
 * stored once; their symbols are aliases (tools/gen_spr_data.py). */
INCLUDE tools/spr_alias_s2.ld
INCLUDE tools/spr_alias_s3.ld
INCLUDE tools/spr_alias_s4.ld
INCLUDE tools/spr_alias_s5.ld
