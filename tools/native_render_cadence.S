/* Preserve displaced source effects, including CCR, and keep expensive native
 * geometry off the original 128-byte task stacks. IRQ has its own stack/SP. */
#include "render_imports.h"
.section .text,"ax"
.balign 2
.globl renderSingle,renderDouble,renderAcquireAndPublish
.globl renderSourceIRQ
/* Preserve the actual interrupted C context for original15B4/RTE. During native
 * preparation, source IRQ code must see logical source registers, not temporary
 * C values: its A5 is global and HUD stores consume D4's existing high byte.
 * The producer wrapper's immutable saved context is the source-side boundary. */
renderSourceIRQ:
 movem.l %d0-%d7/%a0-%a6,-(%sp)
 tst.w renderSourceContextReady
 beq .LsourceIRQReady
 movea.l renderPrepareSP,%a0
 move.w 60(%a0),%ccr
 movem.l (%a0),%d0-%d7/%a0-%a6
.LsourceIRQReady:
 jmp 0x1558
renderSingle:
 jsr METADATASINGLE
 bra renderPrepareWrapper
renderDouble:
 jsr METADATADOUBLE
renderPrepareWrapper:
 move.w %sr,-(%sp)
 movem.l %d0-%d7/%a0-%a6,-(%sp)
 tst.w renderPrepareBusy
 beq .LprepareEnter
 addq.l #1,0x20f578
 bra .LprepareRestore
.LprepareEnter:
 move.w #1,renderPrepareBusy
 move.l %sp,renderPrepareSP
 move.w #1,renderSourceContextReady
 movea.l #0xff7600,%sp
#ifdef RENDER_BACKGROUND_LIVE
 /* C has no register inputs. Deterministic scratch values prevent dead high
  * halves of word temporaries from inheriting original/relocated bus pointers.
  * The immutable source context above remains the IRQ authority/restoration. */
 moveq #0,%d0
 moveq #0,%d1
 moveq #0,%d2
 moveq #0,%d3
 moveq #0,%d4
 moveq #0,%d5
 moveq #0,%d6
 moveq #0,%d7
#endif
 jsr nativeRenderPrepare
 movea.l renderPrepareSP,%sp
 clr.w renderSourceContextReady
 clr.w renderPrepareBusy
.LprepareRestore:
 movem.l (%sp)+,%d0-%d7/%a0-%a6
 move.w (%sp)+,%ccr
 rts
renderAcquireAndPublish:
 jsr METADATAACQUIREANDLATCH
 move.w %sr,-(%sp)
 movem.l %d0-%d7/%a0-%a6,-(%sp)
 tst.w renderPublishBusy
 beq .LpublishEnter
 addq.l #1,0x20f57c
 bra .LpublishRestore
.LpublishEnter:
 move.w #1,renderPublishBusy
 move.l %sp,renderPublishSP
 movea.l #0xff7800,%sp
 jsr nativeRenderPublish
 movea.l renderPublishSP,%sp
 clr.w renderPublishBusy
.LpublishRestore:
 movem.l (%sp)+,%d0-%d7/%a0-%a6
 move.w (%sp)+,%ccr
 rts

#ifdef RENDER_BACKGROUND_LIVE
.globl renderBackgroundEndIRQ,renderBackgroundRTE
renderBackgroundEndIRQ:
 move.l %sp,renderBackgroundIRQSP
 movea.l #0xff7800,%sp
 jsr nativeBackgroundPublish
 movea.l renderBackgroundIRQSP,%sp
 movem.l (%sp)+,%d0-%d7/%a0-%a6
renderBackgroundRTE:
 rte
#endif

#ifdef RENDER_BACKGROUND_FAR_STREAM
.section .willow_far_stream,"ax"
.balign 2
.globl renderFarAfterSourceClear
/* The platform chooses this A4 continuation for the unmodified6F2 clear.
 * Preserve its complete output, including CCR and the original zero SP.
 * All saves use the already reserved native stack, leaving cleared source
 * task/global RAM untouched. Original continuation expects A4=000005B6. */
renderFarAfterSourceClear:
 move.w %sr,0xff75fe
 move.l %sp,renderPrepareSP
 lea 0xff75fe,%sp
 movem.l %d0-%d7/%a0-%a6,-(%sp)
 jsr nativeRenderFarAfterClear
 movem.l (%sp)+,%d0-%d7/%a0-%a6
 move.w (%sp)+,%sr
 movea.l renderPrepareSP,%sp
 movea.l #0x5b6,%a4
 jmp (%a4)
.size renderFarAfterSourceClear,.-renderFarAfterSourceClear
#endif
