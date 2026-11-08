/* Optional original-source leaf writer hooks. Not production admission.
 * Whole bodies relocate so backward DBF targets cannot re-enter hook prologues.
 * Save CCR around metadata only; never change IRQ mask, registers or waits.
 * BUSY increments before READY/epoch changes and before every source store.
 * Callers may certify only when BUSY==0, epoch!=0, with coherent rechecks.
 */
#define EPOCH 0x20f568
#define READY 0x20f56c
#define BUSY  0x20f56e
.section .text,"ax"
.balign 2
.globl paletteEpochRaw,paletteEpochCopy,paletteEpochAnimated,paletteEpochFade
.globl paletteEpochRawBody,paletteEpochCopyBody,paletteEpochAnimatedBody,paletteEpochFadeBody
.globl paletteEpochRawEnd,paletteEpochCopyEnd,paletteEpochAnimatedEnd,paletteEpochFadeEnd
.globl paletteEpochHooksStart,paletteEpochHooksEnd

.macro invalidate
 move.w %sr,-(%sp)
 addq.w #1,BUSY
 clr.w READY
 addq.l #1,EPOCH
 move.w (%sp)+,%ccr
.endm
.macro complete
 move.w %sr,-(%sp)
 subq.w #1,BUSY
 move.w (%sp)+,%ccr
 rts
.endm

/* Original095E..0976, including all512 source words. */
paletteEpochHooksStart:
paletteEpochRaw:
 invalidate
paletteEpochRawBody:
 move.w #31,%d7
.Lraw:
 .rept 8
 move.l (%a0)+,(%a1)+
 .endr
 dbra %d7,.Lraw
paletteEpochRawEnd:
 complete

/* Original09FC..0A10. Entry09F8 still initializesD1 before jumping here. */
paletteEpochCopy:
 invalidate
paletteEpochCopyBody:
.Lcopy:
 .rept 8
 move.l (%a0)+,(%a1)+
 .endr
 dbra %d1,.Lcopy
paletteEpochCopyEnd:
 complete

/* Original0C92..0CD0; preserve OR of original destination brightness. */
paletteEpochAnimated:
 invalidate
paletteEpochAnimatedBody:
 moveq #1,%d0
.Lanimated:
 .rept 4
 move.l (%a0)+,%d1
 move.l (%a1),%d2
 and.l #0xf000f000,%d2
 or.l %d2,%d1
 move.l %d1,(%a1)+
 .endr
 dbra %d0,.Lanimated
paletteEpochAnimatedEnd:
 complete

/* Original526E..527E. The5260 first-color decision remains in source ROM;
 * no-write paths never invalidate a lease or change the native generation. */
paletteEpochFade:
 invalidate
paletteEpochFadeBody:
 move.w #3,%d6
.Lfade:
 .rept 4
 add.w %d2,(%a0)+
 .endr
 dbra %d6,.Lfade
paletteEpochFadeEnd:
 complete
paletteEpochHooksEnd:
