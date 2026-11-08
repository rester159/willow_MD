/* Isolated IRQ test carrier. TRAP creates the CPU's genuine six-byte frame;
 * unmodified source1554 restores all registers and consumes it with RTE. */
.text
.balign 2
.globl nativeSourceIRQProbe
nativeSourceIRQProbe:
 trap #13
.globl nativeSourceIRQReturn
nativeSourceIRQReturn:
 bra nativeSourceIRQReturn
.globl nativeSourceIRQCarrier
nativeSourceIRQCarrier:
 and.w #0xf8ff,%sr
 or.w #0x2200,%sr
 jmp 0x1554
