/* Diagnostic hardware IRQ only: exact native save/restore plus read-only
 * observation of the two known implicit source IRQ registers. */
.text
.globl willowBackgroundWriterIRQProbe
willowBackgroundWriterIRQProbe:
 movem.l %d0-%d7/%a0-%a6,-(%sp)
 move.l %d4,0x20f800
 move.l %a5,0x20f804
 addq.w #1,0x20f808
 movem.l (%sp)+,%d0-%d7/%a0-%a6
 rte
