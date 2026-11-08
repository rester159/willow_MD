#include "../src/willow_native_high_scores.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){
 WillowNativeHighScores s;memset(&s,0,sizeof s);uint8_t table[40]={0};unsigned checks=0;
 for(unsigned at=0;at<40;at++)for(unsigned value=0;value<256;value++){
  uint32_t source=0xffffefc6UL+(at/4)*8;
  uint8_t allowed=(source>>((3-at%4)*8))&255;
  uint8_t old[85];memcpy(old,&s,sizeof old);
  int valid=!value||value==allowed;
  assert(willowHighScorePointerWrite(&s,at,value)==valid);
  if(valid)table[at]=value;else assert(!memcmp(old,&s,sizeof old));
  for(unsigned n=0;n<40;n++)assert(willowHighScorePointerRead(&s,n)==table[n]);
  for(unsigned n=0;n<80;n++)assert(!s.records[n]);checks++;
 }
 for(unsigned at=0;at<42;at++)for(unsigned value=0;value<65536;value+=257){
  uint8_t before[85];memcpy(before,&s,sizeof before);
  int valid=!(at&1)&&at<40;
  if(valid){uint32_t pointer=0xffffefc6UL+(at/4)*8;unsigned expected=(pointer>>((2-at%4)*8))&65535;
   valid=(!(value>>8)||(value>>8)==(expected>>8))&&(!(value&255)||(value&255)==(expected&255));}
  assert(willowHighScorePointerWriteWord(&s,at,value)==valid);
  if(valid){table[at]=value>>8;table[at+1]=value;}else assert(!memcmp(before,&s,sizeof before));
  for(unsigned n=0;n<40;n++)assert(willowHighScorePointerRead(&s,n)==table[n]);checks++;
 }
 printf("High-score pointer bitmap: %u byte/word attempts; reset/partial/canonical/sign-extended pointer forms, unchanged records and atomic rejection match a full 40-byte table\n",checks);
}
