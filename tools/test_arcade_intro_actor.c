#include "../src/willow_arcade_intro_actor.h"
#include <stdio.h>
#include <stdlib.h>
static unsigned char program[0x100000],ram[65536];
static unsigned short word(unsigned long a){if(a+1>=sizeof(program))abort();return program[a]*256+program[a+1];}
static unsigned short rw(unsigned a){return ram[a]*256+ram[a+1];}
static WillowIntroActor actor(unsigned a){return (WillowIntroActor){ram[a],ram[a+32],ram[a+2],ram[a+3],rw(a+6),rw(a+10),rw(a+48),rw(a+28),(unsigned long)rw(a+20)*65536+rw(a+22)};}
int main(void){
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f)return 2;
 if(fread(program,1,sizeof(program),f)!=sizeof(program))return 2;fclose(f);
 WillowIntroActor s[2];unsigned previous=0,checks=0;int willowValid=1;
 for(unsigned frame=840;frame<=1140;frame+=5){
  if(frame>850 && frame<990)continue;
  if(frame==990){previous=0;willowValid=0;}
  char path[128];snprintf(path,sizeof(path),"build/arcade-trace/ram-%u.bin",frame);
  f=fopen(path,"rb");if(!f)continue;
  if(fread(ram,1,sizeof(ram),f)!=sizeof(ram))return 2;fclose(f);
  WillowIntroActor expected[2]={actor(0xdb86),actor(0xdb46)};
  if(!previous){s[0]=expected[0];s[1]=expected[1];previous=frame;continue;}
  if(!rw(0x840c))willowValid=0; /* These captures include an intro skip. */
  for(unsigned i=previous;i<frame;i++){
   willowIntroActorStep(&s[0],1,word);
   if(willowValid)willowIntroActorStep(&s[1],1,word);
  }
  for(unsigned i=0;i<(willowValid?2u:1u);i++){
   WillowIntroActor *a=&s[i],*b=&expected[i];
   if(a->state!=b->state || a->substate!=b->substate || a->animation!=b->animation || a->animationTimer!=b->animationTimer || a->x!=b->x || a->y!=b->y || a->timer!=b->timer){fprintf(stderr,"actor %u mismatch frame %u animation %lx/%lx timer %u/%u\n",i,frame,a->animation,b->animation,a->animationTimer,b->animationTimer);return 1;}
   checks++;
  }
  previous=frame;
 }
 printf("Intro actors: %u record comparisons; seeded independently at frames 840 and 990, no subsequent actor replay within either interval. Selector-3 skip path excluded.\n",checks);
 return checks>20?0:1;
}
