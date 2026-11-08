/* Optional original-store column epochs. Never changes original source maps,
 * instructions, D4 or A5. The complete swizzled span is marked once before its
 * first store; no per-pair comparison, hash, payload or native geometry work. */
#ifndef WILLOW_BG_SCROLL2_BASE
#error "Supply Scroll2 base"
#endif
#ifndef WILLOW_BG_SCROLL3_BASE
#error "Supply Scroll3 base"
#endif
.section .text,"ax"
.balign 2
.extern willowNativeBackgroundColumnsState
.equ BG_EPOCH,0
.equ BG_GEN2,4
.equ BG_GEN3,8
.equ BG_BUSY,12
.equ BG_TOUCHED,14
.equ BG_ACTIVE,16
.equ BG_FAULT,18
.equ C_EPOCH,20
.equ C_OPEN,24
.equ C_STATE,28
.equ C_RESERVED,30
.equ C_MASK2,32
.equ C_MASK3,40
.globl willowBackgroundColumnsController2,willowBackgroundColumnsController3
.globl willowBackgroundColumnsBeginEpoch,willowBackgroundColumnsCommitEpoch
willowBackgroundColumnsController2:
 bsr.w willowBackgroundColumnsBeginEpoch
 jmp 0x42602
willowBackgroundColumnsController3:
 jsr 0x42f48
 bsr.w willowBackgroundColumnsCommitEpoch
 rts
willowBackgroundColumnsBeginEpoch:
 move.w %sr,-(%sp)
 movem.l %d1/%a2,-(%sp)
 lea willowNativeBackgroundColumnsState,%a2
 move.w #3,BG_BUSY(%a2)
 tst.w BG_ACTIVE(%a2)
 bne .LbeginNested
 move.w #1,BG_ACTIVE(%a2)
 clr.w BG_TOUCHED(%a2)
 cmpi.l #-1,BG_EPOCH(%a2)
 beq .LbeginBadEpoch
 addq.l #1,BG_EPOCH(%a2)
 tst.w C_RESERVED(%a2)
 bne .LbeginCorrupt
 cmpi.w #0,C_STATE(%a2)
 beq .LbeginDone
 cmpi.w #3,C_STATE(%a2)
 beq .LbeginDone
 cmpi.w #1,C_STATE(%a2)
 bne .LbeginBadState
 move.w #2,C_STATE(%a2)
 tst.l C_EPOCH(%a2)
 beq .LbeginNoBaseline
 tst.w BG_FAULT(%a2)
 bne .LbeginBadState
 tst.l C_OPEN(%a2)
 bne .LbeginBadState
 move.l BG_EPOCH(%a2),%d1
 cmp.l C_EPOCH(%a2),%d1
 bls .LbeginBadEpoch
 move.l %d1,C_OPEN(%a2)
 bra .LbeginDone
.LbeginCorrupt:
 moveq #8,%d1
 bra .LbeginFail
.LbeginBadEpoch:
 moveq #3,%d1
 bra .LbeginFail
.LbeginNoBaseline:
 moveq #1,%d1
 bra .LbeginFail
.LbeginNested:
 move.w #2,BG_ACTIVE(%a2)
.LbeginBadState:
 moveq #2,%d1
.LbeginFail:
 move.w #3,C_STATE(%a2)
 move.w %d1,BG_FAULT(%a2)
.LbeginDone:
 movem.l (%sp)+,%d1/%a2
 move.w (%sp)+,%ccr
 rts
willowBackgroundColumnsCommitEpoch:
 move.w %sr,-(%sp)
 movem.l %d1/%a2,-(%sp)
 lea willowNativeBackgroundColumnsState,%a2
 cmpi.w #1,BG_ACTIVE(%a2)
 bne .LcommitBadState
 clr.w BG_ACTIVE(%a2)
 clr.w BG_BUSY(%a2)
 cmpi.w #0,C_STATE(%a2)
 beq .LcommitDone
 cmpi.w #3,C_STATE(%a2)
 beq .LcommitDone
 cmpi.w #2,C_STATE(%a2)
 bne .LcommitBadState
 tst.w BG_FAULT(%a2)
 bne .LcommitBadState
 tst.w C_RESERVED(%a2)
 bne .LcommitCorrupt
 move.l BG_EPOCH(%a2),%d1
 cmp.l C_OPEN(%a2),%d1
 bne .LcommitBadEpoch
 cmp.l C_EPOCH(%a2),%d1
 bls .LcommitBadEpoch
 move.l %d1,C_EPOCH(%a2)
 clr.l C_OPEN(%a2)
 move.w #1,C_STATE(%a2)
 bra .LcommitDone
.LcommitCorrupt:
 moveq #8,%d1
 bra .LcommitFail
.LcommitBadEpoch:
 moveq #3,%d1
 bra .LcommitFail
.LcommitBadState:
 moveq #2,%d1
.LcommitFail:
 move.w #3,C_STATE(%a2)
 move.w %d1,BG_FAULT(%a2)
.LcommitDone:
 movem.l (%sp)+,%d1/%a2
 move.w (%sp)+,%ccr
 rts

.macro COLUMN_HOOK name,base,layerbit,gen,masks,adjust,row,shift
.globl \name
\name:
 movem.l %d1-%d3/%a2-%a3,-(%sp)
 lea willowNativeBackgroundColumnsState,%a2
 ori.w #\layerbit,BG_BUSY(%a2)
 cmpi.w #1,BG_ACTIVE(%a2)
 bne .Lunbracketed\@
 move.w BG_TOUCHED(%a2),%d3
 andi.w #\layerbit,%d3
 bne .LgenerationDone\@
 ori.w #\layerbit,BG_TOUCHED(%a2)
 bra .Lgeneration\@
.Lunbracketed\@:
 move.w #3,C_STATE(%a2)
 move.w #7,BG_FAULT(%a2)
.Lgeneration\@:
 cmpi.l #-1,\gen(%a2)
 beq .Loverflow\@
 addq.l #1,\gen(%a2)
 bra .LgenerationDone\@
.Loverflow\@:
 move.w #3,C_STATE(%a2)
 move.w #3,BG_FAULT(%a2)
.LgenerationDone\@:
 /* Span end is start+(D0.w*stride), using LONG arithmetic. Original DBRA
  * executes D0.w+1 cells; zero and65535 counters retain those semantics. */
 move.l %a0,%d3
 subi.l #\base,%d3
 cmpi.l #0x3ffc,%d3
 bhi .LbadSpan\@
 move.w %d3,%d2
 andi.w #3,%d2
 bne .LbadSpan\@
 moveq #0,%d2
 move.w %d0,%d2
.if \row
 lsl.l #\shift,%d2
.else
 lsl.l #2,%d2
.endif
 add.l %d3,%d2
 cmpi.l #0x3ffc,%d2
 bhi .LbadSpan\@
 cmpi.w #0,C_STATE(%a2)
 beq .LsourceOnly\@
 cmpi.w #3,C_STATE(%a2)
 beq .LsourceOnly\@
 cmpi.w #2,C_STATE(%a2)
 bne .LbadState\@
 tst.w BG_FAULT(%a2)
 bne .LbadState\@
 tst.w C_RESERVED(%a2)
 bne .Lcorrupt\@
 cmpi.w #1,BG_ACTIVE(%a2)
 bne .LbadState\@
 move.l BG_EPOCH(%a2),%d1
 cmp.l C_OPEN(%a2),%d1
 bne .LbadEpoch\@
 cmp.l C_EPOCH(%a2),%d1
 bls .LbadEpoch\@
 /* Both leaf strides visit every logical column between these unwrapped
  * indices: contiguous4B cells, or exactly one64B/32B source column per row.
  * Saturate a whole64-column revolution; otherwise mark at most63 bits. */
 lsr.w #\shift,%d3
 lsr.w #\shift,%d2
 sub.w %d3,%d2
 lea \masks(%a2),%a3
 cmpi.w #63,%d2
 bcc .LallColumns\@
.LmarkColumn\@:
 moveq #0,%d1
 bset %d3,%d1
 btst #5,%d3
 bne .LhighColumn\@
 or.l %d1,(%a3)
 bra .LnextColumn\@
.LhighColumn\@:
 or.l %d1,4(%a3)
.LnextColumn\@:
 addq.w #1,%d3
 dbra %d2,.LmarkColumn\@
 bra .LsourceOnly\@
.LallColumns\@:
 moveq #-1,%d1
 move.l %d1,(%a3)
 move.l %d1,4(%a3)
 bra .LsourceOnly\@
.LbadSpan\@:
 moveq #4,%d1
 bra .Lfail\@
.Lcorrupt\@:
 moveq #8,%d1
 bra .Lfail\@
.LbadEpoch\@:
 moveq #3,%d1
 bra .Lfail\@
.LbadState\@:
 moveq #2,%d1
.Lfail\@:
 move.w #3,C_STATE(%a2)
 move.w %d1,BG_FAULT(%a2)
.LsourceOnly\@:
.globl \name\()SourceStores
\name\()SourceStores:
 move.w (%a1)+,(%a0)
 addi.w #\adjust,(%a0)+
.if \row
 move.w (%a1),(%a0)
.if \adjust
 lea 30(%a0),%a0
 lea 30(%a1),%a1
.else
 lea 62(%a0),%a0
 lea 62(%a1),%a1
.endif
.else
 move.w (%a1)+,(%a0)+
.endif
 dbra %d0,\name\()SourceStores
 /* MOVEM and RTS preserve the final original attribute-store CCR. */
 movem.l (%sp)+,%d1-%d3/%a2-%a3
 rts
.endm
COLUMN_HOOK willowBackgroundColumnsScroll2Column,WILLOW_BG_SCROLL2_BASE,1,BG_GEN2,C_MASK2,0,0,6
COLUMN_HOOK willowBackgroundColumnsScroll2Row,WILLOW_BG_SCROLL2_BASE,1,BG_GEN2,C_MASK2,0,1,6
COLUMN_HOOK willowBackgroundColumnsScroll3Column,WILLOW_BG_SCROLL3_BASE,2,BG_GEN3,C_MASK3,0x0a00,0,5
COLUMN_HOOK willowBackgroundColumnsScroll3Row,WILLOW_BG_SCROLL3_BASE,2,BG_GEN3,C_MASK3,0x0a00,1,5

/* Other proved source map writers invalidate once per complete source loop,
 * before its first store. Never infer a complete native baseline from these.
 * The same clear helpers also write Scroll1; those calls leave metadata alone.
 */
.globl willowBackgroundColumnsInvalidateWriter
willowBackgroundColumnsInvalidateWriter:
 move.w %sr,-(%sp)
 movem.l %d3/%a2,-(%sp)
 lea willowNativeBackgroundColumnsState,%a2
 move.l %a0,%d3
 subi.l #WILLOW_BG_SCROLL2_BASE,%d3
 cmpi.l #0x3fff,%d3
 bls .LbulkScroll2
 move.l %a0,%d3
 subi.l #WILLOW_BG_SCROLL3_BASE,%d3
 cmpi.l #0x3fff,%d3
 bhi .LbulkDone
 ori.w #2,BG_BUSY(%a2)
 move.w #3,C_STATE(%a2)
 move.w #7,BG_FAULT(%a2)
 cmpi.w #1,BG_ACTIVE(%a2)
 bne .LbulkGen3
 btst #1,BG_TOUCHED+1(%a2)
 bne .LbulkDone
 ori.w #2,BG_TOUCHED(%a2)
.LbulkGen3:
 cmpi.l #-1,BG_GEN3(%a2)
 beq .LbulkOverflow
 addq.l #1,BG_GEN3(%a2)
 bra .LbulkDone
.LbulkScroll2:
 ori.w #1,BG_BUSY(%a2)
 move.w #3,C_STATE(%a2)
 move.w #7,BG_FAULT(%a2)
 cmpi.w #1,BG_ACTIVE(%a2)
 bne .LbulkGen2
 btst #0,BG_TOUCHED+1(%a2)
 bne .LbulkDone
 ori.w #1,BG_TOUCHED(%a2)
.LbulkGen2:
 cmpi.l #-1,BG_GEN2(%a2)
 beq .LbulkOverflow
 addq.l #1,BG_GEN2(%a2)
 bra .LbulkDone
.LbulkOverflow:
 move.w #3,BG_FAULT(%a2)
.LbulkDone:
 movem.l (%sp)+,%d3/%a2
 move.w (%sp)+,%ccr
 rts
.globl willowBackgroundColumnsBulkCopy,willowBackgroundColumnsBulkCopySourceStores
willowBackgroundColumnsBulkCopy:
 bsr.w willowBackgroundColumnsInvalidateWriter
willowBackgroundColumnsBulkCopySourceStores:
 .word 0x3099,0xd358,0x30d9,0x3099,0xd358,0x30d9
 .word 0x3099,0xd358,0x30d9,0x3099,0xd358,0x30d9
 dbra %d0,willowBackgroundColumnsBulkCopySourceStores
 jmp 0x54e4
.globl willowBackgroundColumnsPairClear,willowBackgroundColumnsPairClearSourceStores
willowBackgroundColumnsPairClear:
 bsr.w willowBackgroundColumnsInvalidateWriter
willowBackgroundColumnsPairClearSourceStores:
 .word 0x30c1,0x30c2
 dbra %d0,willowBackgroundColumnsPairClearSourceStores
 rts
.globl willowBackgroundColumnsBulkClear,willowBackgroundColumnsBulkClearSourceStores
willowBackgroundColumnsBulkClear:
 bsr.w willowBackgroundColumnsInvalidateWriter
willowBackgroundColumnsBulkClearSourceStores:
 .word 0x30c1,0x30c2,0x30c1,0x30c2,0x30c1,0x30c2,0x30c1,0x30c2
 dbra %d0,willowBackgroundColumnsBulkClearSourceStores
 rts
