.text
.globl nativeVideoBusProbe
nativeVideoBusProbe:
 move.b #0x12,0x900001
 move.w #0x3456,0x900006
 move.l #0x789abcde,0x910010
 move.w #0xa55a,0x91fffe
 move.b 0x900001,%d0
 move.w 0x900006,%d1
 move.l 0x910010,%d2
 move.w 0x91fffe,%d3
 rts
