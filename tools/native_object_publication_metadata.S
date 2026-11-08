/* Opt-in diagnostic native metadata, not a production memory ownership claim.
 * No interrupt masking or scheduler changes. Publication-ready is written last.
 * All displaced source registers/CCR effects are preserved exactly. */
#ifdef METADATA_ORIGINAL_BUS
#define BANK0 0x900000
#define BANK1 0x908000
#define PRIOR_PORT 0x800100
#else
#define BANK0 0x20c000
#define BANK1 0x20d000
#define PRIOR_PORT 0x20f900
#endif
#define META 0x20f500
.section .text,"ax"
.balign 2
.globl metadataBegin,metadataBeginDisplaced,metadataDouble,metadataDoubleDisplaced
.globl metadataSingle,metadataSingleDisplaced,metadataClear,metadataClearDisplaced
.globl metadataAcquireAndLatch,metadataAcquire,metadataFault

metadataBegin:
 move.w %sr,-(%sp)
 movem.l %d0/%a1,-(%sp)
 moveq #0,%d0
 cmpa.l #BANK0,%a0
 beq .LbeginBank
 moveq #8,%d0
 cmpa.l #BANK1,%a0
 bne .LbeginFault
.LbeginBank:
 lea META+8,%a1
 adda.w %d0,%a1
 clr.w 6(%a1)
 move.w %d0,META+4
 addq.l #1,META
 move.l META,(%a1)
 move.w #1,META+6
 bra .LbeginDone
.LbeginFault:
 bsr metadataFault
.LbeginDone:
 movem.l (%sp)+,%d0/%a1
 move.w (%sp)+,%ccr
metadataBeginDisplaced:
 move.w #256,%d7
 move.w %d7,0x29a(%a5)
 rts

metadataDouble:
metadataDoubleDisplaced:
 bchg #7,0x23(%a5)
 bra .Lcommit
metadataSingle:
metadataSingleDisplaced:
 move.w #0x9000,0x22(%a5)
.Lcommit:
 move.w %sr,-(%sp)
 movem.l %d0-%d1/%a1,-(%sp)
 cmp.w #1,META+6
 bne .LcommitFault
 cmp.w #256,%d6
 bhi .LcommitFault
 move.w META+4,%d0
 cmp.w #8,%d0
 bhi .LcommitFault
 move.w %d0,%d1
 lsl.w #4,%d1
 or.w #0x9000,%d1
 cmp.w 0x22(%a5),%d1
 bne .LcommitFault
 lea META+8,%a1
 adda.w %d0,%a1
 move.w #256,%d1
 sub.w %d6,%d1
 move.w %d1,4(%a1)
 clr.w META+6
 move.w #1,6(%a1)
 bra .LcommitDone
.LcommitFault:
 bsr metadataFault
.LcommitDone:
 movem.l (%sp)+,%d0-%d1/%a1
 move.w (%sp)+,%ccr
 rts

/* Both exact source clear loops begin MOVE.W#255,D0; MOVEQ#0,D1. */
metadataClear:
metadataClearDisplaced:
 move.w #255,%d0
 moveq #0,%d1
 move.w %sr,-(%sp)
 movem.l %d0/%a1,-(%sp)
 moveq #0,%d0
 cmpa.l #BANK0,%a0
 beq .LclearBank
 moveq #8,%d0
 cmpa.l #BANK1,%a0
 bne .LclearFault
.LclearBank:
 lea META+8,%a1
 adda.w %d0,%a1
 clr.w 6(%a1)
 bra .LclearDone
.LclearFault:
 bsr metadataFault
.LclearDone:
 movem.l (%sp)+,%d0/%a1
 move.w (%sp)+,%ccr
 rts

/* Replacement callee for the existing pre-IRQ JSR nativeCadenceLatch.
 * Its caller already saved D0-D3/A0-A2 and SR. Preserve C ABI D2-D7/A2-A6.
 * The full original raw latch is retained during this metadata-only admission. */
metadataAcquireAndLatch:
 bsr metadataAcquire
 jsr 0x100218
 rts
metadataAcquire:
 clr.w META+32
 clr.l META+24
 clr.w META+28
 move.w PRIOR_PORT,%d0
 moveq #0,%d1
 cmp.w #0x9000,%d0
 beq .LacquireBank
 moveq #8,%d1
 cmp.w #0x9080,%d0
 bne .LacquireFault
.LacquireBank:
 move.w %d1,%d0
 lsr.w #3,%d0
 move.w %d0,META+30
 lea META+8,%a0
 adda.w %d1,%a0
 cmp.w #1,6(%a0)
 bne .LnotReady
 move.l (%a0),META+24
 move.w 4(%a0),META+28
 cmp.w #256,META+28
 bhi .LacquireFault
 move.w #1,META+32
 addq.l #1,META+36
 rts
.LnotReady:
 addq.l #1,META+40
 rts
.LacquireFault:
 bsr metadataFault
 bra .LnotReady
metadataFault:
 addq.l #1,META+44
 clr.w META+14
 clr.w META+22
 rts
