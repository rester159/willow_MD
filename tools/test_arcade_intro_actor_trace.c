#include "../src/willow_arcade_intro_actor.h"
#include <stdio.h>
#include <stdlib.h>
static unsigned char program[0x100000];
static unsigned short word(unsigned long a){if(a+1>=sizeof(program))abort();return program[a]*256+program[a+1];}
int main(void){
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f)return 2;
 if(fread(program,1,sizeof(program),f)!=sizeof(program))return 2;fclose(f);
 unsigned busy[1651]={0},timers[1651]={0},cf,cm,cs,ct,cy,cc,cg,cb;char control[256];
 f=fopen("build/arcade-trace/actor-native/controller.csv","r");if(!f)return 2;fgets(control,sizeof(control),f);
 while(fgets(control,sizeof(control),f))if(sscanf(control,"%u,%u,%u,%u,%u,%u,%u,%u",&cf,&cm,&cs,&ct,&cy,&cc,&cg,&cb)==8 && cf<1651){busy[cf]=cb;timers[cf]=ct;}
 fclose(f);
 f=fopen("build/arcade-trace/actor-native/actors.csv","r");if(!f)return 2;
 char line[256];fgets(line,sizeof(line),f);
 WillowIntroActor actors[2]={{0}};unsigned count=0,spawned=0,spawnFrame=0,previousCamera=512,previousManager=10;
 unsigned frame,camera,manager,slot,active,selector,state,sub,x,y,timer,at;unsigned long animation;
 while(fgets(line,sizeof(line),f)){
  if(sscanf(line,"%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%lu",&frame,&camera,&manager,&slot,&active,&selector,&state,&sub,&x,&y,&timer,&at,&animation)!=13)return 2;
  if(!spawned && manager==12){willowIntroActorsSpawn(actors);spawned=1;spawnFrame=frame;}
  if(!spawned)continue;
  if(frame!=spawnFrame && willowIntroActorsScheduledStep(previousManager,previousCamera,busy[frame-1],timers[frame-1]))willowIntroActorStep(&actors[slot],previousCamera,word);
  if(slot==1){previousCamera=camera;previousManager=manager;}
  WillowIntroActor *s=&actors[slot];
  if(s->active!=active || s->selector!=selector || s->state!=state || s->substate!=sub || s->x!=x || s->y!=y || s->timer!=timer || s->animationTimer!=at || s->animation!=animation){
   fprintf(stderr,"frame %u slot %u mismatch state %u/%u sub %u/%u timer %u/%u animation %lx/%lx animationTimer %u/%u\n",frame,slot,s->state,state,s->substate,sub,s->timer,timer,s->animation,animation,s->animationTimer,at);return 1;
  }
  count++;
 }
 fclose(f);printf("Fresh unskipped intro: %u consecutive actor records match from source-driven spawn; camera is an external trace input.\n",count);
 return count==1656?0:1;
}
