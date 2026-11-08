/* Continuous trace verification: all runtime initialization comes from ROM/code. */
#include "../src/willow_arcade_scroll2_start.h"
#include "../src/willow_arcade_map_store.h"
#include "../src/willow_arcade_intro_sequence.h"
#include "../src/willow_arcade_player_record.h"
#include "../src/willow_arcade_intro_player.h"
#include "../src/willow_arcade_player_update.h"
#include "../native/intro_probe/src/player_data.h"
#include "../native/intro_probe/src/actor_data.h"
#include "../native/intro_probe/src/camera_data.h"
#include "../native/intro_probe/src/scroll2_data.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint16_t cells[4096];
static WillowMapStore map={cells,nativeScroll2Pairs,WILLOW_SCROLL2_PAIR_COUNT,0,0,0};
static uint8_t byte(uint32_t at){uint8_t v;if(!nativeCameraByteRead(at,&v))abort();return v;}
static uint16_t word(uint32_t at){uint16_t v;if(!nativeCameraWordRead(at,&v))abort();return v;}
static uint8_t copy(uint16_t at,uint32_t source,uint16_t count){return willowMapStoreCopyRange(&map,nativeScroll2SourceIds,at,source,count);}
static void writeWord(uint16_t at,uint16_t v){(void)at;(void)v;abort();}
static uint8_t*allocate(void){abort();}
static void palettes(void){} /* Cold camera calls it; no palette-state claim here. */
static void textWrite(unsigned long at,unsigned short code,unsigned short attr){(void)at;(void)code;(void)attr;}
static uint16_t playerWord(unsigned long at){uint16_t v;if(!nativePlayerDataRead(at,&v))abort();return v;}
static uint8_t attribute(uint16_t at){return willowMapStoreWord(&map,at+2)>>8;}
static unsigned effects(void){abort();}
static uint8_t randomByte(void){abort();}
static void sound(uint16_t v){(void)v;abort();}
static uint8_t* shot(uint8_t type){(void)type;abort();}
static uint16_t paletteRead(unsigned n){(void)n;abort();}
static void paletteWrite(unsigned n,uint16_t v){(void)n;(void)v;abort();}
static void hudWrite(uint32_t at,uint16_t v){(void)at;(void)v;abort();}
int main(void){
 FILE*f=fopen("build/arcade-trace/camera-native/frames.bin","rb");if(!f)return 2;
 uint8_t trace[17670],camera[128]={0};
 willowMotionPutWord(camera+6,192);willowMotionPutWord(camera+10,512); /* A128/A12E */
 WillowScroll2Bounds g={0};g.hp=4;
 WillowScroll2Publication p={0};p.maximumX=1952;p.maximumY=2048; /* 2DCA/2DD8 stage zero */
 uint16_t control=0;unsigned checks=0,maps=0;
 WillowArcadeIntroSequence s;willowArcadeIntroSequenceInit(&s);
 WillowPlayerRecord player;willowPlayerRecordStage0(&player,introActorWord);
 WillowPlayerGateGlobals pg={0};pg.hp=introActorWord(0x1bf2)&255;
 WillowPlayerInput input={0};input.availableEffects=effects;input.allocateEffect=allocate;input.random=randomByte;input.sound=sound;
 WillowPlayerCollision collision={playerWord,attribute,0,0,0,0};
 WillowPlayerShots shots={0};shots.allocate=shot;
 WillowPlayerHealth health={0};health.paletteRead=paletteRead;health.paletteWrite=paletteWrite;health.textWrite=hudWrite;health.sound=sound;
 unsigned normal=0;
 willowMapStoreInit(&map,WILLOW_SCROLL2_ZERO_PAIR);
 while(fread(trace,1,sizeof trace,f)==sizeof trace){
  unsigned frame=willowMotionLong(trace);if(frame<691)continue;if(frame>1620)break;
  uint8_t normalScheduled=s.handoff && player.bytes[2]==2;
  uint16_t old=s.camera.state;
  uint8_t cameraScheduled=old<12 || s.handoff || willowIntroActorsScheduledStep(s.camera.state,s.camera.cameraY,s.camera.messageBusy,s.camera.timer);
  if(willowIntroActorsScheduledStep(s.camera.state,s.camera.cameraY,s.camera.messageBusy,s.camera.timer)){
   if(!player.bytes[2])willowPlayerRecordGate(&player,0,&pg,introActorWord);
  }else if(s.handoff && !player.bytes[2])willowPlayerRecordGate(&player,1,&pg,introActorWord);
  if(normalScheduled){
   input.frame=health.frame=(uint8_t)(frame-690);
   health.hp=pg.hp;health.maxHp=pg.hp;health.armor=pg.armor;health.cachedHp=pg.cachedHp;health.cachedArmor=pg.cachedArmor;
   if(!willowPlayerNormalUpdate(player.bytes,&input,&collision,&shots,&health,0,0,willowMotionWord(camera+6),allocate))return 2;
   normal++;
   for(unsigned n=0;n<128;n++)if(player.bytes[n]!=trace[1158+n]){fprintf(stderr,"Normal player mismatch frame %u byte %u native %02x source %02x\n",frame,n,player.bytes[n],trace[1158+n]);return 1;}
  }
  willowArcadeIntroSequenceStep(&s,introActorWord,textWrite);
  if(cameraScheduled){
   if(!willowScroll2ControllerCopy(camera,player.bytes,&g,&p,&control,s.handoff,192,s.camera.targetY,byte,word,writeWord,allocate,palettes,copy))return 2;
  }
  const uint8_t*expected=trace+1030;
  for(unsigned n=0;n<128;n++)if(camera[n]!=expected[n]){
   fprintf(stderr,"Continuous camera mismatch frame %u byte %u native %02x source %02x\n",frame,n,camera[n],expected[n]);return 1;
  }
  if(frame>=695){
   for(unsigned n=0;n<4096;n++)if(willowMapStorePair(&map,n)!=willowMotionLong(trace+1286+n*4)){
    fprintf(stderr,"Continuous map mismatch frame %u cell %u native %08x source %08x\n",frame,n,(unsigned)willowMapStorePair(&map,n),(unsigned)willowMotionLong(trace+1286+n*4));return 1;
   }
   maps++;
  }
  checks++;
 }
 fclose(f);
 printf("Continuous intro camera: %u full records and %u complete mutable maps match arcade frames 691..1620 from code/ROM initialization\n",checks,maps);
 printf("Normal player: %u full records match from native intro handoff and map, with no input or service substitutions\n",normal);
 return checks==930 && maps==926 && normal==15?0:1;
}
