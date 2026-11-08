#include "../src/willow_arcade_intro_sequence.h"
#include "../src/willow_arcade_intro_player.h"
#include <stdio.h>
#include <stdlib.h>
static unsigned char program[0x100000];
static unsigned short word(unsigned long a){if(a+1>=sizeof(program))abort();return program[a]*256+program[a+1];}
static void writeText(unsigned long a,unsigned short c,unsigned short attr){(void)a;(void)c;(void)attr;}
int main(void){
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f || fread(program,1,sizeof(program),f)!=sizeof(program))return 2;fclose(f);
 f=fopen("build/arcade-trace/actor-native/handoff-player.csv","r");if(!f)return 2;
 char line[512],hex[193];fgets(line,sizeof(line),f);unsigned frame,major,state,gate,checks=0;
 WillowArcadeIntroSequence sequence;willowArcadeIntroSequenceInit(&sequence);
 WillowIntroPlayer player;willowIntroPlayerInit(&player,word);
 for(unsigned tick=691;tick<=1605;tick++){
  WillowArcadeIntro*c=&sequence.camera;
  if(sequence.handoff || willowIntroActorsScheduledStep(c->state,c->cameraY,c->messageBusy,c->timer))
   willowIntroPlayerStep(&player,sequence.handoff,word);
  willowArcadeIntroSequenceStep(&sequence,word,writeText);
  if(tick<1587)continue;
  if(!fgets(line,sizeof(line),f) || sscanf(line,"%u,%u,%u,%u,%192s",&frame,&major,&state,&gate,hex)!=5 || frame!=tick)return 2;
  unsigned b[96];for(unsigned i=0;i<96;i++)sscanf(hex+i*2,"%2x",&b[i]);
  WillowIntroActor*a=&player.actor;
  if(sequence.handoff!=gate || sequence.handoff*2!=major || c->state!=state || a->active!=b[0] || a->state!=b[2] || a->substate!=b[3] || player.facing!=b[15] || a->x!=b[6]*256+b[7] || a->y!=b[10]*256+b[11] || a->timer!=b[34]*256+b[35] || a->animationTimer!=b[28]*256+b[29] || a->animation!=((unsigned long)b[20]<<24|b[21]<<16|b[22]<<8|b[23])){fprintf(stderr,"Player handoff mismatch at %u\n",tick);return 1;}
  checks++;
 }
 fclose(f);printf("Player gate: %u handoff/countdown records match from source-driven initialization; gameplay state reached at 1605\n",checks);
 return checks==19 && player.actor.state==2?0:1;
}
