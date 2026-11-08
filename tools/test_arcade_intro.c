#include "../src/willow_arcade_intro.h"
#include "../src/willow_arcade_intro_text.h"
#include <stdio.h>
#include <stdlib.h>
static FILE *writes;
static unsigned currentFrame;
static void emit(unsigned long address,unsigned short code,unsigned short attr){
 fprintf(writes,"%u,%lu,%u,%u\n",currentFrame,address,code,attr);
}
int main(int argc,char **argv) {
 if(argc!=2)return 2;
 FILE *f=fopen(argv[1],"r");if(!f)return 2;
 char line[256];fgets(line,sizeof(line),f);
 WillowArcadeIntro s;willowArcadeIntroInit(&s);
 WillowArcadeIntroText text={0};
 unsigned frame,state,timer,target,camera,s2,s3,busy,count=0;
 unsigned cameraHistory[2]={512,512};
 writes=fopen("build/arcade-native-text-writes.csv","w");if(!writes)return 2;
 fprintf(writes,"frame,address,code,attribute\n");
 while(fgets(line,sizeof(line),f)) {
  if(sscanf(line,"%u,%u,%u,%u,%u,%u,%u,%u",&frame,&state,&timer,&target,&camera,&s2,&s3,&busy)!=8)return 2;
  if(frame<691)continue;
  unsigned oldState=s.state;
  willowArcadeIntroStep(&s);
  currentFrame=frame;
  willowArcadeIntroTextStep(&text,oldState,s.state,emit);
  s.messageBusy=text.task.busy;
  if(s.messageBusy!=busy){fprintf(stderr,"frame %u text completion mismatch\n",frame);return 1;}
  if(s.state!=state||s.timer!=timer||s.targetY!=target||s.cameraY!=camera){
   fprintf(stderr,"frame %u mismatch: native %u,%u,%u,%u arcade %u,%u,%u,%u\n",frame,s.state,s.timer,s.targetY,s.cameraY,state,timer,target,camera);return 1;
  }
  /* Register writes are compared after the initial layer-fill setup. Two
   * frame latency is the task publication plus staged VBlank register write. */
  if(frame>=697 && (willowArcadeScroll2Y(cameraHistory[0])!=s2||willowArcadeScroll3Y(cameraHistory[0])!=s3)) {
   fprintf(stderr,"frame %u scroll-register mismatch\n",frame);return 1;
  }
  cameraHistory[0]=cameraHistory[1];cameraHistory[1]=s.cameraY;

  count++;
 }
 /* Continue the diagnostic beyond the camera trace for pixel-compositor
  * checking only; these extra frames do not claim arcade state parity. */
 for(currentFrame=1041;currentFrame<=1092;currentFrame++){
  unsigned before=s.state;willowArcadeIntroStep(&s);
  willowArcadeIntroTextStep(&text,before,s.state,emit);s.messageBusy=text.task.busy;
 }
 fclose(f);fclose(writes);if(count!=350)return 1;
 printf("Arcade intro: %u consecutive frames match state, timer, target, camera; both scroll registers match after layer initialization; text completion generated natively.\n",count);
}
