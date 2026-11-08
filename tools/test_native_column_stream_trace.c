#include "native_background_fixture.h"
#include "../src/willow_native_column_stream.h"
#ifdef WILLOW_ADJACENT_COLUMNS
#define columnStep willowColumnStreamAdjacent
#else
#define columnStep willowColumnStreamStep
#endif
typedef struct {
 WillowColumnStream stream;
 uint16_t cpu[43][28],vram[64][28],refs[NATIVE_BACKGROUND_ID_LIMIT];
 unsigned layer,loads,releases;
 int published;
} Plane;
/* Combined background working set, including transient partial columns.
 * Native HUD/actor/pin residency is measured separately in the ROM test. */
static uint16_t sharedRefs[NATIVE_BACKGROUND_ID_LIMIT];
static unsigned resident,peakResident,uploads,peakUploads;
static uint16_t originalEntry(unsigned layer,int col,unsigned row){
 uint16_t x=32+col*8,y=row*8-240;
 uint32_t pair=sourcePair(layer,x,y);
 const uint32_t*pairs=layer==2?nativeScroll2Pairs:nativeScroll3Pairs;
 unsigned count=layer==2?WILLOW_SCROLL2_PAIR_COUNT:WILLOW_SCROLL3_PAIR_COUNT;
 unsigned id=idOf(pair,pairs,count),size=layer==2?16:32;
 unsigned off=(layer==2?nativeBackground2Offsets:nativeBackground3Offsets)[id];assert(off!=65535);
 return (layer==2?nativeBackground2Entries:nativeBackground3Entries)[off+(y%size/8)*(size/8)+x%size/8];
}
static uint8_t loadCell(void*context,int16_t col,uint8_t row){
 Plane*p=context;uint16_t entry,x=32+col*8,y=row*8-240;
 unsigned valid=p->layer==2?willowBackground2Entry(near,nativeBackground2Offsets,WILLOW_SCROLL2_PAIR_COUNT,nativeBackground2Entries,x,y,&entry):willowBackground3Entry(far,nativeBackground3Offsets,WILLOW_SCROLL3_PAIR_COUNT,nativeBackground3Entries,x,y,&entry);
 assert(valid && entry==originalEntry(p->layer,col,row));
 unsigned slot=willowColumnSlot(col);assert(p->cpu[slot][row]==65535);
 p->cpu[slot][row]=p->vram[willowColumnHardware(col)][row]=entry;
 unsigned id=entry&32767;
 if(!sharedRefs[id]++){resident++;uploads++;if(resident>peakResident)peakResident=resident;}
 p->refs[id]++;p->loads++;return 1;
}
static void releaseCells(void*context,int16_t col,uint8_t count){
 Plane*p=context;unsigned slot=willowColumnSlot(col);
 for(unsigned row=0;row<count;row++){
  unsigned entry=p->cpu[slot][row];assert(entry!=65535 && p->refs[entry&32767]);
  unsigned id=entry&32767;assert(sharedRefs[id]);
  if(!--sharedRefs[id])resident--;
  p->refs[id]--;p->cpu[slot][row]=65535;p->releases++;
 }
}
static void audit(Plane*p,int x){
 unsigned expected[NATIVE_BACKGROUND_ID_LIMIT]={0},held=0;
 for(unsigned slot=0;slot<43;slot++)for(unsigned row=0;row<28;row++){
  unsigned entry=p->cpu[slot][row];assert((entry!=65535)==(row<p->stream.rows[slot]));
  if(entry!=65535){expected[entry&32767]++;held++;}
 }
 assert(held==p->loads-p->releases);
 for(unsigned id=0;id<NATIVE_BACKGROUND_ID_LIMIT;id++)assert(expected[id]==p->refs[id]);
 assert(willowColumnStreamReady(&p->stream,x,28));
 for(int col=x/8;col<=(x+319)/8;col++)for(unsigned row=0;row<28;row++){
  unsigned expectedEntry=originalEntry(p->layer,col,row);
  assert(p->cpu[willowColumnSlot(col)][row]==expectedEntry);
  assert(p->vram[willowColumnHardware(col)][row]==expectedEntry);
 }
}
int main(void){
 FILE*f=fopen("build/reference_maincpu.bin","rb");assert(f && fread(rom,1,sizeof rom,f)==sizeof rom);fclose(f);
 FILE*w=fopen("build/arcade-trace/player-walk/frames.bin","rb"),*g=fopen("build/arcade-trace/scroll3-walk/frames.bin","rb"),*m=fopen("build/arcade-trace/scroll3-walk/maps.bin","rb");assert(w&&g&&m);
 Plane planes[2];memset(planes,0,sizeof planes);
 for(unsigned i=0;i<2;i++){planes[i].layer=i+2;memset(planes[i].cpu,255,sizeof planes[i].cpu);memset(planes[i].vram,255,sizeof planes[i].vram);willowColumnStreamInit(&planes[i].stream,0);}
 for(unsigned frame=1604;frame<=1700;frame++){
  assert(!fseek(w,(frame-1587)*sizeof walk,SEEK_SET) && fread(walk,1,sizeof walk,w)==sizeof walk);
  assert(!fseek(g,(frame-650)*sizeof globals,SEEK_SET) && fread(globals,1,sizeof globals,g)==sizeof globals);
  assert(!fseek(m,(frame-650)*sizeof distant,SEEK_SET) && fread(distant,1,sizeof distant,m)==sizeof distant);
  assert(willowMotionLong(walk)==frame && willowMotionLong(globals)==frame);
  for(unsigned cell=0;cell<4096;cell++){
   near[cell]=idOf(willowMotionLong(walk+1286+cell*4),nativeScroll2Pairs,WILLOW_SCROLL2_PAIR_COUNT);
   assert(willowScroll3MapSet(far,cell,idOf(willowMotionLong(distant+cell*4),nativeScroll3Pairs,WILLOW_SCROLL3_PAIR_COUNT)));
  }
  for(unsigned i=0;i<2;i++){
   Plane*p=planes+i;int x=willowMotionWord(globals+4+(i?0x482:0x402)+6);
   assert(willowMotionWord(globals+4+(i?0x482:0x402)+10)==0);
   if(frame==1604)for(int col=0;col<=40;col++){
    assert(willowColumnStreamSeed(&p->stream,col,28));for(unsigned row=0;row<28;row++)assert(loadCell(p,col,row));
   }
   WillowColumnIO io={p,loadCell,releaseCells};unsigned before=p->loads;
   assert(columnStep(&p->stream,x,p->published,28,i?4:7,&io));
   assert(p->loads-before<=(i?4:7));audit(p,p->published);audit(p,x);p->published=x;
  }
  unsigned counted=0;
  for(unsigned id=0;id<NATIVE_BACKGROUND_ID_LIMIT;id++){
   assert(sharedRefs[id]==planes[0].refs[id]+planes[1].refs[id]);
   counted+=sharedRefs[id]!=0;
  }
  assert(counted==resident);
  if(frame!=1604 && uploads>peakUploads)peakUploads=uploads;
  uploads=0;
 }
 fclose(w);fclose(g);fclose(m);
 printf("Source-trace column streamer: 97 frames, both live CPS maps, 7/4-cell budgets; %u/%u loads and %u/%u releases, every visible cell and all references match ROM\n",planes[0].loads,planes[1].loads,planes[0].releases,planes[1].releases);
 printf("Background working set: %u peak resident patterns; %u peak new patterns per update after initialization (excludes native HUD/actor/pins)\n",peakResident,peakUploads);
}
