#include "../src/willow_arcade_stage_spawns.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned char program[0x100000],ram[65536],after[65536];
static unsigned short allocated[128],count;
static WillowStageObject scratch;
static unsigned short allowScratch=1;
static WillowStageObject *scratchAllocate(unsigned short classId){(void)classId;return allowScratch?&scratch:NULL;}
static unsigned short word(unsigned long a){if(a+1>=sizeof(program))abort();return program[a]*256+program[a+1];}
static unsigned short rw(unsigned a){return ram[a]*256+ram[a+1];}
static void ww(unsigned a,unsigned short v){ram[a]=v>>8;ram[a+1]=v;}
static WillowStageObject *allocate(unsigned short classId){
 /* The count and free-list cursor offsets are the operands in each source
  * allocator selected by 40448, not inferred actor capacities or ordering. */
 static const unsigned short counts[10]={0,0,0x9801,0x9804,0,0x980a,0x980d,0x9810,0x9813,0xf48c};
 static const unsigned short cursors[10]={0,0,0x981c,0x9822,0,0x982e,0x9834,0x983a,0x9840,0xf486};
 unsigned type=classId/2;if(type>=10 || !counts[type] || !ram[counts[type]])return NULL;
 unsigned cursor=rw(cursors[type]),address=rw(cursor);ww(cursors[type],cursor+2);ram[counts[type]]--;
 if(address+64>65536 || count>=128)abort();allocated[count++]=address;
 return (WillowStageObject *)(ram+address);
}
static void readFile(const char*path,void*out,size_t size){FILE*f=fopen(path,"rb");if(!f || fread(out,1,size,f)!=size)exit(2);fclose(f);}
int main(void){
 readFile("build/reference_maincpu.bin",program,sizeof(program));
 readFile("build/arcade-trace/actor-native/handoff-ram-1587.bin",ram,sizeof(ram));
 readFile("build/arcade-trace/actor-native/handoff-ram-1588.bin",after,sizeof(after));
 WillowStageSpawnContext c={rw(0x838c),rw(0x984c),rw(0x9850)};
 unsigned long table=((unsigned long)word(0x4034a)<<16)|word(0x4034c);
 unsigned created=willowStageSpawnTable(table,&c,word,allocate);
 if(created!=57 || count!=57)return 1;
 for(unsigned i=0;i<count;i++)if(memcmp(ram+allocated[i],after+allocated[i],64)){fprintf(stderr,"Spawn record mismatch %u at %04x\n",i,allocated[i]);return 1;}
 const unsigned offsets[]={0x9801,0x9804,0x980d,0x9813,0x981c,0x9822,0x9834,0x9840};
 for(unsigned i=0;i<sizeof(offsets)/sizeof(*offsets);i++){
  unsigned at=offsets[i],size=i<4?1:2;
  if(memcmp(ram+at,after+at,size)){fprintf(stderr,"Allocator state mismatch %04x\n",at);return 1;}
 }
 printf("Stage spawn table: %u records (%u bytes), four free-list counts and cursors match source handoff\n",created,created*64);
 /* Synthetic boundary cases supplement the actual entry trace, whose
  * records all lie outside the excluded player rectangle. */
 const unsigned short cases[][6]={
  {4,0,840,200,0,0},{4,0,839,200,0,1},{4,0,1128,200,0,0},{4,0,1129,200,0,1},
  {4,0,1000,100,0,0},{4,0,1000,99,0,1},{4,0,1000,300,0,0},{4,0,1000,301,0,1},
  {4,0,899,600,900,0},{4,0,900,600,900,1},
  {6,0,1000,200,0,1},{12,8,1000,200,0,1},{12,10,1000,200,0,0}};
 unsigned char *r=program+0x60000;memset(r,0,10);c.playerX=1000;c.playerY=200;
 for(unsigned i=0;i<sizeof(cases)/sizeof(*cases);i++){
  const unsigned short *v=cases[i];r[2]=v[0];r[4]=v[1];r[6]=v[2]>>8;r[7]=v[2];r[8]=v[3]>>8;r[9]=v[3];c.minimumX=v[4];
  if(willowStageSpawnTable(0x60000,&c,word,scratchAllocate)!=v[5]){fprintf(stderr,"Spawn boundary case %u failed\n",i);return 1;}
 }
 allowScratch=0;r[2]=6;
 if(willowStageSpawnTable(0x60000,&c,word,scratchAllocate))return 1;
 allowScratch=1;r[0]=r[1]=255;
 if(willowStageSpawnTable(0x60000,&c,word,scratchAllocate))return 1;
 puts("Spawn eligibility: 13 unsigned-coordinate/selector boundaries, allocation failure and empty-table sentinel pass");
 return 0;
}
