#include "../src/willow_cps_video_control.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
 /* Enumerate all layer-order encodings with independent one-hot enables,
  * including duplicate layers. Hardware masks come from CPS-B-03. */
 const uint16_t masks[]={0x20,0x10,8};unsigned checks=0;
 for(unsigned order=0;order<256;order++)for(unsigned enables=0;enables<8;enables++)
 for(unsigned videoBits=0;videoBits<16;videoBits++)for(unsigned flip=0;flip<2;flip++){
  unsigned layer=order<<6;
  for(unsigned n=0;n<3;n++)if(enables&(1u<<n))layer|=masks[n];
  unsigned video=videoBits|(flip?0x8000:0);
  WillowCPSVideoControl state=willowCPSVideoControl(layer,video);
  unsigned wanted=1|((enables&1)?2:0)|((enables&2)&&(videoBits&4)?4:0)|((enables&4)&&(videoBits&8)?8:0);
  assert(state.enabled==wanted&&state.flip==flip&&state.lineScroll==(videoBits&1));
  unsigned encoded=order;
  for(unsigned n=0;n<4;n++){assert(state.order[n]==encoded%4);encoded/=4;}
  /* Bits not used by these hardware outputs must not alter the decode. */
  WillowCPSVideoControl ignored=willowCPSVideoControl(layer|0xc007,video|0x7ff2);
  assert(ignored.enabled==state.enabled&&ignored.flip==state.flip&&ignored.lineScroll==state.lineScroll);
  for(unsigned n=0;n<4;n++)assert(ignored.order[n]==state.order[n]);
  checks++;
 }
 WillowCPSVideoControl reset=willowCPSVideoControl(0x12f8,0x003e);
 const unsigned order[]={3,2,0,1};assert(reset.enabled==15&&!reset.flip&&!reset.lineScroll);
 WillowCPSVideoControl faded=willowCPSVideoControl(0x12f8&0xffc0,0x003e);
 assert(faded.enabled==1); /* Fade completion disables scrolls, not objects. */
 for(unsigned n=0;n<4;n++)assert(reset.order[n]==order[n]&&faded.order[n]==order[n]);
 puts("Willow CPS video control: reset and completed fade preserve object rendering and layer order");
 printf("%u hardware-control combinations verify enables, order, flip, line scroll and ignored bits\n",checks);
 return 0;
}
