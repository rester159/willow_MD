/* LINKER PROPOSAL ONLY: not included by shipping or cadence builds.
 * Restricted to the observed compact 1800-frame route, after source 6F2 clear.
 * The integrating linker must reject every unexpected allocatable section.
 * Tag only the new renderer globals with .native_renderer_bss; do not collect
 * all application .bss or obsolete SGDK globals into this reservation. */
SECTIONS
{
  .native_renderer_bss 0xFF4000 (NOLOAD) :
  {
    __native_renderer_bss_start = .;
    *(.native_renderer_bss .native_renderer_bss.*)
    . = ALIGN(4);
    __native_renderer_bss_used_end = .;
    /* Unoccupied remainder is canary padding, not mutable globals. */
    . = __native_renderer_bss_start + 0x1000;
    __native_renderer_bss_end = .;
  }
  .native_renderer_stack 0xFF7000 (NOLOAD) :
  {
    __native_renderer_stack_bottom = .;
    /* Integration should canary the lowest 32 bytes and check high-water. */
    . += 0x800;
    __native_renderer_stack_top = .;
  }
}
ASSERT(__native_renderer_bss_used_end - __native_renderer_bss_start <= 0x1000,
       "native renderer globals exceed route-proved 4 KiB reservation")
ASSERT(__native_renderer_stack_top == 0xFF7800,
       "native renderer stack reservation changed")
