#include "../src/willow_arcade_intro_sequence.h"
#include "../native/intro_probe/src/actor_data.h"
#include <stdio.h>
#include <stdlib.h>
static unsigned char program[0x100000];static unsigned frame;static FILE*writes;
static unsigned short word(unsigned long a){
 if(a+1>=sizeof(program))abort();
 unsigned short actual=introActorWord(a),expected=program[a]*256+program[a+1];
 if(actual!=expected){fprintf(stderr,"Native ROM bank mismatch at %lx: %x/%x\n",a,actual,expected);abort();}
 return actual;
}
static void emit(unsigned long address,unsigned short code,unsigned short attr){fprintf(writes,"%u,%lu,%u,%u\n",frame,address,code,attr);}
int main(void){
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f)return 2;
 if(fread(program,1,sizeof(program),f)!=sizeof(program))return 2;fclose(f);
 f=fopen("build/arcade-trace/actor-native/controller.csv","r");if(!f)return 2;
 writes=fopen("build/native-intro-sequence-writes.csv","w");if(!writes)return 2;fputs("frame,address,code,attribute\n",writes);
 WillowArcadeIntroSequence s;willowArcadeIntroSequenceInit(&s);
 char line[256];fgets(line,sizeof(line),f);unsigned major,state,timer,target,camera,gate,busy,checks=0;
 while(fgets(line,sizeof(line),f)){
  if(sscanf(line,"%u,%u,%u,%u,%u,%u,%u,%u",&frame,&major,&state,&timer,&target,&camera,&gate,&busy)!=8)return 2;
  if(frame>1588)break;
  willowArcadeIntroSequenceStep(&s,word,emit);
  WillowArcadeIntro*c=&s.camera;
  if(c->state!=state || c->timer!=timer || c->targetY!=target || c->cameraY!=camera || c->messageBusy!=busy || s.handoff!=gate || s.handoff*2!=major){fprintf(stderr,"sequence mismatch frame %u state %u/%u timer %u/%u busy %u/%u\n",frame,c->state,state,c->timer,timer,c->messageBusy,busy);return 1;}
  checks++;
 }
 fclose(f);fclose(writes);printf("Intro manager: %u consecutive frames match state, timers, camera, message completion and handoff boundary\n",checks);return checks==898?0:1;
}
