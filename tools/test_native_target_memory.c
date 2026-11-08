/* Byte-order/address-space regression for the headless fixture adapter. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "native_target_memory.h"
int main(void)
{
 uint8_t ram[65536],sram[65536];
 for(unsigned i=0;i<65536;i++){
  ram[i^1]=(i*73+(i>>8))&255;sram[i]=(i*31+(i>>8)+17)&255;
 }
 for(unsigned i=0;i<65536;i++){
  assert(nativeTargetByte(ram,sram,i)==ram+(i^1));
  assert(nativeTargetByte(ram,sram,0xff0000+i)==ram+(i^1));
  assert(nativeTargetByte(ram,sram,0xe0ff0000+i)==ram+(i^1));
  assert(nativeTargetByte(ram,sram,0x200000+i)==sram+i);
 }
 for(unsigned i=0;i<65535;i++){
  uint16_t r=((uint16_t)ram[i^1]<<8)|ram[(i+1)^1];
  uint16_t s=((uint16_t)sram[i]<<8)|sram[i+1];
  assert(nativeTargetWord(ram,sram,0xe0ff0000+i)==r);
  assert(nativeTargetWord(ram,sram,0x200000+i)==s);
 }
 uint8_t saved[65536];memcpy(saved,ram,sizeof saved);
 for(unsigned i=0;i<65535;i++){
  nativeTargetPutWord(ram,sram,0x200000+i,i^0x1957);
  assert(sram[i]==((i^0x1957)>>8));assert(sram[i+1]==((i^0x1957)&255));
 }
 assert(!memcmp(saved,ram,sizeof saved));memcpy(saved,sram,sizeof saved);
 for(unsigned i=0;i<65535;i++){
  nativeTargetPutWord(ram,sram,0xe0ff0000+i,i^0xa624);
  assert(ram[i^1]==((i^0xa624)>>8));assert(ram[(i+1)^1]==((i^0xa624)&255));
 }
 assert(!memcmp(saved,sram,sizeof saved));
 puts("Native target memory: all byte addresses, both 68000 RAM aliases, every in-range word and isolated SRAM/RAM writes pass");
}
