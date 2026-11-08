/* Header-embedded local native functions. Standard C ABI and no VDP writes. */
.pushsection .text
.balign 2
.local willowNativeNearCapturePreparedM68k
willowNativeNearCapturePreparedM68k:
 movem.l %d2-%d7/%a2-%a3,-(%sp)
 move.l 36(%sp),%a0
 move.l (%a0),%a1
 move.l 4(%a0),%a2
 move.l 8(%a0),%a3
 move.w 12(%a0),%d0
 move.w 14(%a0),%d6
 move.w 18(%a0),%d4
 move.w 20(%a0),%d5
 move.w 16(%a0),%d1
 moveq #1,%d2
 lsl.l %d1,%d2
 moveq #0,%d3
1:
 move.w %d0,%d1
 lsr.w #2,%d1
 moveq #0,%d7
 move.b (%a2,%d1.w),%d7
 move.w %d0,%d1
 andi.w #3,%d1
 add.w %d1,%d1
 lsr.w %d1,%d7
 andi.w #3,%d7
 lsl.w #8,%d7
 move.b (%a1,%d0.w),%d7
 cmp.w %d5,%d7
 bhs 8f
 lsl.w #4,%d7
 btst #0,%d6
 beq 10f
 addq.w #8,%d7
10:
 move.b 4(%a3,%d7.w),%d1
 btst #1,%d6
 bne 4f
 tst.b %d1
 beq 3f
 btst #0,%d1
 beq 2f
 or.l %d2,%d3
2:
 rol.l #1,%d2
 subq.w #1,%d4
 beq 7f
 btst #1,%d1
 beq 5f
 or.l %d2,%d3
 bra 5f
3:
 cmpi.w #1,%d4
 beq 7f
 rol.l #2,%d2
 subq.w #2,%d4
 beq 7f
 bra 6f
4:
 btst #1,%d1
 beq 5f
 or.l %d2,%d3
5:
 rol.l #1,%d2
 subq.w #1,%d4
 beq 7f
6:
 andi.w #1,%d6
 addi.w #22,%d0
 cmpi.w #330,%d0
 blo 1b
 subi.w #330,%d0
 bra 1b
7:
 move.l %d3,22(%a0)
 moveq #1,%d0
 bra 9f
8:
 moveq #0,%d0
9:
 movem.l (%sp)+,%d2-%d7/%a2-%a3
 rts

.macro STORE_NEAR_ROW wordoff,rowoff,bitreg
 cmp.w \wordoff(%a5),%d7
 beq .LnearSame\@
 move.w %d7,\wordoff(%a5)
 bset #8,%d6
 move.l (%a2),%d1
 and.l \bitreg,%d1
 bne .LnearKnown\@
 or.l \bitreg,(%a2)
 move.b 39(%a0),\rowoff(%a1)
 move.b 39(%a0),32+\rowoff(%a1)
 bra .LnearSame\@
.LnearKnown\@:
 move.w 38(%a0),%d1
 cmp.b \rowoff(%a1),%d1
 bhs .LnearLast\@
 move.b %d1,\rowoff(%a1)
.LnearLast\@:
 cmp.b 32+\rowoff(%a1),%d1
 bls .LnearSame\@
 move.b %d1,32+\rowoff(%a1)
.LnearSame\@:
.endm
.macro ADVANCE_NEAR rows,bytes
 lea \bytes(%a5),%a5
 move.l %a5,%d1
 sub.l 16(%a0),%d1
 cmpi.l #4116,%d1
 blo .LnearPlaneDone\@
 suba.w #4096,%a5
.LnearPlaneDone\@:
 addq.l #\rows,%a1
 addq.l #\rows,%a6
 move.l %a6,%d1
 sub.l 20(%a0),%d1
 cmpi.w #32,%d1
 bne .LnearCountDone\@
 move.l 20(%a0),%a6
 move.l 16(%a0),%a1
 lea 4116(%a1),%a1
.LnearCountDone\@:
 rol.l #\rows,%d2
.endm
.balign 2
.local willowNativeNearEnterPreparedM68k
willowNativeNearEnterPreparedM68k:
 movem.l %d2-%d7/%a2-%a6,-(%sp)
 move.l 48(%sp),%a0
 move.w 28(%a0),%d0
 move.w 30(%a0),%d6
 move.w 34(%a0),%d4
 move.w 32(%a0),%d1
 moveq #1,%d2
 lsl.l %d1,%d2
 move.l 40(%a0),%d3
 ror.l %d1,%d3
 move.l 44(%a0),%a5
 move.l 20(%a0),%a6
 adda.w %d1,%a6
 move.l 20(%a0),%d1
 btst #0,%d1
 beq .LnearID
 bset #9,%d6
.LnearID:
 move.l (%a0),%a1
 move.l 4(%a0),%a2
 move.w %d0,%d1
 lsr.w #2,%d1
 moveq #0,%d7
 move.b (%a2,%d1.w),%d7
 move.w %d0,%d1
 andi.w #3,%d1
 add.w %d1,%d1
 lsr.w %d1,%d7
 andi.w #3,%d7
 lsl.w #8,%d7
 move.b (%a1,%d0.w),%d7
 cmp.w 36(%a0),%d7
 bhs .LnearFail
 lsl.w #4,%d7
 btst #0,%d6
 beq .LnearIDCol
 addq.w #8,%d7
.LnearIDCol:
 move.l 12(%a0),%a4
 adda.w %d7,%a4
 lea 4(%a4),%a3
 move.l %a6,%d1
 sub.l 20(%a0),%d1
 move.l 16(%a0),%a1
 lea 4116(%a1),%a1
 adda.w %d1,%a1
 move.l 16(%a0),%a2
 lea 16(%a2),%a2
 btst #1,%d6
 beq .LnearEven
 addq.l #2,%a4
 bra .LnearScalar
.LnearEven:
 btst #9,%d6
 bne .LnearScalar
 cmpi.w #1,%d4
 beq .LnearScalar
 /* A complete even-row pair has aligned counts and cannot cross row31/0.
  * Each packed byte result must be0..43: underflow/borrow yields a rejected
  * byte>=255; valid arithmetic never carries or borrows between bytes. */
 move.l (%a4),%d7
 cmpi.w #0xffff,%d7
 beq .LnearFail
 swap %d7
 cmpi.w #0xffff,%d7
 beq .LnearFail
 move.w (%a6),%d1
 /* Any incoming byte above44 cannot yield a valid0..43 result. Bound
  * before packed addition so corrupt255 cannot wrap and carry to its peer. */
 cmpi.b #44,%d1
 bhi .LnearFail
 cmpi.w #0x2d00,%d1
 bhs .LnearFail
 add.w 6(%a4),%d1
 move.w %d3,%d5
 andi.w #3,%d5
 add.w %d5,%d5
 lea .LnearLeaving(%pc),%a3
 sub.w (%a3,%d5.w),%d1
 cmpi.b #43,%d1
 bhi .LnearFail
 cmpi.w #0x2c00,%d1
 bhs .LnearFail
 move.w %d1,(%a6)
 lsr.l #2,%d3
 /* Common new-column case: both native words change, neither row was dirty.
  * Set the two row envelopes with paired native stores. */
 cmp.w (%a5),%d7
 beq .LnearPairGeneric
 swap %d7
 cmp.w 128(%a5),%d7
 beq .LnearPairSwapped
 move.l %d2,%d5
 lsl.l #1,%d5
 or.l %d2,%d5
 move.l (%a2),%d1
 and.l %d5,%d1
 bne .LnearPairSwapped
 move.w %d7,128(%a5)
 swap %d7
 move.w %d7,(%a5)
 or.l %d5,(%a2)
 move.w 48(%a0),(%a1)
 move.w 48(%a0),32(%a1)
 bset #8,%d6
 bra .LnearPairDone
.LnearPairSwapped:
 swap %d7
.LnearPairGeneric:
 STORE_NEAR_ROW 0,0,%d2
 swap %d7
 move.l %d2,%d5
 lsl.l #1,%d5
 STORE_NEAR_ROW 128,1,%d5
.LnearPairDone:
 subq.w #2,%d4
 beq .LnearDone
 ADVANCE_NEAR 2,256
 bra .LnearNextID
.LnearScalar:
 move.w (%a4),%d7
 cmpi.w #0xffff,%d7
 beq .LnearFail
 moveq #0,%d1
 move.b (%a6),%d1
 btst #1,%d6
 beq .LnearTopFlag
 btst #1,(%a3)
 bra .LnearFlagKnown
.LnearTopFlag:
 btst #0,(%a3)
.LnearFlagKnown:
 beq .LnearNoFlag
 addq.w #1,%d1
.LnearNoFlag:
 lsr.l #1,%d3
 bcc .LnearNoLeaving
 subq.w #1,%d1
.LnearNoLeaving:
 cmpi.w #43,%d1
 bhi .LnearFail
 move.b %d1,(%a6)
 STORE_NEAR_ROW 0,0,%d2
 subq.w #1,%d4
 beq .LnearDone
 ADVANCE_NEAR 1,128
 btst #1,%d6
 bne .LnearNextID
 bset #1,%d6
 addq.l #2,%a4
 bra .LnearScalar
.LnearNextID:
 andi.w #0x301,%d6
 addi.w #22,%d0
 cmpi.w #330,%d0
 blo .LnearID
 subi.w #330,%d0
 bra .LnearID
.LnearDone:
 btst #8,%d6
 beq .LnearSuccess
 move.l 24(%a0),%a1
 move.w 38(%a0),%d1
 cmpi.w #32,%d1
 blo .LnearColumn
 addq.l #4,%a1
 subi.w #32,%d1
.LnearColumn:
 moveq #1,%d2
 lsl.l %d1,%d2
 or.l %d2,(%a1)
.LnearSuccess:
 moveq #1,%d0
 bra .LnearReturn
.LnearFail:
 moveq #0,%d0
.LnearReturn:
 movem.l (%sp)+,%d2-%d7/%a2-%a6
 rts
.balign 2
.LnearLeaving:
 .word 0,0x0100,0x0001,0x0101
.purgem STORE_NEAR_ROW
.purgem ADVANCE_NEAR
.popsection
