#include "../src/willow_arcade_player_clock.h"
#include "../native/intro_probe/src/player_data.h"
#include <stdio.h>
#include <stdlib.h>
static uint16_t word(unsigned long at){uint16_t v;if(!nativePlayerDataRead(at,&v))abort();return v;}
static void writeText(uint32_t at,uint16_t v){(void)at;(void)v;}
static void sound(uint16_t v){(void)v;abort();}
int main(void){
 WillowPlayerHud d={0};WillowPlayerHealth h={.textWrite=writeText,.sound=sound};
 WillowPlayerCharge q={0};WillowPlayerClock t={0};WillowPlayerCollision c={word,0,0,0,0,0};
 willowPlayerHudColdStage0(&d,&h,&q,&t,&c,5);
 FILE*f=fopen("build/arcade-trace/camera-native/frames.bin","rb");if(!f)return 2;
 uint8_t trace[17670];unsigned checks=0,updates=0;
 while(fread(trace,1,sizeof trace,f)==sizeof trace){
  unsigned frame=willowMotionLong(trace);if(frame<691)continue;
  /* A114 initializes stage time, then 9530's manager path starts the clock
   * on the update after A444 hands off. No trace bytes seed native state. */
  if(frame>1588){t.frame++;willowPlayerClockStep(&t,&d,&h);updates++;}
  if(d.time[0]!=trace[4+0x3c6] || d.time[1]!=trace[4+0x3c7] || t.subframes!=trace[4+0x3c8]){
   fprintf(stderr,"Clock trace mismatch at %u\n",frame);return 1;
  }
  checks++;
 }
 fclose(f);if(checks!=930 || updates!=32)return 1;
 printf("Clock trace: %u source states and %u post-handoff updates match from ROM-only cold initialization\n",checks,updates);
 return 0;
}
