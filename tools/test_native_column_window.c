#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "../src/willow_native_column_window.h"
static unsigned checks;
static int inside(int column,int first){return column>=first && column<first+43;}
static void check(int16_t old,int16_t x){
 WillowColumnChange c=willowColumnPlan(old,x);
 int floor=x>=0?x/8:-((-((int)x)+7)/8);
 assert(c.first==floor-1);
 assert(c.retire.count<=43 && c.prepare.count<=43);
 int cpu[43],vram[64];for(unsigned i=0;i<43;i++)cpu[i]=-99999;for(unsigned i=0;i<64;i++)vram[i]=-99999;
 for(int col=old;col<old+43;col++){cpu[willowColumnSlot(col)]=col;vram[willowColumnHardware(col)]=col;}
 for(int col=old;col<old+43;col++){
  int retired=col>=c.retire.first && col<c.retire.first+c.retire.count;
  assert(retired==!inside(col,c.first));
 }
 for(unsigned n=0;n<c.prepare.count;n++){
  int col=c.prepare.first+n;assert(inside(col,c.first) && !inside(col,old));
  cpu[willowColumnSlot(col)]=col;vram[willowColumnHardware(col)]=col;
 }
 for(int col=c.first;col<c.first+43;col++){
  int prepared=col>=c.prepare.first && col<c.prepare.first+c.prepare.count;
  assert(prepared==!inside(col,old));
  assert(cpu[willowColumnSlot(col)]==col && vram[willowColumnHardware(col)]==col);
 }
 /* Every screen pixel, including fractional edge tiles, has a correct
  * independent CPU and VRAM slot after the proposed column uploads. */
 for(int px=0;px<320;px++){
  int world=(int)x+px,col=world>=0?world/8:-((-world+7)/8);
  assert(inside(col,c.first));assert(cpu[willowColumnSlot(col)]==col && vram[willowColumnHardware(col)]==col);
 }
 checks++;
}
int main(void){
 for(int x=-32768;x<=32767;x++){
  int expected=x%43;if(expected<0)expected+=43;
  assert(willowColumnSlot(x)==expected);
  int16_t origin=willowColumnFirst((int16_t)x);
  check(origin,x);check(origin-1,x);check(origin+1,x);
  check(origin-43,x);check(origin+43,x);
 }
 int16_t first=-1;
 /* Continuous forward/backward travel crosses both distinct wrap sizes. */
 for(unsigned repeat=0;repeat<4;repeat++){
  for(int x=0;x<8192;x+=2){check(first,x);first=willowColumnFirst(x);}
  for(int x=8190;x>=0;x-=2){check(first,x);first=willowColumnFirst(x);}
 }
 printf("Column window: %u transitions, all visible pixels and independent 43/64-column wraps checked\n",checks);
}
