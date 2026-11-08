/* Continuously replay initialization and active Scroll3 from one captured initial state.
 * Captures are test fixtures only, never native runtime assets. */
#include "../src/willow_arcade_scroll3_start.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../src/willow_arcade_scroll3_map_store.h"
#define STRIDE 1286
#ifdef WILLOW_TEST_PACKED_MAP
#include "../native/intro_probe/src/scroll3_data.h"
static uint16_t packed[WILLOW_SCROLL3_MAP_WORDS];
static uint8_t copyPairs(uint16_t at,uint32_t source,uint16_t count)
#ifdef WILLOW_PACKED_SCROLL3_SOURCE_PROBE
{return willowScroll3MapCopyPacked(packed,nativeScroll3PackedSourceIds,at,source,count);}
#else
{return willowScroll3MapCopy(packed,nativeScroll3SourceIds,at,source,count);}
#endif
#endif
static uint8_t rom[0x100000],ring[16384];
static uint8_t byte(uint32_t at){if(at>=sizeof rom)abort();return rom[at];}
static uint16_t word(uint32_t at){if(at+1>=sizeof rom)abort();return willowMotionWord(rom+at);}
static void writeWord(uint16_t at,uint16_t value){if(at+1>=sizeof ring)abort();willowMotionPutWord(ring+at,value);}
int main(void)
{
 FILE*f=fopen("build/arcade-trace/scroll3-walk/frames.bin","rb");
 if(!f)return 2;
 FILE*maps=fopen("build/arcade-trace/scroll3-walk/maps.bin","rb");
 FILE*program=fopen("build/reference_maincpu.bin","rb");
 if(!maps || !program || fread(rom,1,sizeof rom,program)!=sizeof rom)return 2;
 fclose(program);
 uint8_t oldMap[16384],nextMap[16384];
 if(fread(oldMap,1,sizeof oldMap,maps)!=sizeof oldMap)return 2;
 uint16_t sx=0,sy=0;
 uint8_t previous[STRIDE],current[STRIDE],native[128];unsigned checks=0;
 if(fread(previous,1,STRIDE,f)!=STRIDE)return 2;
 for(unsigned frame=651;frame<=1700;frame++){
  if(fread(current,1,STRIDE,f)!=STRIDE || willowMotionLong(current)!=frame)return 2;
  if(fread(nextMap,1,sizeof nextMap,maps)!=sizeof nextMap)return 2;
  uint8_t*g=current+4,*r=g+0x482,*old=previous+4+0x482;
  if(frame>=691){
   /* Retain all motion state from the preceding output, never inject the
    * current expected coordinates/directions/timers. */
   if(!checks){memcpy(native,old,128);memcpy(ring,oldMap,sizeof ring);
    sx=willowMotionWord(previous+4+54);sy=willowMotionWord(previous+4+56);}
#ifdef WILLOW_TEST_PACKED_MAP
   if(!checks)for(unsigned cell=0;cell<4096;cell++){
    uint32_t pair=willowMotionLong(ring+cell*4);unsigned id=0;
    while(id<WILLOW_SCROLL3_PAIR_COUNT && nativeScroll3Pairs[id]!=pair)id++;
    if(id==WILLOW_SCROLL3_PAIR_COUNT){fprintf(stderr,"Initial fixture pair %08x absent\n",pair);return 2;}
    if(!willowScroll3MapSet(packed,cell,id))return 2;
   }
#endif
   /* Stage-0 entry A134/A13A initializes these positions before 42F48. */
   if(frame==691){willowMotionPutWord(native+6,192);willowMotionPutWord(native+10,512);}
   uint16_t stage=willowMotionWord(g+0x344);
#ifdef WILLOW_TEST_PACKED_MAP
   if(!willowScroll3ControllerCopy(native,stage,willowMotionWord(g+0x374),g[0x470],
    willowMotionWord(g+0x408),willowMotionWord(g+0x40c),willowMotionWord(g+0x398),
    &sx,&sy,byte,word,writeWord,copyPairs))return 2;
   for(unsigned cell=0;cell<4096;cell++)willowMotionPutLong(ring+cell*4,nativeScroll3Pairs[willowScroll3MapId(packed,cell)]);
#else
   if(!willowScroll3Controller(native,stage,willowMotionWord(g+0x374),g[0x470],
    willowMotionWord(g+0x408),willowMotionWord(g+0x40c),willowMotionWord(g+0x398),
    &sx,&sy,byte,word,writeWord))return 2;
#endif
   for(unsigned at=0;at<128;at++)if(native[at]!=r[at]){
    fprintf(stderr,"Frame %u Scroll3 byte %u native %02x source %02x\n",frame,at,native[at],r[at]);return 1;
   }
   for(unsigned at=0;at<sizeof ring;at++)if(ring[at]!=nextMap[at]){
    fprintf(stderr,"Frame %u Scroll3 map byte %u native %02x source %02x\n",frame,at,ring[at],nextMap[at]);return 1;
   }
   if(willowMotionWord(g+54)!=sx ||
      willowMotionWord(g+56)!=sy)return 1;
   checks++;
  }
  memcpy(previous,current,STRIDE);memcpy(oldMap,nextMap,sizeof oldMap);
 }
 if(fgetc(f)!=EOF || fgetc(maps)!=EOF || checks!=1010)return 2;
 fclose(f);fclose(maps);
 printf("Scroll3 initialization and continuous active update: %u original frames match all 128 controller bytes, all 16384 map bytes and staged registers through intro, right walk and release\n",checks);
 return 0;
}
