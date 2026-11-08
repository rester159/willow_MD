/* Differential verification of shared services against original instructions. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_actor_pool.h"
#include "../src/willow_native_actor_pool_bus.h"
#include "../src/willow_arcade_actor4_pool_services.h"
#include "../src/willow_arcade_actor10_zero.h"
#include "../src/willow_arcade_actor12_explosion.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
static uint8_t rom[0x200000];
static bool env(unsigned cmd,void *data){
 if(cmd==RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY || cmd==RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY){*(const char**)data="/tmp";return true;}
 if(cmd==RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE){*(bool*)data=false;return true;}
 if(cmd==RETRO_ENVIRONMENT_GET_VARIABLE){((struct retro_variable*)data)->value=NULL;return false;}
 if(cmd==RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION){*(unsigned*)data=0;return true;}
 return cmd==RETRO_ENVIRONMENT_SET_PIXEL_FORMAT || cmd==RETRO_ENVIRONMENT_SET_VARIABLES || cmd==RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS || cmd==RETRO_ENVIRONMENT_SET_CONTROLLER_INFO;
}
static void video(const void*d,unsigned w,unsigned h,size_t p){(void)d;(void)w;(void)h;(void)p;}
static void audio(int16_t l,int16_t r){(void)l;(void)r;}
static size_t batch(const int16_t*d,size_t n){(void)d;return n;}
static void poll(void){}
static int16_t input(unsigned p,unsigned d,unsigned i,unsigned id){(void)p;(void)d;(void)i;(void)id;return 0;}
#define FN(ret,n,args) ret(*n)args=dlsym(core,#n);if(!n){fprintf(stderr,"Missing core symbol %s\n",#n);return 2;}

static void putRamWord(uint8_t*r,unsigned a,uint16_t v){r[a^1]=v>>8;r[(a+1)^1]=v;}
static unsigned ramWord(uint8_t*r,unsigned a){return (r[a^1]<<8)|r[(a+1)^1];}
static WillowActorPool*boundPool;
static uint16_t boundClass;
static unsigned busMode;
static uint16_t busWord(uint16_t at){uint16_t value;if(!willowNativePoolReadWord(boundPool,boundClass,at,&value))abort();return value;}
static uint8_t busByte(uint16_t at){uint8_t value;if(!willowNativePoolReadByte(boundPool,boundClass,at,&value))abort();return value;}
static void busPutWord(uint16_t at,uint16_t value){if(!willowNativePoolWriteWord(boundPool,boundClass,at,value))abort();}
static void busPutByte(uint16_t at,uint8_t value){if(!willowNativePoolWriteByte(boundPool,boundClass,at,value))abort();}
static const WillowVisibilityBus poolBus={busWord,busByte,busPutWord,busPutByte};
static uint8_t*allocateNative(WillowActorPool*p){
 if(!busMode)return willowActorPoolAllocate(p);
 const WillowNativePoolAddresses*a=willowNativePoolAddresses(boundClass);
 uint16_t handle=willowActor18Pop(&poolBus,a->count,a->cursor);
 return handle?willowActorPoolResolve(p,willowActorPoolSpecification(boundClass)->sourceBase,handle):0;
}
static void releaseNative(WillowActorPool*p,uint16_t index){
 uint16_t at=willowActorPoolHandle(p,willowActorPoolSpecification(boundClass)->sourceBase,index);
 if(busMode&&boundClass==4)willowArcadeActor4Release(&poolBus,at);
 else if(busMode&&boundClass==10)willowArcadeActor10Release(&poolBus,at);
 else if(busMode&&boundClass==12)willowArcadeActor12Release(&poolBus,at);
 else willowActorPoolRelease(p,index);
}
int main(int argc,char**argv){
#ifdef WILLOW_TEST_COMPILED_POOL_BUS
 if(argc!=4)return 2;
 unsigned targetEntry=0;FILE*symbols=fopen(argv[3],"r");if(!symbols)return 2;
 char symbolLine[512],symbolName[256],symbolKind;unsigned symbolAddress;
 while(fgets(symbolLine,sizeof symbolLine,symbols))if(sscanf(symbolLine,"%x %c %255s",&symbolAddress,&symbolKind,symbolName)==3&&!strcmp(symbolName,"nativePoolProbe"))targetEntry=symbolAddress;
 fclose(symbols);if(targetEntry<0x110000||targetEntry>=0x120000)return 2;
 FILE*targetFile=fopen(argv[2],"rb");if(!targetFile)return 2;
 size_t targetBytes=fread(rom+0x110000,1,0x10000,targetFile);int extra=fgetc(targetFile);fclose(targetFile);
 if(!targetBytes||extra!=EOF)return 2;
 unsigned targetChecks=0;
#else
 if(argc!=2)return 2;
#endif
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f || fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 rom[0x100000]=0x60;rom[0x100001]=0xfe;
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/actor-pool-oracle.bin","wb");if(!f || fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/actor-pool-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game)){fputs("Oracle ROM load failed\n",stderr);return 2;}
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;

#ifdef WILLOW_TEST_COMPILED_POOL_BUS
#define TARGET_RUN(operation,index,expected) do { \
 unsigned savedRegs[18];for(unsigned reg=0;reg<18;reg++)savedRegs[reg]=m68k_get_reg(reg); \
 uint8_t sourceSnapshot[32768];memcpy(sourceSnapshot,ram+0x8000,sizeof sourceSnapshot); \
 putRamWord(ram,0x2010,boundClass);putRamWord(ram,0x2012,operation);putRamWord(ram,0x2014,index); \
 putRamWord(ram,0x6000,0x10);putRamWord(ram,0x6002,0); \
 m68k_set_reg(17,0x2700);m68k_set_reg(15,0xff6000);m68k_set_reg(16,targetEntry);for(unsigned run=0;run<8&&m68k_get_reg(16)!=0x100000;run++)retro_run(); \
 if(m68k_get_reg(16)!=0x100000||m68k_get_reg(15)!=0xff6004||ramWord(ram,0x2018)!=1||ramWord(ram,0x2016)!=(expected)) {fprintf(stderr,"Compiled pool call failed class %u op %u PC %x result %x status %u\n",boundClass,(unsigned)(operation),m68k_get_reg(16),ramWord(ram,0x2016),ramWord(ram,0x2018));return 1;} \
 if(memcmp(sourceSnapshot,ram+0x8000,sizeof sourceSnapshot))return 1; \
 if(ramWord(ram,0x2000)!=0xff||ramWord(ram,0x2002)!=0x3000||ramWord(ram,0x2004)!=0xff||ramWord(ram,0x2006)!=0x2100||     ram[0x2008^1]!=boundPool->capacity||ram[0x2009^1]!=boundPool->stride||ram[0x200a^1]!=boundPool->cursor||ram[0x200b^1]!=boundPool->available)return 1; \
 for(unsigned n=0;n<(unsigned)boundPool->capacity*boundPool->stride;n++)if(ram[(0x3000+n)^1]!=boundPool->records[n])return 1; \
 for(unsigned n=0;n<boundPool->capacity;n++)if(ram[(0x2100+n)^1]!=boundPool->indices[n])return 1; \
 for(unsigned n=0;n<256;n++)if(ram[(0x2f00+n)^1]!=0xa5||ram[(0x4900+n)^1]!=0xa5||ram[(0x5e00+n)^1]!=0xa5)return 1; \
 m68k_set_reg(17,savedRegs[17]);for(unsigned reg=0;reg<17;reg++)m68k_set_reg(reg,savedRegs[reg]);targetChecks++; \
}while(0)
#define TARGET_SERVICE(at) do { \
 unsigned op=(at)==allocations[kind]?1:2; \
 unsigned slot=op==2?((m68k_get_reg(14)&65535)-base)/stride:0; \
 unsigned expected=op==1&&m68k_get_reg(0)?m68k_get_reg(12)&65535:0; \
 TARGET_RUN(op,slot,expected); \
}while(0)
#else
#define TARGET_SERVICE(at) ((void)0)
#endif

#define RUN(at,d0) do { \
 putRamWord(ram,0xff00,0x0010);putRamWord(ram,0xff02,0); \
 m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00); \
 m68k_set_reg(0,(d0));m68k_set_reg(16,(at));retro_run(); \
 if(m68k_get_reg(16)!=0x100000)return 1; \
 TARGET_SERVICE(at); \
} while(0)
 const unsigned classes[]={2,4,6,8,10,12,14,16,18};
 const unsigned bases[]={0x98c6,0x9dc6,0xb6c6,0xc2c6,0xc7c6,0xccc6,0xdbc6,0xeac6,0xf490};
 /* LEA d16(A5),A6 operands in the original pool initializers. */
 const unsigned baseOperands[]={0x2944,0x265e,0x26c6,0x26f8,0x272a,0x275c,0x28e0,0x278e,0x2692};
 for(unsigned n=0;n<9;n++)if((uint16_t)(0x8000+willowMotionWord(rom+baseOperands[n]))!=bases[n])return 2;

 const unsigned capacities[]={20,50,48,20,20,60,60,20,8};
 const unsigned capacityOperands[]={0x2948,0x2662,0x26ca,0x26fc,0x272e,0x2760,0x28e4,0x2792,0x2696};
 const unsigned strideOperands[]={0x2968,0x2682,0x26e8,0x271a,0x274c,0x277e,0x2904,0x27b0,0x26b6};
 for(unsigned n=0;n<9;n++){
  const WillowActorPoolSpec*s=willowActorPoolSpecification(classes[n]);
  if(s->capacity!=willowMotionWord(rom+capacityOperands[n]) || s->stride!=willowMotionWord(rom+strideOperands[n]))return 2;
 }
 for(unsigned id=0;id<256;id++)if(!!willowActorPoolSpecification(id)!=!!(id && id<=18 && !(id&1)))return 1;
 const unsigned allocations[]={0x29d0,0x2a1a,0x2ab6,0x2b04,0x2b4e,0x2b9c,0x2bea,0x2c38,0x2a68};
 const unsigned releases[]={0x29ec,0x2a36,0x2ad2,0x2b20,0x2b6a,0x2bb8,0x2c06,0x2c54,0x2a84};
 const unsigned counts[]={0x97fe,0x9801,0x9804,0x9807,0x980a,0x980d,0x9810,0x9813,0xf48c};
 const unsigned cursors[]={0x9816,0x981c,0x9822,0x9828,0x982e,0x9834,0x983a,0x9840,0xf486};
 uint8_t records[6400];WillowActorPoolIndex indices[60];uint32_t random=0x57494c4c;unsigned checks=0;
 for(unsigned kind=0;kind<9;kind++)for(unsigned sample=0;sample<128;sample++){
  unsigned capacity=capacities[kind],stride=(classes[kind]==4 || classes[kind]==18)?128:64,base=bases[kind];
  const WillowActorPoolSpec*spec=willowActorPoolSpecification(classes[kind]);
  if(!spec || spec->sourceBase!=base || spec->capacity!=capacity || spec->stride!=stride)return 1;
  const WillowNativePoolAddresses*view=willowNativePoolAddresses(classes[kind]);
  unsigned stackBase=view->end-capacity*2;
  if(view->count!=counts[kind]||view->cursor!=cursors[kind])return 1;
  WillowActorPool pool;willowActorPoolInit(&pool,records,indices,capacity,stride,classes[kind]);
  boundPool=&pool;boundClass=classes[kind];busMode=sample&1;
#ifdef WILLOW_TEST_COMPILED_POOL_BUS
  for(unsigned n=0;n<256;n++)ram[(0x2f00+n)^1]=ram[(0x4900+n)^1]=ram[(0x5e00+n)^1]=0xa5;
  TARGET_RUN(0,0,0);
#endif
  if(!sample){
   for(unsigned handle=0;handle<65536;handle++){
    uint8_t*r=willowActorPoolResolve(&pool,base,handle);
    unsigned valid=handle>=base && handle<base+capacity*stride && (handle-base)%stride==0;
    if(!!r!=valid || (r && r!=records+(handle-base)))return 1;
   }
   for(unsigned index=0;index<capacity;index++){
    unsigned handle=willowActorPoolHandle(&pool,base,index);
    if(handle!=base+index*stride || willowActorPoolResolve(&pool,base,handle)!=records+index*stride)return 1;
   }
   if(willowActorPoolHandle(&pool,base,capacity) || willowActorPoolHandle(&pool,65535,1))return 1;
  }
  for(unsigned n=0;n<capacity;n++){
   for(unsigned j=0;j<stride;j++){random=random*1664525u+1013904223u;if(!willowNativePoolWriteByte(&pool,classes[kind],base+n*stride+j,random>>24))return 1;}
   records[n*stride+30]=classes[kind];
   for(unsigned j=0;j<stride;j++)ram[(base+n*stride+j)^1]=records[n*stride+j];
   if(!willowNativePoolWriteWord(&pool,classes[kind],stackBase+n*2,base+indices[n]*stride))return 1;
   putRamWord(ram,stackBase+n*2,base+indices[n]*stride);
  }
  ram[counts[kind]^1]=capacity;putRamWord(ram,cursors[kind],stackBase);
  if(!willowNativePoolWriteByte(&pool,classes[kind],counts[kind],capacity)||
     !willowNativePoolWriteWord(&pool,classes[kind],cursors[kind],stackBase))return 1;
#ifdef WILLOW_TEST_COMPILED_POOL_BUS
  for(unsigned n=0;n<capacity*stride;n++)ram[(0x3000+n)^1]=records[n];
#endif
  unsigned pops=1+(sample*17)%capacity;
  for(unsigned n=0;n<pops;n++){
   uint8_t*r=allocateNative(&pool);RUN(allocations[kind],0xdeadbeef);
   if(!r || (m68k_get_reg(12)&65535)!=base+(unsigned)(r-records) || m68k_get_reg(0)!=1)return 1;
   if(ram[counts[kind]^1]!=pool.available || ramWord(ram,cursors[kind])!=stackBase+pool.cursor*2)return 1;
  }
  unsigned index=capacity-1-(sample%pops);
  releaseNative(&pool,index);m68k_set_reg(14,0xff0000+base+index*stride);RUN(releases[kind],0xdeadbeef);
  for(unsigned n=0;n<capacity*stride;n++)if(records[n]!=ram[(base+n)^1]){fprintf(stderr,"Pool release mismatch class %u case %u byte %u\n",classes[kind],sample,n);return 1;}
  if(ram[counts[kind]^1]!=pool.available || ramWord(ram,cursors[kind])!=stackBase+pool.cursor*2)return 1;
  for(unsigned n=pool.cursor;n<capacity;n++)if(ramWord(ram,stackBase+n*2)!=base+pool.indices[n]*stride)return 1;
  uint8_t*r=allocateNative(&pool);RUN(allocations[kind],0);
  if(r!=records+index*stride || (m68k_get_reg(12)&65535)!=base+index*stride)return 1;
  while(pool.available){
   r=allocateNative(&pool);RUN(allocations[kind],0);
   if((m68k_get_reg(12)&65535)!=base+(unsigned)(r-records))return 1;
  }
  if(allocateNative(&pool))return 1;RUN(allocations[kind],0xdeadbeef);
  if(m68k_get_reg(0) || ram[counts[kind]^1] || ramWord(ram,cursors[kind])!=stackBase+capacity*2)return 1;
  for(unsigned at=base;at<base+capacity*stride;at++){
   uint8_t value;if(!willowNativePoolReadByte(&pool,classes[kind],at,&value)||value!=ram[at^1])return 1;
  }
  for(unsigned at=stackBase;at<view->end;at+=2){
   uint16_t value;if(!willowNativePoolReadWord(&pool,classes[kind],at,&value)||value!=ramWord(ram,at))return 1;
  }
  uint8_t available;uint16_t cursor;
  if(!willowNativePoolReadByte(&pool,classes[kind],counts[kind],&available)||available!=ram[counts[kind]^1]||
     !willowNativePoolReadWord(&pool,classes[kind],cursors[kind],&cursor)||cursor!=ramWord(ram,cursors[kind]))return 1;
  uint8_t savedRecords[6400],savedIndices[60];WillowActorPool savedPool=pool;
  memcpy(savedRecords,records,capacity*stride);memcpy(savedIndices,indices,capacity);
  if(willowNativePoolWriteByte(&pool,classes[kind],counts[kind],capacity+1)||
     willowNativePoolWriteWord(&pool,classes[kind],cursors[kind],stackBase+1)||
     willowNativePoolWriteWord(&pool,classes[kind],stackBase,base+1)||
     willowNativePoolWriteWord(&pool,classes[kind],base+1,0xffff)||
     willowNativePoolWriteByte(&pool,classes[kind],cursors[kind],0))return 1;
  if(memcmp(savedRecords,records,capacity*stride)||memcmp(savedIndices,indices,capacity)||
     savedPool.records!=pool.records||savedPool.indices!=pool.indices||
     savedPool.capacity!=pool.capacity||savedPool.stride!=pool.stride||
     savedPool.cursor!=pool.cursor||savedPool.available!=pool.available)return 1;
  uint16_t untouched=0xbeef;
  if(willowNativePoolReadWord(&pool,classes[kind],base-2,&untouched)||untouched!=0xbeef||
     willowNativePoolReadWord(&pool,classes[kind],base+1,&untouched)||untouched!=0xbeef||
     willowNativePoolRecordSpan(&pool,classes[kind],base+capacity*stride-1,2)||
     willowNativePoolRecordSpan(&pool,classes[kind],base,0)||
     willowNativePoolRecordSpan(0,classes[kind],base,1)||
     willowNativePoolRecordSpan(&pool,0,base,1))return 1;
  if(!sample)for(unsigned at=0;at<65536;at++){
   uint8_t*span=willowNativePoolRecordSpan(&pool,classes[kind],at,1);
   unsigned valid=at>=base&&at<base+capacity*stride;
   if(!!span!=valid||(span&&span!=records+(at-base)))return 1;
   unsigned owns=valid||(at>=stackBase&&at<view->end)||at==counts[kind]||at==cursors[kind]||at==cursors[kind]+1;
   if((willowNativePoolClassAt(at)==classes[kind])!=owns)return 1;
  }
#ifdef WILLOW_TEST_COMPILED_POOL_BUS
  TARGET_RUN(3,0,0);
#endif
  checks++;
 }
#ifdef WILLOW_TEST_COMPILED_POOL_BUS
 printf("Compiled native pool bus: %u initialization/allocation/release/exhaustion boundaries match host and original execution; source RAM, record bounds and stack guards preserved\n",targetChecks);
#endif
 puts("Translated pop services and class-4/10/12 release services also pass through the native source-address bus");
 puts("Native source-address pool bus: records, original handles, counts/cursors and rejected-write atomicity match all nine classes");
 puts("Actor handle adapter: all 65536 handles across nine original pool bases, plus checked index round trips");
 printf("Native actor pools: %u nine-class allocation/release/reuse/exhaustion cases match original instructions, including class-2/8 preserved bytes\n",checks);
 return 0;
}
