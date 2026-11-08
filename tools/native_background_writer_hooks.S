/* Source-store hooks, native68000 only. No C calls or task-stack switch.
 * Build must supply checked map bases and an owner-selected state symbol.
 * D4 and A5 remain untouched, including all IRQ-interruptible instruction gaps.
 */
#ifndef WILLOW_BG_SCROLL2_BASE
#error "Supply checked Scroll2 base"
#endif
#ifndef WILLOW_BG_SCROLL3_BASE
#error "Supply checked Scroll3 base"
#endif
.section .text,"ax"
.balign 2
.extern willowNativeBackgroundWriterState
.equ BG_EPOCH,0
.equ BG_GEN2,4
.equ BG_GEN3,8
.equ BG_BUSY,12
.equ BG_TOUCHED,14
.equ BG_ACTIVE,16
.equ BG_FAULT,18
.equ J_EPOCH,20
.equ J_OPEN,24
.equ J_COUNT,28
.equ J_STATE,30
.equ J_FAULT,32
.equ J_ENTRIES,34
.equ BG_INDEX,418
.equ BG_INDEXED_COUNT,482
.globl willowBackgroundController2,willowBackgroundController3
.globl willowBackgroundBeginEpoch,willowBackgroundCommitEpoch
willowBackgroundController2:
 bsr.w willowBackgroundBeginEpoch
 jmp 0x42602
willowBackgroundController3:
 jsr 0x42f48
 bsr.w willowBackgroundCommitEpoch
 rts
willowBackgroundBeginEpoch:
 move.w %sr,-(%sp)
 movem.l %d1/%a2,-(%sp)
 lea willowNativeBackgroundWriterState,%a2
 move.w #3,BG_BUSY(%a2)
 tst.w BG_ACTIVE(%a2)
 bne .LbeginNested
 move.w #1,BG_ACTIVE(%a2)
 clr.w BG_TOUCHED(%a2)
 cmpi.l #-1,BG_EPOCH(%a2)
 beq .LbeginBadEpoch
 addq.l #1,BG_EPOCH(%a2)
 cmpi.w #0,J_STATE(%a2)
 beq .LbeginDone
 cmpi.w #3,J_STATE(%a2)
 beq .LbeginDone
 cmpi.w #1,J_STATE(%a2)
 bne .LbeginBadState
 move.w #2,J_STATE(%a2)
 tst.l J_EPOCH(%a2)
 beq .LbeginNoBaseline
 tst.w J_FAULT(%a2)
 bne .LbeginBadState
 cmpi.w #64,J_COUNT(%a2)
 bhi .LbeginCorrupt
 move.w J_COUNT(%a2),%d1
 cmp.w BG_INDEXED_COUNT(%a2),%d1
 beq .LbeginIndexReady
 tst.w %d1
 bne .LbeginCorrupt
 /* Acknowledgment retired every entry. Clear the private index once. */
 movem.l %d2/%a3,-(%sp)
 lea BG_INDEX(%a2),%a3
 moveq #15,%d2
.LclearIndex:
 clr.l (%a3)+
 dbra %d2,.LclearIndex
 clr.w BG_INDEXED_COUNT(%a2)
 movem.l (%sp)+,%d2/%a3
.LbeginIndexReady:
 move.l BG_EPOCH(%a2),%d1
 cmp.l J_EPOCH(%a2),%d1
 bls .LbeginBadEpoch
 move.l %d1,J_OPEN(%a2)
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
 move.w #3,J_STATE(%a2)
 move.w %d1,J_FAULT(%a2)
 move.w %d1,BG_FAULT(%a2)
.LbeginDone:
 movem.l (%sp)+,%d1/%a2
 move.w (%sp)+,%ccr
 rts
willowBackgroundCommitEpoch:
 move.w %sr,-(%sp)
 movem.l %d1/%a2,-(%sp)
 lea willowNativeBackgroundWriterState,%a2
 cmpi.w #1,BG_ACTIVE(%a2)
 bne .LcommitBadState
 clr.w BG_ACTIVE(%a2)
 clr.w BG_BUSY(%a2)
 cmpi.w #0,J_STATE(%a2)
 beq .LcommitDone
 cmpi.w #3,J_STATE(%a2)
 beq .LcommitDone
 cmpi.w #2,J_STATE(%a2)
 bne .LcommitBadState
 tst.w J_FAULT(%a2)
 bne .LcommitBadState
 tst.w BG_FAULT(%a2)
 bne .LcommitBadState
 cmpi.w #64,J_COUNT(%a2)
 bhi .LcommitCorrupt
 move.w J_COUNT(%a2),%d1
 cmp.w BG_INDEXED_COUNT(%a2),%d1
 bne .LcommitCorrupt
 move.l BG_EPOCH(%a2),%d1
 cmp.l J_OPEN(%a2),%d1
 bne .LcommitBadEpoch
 cmp.l J_EPOCH(%a2),%d1
 bls .LcommitBadEpoch
 move.l %d1,J_EPOCH(%a2)
 clr.l J_OPEN(%a2)
 move.w #1,J_STATE(%a2)
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
 move.w #3,J_STATE(%a2)
 move.w %d1,J_FAULT(%a2)
 move.w %d1,BG_FAULT(%a2)
.LcommitDone:
 movem.l (%sp)+,%d1/%a2
 move.w (%sp)+,%ccr
 rts

.macro PAIR_HOOK name,base,layerbit,gen,keybit,adjust,row
.globl \name
\name:
 /* One save for the original bounded leaf loop, not one save per pair. */
 movem.l %d1-%d3/%a2-%a4,-(%sp)
 subq.l #2,%sp
 lea willowNativeBackgroundWriterState,%a2
 ori.w #\layerbit,BG_BUSY(%a2)
 cmpi.w #1,BG_ACTIVE(%a2)
 bne .Lunbracketed\@
 move.w BG_TOUCHED(%a2),%d3
 andi.w #\layerbit,%d3
 bne .LgenerationDone\@
 ori.w #\layerbit,BG_TOUCHED(%a2)
 bra .Lgeneration\@
.Lunbracketed\@:
 move.w #3,J_STATE(%a2)
 move.w #7,J_FAULT(%a2)
 move.w #7,BG_FAULT(%a2)
.Lgeneration\@:
 cmpi.l #-1,\gen(%a2)
 beq .LgenerationOverflow\@
 addq.l #1,\gen(%a2)
 bra .LgenerationDone\@
.LgenerationOverflow\@:
 move.w #3,J_STATE(%a2)
 move.w #3,J_FAULT(%a2)
 move.w #3,BG_FAULT(%a2)
.LgenerationDone\@:
 /* Prove every pair touched by this original DBRA loop is a complete source
  * map cell. D0.w+1 spans1..65536; long arithmetic cannot overflow here. */
 move.l %a0,%d3
 subi.l #\base,%d3
 cmpi.l #0x3ffc,%d3
 bhi .LbadAddress\@
 move.w %d3,%d2
 andi.w #3,%d2
 bne .LbadAddress\@
 moveq #0,%d2
 move.w %d0,%d2
.if \row
.if \adjust
 lsl.l #5,%d2
.else
 lsl.l #6,%d2
.endif
.else
 lsl.l #2,%d2
.endif
 add.l %d3,%d2
 cmpi.l #0x3ffc,%d2
 bhi .LbadAddress\@
 lsr.w #2,%d3
 ori.w #\keybit,%d3
 /* Hoisted guards remain valid for this serialized producer loop. Only this
  * leaf mutates count; append checks64 before writing. No source IRQ callee
  * writes Scroll2/3 or these owner fields. A consumer must not mutate BUSY. */
 cmpi.w #0,J_STATE(%a2)
 beq .LsourceOnly\@
 cmpi.w #3,J_STATE(%a2)
 beq .LsourceOnly\@
 cmpi.w #2,J_STATE(%a2)
 bne .LpairBadState\@
 tst.w J_FAULT(%a2)
 bne .LpairBadState\@
 cmpi.w #1,BG_ACTIVE(%a2)
 bne .LpairBadState\@
 tst.l J_OPEN(%a2)
 beq .LpairBadState\@
 move.l BG_EPOCH(%a2),%a4
 cmpa.l J_OPEN(%a2),%a4
 bne .LpairBadEpoch\@
 cmpa.l J_EPOCH(%a2),%a4
 bls .LpairBadEpoch\@
 cmpi.w #64,J_COUNT(%a2)
 bhi .LpairCorrupt\@
 move.w J_COUNT(%a2),%d2
 cmp.w BG_INDEXED_COUNT(%a2),%d2
 bne .LpairCorrupt\@
.LpairLoop\@:
 move.l (%a0),%d1
.globl \name\()SourceStores
\name\()SourceStores:
 /* Exact original8-byte raw+adjust+attribute stores. */
 move.w (%a1)+,(%a0)
 addi.w #\adjust,(%a0)+
.if \row
 move.w (%a1),(%a0)
 move.w %sr,(%sp)
 move.l -2(%a0),%d2
.else
 move.w (%a1)+,(%a0)+
 move.w %sr,(%sp)
 move.l -4(%a0),%d2
.endif
 cmp.l %d1,%d2
 beq .LnextPair\@
 move.l %d1,-(%sp)
 move.l %d2,-(%sp)
 move.w %d3,%d2
 lsr.w #6,%d2
 eor.w %d3,%d2
 andi.w #63,%d2
 lea BG_INDEX(%a2),%a3
 adda.w %d2,%a3
 moveq #63,%d1
.Lsearch\@:
 moveq #0,%d2
 move.b (%a3),%d2
 beq .Lappend\@
 cmp.w J_COUNT(%a2),%d2
 bhi .LindexCorrupt\@
 subq.w #1,%d2
 lea J_ENTRIES(%a2),%a4
 add.w %d2,%d2
 adda.w %d2,%a4
 add.w %d2,%d2
 adda.w %d2,%a4
 cmp.w (%a4),%d3
 beq .Lreplace\@
 addq.l #1,%a3
 cmpa.l #willowNativeBackgroundWriterState+BG_INDEX+64,%a3
 bne .LprobeNext\@
 lea BG_INDEX(%a2),%a3
.LprobeNext\@:
 dbra %d1,.Lsearch\@
 cmpi.w #64,J_COUNT(%a2)
 beq .LpairOverflowAfter\@
 bra .LindexCorrupt\@
.Lreplace\@:
 move.l 4(%sp),%d2
 cmp.l 2(%a4),%d2
 bne .LpairPredecessorAfter\@
 move.l (%sp),2(%a4)
 addq.l #8,%sp
 bra .LnextPair\@
.Lappend\@:
 cmpi.w #64,J_COUNT(%a2)
 beq .LpairOverflowAfter\@
 move.w J_COUNT(%a2),%d2
 lea J_ENTRIES(%a2),%a4
 add.w %d2,%d2
 adda.w %d2,%a4
 add.w %d2,%d2
 adda.w %d2,%a4
 move.w %d3,(%a4)
 move.l (%sp),2(%a4)
 addq.w #1,J_COUNT(%a2)
 addq.w #1,BG_INDEXED_COUNT(%a2)
 move.w J_COUNT(%a2),%d2
 move.b %d2,(%a3)
 addq.l #8,%sp
.LnextPair\@:
.if \row
.if \adjust
 lea 30(%a0),%a0
 lea 30(%a1),%a1
 addq.w #8,%d3
.else
 lea 62(%a0),%a0
 lea 62(%a1),%a1
 addi.w #16,%d3
.endif
.else
 addq.w #1,%d3
.endif
 dbra %d0,.LpairLoop\@
 move.w (%sp)+,%ccr
 movem.l (%sp)+,%d1-%d3/%a2-%a4
 rts
.LbadAddress\@:
 moveq #4,%d3
 bra .LfailBefore\@
.LpairCorrupt\@:
 moveq #8,%d3
 bra .LfailBefore\@
.LpairBadEpoch\@:
 moveq #3,%d3
 bra .LfailBefore\@
.LpairBadState\@:
 moveq #2,%d3
.LfailBefore\@:
 move.w #3,J_STATE(%a2)
 move.w %d3,J_FAULT(%a2)
 move.w %d3,BG_FAULT(%a2)
 bra .LsourceOnly\@
.LpairPredecessorAfter\@:
 moveq #6,%d3
 bra .LfailAfter\@
.LpairOverflowAfter\@:
 moveq #5,%d3
 bra .LfailAfter\@
.LindexCorrupt\@:
 moveq #8,%d3
.LfailAfter\@:
 addq.l #8,%sp
 move.w #3,J_STATE(%a2)
 move.w %d3,J_FAULT(%a2)
 move.w %d3,BG_FAULT(%a2)
 /* Finish the current original row's LEAs/DBRA before the untouched remaining
  * source iterations; no further before-pair reads after failed metadata. */
.if \row
.if \adjust
 lea 30(%a0),%a0
 lea 30(%a1),%a1
.else
 lea 62(%a0),%a0
 lea 62(%a1),%a1
.endif
.endif
 dbra %d0,.LsourceOnly\@
 move.w (%sp)+,%ccr
 movem.l (%sp)+,%d1-%d3/%a2-%a4
 rts
.LsourceOnly\@:
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
 dbra %d0,.LsourceOnly\@
 addq.l #2,%sp
 movem.l (%sp)+,%d1-%d3/%a2-%a4
 rts
.endm
PAIR_HOOK willowBackgroundScroll2Column,WILLOW_BG_SCROLL2_BASE,1,BG_GEN2,0,0,0
PAIR_HOOK willowBackgroundScroll2Row,WILLOW_BG_SCROLL2_BASE,1,BG_GEN2,0,0,1
PAIR_HOOK willowBackgroundScroll3Column,WILLOW_BG_SCROLL3_BASE,2,BG_GEN3,0x1000,0x0a00,0
PAIR_HOOK willowBackgroundScroll3Row,WILLOW_BG_SCROLL3_BASE,2,BG_GEN3,0x1000,0x0a00,1

/* Other proved source map writers invalidate once per complete source loop,
 * before its first store. Never infer a complete native baseline from these.
 * The same clear helpers also write Scroll1; those calls leave metadata alone.
 */
.globl willowBackgroundInvalidateWriter
willowBackgroundInvalidateWriter:
 move.w %sr,-(%sp)
 movem.l %d3/%a2,-(%sp)
 lea willowNativeBackgroundWriterState,%a2
 move.l %a0,%d3
 subi.l #WILLOW_BG_SCROLL2_BASE,%d3
 cmpi.l #0x3fff,%d3
 bls .LbulkScroll2
 move.l %a0,%d3
 subi.l #WILLOW_BG_SCROLL3_BASE,%d3
 cmpi.l #0x3fff,%d3
 bhi .LbulkDone
 ori.w #2,BG_BUSY(%a2)
 move.w #3,J_STATE(%a2)
 move.w #7,J_FAULT(%a2)
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
 move.w #3,J_STATE(%a2)
 move.w #7,J_FAULT(%a2)
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
 move.w #3,J_FAULT(%a2)
 move.w #3,BG_FAULT(%a2)
.LbulkDone:
 movem.l (%sp)+,%d3/%a2
 move.w (%sp)+,%ccr
 rts
.globl willowBackgroundBulkCopy,willowBackgroundBulkCopySourceStores
willowBackgroundBulkCopy:
 bsr.w willowBackgroundInvalidateWriter
willowBackgroundBulkCopySourceStores:
 .word 0x3099,0xd358,0x30d9,0x3099,0xd358,0x30d9
 .word 0x3099,0xd358,0x30d9,0x3099,0xd358,0x30d9
 dbra %d0,willowBackgroundBulkCopySourceStores
 jmp 0x54e4
.globl willowBackgroundPairClear,willowBackgroundPairClearSourceStores
willowBackgroundPairClear:
 bsr.w willowBackgroundInvalidateWriter
willowBackgroundPairClearSourceStores:
 .word 0x30c1,0x30c2
 dbra %d0,willowBackgroundPairClearSourceStores
 rts
.globl willowBackgroundBulkClear,willowBackgroundBulkClearSourceStores
willowBackgroundBulkClear:
 bsr.w willowBackgroundInvalidateWriter
willowBackgroundBulkClearSourceStores:
 .word 0x30c1,0x30c2,0x30c1,0x30c2,0x30c1,0x30c2,0x30c1,0x30c2
 dbra %d0,willowBackgroundBulkClearSourceStores
 rts
