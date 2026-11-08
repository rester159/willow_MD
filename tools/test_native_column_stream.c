#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/willow_native_column_stream.h"
#ifdef WILLOW_ADJACENT_COLUMNS
#define columnStep willowColumnStreamAdjacent
#else
#define columnStep willowColumnStreamStep
#endif
#define HEIGHT 28
#define EMPTY (-999999)
typedef struct {
 int cpu[43][HEIGHT],vram[64][HEIGHT];
 int published,fail,clearing;
 unsigned loads,releases,steps,cancelled;
 WillowColumnStream stream;
} Rig;
static int firstPixelColumn(int x){return x>=0?x/8:-((-x+7)/8);}
static uint8_t load(void*p,int16_t col,uint8_t row){
 Rig*r=p;if(r->fail){r->fail=0;return 0;}
 unsigned slot=willowColumnSlot(col),hardware=willowColumnHardware(col);
 assert(r->cpu[slot][row]==EMPTY);
 int old=r->vram[hardware][row];
 if(old!=EMPTY && old!=col)assert(old<firstPixelColumn(r->published) || old>firstPixelColumn(r->published+319));
 r->cpu[slot][row]=r->vram[hardware][row]=col;r->loads++;return 1;
}
static void release(void*p,int16_t col,uint8_t rows){
 Rig*r=p;unsigned slot=willowColumnSlot(col);
 assert(r->clearing || col<firstPixelColumn(r->published) || col>firstPixelColumn(r->published+319));
 if(rows && rows<HEIGHT)r->cancelled++;
 for(unsigned y=0;y<HEIGHT;y++){
  assert((r->cpu[slot][y]==col)==(y<rows));
  if(y<rows){r->cpu[slot][y]=EMPTY;r->releases++;}
 }
}
static void init(Rig*r){
 memset(r,0,sizeof *r);
 for(unsigned s=0;s<43;s++)for(unsigned y=0;y<HEIGHT;y++)r->cpu[s][y]=EMPTY;
 for(unsigned s=0;s<64;s++)for(unsigned y=0;y<HEIGHT;y++)r->vram[s][y]=EMPTY;
 willowColumnStreamInit(&r->stream,0);
 for(int col=0;col<=40;col++){
  assert(willowColumnStreamSeed(&r->stream,col,HEIGHT));
  for(unsigned row=0;row<HEIGHT;row++)assert(load(r,col,row));
 }
}
static void audit(Rig*r){
 unsigned held=0;
 for(unsigned s=0;s<43;s++){
  int col=r->stream.columns[s];unsigned rows=r->stream.rows[s];
  assert(rows<=HEIGHT);if(col!=WILLOW_COLUMN_EMPTY)assert(willowColumnSlot(col)==s);
  for(unsigned y=0;y<HEIGHT;y++){
   assert(r->cpu[s][y]==(y<rows?col:EMPTY));held+=y<rows;
  }
 }
 assert(r->loads-r->releases==held);
}
static void pixels(Rig*r,int x){
 if(!willowColumnStreamReady(&r->stream,x,HEIGHT)){
  fprintf(stderr,"Not ready: update %u published %d requested %d\n",r->steps,r->published,x);
  for(int c=firstPixelColumn(x);c<=firstPixelColumn(x+319);c++)if(!willowColumnStreamHas(&r->stream,c,HEIGHT))fprintf(stderr,"Missing col %d slot %u tag %d rows %u\n",c,willowColumnSlot(c),r->stream.columns[willowColumnSlot(c)],r->stream.rows[willowColumnSlot(c)]);
  abort();
 }
 for(int px=0;px<320;px++){
  int col=firstPixelColumn(x+px);
  for(unsigned y=0;y<HEIGHT;y++)assert(r->cpu[willowColumnSlot(col)][y]==col && r->vram[willowColumnHardware(col)][y]==col);
 }
}
static void step(Rig*r,int x,unsigned budget){
 WillowColumnIO io={r,load,release};
 assert(columnStep(&r->stream,x,r->published,HEIGHT,budget,&io));
 audit(r);pixels(r,r->published);pixels(r,x);r->published=x;r->steps++;
}
int main(void){
 Rig r;unsigned steps=0,cancelled=0;
 for(unsigned speed=1;speed<=2;speed++){
  unsigned budget=speed==2?7:4;
  init(&r);step(&r,0,budget);step(&r,0,budget);
  for(int x=speed;x<=8192;x+=speed)step(&r,x,budget);
  for(int x=8192-speed;x>=-8192;x-=speed)step(&r,x,budget);
  for(int x=-8192+speed;x<=0;x+=speed)step(&r,x,budget);
  steps+=r.steps;cancelled+=r.cancelled;
 }
 init(&r);step(&r,0,7);step(&r,0,7);
 uint32_t rng=1;int x=0;
 for(unsigned n=0;n<20000;n++){
  rng=rng*1664525u+1013904223u;x+=(rng>>29)<4?-2:2;
  if(x>8000)x=8000;if(x<0)x=0;step(&r,x,7);
 }
 steps+=r.steps;cancelled+=r.cancelled;assert(cancelled);
 /* Resource failure leaves an exact, retryable partial column. */
 init(&r);r.fail=1;WillowColumnIO io={&r,load,release};
 assert(!columnStep(&r.stream,0,0,HEIGHT,7,&io));audit(&r);pixels(&r,0);step(&r,0,7);
 /* A large jump cannot overwrite the still-published view. */
 assert(!columnStep(&r.stream,2048,0,HEIGHT,2000,&io));audit(&r);pixels(&r,0);
 /* Fade-completed scene clear includes all visible/partial/empty columns.
  * Keep the callback's ownership checks; only its visible-view prohibition
  * changes while publication is explicitly disabled. */
 unsigned clearCases=0;
 for(unsigned sample=0;sample<256;sample++){
  memset(&r,0,sizeof r);
  for(unsigned s=0;s<43;s++)for(unsigned y=0;y<HEIGHT;y++)r.cpu[s][y]=EMPTY;
  for(unsigned s=0;s<64;s++)for(unsigned y=0;y<HEIGHT;y++)r.vram[s][y]=EMPTY;
  int first=(int)sample-128;r.published=first*8;
  willowColumnStreamInit(&r.stream,r.published);
  for(int col=first;col<first+43;col++){
   unsigned rows=(sample+(unsigned)(col-first))%(HEIGHT+1);
   assert(willowColumnStreamSeed(&r.stream,col,rows));
   for(unsigned y=0;y<rows;y++)assert(load(&r,col,y));
  }
  audit(&r);int previous=r.stream.previousX;r.clearing=1;
  willowColumnStreamClear(&r.stream,&io);audit(&r);
  assert(r.loads==r.releases && r.stream.previousX==previous);
  assert(!willowColumnStreamReady(&r.stream,r.published,HEIGHT));
  unsigned released=r.releases;willowColumnStreamClear(&r.stream,&io);
  assert(r.releases==released);audit(&r);
  /* Publication clears hardware independently before the new map is read. */
  for(unsigned s=0;s<64;s++)for(unsigned y=0;y<HEIGHT;y++)r.vram[s][y]=EMPTY;
  r.clearing=0;
  assert(willowColumnStreamStep(&r.stream,r.published,r.published,HEIGHT,43*HEIGHT,&io));
  audit(&r);pixels(&r,r.published);step(&r,r.published+1,7);clearCases++;
 }
 printf("Incremental columns: %u movement updates, %u partial cancellations; exact ownership, CPU/VDP pixels, failure/retry and published-view protection pass\n",steps,cancelled);
 printf("Scene clear: %u mixed-ownership clears, repeated clears and full refills pass without leaked/double-released cells\n",clearCases);
 return 0;
}
