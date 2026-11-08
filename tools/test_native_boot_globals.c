#include "../src/willow_native_boot_globals.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){
 WillowNativeBootGlobals s;memset(&s,0,sizeof s);
 /* Independently declared source spans from the verified boot trace. */
 const unsigned ranges[][2]={{0xa3e,4},{0xe58,4},{0xe60,4},{0x8000,20},{0x8018,2},{0x802e,22},{0x8044,2},{0x804a,10},{0x8058,26},{0x8276,2},{0x827e,2},{0x8284,3},{0x8287,2},{0x828e,6},{0x8294,1},{0x8295,2},{0x829c,4},{0x82a2,1},{0x82a4,1},{0x82a6,1},{0x82a8,26},{0x8343,1},{0xf442,20},{0xf890,2},{0xf99c,1},{0xf9a0,1},{0xf9a3,2},{0xf9a6,1},{0x8022,2},{0x829a,2},{0x82a0,1},{0x8297,1}};
 for(unsigned at=0;at<65536;at++){
  uint8_t*p=willowNativeBootCameraWord(&s,at);
  if((at>=0x8032&&at<=0x8038&&!(at&1))||at==0x804a)
   assert(p==willowNativeBootGlobalsResolve(&s,at,2));
  else assert(!p);
 }
 assert(willowNativeBootInputHistory(&s)==willowNativeBootGlobalsResolve(&s,0x806a,2));
 unsigned checks=0;
 for(unsigned at=0;at<65536;at++)for(unsigned count=0;count<=4;count++){
  int wanted=-1;unsigned offset=0;
  for(unsigned r=0;r<sizeof ranges/sizeof ranges[0];r++){
   if(count&&at>=ranges[r][0]&&at+count<=ranges[r][0]+ranges[r][1])wanted=offset+at-ranges[r][0];
   offset+=ranges[r][1];
  }
  assert(offset==sizeof s);
  uint8_t*p=willowNativeBootGlobalsResolve(&s,at,count);
  assert(wanted<0?!p:p==s.bytes+wanted);
  uint8_t*input=willowNativeBootInputResolve(&s,at,count);
  if(count&&((at>=0x8058&&at+count<=0x8072)||(at>=0x828e&&at+count<=0x8294)))assert(input==p);
  else assert(!input);
  if(p){unsigned char before[sizeof s.bytes];memcpy(before,s.bytes,sizeof before);
   for(unsigned n=0;n<count;n++)before[wanted+n]=p[n]=(at+n*73)&255;
   assert(!memcmp(before,s.bytes,sizeof before));}
  checks++;
 }
 printf("Boot globals: %u address/span checks; all 179 bytes distinct, gaps/zero/wrapped/straddling spans rejected\n",checks);
}
