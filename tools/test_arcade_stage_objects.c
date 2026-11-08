#include "../src/willow_arcade_stage_objects.h"
#define WillowPaletteController WillowSourcePaletteController
#include "../src/willow_arcade_palette_controllers.h"
#undef WillowPaletteController
#include "../native/engine/src/stage_palette_controllers.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned char program[0x100000],before[65536],after[65536];
static unsigned short word(unsigned long a){if(a+1>=sizeof(program))abort();return program[a]*256+program[a+1];}
static void readFile(const char *path,void *out,size_t size){FILE*f=fopen(path,"rb");if(!f || fread(out,1,size,f)!=size)exit(2);fclose(f);}
int main(void){
 readFile("build/reference_maincpu.bin",program,sizeof(program));
 readFile("build/arcade-trace/actor-native/handoff-ram-1587.bin",before,sizeof(before));
 readFile("build/arcade-trace/actor-native/handoff-ram-1588.bin",after,sizeof(after));
 WillowStageObject pool[48];memcpy(pool,before+0x8502,sizeof(pool));
 unsigned count=willowStageObjectsInit(pool,0,word);
 if(count!=5 || memcmp(pool,after+0x8502,sizeof(pool))){fputs("Stage-zero object pool differs at handoff\n",stderr);return 1;}
 /* 2CA4 scans active bytes; exhaustion returns without clearing or moving
  * existing records. Partial allocation must retain dirty unused bytes. */
 memset(pool,0x55,sizeof(pool));WillowStageObject copy[48];memcpy(copy,pool,sizeof(pool));
 if(willowStageObjectsInit(pool,0,word) || memcmp(copy,pool,sizeof(pool)))return 1;
 pool[47].bytes[0]=0;memcpy(copy,pool,sizeof(pool));
 if(willowStageObjectsInit(pool,0,word)!=1 || memcmp(copy,pool,47*sizeof(*pool)))return 1;
 for(unsigned i=0;i<64;i++){
  int changed=i==0 || (i>=6 && i<=19);
  if(!changed && pool[47].bytes[i]!=0x55)return 1;
 }
 puts("Stage object initializer: 3072 pool bytes match source handoff; full and partially exhausted pools preserve prior records");
 WillowSourcePaletteController palettes[16],old[16];memcpy(palettes,before+0xf042,sizeof(palettes));
 if(willowPaletteControllersInit(palettes,0,word)!=6 || memcmp(palettes,after+0xf042,sizeof(palettes))){fputs("Palette controller pool mismatch\n",stderr);return 1;}
 memset(palettes,0x55,sizeof(palettes));memcpy(old,palettes,sizeof(old));
 if(willowPaletteControllersInit(palettes,0,word) || memcmp(old,palettes,sizeof(old)))return 1;
 palettes[15].bytes[0]=0;memcpy(old,palettes,sizeof(old));
 if(willowPaletteControllersInit(palettes,0,word)!=1 || memcmp(old,palettes,15*sizeof(*palettes)))return 1;
 for(unsigned i=0;i<64;i++){
  int changed=i==0 || (i>=6 && i<=13) || i==32 || (i>=52 && i<=56) || (i>=58 && i<=60);
  if(!changed && palettes[15].bytes[i]!=0x55)return 1;
 }
 memset(palettes,0,sizeof(palettes));memcpy(old,palettes,sizeof(old));
 if(willowPaletteControllersInit(palettes,3,word) || memcmp(old,palettes,sizeof(old)))return 1;
 puts("Palette controller initializer: 1024 pool bytes match source handoff; exhaustion, retained bytes and empty-table sentinel pass");
 uint8_t compiled[20016];readFile("build/native-world/campaign-entry-data-v1/palette-controllers.bin",compiled,sizeof compiled);
 WillowNativeStagePaletteControllers data;assert(willowNativeStagePaletteControllersBind(&data,compiled,sizeof compiled));
 unsigned checks=0;
 for(unsigned stage=0;stage<6;stage++)for(unsigned sample=0;sample<256;sample++){
  WillowPaletteController native[16],saved[16];
  for(unsigned slot=0;slot<16;slot++){
   uint8_t*r=palettes[slot].bytes;for(unsigned n=0;n<64;n++)r[n]=sample*31+slot*19+n*7;
   r[0]=sample<17?(slot<sample):((sample>>(slot&7))&1);
   willowStageObjectStoreWord(palettes+slot,52,0xe);willowStageObjectStoreWord(palettes+slot,54,0xa260);
   native[slot]=(WillowPaletteController){.colors=program+0xea260,.active=r[0],.phase=r[2],.subphase=r[3],
    .x=willowWorldU16(r+6),.width=willowWorldU16(r+8),.y=willowWorldU16(r+10),.height=willowWorldU16(r+12),
    .timer=willowWorldU16(r+40),.destination=willowWorldU16(r+62),.kind=r[32],.bank=r[60],.palette=r[56],
    .current=r[57],.delay=r[58],.sequence=r[59],.countdown=r[61],.index=r[63]};
   saved[slot]=native[slot];
  }
  memcpy(old,palettes,sizeof old);willowPaletteControllersInit(palettes,stage,word);
  assert(willowNativeStagePaletteControllersInit(&data,native,stage));
  for(unsigned slot=0;slot<16;slot++){
   const uint8_t*r=palettes[slot].bytes;WillowPaletteController*n=native+slot;
   assert(n->active==r[0]&&n->phase==r[2]&&n->subphase==r[3]&&n->x==willowWorldU16(r+6)&&n->width==willowWorldU16(r+8)&&
    n->y==willowWorldU16(r+10)&&n->height==willowWorldU16(r+12)&&n->timer==saved[slot].timer&&n->destination==saved[slot].destination&&
    n->kind==r[32]&&n->bank==r[60]&&n->palette==r[56]&&n->current==r[57]&&n->delay==r[58]&&n->sequence==r[59]&&
    n->countdown==saved[slot].countdown&&n->index==saved[slot].index);
   if(!old[slot].bytes[0]&&r[0])assert(!memcmp(n->colors,program+willowWorldU32(r+52),8192));
   else assert(n->colors==saved[slot].colors);
  }
  checks++;
 }
 printf("Native stage palette initialization: %u six-stage comparisons; exact shared color windows, occupied/scattered/full pools and retained fields match\n",checks);
 return 0;
}
