/* Closed source-vs-hook 68000 differential. Host setup is outside each call;
 * no target CPU/RAM writes occur from the host during native execution. */
#define WILLOW_CLOSED_HELPERS_ONLY 1
#include "test_native_render_cache_closed.c"
#include "../src/willow_native_background_writer.h"
#include "writer-fixture.h"
#include "writer-route.h"
#undef CHECK
#define REQUIRE(x) do{if(!(x)){fprintf(stderr,"Writer failure line%u case%u bus%u kind%u mode%u count%u: %s PC%x\n",__LINE__,cases,bus,kind,mode,count,#x,getreg(16));exit(1);}}while(0)
static unsigned cases,bus,kind,mode,count,hooked,instructionGaps,irqCases;
static void(*irq68k)(unsigned);
static uint8_t sourceVideo[65536],initialVideo[65536],referenceVideo[65536],referenceRAM[65536],referenceSRAM[65536];
static unsigned referenceRegisters[21],initialRegisters[15];
static struct Event {unsigned at,value;} events[2][40000];
static unsigned eventCount[2],minSP;
static void(*sramWrite16)(unsigned,unsigned);
static unsigned canonical(unsigned at){
 if(!bus)return at;
 if(at>=0x200000&&at<0x204000)return at-0x200000+0x910000;
 if(at>=0x204000&&at<0x208000)return at-0x204000+0x918000;
 if(at>=0x208000&&at<0x20c000)return at-0x208000+0x914000;
 return 0;
}
static void recordWrite(unsigned at,unsigned value){
 unsigned source=canonical(at);if(!source)return;
 if(eventCount[hooked]>=40000)abort();events[hooked][eventCount[hooked]++]=(struct Event){source,value&65535};
 if(hooked&&kind<4){
  /* Every raw/adjust/attribute source write occurs while the entire controller
   * bracket is busy. Journal must never advertise a ready partial map. */
  if(word(sram+0xf00c)!=3||word(sram+0xf01e)==1){fprintf(stderr,"Source store exposed ready metadata at%x\n",at);abort();}
 }
}
static unsigned sourceRead8(unsigned at){if(at<0x910000||at>=0x920000)abort();return sourceVideo[at&65535];}
static unsigned sourceRead16(unsigned at){if(at&1||at<0x910000||at>=0x91ffff)abort();return word(sourceVideo+(at&65535));}
static void sourceWrite8(unsigned at,unsigned v){(void)at;(void)v;abort();}
static void sourceWrite16(unsigned at,unsigned v){if(at&1||at<0x910000||at>=0x91ffff)abort();recordWrite(at,v);pw(sourceVideo+(at&65535),v);}
static void nativeWrite16(unsigned at,unsigned v){if(bus&&at>=0x200000&&at<0x20c000)recordWrite(at,v);sramWrite16(at,v);}
static void patch(unsigned at,const uint8_t*data,unsigned n){uint8_t*p=cpu->map[0].base;for(unsigned i=0;i<n;i++)p[(at+i)^1]=data[i];}
static void targetState(const WillowNativeBackgroundWriter*m){
 uint8_t*p=sram+0xf000;pl(p,m->rawEpoch);pl(p+4,m->mapGeneration[0]);pl(p+8,m->mapGeneration[1]);
 pw(p+12,m->rawBusy);pw(p+14,m->touchedMask);pw(p+16,m->active);pw(p+18,m->fault);
 const WillowNativeBackgroundJournal*j=&m->journal;pl(p+20,j->epoch);pl(p+24,j->openEpoch);pw(p+28,j->count);pw(p+30,j->state);pw(p+32,j->fault);
 for(unsigned i=0;i<64;i++){pw(p+34+i*6,j->entries[i].key);pw(p+36+i*6,j->entries[i].code);pw(p+38+i*6,j->entries[i].attribute);}
 memcpy(p+418,m->index,64);pw(p+482,m->indexedCount);
}
static void expectState(const WillowNativeBackgroundWriter*m){
 uint8_t actual[484];memcpy(actual,sram+0xf000,484);targetState(m);
 for(unsigned i=0;i<484;i++)if(actual[i]!=sram[0xf000+i]){fprintf(stderr,"Metadata byte%x actual%x expected%x case%u\n",i,actual[i],sram[0xf000+i],cases);exit(1);}
 /* targetState writes only the diagnostic expected bytes between calls. */
}
static unsigned execute(unsigned pc){
 wl(0xff5ffc,0x100000);setreg(15,0xff5ffc);setreg(16,pc);cpu->cycles=0;cpu->refresh=896;unsigned steps=0;minSP=0xff5ffc;
 unsigned d4=getreg(4),a5=getreg(13);
 while(getreg(16)!=0x100000&&steps++<500000){REQUIRE(getreg(4)==d4&&getreg(13)==a5);instructionGaps++;unsigned sp=getreg(15);if(sp<minSP)minSP=sp;run68k(cpu->cycles+1);}
 REQUIRE(steps<500000&&getreg(15)==0xff6000);return cpu->cycles;
}
static unsigned preserving(unsigned pc){
 unsigned r[15],sr=getreg(17);for(unsigned n=0;n<15;n++)r[n]=getreg(n);
 unsigned cycles=execute(pc);for(unsigned n=0;n<15;n++)REQUIRE(getreg(n)==r[n]);REQUIRE(getreg(17)==sr);REQUIRE(minSP>=0xff5fea);return cycles;
}
static uint8_t*mapPointer(unsigned layer,unsigned cell){unsigned offset=layer==2?0x8000:layer==3?0x4000:0;return sourceVideo+offset+cell*4;}
static unsigned mapBase(unsigned layer){return bus?(layer==2?0x204000:layer==3?0x208000:0x200000):(layer==2?0x918000:layer==3?0x914000:0x910000);}
static void modelReindex(WillowNativeBackgroundWriter*m){
 memset(m->index,0,64);m->indexedCount=m->journal.count;
 for(unsigned i=0;i<m->journal.count&&i<64;i++){
  unsigned key=m->journal.entries[i].key,slot=(key^(key>>6))&63;
  while(m->index[slot])slot=(slot+1)&63;
  m->index[slot]=(uint8_t)(i+1);
 }
}
static void modelBegin(WillowNativeBackgroundWriter*m){
 m->rawBusy=3;m->active=1;m->touchedMask=0;m->rawEpoch++;
 if(m->journal.state==WILLOW_BACKGROUND_JOURNAL_READY&&m->journal.count!=m->indexedCount){
  if(!m->journal.count)modelReindex(m);
  else{m->journal.state=3;m->journal.fault=m->fault=8;return;}
 }
 if(m->journal.state==WILLOW_BACKGROUND_JOURNAL_READY&&!willowNativeBackgroundJournalBegin(&m->journal,m->rawEpoch))m->fault=m->journal.fault;
}
static void modelCommit(WillowNativeBackgroundWriter*m){
 m->active=0;m->rawBusy=0;
 if(m->journal.state==WILLOW_BACKGROUND_JOURNAL_BUSY&&!willowNativeBackgroundJournalCommit(&m->journal,m->rawEpoch))m->fault=m->journal.fault;
}
static void setupModel(WillowNativeBackgroundWriter*m,unsigned layer,unsigned cell,unsigned stride){
 memset(m,0xa5,sizeof *m);willowNativeBackgroundWriterInit(m);m->rawEpoch=100;m->mapGeneration[0]=20;m->mapGeneration[1]=30;
 if(mode!=3)REQUIRE(willowNativeBackgroundJournalBaseline(&m->journal,100));
 if(mode==2||mode==4){
  m->journal.count=64;
  for(unsigned i=0;i<64;i++){
   unsigned c=mode==2?(cell+i*stride/4)&4095:i;
   unsigned l=mode==2?layer:5-layer;
   const uint8_t*p=mapPointer(l,c);m->journal.entries[i]=(WillowNativeBackgroundJournalEntry){(uint16_t)(((l-2)<<12)|c),(uint16_t)word(p),(uint16_t)word(p+2)};
  }
 }else if(mode==5){m->journal.count=1;m->journal.entries[0]=(WillowNativeBackgroundJournalEntry){(uint16_t)(((layer-2)<<12)|cell),0xdead,0xbeef};}
 else if(mode==6)m->journal.count=65;
 else if(mode>=7){m->journal.count=1;m->journal.entries[0]=(WillowNativeBackgroundJournalEntry){(uint16_t)(((layer-2)<<12)|(mode==9?((cell+200)&4095):cell)),0xdead,0xbeef};}
 modelReindex(m);
 if(mode==7){unsigned key=((layer-2)<<12)|cell;m->index[(key^(key>>6))&63]=65;}
 if(mode==8)m->indexedCount=0;
 if(mode==9)memset(m->index,1,64);
 if(mode==10)m->journal.count=0;
}
static void pairCase(unsigned which,unsigned n,unsigned variant,unsigned flags){
 kind=which;count=n;mode=variant;unsigned layer=2+(kind>>1),stride=(kind&1)?(layer==2?64:32):4;
 unsigned extent=(count-1)*stride+4;
 unsigned start=(cases&1)?16384-extent:((cases&2)&&extent<=4096?4092:0);start&=~3u;
 uint16_t old[65][2],final[65][2];WillowNativeBackgroundWriter model,initial;
 for(unsigned i=0;i<65536;i++)sourceVideo[i]=(uint8_t)(i*37+(i>>7)*19+cases*13);
 for(unsigned i=0;i<count;i++){
  const uint8_t*p=mapPointer(layer,(start+i*stride)/4);old[i][0]=word(p);old[i][1]=word(p+2);
  final[i][0]=mode==1?old[i][0]:(uint16_t)(old[i][0]^(0x0101+i));final[i][1]=mode==1?old[i][1]:(uint16_t)(old[i][1]^(0x8001+i));
  uint8_t pair[4];pw(pair,(final[i][0]-(layer==3?0xa00:0))&65535);pw(pair+2,final[i][1]);patch(0x1e0000+i*stride,pair,4);
 }
 memcpy(initialVideo,sourceVideo,sizeof sourceVideo);setupModel(&initial,layer,start/4,stride);model=initial;
 unsigned costs[2];
 for(hooked=0;hooked<2;hooked++){
  memcpy(sourceVideo,initialVideo,sizeof sourceVideo);memset(sram,0xa5,65536);
  memcpy(sram,sourceVideo,0x4000);memcpy(sram+0x4000,sourceVideo+0x8000,0x4000);memcpy(sram+0x8000,sourceVideo+0x4000,0x4000);
  for(unsigned i=0;i<65536;i++)ram[i^1]=(uint8_t)(i*13+cases*7);
  targetState(&initial);
  for(unsigned r=0;r<15;r++){initialRegisters[r]=0x51390000u+r*0x1021u+flags;setreg(r,initialRegisters[r]);}
  initialRegisters[0]=0xabcd0000u|(count-1);initialRegisters[8]=mapBase(layer)+start;initialRegisters[9]=0x1e0000;initialRegisters[13]=0xff8000;initialRegisters[14]=0xff4000;
  for(unsigned r=0;r<15;r++)setreg(r,initialRegisters[r]);setreg(17,0x2700|flags);setreg(19,0xff7000);
  if(hooked){preserving(BEGIN);modelBegin(&model);expectState(&model);}
  patch(leafPC[kind],hooked?leafPatch[kind]:leafOriginal[kind],8);eventCount[hooked]=0;
  costs[hooked]=execute(leafPC[kind]);
  if(!hooked){
   for(unsigned i=0;i<21;i++)referenceRegisters[i]=getreg(i);for(unsigned i=0;i<65536;i++)referenceRAM[i]=ram[i^1];memcpy(referenceSRAM,sram,65536);
   if(!bus)memcpy(referenceVideo,sourceVideo,sizeof sourceVideo);
  }else{
   for(unsigned i=0;i<21;i++)REQUIRE(getreg(i)==referenceRegisters[i]);REQUIRE(minSP>=0xff5fda);
   for(unsigned i=0;i<65536;i++)if(!(i>=0x5fda&&i<0x6000))REQUIRE(ram[i^1]==referenceRAM[i]);
   REQUIRE(eventCount[0]==count*3&&eventCount[1]==eventCount[0]);REQUIRE(!memcmp(events[0],events[1],eventCount[0]*sizeof events[0][0]));
   if(!bus)REQUIRE(!memcmp(sourceVideo,referenceVideo,65536));
   for(unsigned i=0;i<65536;i++)if(!(i>=0xf000&&i<0xf000+484))REQUIRE(sram[i]==referenceSRAM[i]);
   /* The complete private register image and return slot explain the extra
    * task-stack bytes; the independent below-boundary canary is unchanged. */
   unsigned indexes[]={1,2,3,10,11,12};for(unsigned i=0;i<6;i++)REQUIRE(rl(0xff5fe4+i*4)==initialRegisters[indexes[i]]);
   REQUIRE(rl(0xff5ffc)==0x100000);
   model.touchedMask|=1u<<(layer-2);model.mapGeneration[layer-2]++;
   if(mode==7||mode==9){model.journal.state=3;model.journal.fault=model.fault=8;}
   for(unsigned i=0;i<count;i++)if((old[i][0]!=final[i][0]||old[i][1]!=final[i][1])&&model.journal.state==WILLOW_BACKGROUND_JOURNAL_BUSY){
    if(!willowNativeBackgroundJournalRecord(&model.journal,layer,(start+i*stride)/4,old[i][0],old[i][1],final[i][0],final[i][1]))model.fault=model.journal.fault;
    else modelReindex(&model);
   }
   expectState(&model);preserving(COMMIT);modelCommit(&model);expectState(&model);
  }
 }
 printf("pair,%u,%u,%u,%u,%u,%u,%u,%u,%u\n",bus,kind,mode,count,flags,costs[0],costs[1],costs[1]-costs[0],model.journal.count);cases++;
}
static void bulkCase(unsigned which,unsigned layer,unsigned n,unsigned bracket,unsigned flags){
 kind=4+which;count=n;mode=bracket;WillowNativeBackgroundWriter initial,model;
 for(unsigned i=0;i<65536;i++)sourceVideo[i]=(uint8_t)(i*37+(i>>7)*19+cases*13);
 memcpy(initialVideo,sourceVideo,sizeof sourceVideo);memset(&initial,0xa5,sizeof initial);willowNativeBackgroundWriterInit(&initial);
 initial.rawEpoch=100;initial.mapGeneration[0]=20;initial.mapGeneration[1]=30;REQUIRE(willowNativeBackgroundJournalBaseline(&initial.journal,100));model=initial;
 unsigned bytesPer=which==1?4:16;for(unsigned i=0;i<count*bytesPer;i++){uint8_t v=(uint8_t)(i*83+flags);patch(0x1e0000+i,&v,1);}
 unsigned costs[2];
 for(hooked=0;hooked<2;hooked++){
  memcpy(sourceVideo,initialVideo,65536);memset(sram,0xa5,65536);memcpy(sram,sourceVideo,0x4000);memcpy(sram+0x4000,sourceVideo+0x8000,0x4000);memcpy(sram+0x8000,sourceVideo+0x4000,0x4000);
  for(unsigned i=0;i<65536;i++)ram[i^1]=(uint8_t)(i*13+cases*7);targetState(&initial);
  for(unsigned r=0;r<15;r++)initialRegisters[r]=0x51390000u+r*0x1021u+flags;
  initialRegisters[0]=0xabcd0000u|(count-1);initialRegisters[1]=0x7654fa00;initialRegisters[2]=0x0123abcd;
  initialRegisters[8]=mapBase(layer);initialRegisters[9]=0x1e0000;initialRegisters[13]=0xff8000;initialRegisters[14]=0xff4000;
  for(unsigned r=0;r<15;r++)setreg(r,initialRegisters[r]);setreg(17,0x2700|flags);setreg(19,0xff7000);
  if(hooked&&bracket){preserving(BEGIN);modelBegin(&model);expectState(&model);}
  patch(bulkPC[which],hooked?bulkPatch[which]:bulkOriginal[which],6);eventCount[hooked]=0;costs[hooked]=execute(bulkPC[which]);
  if(!hooked){for(unsigned i=0;i<21;i++)referenceRegisters[i]=getreg(i);for(unsigned i=0;i<65536;i++)referenceRAM[i]=ram[i^1];memcpy(referenceSRAM,sram,65536);if(!bus)memcpy(referenceVideo,sourceVideo,65536);}
  else{
   for(unsigned i=0;i<21;i++)REQUIRE(getreg(i)==referenceRegisters[i]);REQUIRE(minSP>=0xff5fee);
   for(unsigned i=0;i<65536;i++)if(!(i>=0x5fee&&i<0x6000))REQUIRE(ram[i^1]==referenceRAM[i]);
   REQUIRE(eventCount[0]==count*(which==0?12:which==1?2:8));REQUIRE(eventCount[1]==eventCount[0]);REQUIRE(!memcmp(events[0],events[1],eventCount[0]*sizeof events[0][0]));
   if(!bus)REQUIRE(!memcmp(sourceVideo,referenceVideo,65536));
   for(unsigned i=0;i<65536;i++)if(!(i>=0xf000&&i<0xf000+484))REQUIRE(sram[i]==referenceSRAM[i]);
   if(layer!=1){model.rawBusy|=1u<<(layer-2);model.mapGeneration[layer-2]++;if(bracket)model.touchedMask|=1u<<(layer-2);willowNativeBackgroundJournalInvalidate(&model.journal);model.fault=7;}
   expectState(&model);if(bracket){preserving(COMMIT);modelCommit(&model);expectState(&model);}
  }
 }
 printf("bulk,%u,%u,%u,%u,%u,%u,%u,%u,%u\n",bus,kind,mode,count,flags,costs[0],costs[1],costs[1]-costs[0],model.journal.count);cases++;
}
static void lifecycle(void){
 kind=9;count=0;
 for(mode=0;mode<7;mode++)for(unsigned flags=0;flags<32;flags++){
  WillowNativeBackgroundWriter m;memset(&m,0xa5,sizeof m);willowNativeBackgroundWriterInit(&m);
  m.rawEpoch=100;m.mapGeneration[0]=20;m.mapGeneration[1]=30;REQUIRE(willowNativeBackgroundJournalBaseline(&m.journal,100));
  if(mode==1)m.rawEpoch=UINT32_MAX;
  if(mode==2)m.journal.epoch=101;
  if(mode==3)m.journal.epoch=0;
  if(mode==4)m.journal.state=WILLOW_BACKGROUND_JOURNAL_BUSY;
  if(mode==5)m.journal.fault=1;
  targetState(&m);for(unsigned r=0;r<15;r++)setreg(r,0x93290000u+r*0x1231u+flags);setreg(17,0x2700|flags);
  preserving(BEGIN);
  if(mode==0){preserving(BEGIN);REQUIRE(word(sram+0xf010)==2);preserving(COMMIT);REQUIRE(word(sram+0xf010)==2&&word(sram+0xf00c)==3&&word(sram+0xf01e)==3);}
  else if(mode==6){pl(sram+0xf018,102);preserving(COMMIT);REQUIRE(word(sram+0xf01e)==3&&word(sram+0xf020)==3);}
  else{REQUIRE(word(sram+0xf01e)==3);REQUIRE(word(sram+0xf020)==(mode==1||mode==2?3:mode==3?1:2));}
  cases++;
 }
 /* Explicit baseline refuses changing/incomplete generations and has no writes
  * on refusal. This tests the owner API in sanitized host code. */
 WillowNativeBackgroundWriter m,old;memset(&m,0xa5,sizeof m);willowNativeBackgroundWriterInit(&m);
 REQUIRE(!willowNativeBackgroundWriterBaseline(&m,0,0,0));m.rawEpoch=7;m.mapGeneration[0]=8;m.mapGeneration[1]=9;
 for(unsigned wrong=0;wrong<5;wrong++){
  m.rawBusy=wrong==0?1:0;m.active=wrong==1?1:0;old=m;
  REQUIRE(!willowNativeBackgroundWriterBaseline(&m,wrong==2?6:7,wrong==3?7:8,wrong==4?8:9));REQUIRE(!memcmp(&m,&old,sizeof m));
 }
 m.active=m.rawBusy=0;REQUIRE(willowNativeBackgroundWriterBaseline(&m,7,8,9));REQUIRE(m.rawEpoch==8&&m.journal.epoch==8&&m.journal.state==1&&m.journal.count==0);
}
static void routeCases(const char*dir){
 char name[4096];snprintf(name,sizeof name,"%s/route-baseline.bin",dir);FILE*f=fopen(name,"rb");REQUIRE(f);
 memset(sourceVideo,0,65536);REQUIRE(fread(sourceVideo+0x8000,1,0x4000,f)==0x4000);REQUIRE(fread(sourceVideo+0x4000,1,0x4000,f)==0x4000);fclose(f);
 WillowNativeBackgroundWriter initial,model;memset(&model,0xa5,sizeof model);willowNativeBackgroundWriterInit(&model);model.rawEpoch=100;model.mapGeneration[0]=20;model.mapGeneration[1]=30;REQUIRE(willowNativeBackgroundJournalBaseline(&model.journal,100));
 unsigned total=sizeof routeGroups/sizeof routeGroups[0],g=0;
 while(g<total){
  unsigned end=g+1,frame=routeGroups[g][0];while(end<total&&routeGroups[end][0]==frame)end++;
  initial=model;memcpy(initialVideo,sourceVideo,65536);unsigned costs[2]={0,0},saved[32][21];kind=0;mode=8;count=0;
  for(unsigned k=g;k<end;k++)count+=routeGroups[k][6];
  for(hooked=0;hooked<2;hooked++){
   memcpy(sourceVideo,initialVideo,65536);memset(sram,0xa5,65536);memcpy(sram,sourceVideo,0x4000);memcpy(sram+0x4000,sourceVideo+0x8000,0x4000);memcpy(sram+0x8000,sourceVideo+0x4000,0x4000);
   for(unsigned i=0;i<65536;i++)ram[i^1]=(uint8_t)(i*13+cases*7);targetState(&initial);eventCount[hooked]=0;
   for(unsigned r=0;r<15;r++)setreg(r,0x735a0000u+r*0x1421u);setreg(17,0x2700|31);
   if(hooked){costs[hooked]+=preserving(BEGIN);modelBegin(&model);expectState(&model);}
   for(unsigned k=g;k<end;k++){
    const unsigned*q=routeGroups[k];unsigned layer=q[1],cell=(q[2]-(layer==2?0x918000:0x914000))/4;kind=(layer-2)*2;
    for(unsigned r=0;r<15;r++)setreg(r,0x51390000u+r*0x1021u+31);
    setreg(0,q[4]);setreg(8,mapBase(layer)+cell*4);setreg(9,q[3]);setreg(13,0xff8000);setreg(14,0xff4000);setreg(17,0x271f);setreg(19,0xff7000);
    patch(leafPC[kind],hooked?leafPatch[kind]:leafOriginal[kind],8);costs[hooked]+=execute(leafPC[kind]);
    if(!hooked)for(unsigned r=0;r<21;r++)saved[k-g][r]=getreg(r);
    else{
     for(unsigned r=0;r<21;r++)REQUIRE(saved[k-g][r]==getreg(r));REQUIRE(minSP>=0xff5fda);
     if(!(model.touchedMask&(1u<<(layer-2)))){model.touchedMask|=1u<<(layer-2);model.mapGeneration[layer-2]++;}
     for(unsigned j=0;j<q[6];j++){
      const unsigned*v=routePairs[q[5]+j];const uint8_t*before=initialVideo+(v[2]&65535);
      REQUIRE(word(before)==v[5]&&word(before+2)==v[6]);
      if(v[5]!=v[7]||v[6]!=v[8]){REQUIRE(willowNativeBackgroundJournalRecord(&model.journal,layer,cell+j,v[5],v[6],v[7],v[8]));modelReindex(&model);}
     }
     expectState(&model);
    }
   }
   if(!hooked){for(unsigned i=0;i<65536;i++)referenceRAM[i]=ram[i^1];memcpy(referenceSRAM,sram,65536);memcpy(referenceVideo,sourceVideo,65536);}
   else{
    REQUIRE(eventCount[0]==count*3&&eventCount[1]==eventCount[0]);REQUIRE(!memcmp(events[0],events[1],eventCount[0]*sizeof events[0][0]));
    if(!bus)REQUIRE(!memcmp(sourceVideo,referenceVideo,65536));
    for(unsigned i=0;i<65536;i++){if(!(i>=0x5fda&&i<0x6000))REQUIRE(ram[i^1]==referenceRAM[i]);if(!(i>=0xf000&&i<0xf000+484))REQUIRE(sram[i]==referenceSRAM[i]);}
    costs[hooked]+=preserving(COMMIT);modelCommit(&model);expectState(&model);REQUIRE(model.journal.count==14&&model.journal.state==1);
    if(bus){memcpy(sourceVideo,sram,0x4000);memcpy(sourceVideo+0x8000,sram+0x4000,0x4000);memcpy(sourceVideo+0x4000,sram+0x8000,0x4000);}
   }
  }
  printf("route,%u,0,8,%u,%u,%u,%u,%u,%u\n",bus,count,frame,costs[0],costs[1],costs[1]-costs[0],model.journal.count);
  WillowNativeBackgroundJournalView view;REQUIRE(willowNativeBackgroundJournalAcquire(&model.journal,&view));REQUIRE(willowNativeBackgroundJournalAcknowledge(&model.journal,&view));
  /* Native next Begin must lazily reset the still-populated private index. */
  REQUIRE(model.journal.count==0&&model.indexedCount==14);g=end;cases++;
 }
}
static unsigned irqLeafRun(unsigned gap,unsigned supervisor,unsigned flags,unsigned variant){
 unsigned layer=2+(kind>>1),stride=(kind&1)?(layer==2?64:32):4;
 for(unsigned i=0;i<65536;i++){sourceVideo[i]=(uint8_t)(i*37+(i>>7)*19);ram[i^1]=(uint8_t)(i*13+7);}
 memcpy(sram,sourceVideo,0x4000);memcpy(sram+0x4000,sourceVideo+0x8000,0x4000);memcpy(sram+0x8000,sourceVideo+0x4000,0x4000);memset(sram+0xc000,0xa5,0x4000);memset(sram+0xf800,0,10);
 WillowNativeBackgroundWriter m;memset(&m,0xa5,sizeof m);willowNativeBackgroundWriterInit(&m);m.rawEpoch=100;m.mapGeneration[0]=20;m.mapGeneration[1]=30;
 REQUIRE(willowNativeBackgroundJournalBaseline(&m.journal,100));modelBegin(&m);targetState(&m);
 for(unsigned i=0;i<2;i++){
  const uint8_t*p=mapPointer(layer,i*stride/4);uint8_t pair[4];
  pw(pair,(word(p)+(variant?0:1)-(layer==3?0xa00:0))&65535);pw(pair+2,word(p+2)^(variant?0:1));patch(0x1e0000+i*stride,pair,4);
 }
 patch(leafPC[kind],leafPatch[kind],8);eventCount[1]=0;hooked=1;
 setreg(17,0x2700);setreg(15,0xff7000);setreg(19,0xff5ffc);setreg(17,(supervisor?0x2000:0)|flags);setreg(15,0xff5ffc);wl(0xff5ffc,0x100000);
 for(unsigned n=0;n<15;n++)setreg(n,0x13790000u+n*0x1021u);
 setreg(0,0xabcd0001);setreg(8,mapBase(layer));setreg(9,0x1e0000);setreg(13,0xffff8000);setreg(14,0xff4000);
 setreg(16,leafPC[kind]);cpu->cycles=0;cpu->refresh=896;unsigned sourceSteps=0,steps=0,fired=0,returned=0,interrupted[21]={0};irq68k(0);
 while(getreg(16)!=0x100000&&steps++<10000){
  unsigned pc=getreg(16);REQUIRE(getreg(4)==0x13794084u&&getreg(13)==0xffff8000);instructionGaps++;
  if(pc<0x1f0000||pc>=0x1f0100){if(sourceSteps==gap&&!fired){for(unsigned r=0;r<21;r++)interrupted[r]=getreg(r);irq68k(6);fired=1;}sourceSteps++;}
  run68k(cpu->cycles+1);
  if(getreg(16)>=0x1f0000&&getreg(16)<0x1f0100)irq68k(0);
  if(pc>=0x1f0000&&pc<0x1f0100&&(getreg(16)<0x1f0000||getreg(16)>=0x1f0100)){
   for(unsigned r=0;r<21;r++)REQUIRE(getreg(r)==interrupted[r]);
   unsigned base=(supervisor?interrupted[15]:interrupted[20])-66;
   for(unsigned r=0;r<15;r++)REQUIRE(rl(base+r*4)==interrupted[r]);
   REQUIRE(rw(base+60)==interrupted[17]&&rl(base+62)==interrupted[16]);returned++;
  }
 }
 irq68k(0);REQUIRE(steps<10000&&getreg(15)==0xff6000&&getreg(17)==((supervisor?0x2000u:0)|(getreg(17)&31)));
 for(unsigned i=0;i<0x5f80;i++)REQUIRE(ram[i^1]==(uint8_t)(i*13+7));
 for(unsigned i=0x6000;i<0x6f80;i++)REQUIRE(ram[i^1]==(uint8_t)(i*13+7));
 if(fired){REQUIRE(returned==1);REQUIRE(lng(sram+0xf800)==0x13794084u&&lng(sram+0xf804)==0xffff8000&&word(sram+0xf808)==1);irqCases++;}
 else REQUIRE(word(sram+0xf808)==0);
 return sourceSteps;
}
static void irqBoundaryCases(void){
 uint8_t savedMetadata[484];unsigned savedRegs[21];
 for(kind=0;kind<4;kind++)for(unsigned supervisor=0;supervisor<2;supervisor++)for(unsigned variant=0;variant<2;variant++){
  count=2;mode=11;unsigned flags=variant?0:31;
  unsigned steps=irqLeafRun(UINT32_MAX,supervisor,flags,variant);
  memcpy(savedMetadata,sram+0xf000,484);memcpy(referenceVideo,sourceVideo,65536);memcpy(referenceSRAM,sram,0xc000);for(unsigned r=0;r<21;r++)savedRegs[r]=getreg(r);
  for(unsigned gap=0;gap<steps;gap++){
   irqLeafRun(gap,supervisor,flags,variant);
   for(unsigned r=0;r<21;r++)REQUIRE(getreg(r)==savedRegs[r]);
   REQUIRE(!memcmp(savedMetadata,sram+0xf000,484));REQUIRE(!memcmp(referenceVideo,sourceVideo,65536));REQUIRE(!memcmp(referenceSRAM,sram,0xc000));
  }
 }
 fprintf(stderr,"IRQS %u hardware level6/RTE injections, every original hook instruction boundary of all four two-pair changed/noop leaves in user/supervisor mode; exact final registers/CCR/USP, map/journal,128-byte task-stack canaries; diagnostic ISR checks implicit D4/A5, not full source IRQ behavior.\n",irqCases);
}
int main(int argc,char**argv){
 if(argc!=4)return 2;bus=(unsigned)strtoul(argv[3],0,0);if(bus>1)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 char file[4096];snprintf(file,sizeof file,"%s/hooks.bin",argv[2]);f=fopen(file,"rb");if(!f)return 2;size_t bytes=fread(rom+0x100100,1,0x10000,f);if(!bytes||!feof(f))return 2;fclose(f);
 pl(rom,0xffff00);pl(rom+4,0x100004);pw(rom+0x100000,0x60fe);
 const uint8_t boot[]={0x13,0xfc,0,1,0,0xa1,0x30,0xf1,0x60,0xf2};memcpy(rom+0x100004,boot,sizeof boot);
 snprintf(file,sizeof file,"%s/irq.bin",argv[2]);f=fopen(file,"rb");if(!f)return 2;bytes=fread(rom+0x1f0000,1,256,f);if(!bytes||!feof(f))return 2;fclose(f);pl(rom+0x78,0x1f0000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 rom[0x1b0]='R';rom[0x1b1]='A';rom[0x1b2]=0xf8;rom[0x1b3]=0x20;pl(rom+0x1b4,0x200000);pl(rom+0x1b8,0x20ffff);
 snprintf(file,sizeof file,"%s/closed.bin",argv[2]);f=fopen(file,"wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core)return 2;
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void*,retro_get_memory_data,(unsigned));FN(void,m68k_set_irq,(unsigned));
 setreg=dlsym(core,"m68k_set_reg");getreg=dlsym(core,"m68k_get_reg");run68k=dlsym(core,"m68k_run");if(!setreg||!getreg||!run68k)return 2;
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 struct retro_game_info game={file,rom,sizeof rom,0};if(!retro_load_game(&game))return 2;
 ram=dlsym(core,"work_ram");cpu=dlsym(core,"m68k");sram=retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);if(!ram||!cpu||!sram)return 2;
 irq68k=m68k_set_irq;retro_run();m68k_set_irq(0);if(getreg(16)!=0x100000)return 2;
 cpu->map[0x91].base=sourceVideo;cpu->map[0x91].read8=sourceRead8;cpu->map[0x91].read16=sourceRead16;cpu->map[0x91].write8=sourceWrite8;cpu->map[0x91].write16=sourceWrite16;
 sramWrite16=cpu->map[0x20].write16;if(!sramWrite16)return 2;cpu->map[0x20].write16=nativeWrite16;
 const unsigned counts[]={1,2,8,16,18,34,42,64,65};
 puts("type,bus,kind,mode,count,ccr,source_master,hook_master,added_master,entries");
 for(unsigned k=0;k<4;k++)for(unsigned c=0;c<9;c++)for(unsigned flags=0;flags<32;flags++)pairCase(k,counts[c],0,flags);
 for(unsigned k=0;k<4;k++)for(unsigned m=1;m<=10;m++)for(unsigned flags=0;flags<32;flags++)pairCase(k,m==2?32:16,m,flags);
 for(unsigned k=0;k<3;k++)for(unsigned layer=1;layer<=3;layer++)for(unsigned b=0;b<2;b++)for(unsigned flags=0;flags<32;flags++)bulkCase(k,layer,(flags&1)?(k==1?4096:1024):1,b,flags);
 lifecycle();routeCases(argv[2]);irqBoundaryCases();
 fprintf(stderr,"GAPS %u checks preserving implicit source D4/A5 at every instruction boundary.\n",instructionGaps);
 fprintf(stderr,"PASS %u closed source-leaf pairs bus%u; ordered source stores, all registers/CCR, whole source maps/RAM/SRAM outside exact testedmetadata/taskrecord, guarded journal and rawgeneration.\n",cases,bus);return 0;
}
