#include <assert.h>
#include <stdint.h>
#include <stdio.h>
typedef uint8_t u8;typedef uint16_t u16;typedef uint32_t u32;typedef int16_t s16;typedef int bool;
#define TRUE 1
#define FALSE 0
#include "../build/willow_native_bg.h"
#include "../build/terrain_collision_fixture.h"
#include "../src/willow_terrain.h"
int main(void){
 assert(NATIVE_BG_WIDTH*8==3840);
 assert(willowTerrainSupported(160,200));
 assert(willowTerrainSupported(512,200));
 assert(willowTerrainLanding(768,100,150)==129);
 assert(willowTerrainLanding(768,195,201)==200);
 assert(willowTerrainSupported(768,200));
 assert(!willowTerrainSupported(768,180));
 assert(willowTerrainLanding(1536,195,300)==-1);
 assert(willowTerrainLanding(-1,195,201)==-1);
 assert(willowTerrainLanding(3840,195,201)==-1);
 puts("Terrain: normal-play map, ground below ledge, ledge landing, pit and bounds PASS");
}
