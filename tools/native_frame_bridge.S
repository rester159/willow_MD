/* Native context bridge for the isolated source-layout CPU diagnostic.
 * Source program addresses and trap handlers stay at their original locations.
 * No instruction interpretation and no host intervention within one update. */
.text
.balign 2
.globl willowNativeFrameBridgeStep
.type willowNativeFrameBridgeStep,@function
willowNativeFrameBridgeStep:
 move.w %sr,willowBridgeCallerSR
 movem.l %d0-%a6,willowBridgeCallerRegisters
 move.l %sp,willowBridgeCallerSP
 move.l %usp,%a0
 move.l %a0,willowBridgeCallerUSP
 move.w #0x2700,%sr
 move.l willowBridgeGameUSP,%a0
 move.l %a0,%usp
 move.l willowBridgeMainSP,%sp
 move.l willowBridgeMainPC,-(%sp)
 movem.l willowBridgeMainRegisters,%d0-%a6
 move.w #0x2700,%sr
 rts
.size willowNativeFrameBridgeStep,.-willowNativeFrameBridgeStep

/* Gates preserve source SR and pending return address in both modes.
 * Mode zero stops the host-driven oracle; mode one stays on the native CPU. */
.globl willowBridgeWaitGate
willowBridgeWaitGate:
 move.w %sr,-(%sp)
 tst.b willowBridgeMode
 beq .LhostWait
 move.w (%sp)+,%sr
 jmp willowBridgeMainCheckpoint
.LhostWait:
 move.w (%sp)+,%sr
 jmp 0x100000

.globl willowBridgeSchedulerGate
willowBridgeSchedulerGate:
 move.w %sr,-(%sp)
 tst.b willowBridgeMode
 beq .LhostScheduler
 move.w (%sp)+,%sr
 jmp willowBridgeTaskCheckpoint
.LhostScheduler:
 move.w (%sp)+,%sr
 jmp 0x100000

.globl willowBridgeMainCheckpoint
willowBridgeMainCheckpoint:
 move.w %sr,willowBridgeMainSR
 movem.l %d0-%a6,willowBridgeMainRegisters
 move.l (%sp)+,willowBridgeMainPC
 move.l %sp,willowBridgeMainSP
 move.w #0x2700,%sr
 move.l #0xfff000,%sp
 move.l #0xff8000,%a5
 move.l #0xff0000,%a0
 moveq #15,%d0
 jmp 0x14d8

.globl willowBridgeTaskCheckpoint
willowBridgeTaskCheckpoint:
 move.w #0x2700,%sr
 move.l #0xfff004,%sp
 move.l #0xff8000,%a5
 move.w #0x2700,%sr
 jsr 0x1590
.globl willowBridgeWakeCheckpoint
willowBridgeWakeCheckpoint:
 move.l %usp,%a0
 move.l %a0,willowBridgeGameUSP
 move.l willowBridgeCallerUSP,%a0
 move.l %a0,%usp
 move.l willowBridgeCallerSP,%sp
 movem.l willowBridgeCallerRegisters,%d0-%a6
 move.w willowBridgeCallerSR,%sr
 rts

.bss
.balign 2
.globl willowBridgeMainRegisters,willowBridgeMainPC,willowBridgeMainSP
.globl willowBridgeMainSR,willowBridgeGameUSP,willowBridgeMode
willowBridgeMainRegisters: .space 60
willowBridgeMainPC: .space 4
willowBridgeMainSP: .space 4
willowBridgeMainSR: .space 2
willowBridgeGameUSP: .space 4
willowBridgeMode: .space 2
willowBridgeCallerRegisters: .space 60
willowBridgeCallerSP: .space 4
willowBridgeCallerSR: .space 2
willowBridgeCallerUSP: .space 4
.text
.globl willowBridgeEnableSRAM,willowBridgeSetUSP
willowBridgeEnableSRAM:
 move.b #1,0xa130f1
 rts
willowBridgeSetUSP:
 move.l %a0,%usp
 rts
