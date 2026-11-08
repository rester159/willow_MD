#include "../src/willow_arcade_intro_player.h"
#include <stdio.h>
#include <stdlib.h>
static unsigned char program[0x100000];
static unsigned short word(unsigned long a){if(a+1>=sizeof(program))abort();return program[a]*256+program[a+1];}
static unsigned short rw(unsigned char*r,unsigned a){return r[a]*256+r[a+1];}
int main(void){
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f)return 2;fread(program,1,sizeof(program),f);fclose(f);
 f=fopen("build/arcade-trace/actor-native/player.csv","r");if(!f)return 2;
 char line[512],hex[193];fgets(line,sizeof(line),f);unsigned frame,cam,manager,gate,oldManager=0,oldCam=0,n=0;
 WillowIntroPlayer p;willowIntroPlayerInit(&p,word);
 while(fgets(line,sizeof(line),f)){
  if(sscanf(line,"%u,%u,%u,%u,%192s",&frame,&cam,&manager,&gate,hex)!=5)return 2;
  unsigned char r[96];for(unsigned i=0;i<96;i++){unsigned x;sscanf(hex+i*2,"%2x",&x);r[i]=x;}
  if(frame>=691){
   if(willowIntroActorsScheduled(oldManager,oldCam))willowIntroPlayerStep(&p,gate,word);
   WillowIntroActor*a=&p.actor;
   if(a->active!=r[0] || a->state!=r[2] || a->substate!=r[3] || a->x!=rw(r,6) || a->y!=rw(r,10) || a->animation!=((unsigned long)rw(r,20)<<16|rw(r,22)) || a->animationTimer!=rw(r,28) || a->timer!=rw(r,34) || p.facing!=r[15]){fprintf(stderr,"Player mismatch frame %u\n",frame);return 1;}
   n++;
  }
  oldCam=cam;oldManager=manager;
 }
 fclose(f);printf("Intro player: %u consecutive records match from source position initialization; selected state/pose fields only.\n",n);return n==897?0:1;
}
