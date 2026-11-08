#include "../src/willow_native_scene_memory.h"
typedef struct {
 WillowNativeScroll1 text;
 uint16_t nearMap[4096],farMap[WILLOW_SCROLL3_MAP_WORDS];
 WillowNativeObjectBanks objects;
 WillowHudSurface hud;
} SceneMemoryProbeStorage;
static const uint32_t probeNearDictionary[]={0x12345678,0,0x98765432};
static const uint32_t probeFarDictionary[]={0x54321234,0x0a000000,0xabcd5678};
static uint8_t sceneMemoryProbeRun(SceneMemoryProbeStorage*s,const uint16_t*p)
{
 WillowMapStore near={s->nearMap,probeNearDictionary,3,0,0,0};
 WillowNativeSceneMemory memory={&s->text,&near,s->farMap,probeFarDictionary,
  &s->hud,&s->objects,3,1,1};
 switch(p[7]){
 case 1:memory.scroll1=0;break;
 case 2:memory.objects=0;break;
 case 3:memory.scroll2Clear=0;break;
 case 4:memory.scroll3Clear=2048;break;
 case 5:memory.hud=0;break;
 }
 uint32_t at=((uint32_t)p[2]<<16)|p[3];
 return p[0]==0?willowNativeSceneMemoryFill(&memory,at,p[4],p[5],p[6]):
  p[0]==1?willowNativeScroll1Write(memory.scroll1,memory.hud,at,p[4]):0;
}
