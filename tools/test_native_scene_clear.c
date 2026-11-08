#include "../src/willow_native_scene_clear.h"
#include <stdio.h>
#include <string.h>
#ifdef WILLOW_TEST_MAP14
static struct {uint16_t before,cells[WILLOW_MAP14_WORDS],after;} nearMap;
#else
static struct {uint16_t before,cells[4096],after;} nearMap;
#endif
static struct {uint16_t before,cells[WILLOW_SCROLL3_MAP_WORDS],after;} farMap;
static WillowHudSurface hud;
static WillowMapOverride overrides[8];
int main(void)
{
 const uint32_t nearDictionary[]={0x12345678,0,0xabcd1234};
 const uint32_t farDictionary[]={0,0x0a000000,0x0abc1234};
 WillowMapStore map={nearMap.cells,nearDictionary,3,overrides,8,0};
#ifdef WILLOW_TEST_MAP14
 if(!willowMapStoreInitPacked14(&map,1))return 1;
#endif
 unsigned checks=0;
 for(unsigned sample=0;sample<256;sample++){
  memset(&nearMap,0xa5,sizeof nearMap);memset(&farMap,0x5a,sizeof farMap);
  memset(&hud,sample,sizeof hud);
  for(unsigned n=0;n<4096;n++){
   willowMapStorePutCell(&map,n,0x8000|(n&7));
   willowScroll3MapSet(farMap.cells,n,(sample+n)&2047);
  }
  for(unsigned n=0;n<8;n++){overrides[n].references=512;overrides[n].value=sample*977+n;}
  static unsigned char oldNear[sizeof nearMap],oldFar[sizeof farMap],oldHud[sizeof hud],oldOverrides[sizeof overrides];
  memcpy(oldNear,&nearMap,sizeof nearMap);memcpy(oldFar,&farMap,sizeof farMap);
  memcpy(oldHud,&hud,sizeof hud);memcpy(oldOverrides,overrides,sizeof overrides);
  for(unsigned bad=0;bad<3;bad++){
   if(willowNativeSceneMapsClear(&map,farMap.cells,farDictionary,3,&hud,bad==0?0:1,bad==1?0:bad==2?2048:1))return 1;
   if(memcmp(oldNear,&nearMap,sizeof nearMap)||memcmp(oldFar,&farMap,sizeof farMap)||memcmp(oldHud,&hud,sizeof hud)||memcmp(oldOverrides,overrides,sizeof overrides))return 1;
  }
  if(!willowNativeSceneMapsClear(&map,farMap.cells,farDictionary,3,&hud,1,1))return 1;
  for(unsigned n=0;n<4096;n++){
   if(willowMapStorePair(&map,n)!=0||farDictionary[willowScroll3MapId(farMap.cells,n)]!=0x0a000000)return 1;
   checks+=2;
  }
  for(unsigned r=0;r<6;r++){
   for(unsigned x=0;x<48;x++)if(hud.cells[r][x][0]!=0x7020||hud.cells[r][x][1])return 1;
   if(hud.dirty[r][0]!=65535||hud.dirty[r][1]!=65535||hud.dirty[r][2]!=255)return 1;
  }
  for(unsigned n=0;n<8;n++)if(overrides[n].references)return 1;
  static unsigned char clearedNear[sizeof nearMap],clearedFar[sizeof farMap],clearedHud[sizeof hud],clearedOverrides[sizeof overrides];
  memcpy(clearedNear,&nearMap,sizeof nearMap);memcpy(clearedFar,&farMap,sizeof farMap);
  memcpy(clearedHud,&hud,sizeof hud);memcpy(clearedOverrides,overrides,sizeof overrides);
  memcpy(&nearMap,oldNear,sizeof nearMap);memcpy(&farMap,oldFar,sizeof farMap);
  memcpy(&hud,oldHud,sizeof hud);memcpy(overrides,oldOverrides,sizeof overrides);
  const struct {uint32_t address;uint16_t first,second,pairs;} rejected[]={
   {0x900000,0,0,512},{0x908000,0,0,512},{0x910004,0x7020,0,4096},
   {0x910000,0x7020,0,4095},{0x910000,0x7020,1,4096},
   {0x910000,0x7120,0,4096},{0x918000,1,0,4096},{0x914000,0,0,4096}
  };
  for(unsigned n=0;n<sizeof rejected/sizeof *rejected;n++){
   if(willowNativeSceneMapFill(&map,farMap.cells,farDictionary,3,&hud,1,1,
      rejected[n].address,rejected[n].first,rejected[n].second,rejected[n].pairs))return 1;
   if(memcmp(oldNear,&nearMap,sizeof nearMap)||memcmp(oldFar,&farMap,sizeof farMap)||
      memcmp(oldHud,&hud,sizeof hud)||memcmp(oldOverrides,overrides,sizeof overrides))return 1;
  }
  const uint32_t addresses[]={0x910000,0x918000,0x914000};
  const uint16_t tiles[]={0x7020,0,0x0a00};
  for(unsigned region=0;region<3;region++){
   /* Invalid dictionary IDs must reject even a HUD-only request before writes. */
   for(unsigned bad=0;bad<3;bad++){
    if(willowNativeSceneMapFill(&map,farMap.cells,farDictionary,3,&hud,
       bad==0?0:1,bad==1?0:bad==2?2048:1,addresses[region],tiles[region],0,4096))return 1;
    if(memcmp(region>1?clearedNear:oldNear,&nearMap,sizeof nearMap)||
       memcmp(oldFar,&farMap,sizeof farMap)||
       memcmp(region?clearedHud:oldHud,&hud,sizeof hud)||
       memcmp(region>1?clearedOverrides:oldOverrides,overrides,sizeof overrides))return 1;
   }
   if(!willowNativeSceneMapFill(&map,farMap.cells,farDictionary,3,&hud,1,1,
      addresses[region],tiles[region],0,4096))return 1;
   if(memcmp(region?clearedNear:oldNear,&nearMap,sizeof nearMap)||
      memcmp(region==2?clearedFar:oldFar,&farMap,sizeof farMap)||
      memcmp(clearedHud,&hud,sizeof hud)||
      memcmp(region?clearedOverrides:oldOverrides,overrides,sizeof overrides))return 1;
  }
  if(nearMap.before!=0xa5a5||nearMap.after!=0xa5a5||farMap.before!=0x5a5a||farMap.after!=0x5a5a)return 1;
 }
 for(unsigned id=0;id<2048;id++){
  if(!willowScroll3MapFill(farMap.cells,id))return 1;
  for(unsigned cell=0;cell<4096;cell++)if(willowScroll3MapId(farMap.cells,cell)!=id)return 1;
 }
 if(willowScroll3MapFill(farMap.cells,2048)||farMap.before!=0x5a5a||farMap.after!=0x5a5a)return 1;
 for(unsigned cell=0;cell<4096;cell++)if(willowScroll3MapId(farMap.cells,cell)!=2047)return 1;
 printf("Scene map clear: %u retained map cells checked; exact CPS pairs, HUD dirty state, released overrides, invalid-ID atomicity and guards pass; all 2048 packed fill IDs verified; 768 ordered region fills and 4352 atomic rejections pass\n",checks);
 return 0;
}
