#include <assert.h>
#include <stdint.h>
#include <stdio.h>
typedef uint32_t u32;
#include "../src/willow_magic.h"
#include "../build/native_magic_assets.h"
static void tick(WillowMagic*m,int held,int pressed,int facing,int crouched){willowMagicStep(m,held,pressed,facing,crouched,160,200,native_magic_charge_level);}
int main(void){
 WillowMagic m={0};
 tick(&m,1,1,0,0);assert(m.cast==10 && !m.spawned);
 for(int i=0;i<4;i++)tick(&m,1,0,0,0);
 assert(!m.spawned);tick(&m,1,0,0,0);
 assert(m.spawned==1 && m.missile[0].x==160*256 && m.missile[0].y==176);
 tick(&m,1,0,0,0);assert(m.missile[0].x==164*256);
 for(int i=0;i<63;i++)tick(&m,1,0,0,0);
 assert(!m.missile[0].live && m.expired==1);
 for(int i=0;i<100;i++)tick(&m,1,0,0,0);
 assert(m.charge==127);tick(&m,0,0,1,1);
 assert(m.cast==10 && m.kind==3);
 for(int i=0;i<5;i++)tick(&m,0,0,1,1);
 assert(m.spawned==2 && m.missile[0].y==184 && m.missile[0].facing==1);
 tick(&m,0,0,1,1);assert(m.missile[0].x==160*256-0x4c0);
 m=(WillowMagic){0};
 for(int shot=0;shot<5;shot++){
  tick(&m,1,1,0,0);
  for(int i=0;i<10;i++)tick(&m,0,0,0,0);
 }
 assert(m.spawned==4); // All four slots still occupied at the fifth launch.
 m=(WillowMagic){0};tick(&m,1,1,0,0);
 for(int i=0;i<10;i++)tick(&m,1,0,0,0);
 for(int i=0;i<15;i++)tick(&m,1,0,0,0);
 tick(&m,0,0,0,0);assert(!m.cast); // Short hold must not add a charged shot.
 puts("Willow magic: delayed launch, travel, expiry, charge, direction, crouch, pool cap PASS");
}
