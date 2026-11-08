#include "../src/willow_native_visibility_payload.h"
#include "../src/willow_native_demo_state.h"
#include "../src/willow_native_demo_start_state.h"
#include "../src/willow_native_demo_update_state.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){
 WillowNativeDemoState s;memset(&s,0,sizeof s);
 /* Independent source ownership: existing aliases must never be shadowed. */
 const unsigned spans[][2]={{0x8289,5},{0x82c2,129},{0x8346,125},{0x83c4,7},{0x83cc,54},{0xf9a2,1}};
 unsigned checks=0;
 for(unsigned at=0;at<65536;at++)for(unsigned count=0;count<5;count++){
  int wanted=-1;unsigned offset=0;
  for(unsigned n=0;n<6;n++){
   if(count&&at>=spans[n][0]&&at+count<=spans[n][0]+spans[n][1])wanted=offset+at-spans[n][0];
   offset+=spans[n][1];
  }
  assert(offset==sizeof s);
  uint8_t*p=willowNativeDemoStateResolve(&s,at,count);
  assert(wanted<0?!p:p==s.bytes+wanted);
  if(p){uint8_t before[321];memcpy(before,s.bytes,sizeof before);
   for(unsigned n=0;n<count;n++)before[wanted+n]=p[n]=(at+n*73)&255;
   assert(!memcmp(before,s.bytes,sizeof before));}
  checks++;
 }
 assert(!willowNativeDemoStateResolve(NULL,0x8289,1));
 puts("Demo state: 327680 address/span checks; 321 owned bytes, aliases and crossing spans excluded");
 WillowNativeDemoStartState start;memset(&start,0xa5,sizeof start);
 const unsigned low[][2]={{0xa30,2},{0xa3c,1},{0xa42,8},{0xe68,4},{0xf40,64},{0xf88,32}};
 for(unsigned at=0;at<65536;at++)for(unsigned count=0;count<5;count++){
  int wanted=-1;unsigned offset=0;
  for(unsigned n=0;n<6;n++){
   if(count&&at>=low[n][0]&&at+count<=low[n][0]+low[n][1])wanted=offset+at-low[n][0];
   offset+=low[n][1];
  }
  assert(offset==111);
  uint8_t*p=willowNativeDemoStartStateResolve(&start,at,count);
  assert(wanted<0?!p:p==start.bytes+wanted);
 }
 assert(!willowNativeDemoStartStateResolve(NULL,0xa30,1));
 willowNativeDemoStartStateColdReset(&start);
 for(unsigned n=0;n<sizeof start.bytes;n++)assert(!start.bytes[n]);
 puts("Demo-start low globals: 327680 independent address/span checks; 111 bytes, holes rejected");
 WillowNativeDemoUpdateState update;memset(&update,0xa5,sizeof update);
 const unsigned extra[][2]={{0xa32,4},{0xa3a,2},{0xe92,3},{0xf08,2},{0xfb0,4},{0xf892,6},{0xf99e,1},{0xe96,1},{0xfaa,4}};
 for(unsigned at=0;at<65536;at++)for(unsigned count=0;count<5;count++){
  int wanted=-1;unsigned offset=0;
  for(unsigned n=0;n<9;n++){
   if(count&&at>=extra[n][0]&&at+count<=extra[n][0]+extra[n][1])wanted=offset+at-extra[n][0];
   offset+=extra[n][1];
  }
  assert(offset==27);uint8_t*p=willowNativeDemoUpdateStateResolve(&update,at,count);
  assert(wanted<0?!p:p==update.bytes+wanted);
  if(p){assert(!willowNativeDemoStartStateResolve(&start,at,count));assert(!willowNativeDemoStateResolve(&s,at,count));}
 }
 assert(!willowNativeDemoUpdateStateResolve(NULL,0xa32,1));
 willowNativeDemoUpdateStateColdReset(&update);
 for(unsigned n=0;n<sizeof update.bytes;n++)assert(!update.bytes[n]);
 puts("Demo-update globals: 327680 independent address/span checks; 27 bytes, holes and overlapping owners rejected");
 WillowNativeVisibilityPayload payload;memset(&payload,0xa5,sizeof payload);
 const unsigned lanes[][2]={{0x9356,80},{0x946e,352},{0x96be,320},{0xf466,32}};
 for(unsigned at=0;at<65536;at++)for(unsigned count=0;count<5;count++){
  int wanted=-1;unsigned offset=0;
  for(unsigned n=0;n<4;n++){
   if(count&&at>=lanes[n][0]&&at+count<=lanes[n][0]+lanes[n][1])wanted=offset+at-lanes[n][0];
   offset+=lanes[n][1];
  }
  assert(offset==784);uint8_t*p=willowNativeVisibilityPayloadResolve(&payload,at,count);
  assert(wanted<0?!p:p==payload.bytes+wanted);
  if(p){assert(!willowNativeDemoUpdateStateResolve(&update,at,count));assert(!willowNativeDemoStartStateResolve(&start,at,count));assert(!willowNativeDemoStateResolve(&s,at,count));}
 }
 assert(!willowNativeVisibilityPayloadResolve(NULL,0x9356,1));
 willowNativeVisibilityPayloadColdReset(&payload);for(unsigned n=0;n<sizeof payload.bytes;n++)assert(!payload.bytes[n]);
 puts("Remaining visibility payload: 327680 independent address/span checks; 784 bytes with adjacent owners excluded");
 return checks!=327680;
}
