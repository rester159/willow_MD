/* Diagnostic Genesis platform binding. Continuous source scheduling, no
 * source wait/scheduler/trap patch. Hardware level6 VBlank carries sourceIRQ2.
 * SRAM F000..F73F contains only observation counters/ring, F800..F9FF ports. */
.text
.balign 2
.globl nativeCadenceBoot
nativeCadenceBoot:
 move.w #0x2700,%sr
 move.l #0x53454741,0xa14000
 move.b #1,0xa130f1
 move.w #0x8004,0xc00004
 move.w #0x8104,0xc00004
 move.w #0x8c00,0xc00004
 move.w #0x8f02,0xc00004
 lea 0x20f000,%a0
 move.w #511,%d0
.LclearDiagnostic:
 clr.l (%a0)+
 dbra %d0,.LclearDiagnostic
 /* Raw input and DIP bytes are supplied by the host before each frame.
  * Enable VBlank while source init is masked; pending IRQ waits for14C8. */
 move.w #0x8124,0xc00004
 movea.l #0x5b6,%a4
 jmp 0x6f2

.globl nativeCadenceVBlank
nativeCadenceVBlank:
 move.w %sr,-(%sp)
 movem.l %d0-%d3/%a0-%a2,-(%sp)
 addq.l #1,0x20f000
 move.l 0x20f000,%d0
 subq.l #1,%d0
 and.l #31,%d0
 lsl.l #5,%d0
 lea 0x20f040,%a1
 adda.l %d0,%a1
 move.l 0x20f000,(%a1)
 move.w 0xc00008,4(%a1)
 move.w 30(%sp),6(%a1)
 move.b 0xff8018,8(%a1)
 move.b 0xff0000,9(%a1)
 move.b 0xff0020,10(%a1)
 clr.b 11(%a1)
 move.w 0xff8004,12(%a1)
 move.w 0xff8008,14(%a1)
 move.l 32(%sp),16(%a1)
 clr.l 20(%a1)
 clr.l 24(%a1)
 clr.l 28(%a1)
 lea 0xff0000,%a0
 moveq #15,%d0
 moveq #1,%d1
.Ltask:
 cmp.b #4,(%a0)
 blo .Lsleep
 or.w %d1,22(%a1)
 bra .LnextTask
.Lsleep:
 cmp.w #0x0101,(%a0)
 bne .LnextTask
 cmp.l #0x1756,4(%a0)
 bne .LnextTask
 move.l 8(%a0),%d2
 move.l 12(%a0),%d3
 cmp.l %d3,%d2
 bhi .LbadContext
 sub.l #128,%d3
 cmp.l %d3,%d2
 blo .LbadContext
 add.l #64,%d3
 cmp.l %d3,%d2
 bhi .LbadContext
 movea.l %d2,%a2
 move.l 60(%a2),%d2
 or.w %d1,20(%a1)
 addq.l #1,0x20f004
 cmp.l #0x932c,%d2
 bne .Lmanager
 or.w %d1,24(%a1)
 addq.l #1,0x20f008
 bra .LnextTask
.Lmanager:
 cmp.l #0x7ff2,%d2
 bne .Lattract
 or.w %d1,28(%a1)
 addq.l #1,0x20f00c
 bra .LnextTask
.Lattract:
 cmp.l #0x810e,%d2
 bne .Lnested
 or.w %d1,30(%a1)
 addq.l #1,0x20f01c
 bra .LnextTask
.Lnested:
 cmp.l #0x1ea2,%d2
 beq .LcountNested
 cmp.l #0x1ec4,%d2
 bne .LnextTask
.LcountNested:
 or.w %d1,26(%a1)
 addq.l #1,0x20f010
 bra .LnextTask
.LbadContext:
 addq.l #1,0x20f024
.LnextTask:
 lea 32(%a0),%a0
 add.w %d1,%d1
 dbra %d0,.Ltask
 tst.w 22(%a1)
 beq .Lactive
 addq.l #1,0x20f018
.Lactive:
 cmp.w #2,12(%a1)
 bne .Lfinish
 cmp.w #2,14(%a1)
 bne .Lfinish
 addq.l #1,0x20f020
 tst.w 24(%a1)
 bne .Lfinish
 addq.l #1,0x20f014
.Lfinish:
 movem.l (%sp)+,%d0-%d3/%a0-%a2
 move.w (%sp)+,%sr
 and.w #0xf8ff,%sr
 or.w #0x2200,%sr
 jmp 0x1554
