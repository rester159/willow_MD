/* Independent lifecycle assertions plus actual68000/host differential.
 * Host seeding is restricted to this closed API fixture, never a live route. */
#define WILLOW_CLOSED_HELPERS_ONLY 1
#include "test_native_render_cache_closed.c"
#include "../src/willow_native_far_cache.h"
#include "far_stream_imports.h"
#include "far-cache-fixture.h"
#undef CHECK
#define CHECK(x) do{if(!(x)){fprintf(stderr,"Far cache failure case%u line%u: %s\n",cases,__LINE__,#x);exit(1);}}while(0)
static unsigned cases,checks,uploads,refusals,duplicateCases,boundaryCases;
static unsigned hostCallbacks,hostViolation,failAfter,failCall;
static unsigned nativeData,hostData,hostControls;
static uint8_t initialVRAM[65536],expectedVRAM[65536],beforeSRAM[65536];
static uint8_t protectedCRAM[128],protectedVSRAM[128],protectedRegs[32];
static void*coreHandle;
static WillowNativeFarCache state,initialState;
static WillowNativeFarResources resources;
static WillowNativeFarContext context={101,201,301,{132,768,34,1792}};
static uint16_t resultWord,writtenWords,words[2048],nearWords[2048];
static FILE*eventFile,*resolvedFile;
static void encode(uint8_t*p,const WillowNativeFarCache*s)
{
 for(unsigned n=0;n<165;n++)pw(p+n*2,s->owner[n]);
 for(unsigned n=0;n<6;n++){pl(p+330+n*4,s->displayedUse[n]);pl(p+354+n*4,s->preparedUse[n]);}
 memcpy(p+378,s->slots,16);pw(p+394,s->key);pw(p+396,s->requested);pw(p+398,s->completed);pw(p+400,s->phase);
 pl(p+402,s->residencyEpoch);pl(p+406,s->sourceEpoch);pl(p+410,s->paletteEpoch);pl(p+414,s->preparationGeneration);
 for(unsigned n=0;n<4;n++)pw(p+418+n*2,s->view[n]);
 pw(p+426,s->cursor);pl(p+428,s->preparedResidencyEpoch);pw(p+432,s->missing);pw(p+434,s->reserved);
}
static unsigned invoke(unsigned pc,unsigned seed)
{
 unsigned saved[15],stack=0xff6000;
 setreg(17,0x2700|(seed&31));setreg(15,stack-4);wl(stack-4,0x100000);
 for(unsigned n=0;n<15;n++){saved[n]=0x13790000+n*0x1021+seed;setreg(n,saved[n]);}
 setreg(16,pc);cpu->cycles=0;cpu->refresh=896;unsigned steps=0;
 while(getreg(16)!=0x100000&&steps++<3000000)run68k(cpu->cycles+1);
 CHECK(steps<3000000&&getreg(15)==stack);
 for(unsigned n=2;n<15;n++)if(n!=8&&n!=9)CHECK(getreg(n)==saved[n]);
 return cpu->cycles;
}
static void vdpObserver(unsigned at,unsigned value)
{
 if((at&0xfffffc)==0xc00000)nativeData++;
 else CHECK((at&0xfffffc)==0xc00004);
 observeVDP(at,value);
}
static uint16_t hostUpload(void*ctx,uint16_t tile,const uint8_t*pattern)
{
 unsigned slot=willowFarSlot(tile),call=++hostCallbacks;
 if(ctx!=&state||slot>=165||state.owner[slot]!=65535||state.phase!=3||
    pattern<resources.patterns||pattern+32>resources.patterns+resources.patternCount*32||
    (pattern-resources.patterns)%32)hostViolation=1;
 uint16_t unresolved=0;
 if(willowFarCacheResolve(&state,&resources,0,0,1,&unresolved)||unresolved!=65535||
    willowFarCacheBindPrepared(&state,&context)||willowFarCacheRequest(&state,&resources,0,1))hostViolation=1;
 unsigned count=failCall==call?failAfter:16;
 CHECK(count<=16);if(count)hostControls+=2;
 memcpy(expectedVRAM+tile*32,pattern,count*2);hostData+=count;
 return failCall!=call;
}
static unsigned operation(unsigned op,unsigned a,unsigned b,unsigned c)
{
 uint8_t expectedState[436];encode(expectedState,&state);
 memset(sram,0xa5,65536);memset(ram,0xa5,65536);
 memcpy(sram+0xf000,expectedState,436);
 for(unsigned n=0;n<2048;n++)pw(sram+0xe000+n*2,words[n]);
 for(unsigned n=0;n<2048;n++)pw(sram+0xc000+n*2,nearWords[n]);
 memset(sram+0xf200,0,0x60);pw(sram+0xf200,op);pw(sram+0xf204,a);pw(sram+0xf206,b);pw(sram+0xf208,c);
 pw(sram+0xf214,failAfter);pw(sram+0xf216,failCall);pl(sram+0xf230,0xcafebabe);
 pl(sram+0xf240,context.sourceEpoch);pl(sram+0xf244,context.paletteEpoch);pl(sram+0xf248,context.generation);
 for(unsigned n=0;n<4;n++)pw(sram+0xf24c+n*2,context.view[n]);
 memcpy(beforeSRAM,sram,65536);
 unsigned result=0;uint16_t out=0xbeef,written=0xbeef;
 hostCallbacks=hostViolation=hostData=hostControls=0;
 switch(op){
 case 0:result=willowFarCacheInit(&state,&resources);break;
 case 1:result=willowFarCacheResolve(&state,&resources,a,b,c,&out);break;
 case 2:result=willowFarCacheRequest(&state,&resources,a,b);break;
 case 3:result=willowFarCacheUpload(&state,&resources,a,hostUpload,&state,&written);break;
 case 4:result=willowFarCachePreparedUse(&state,words,a);break;
 case 5:result=willowFarCacheBindPrepared(&state,&context);break;
 case 6:result=willowFarCachePreparedCurrent(&state,&context);break;
 case 7:result=willowFarCachePublished(&state,&context,a);break;
 case 8:result=willowFarResourcesValid(&resources);break;
 case 9:result=willowFarCacheValid(&state);break;
 case 10:result=willowFarCachePreparedPlanes(&state,c&1?0:words,a,c&2?0:nearWords,b);break;
 default:CHECK(0);
 }
 encode(expectedState,&state);memcpy(beforeSRAM+0xf000,expectedState,436);
 pw(beforeSRAM+0xf202,result);pw(beforeSRAM+0xf20a,out);pw(beforeSRAM+0xf20e,written);
 pw(beforeSRAM+0xf210,hostCallbacks);pw(beforeSRAM+0xf212,hostViolation);
 vdpDataWords=vdpControlWords=nativeData=0;
 unsigned cycles=invoke(API_ENTRY,cases);
 CHECK(!memcmp(sram,beforeSRAM,65536));CHECK(!hostViolation);
 CHECK(vdpDataWords==hostData&&nativeData==hostData&&vdpControlWords==hostControls);
 for(unsigned n=0;n<65536;n++){
  CHECK(vram[n^1]==expectedVRAM[n]);
  if(n<0x5800||n>=0x6000)CHECK(ram[n^1]==0xa5);
 }
 uint8_t*cram=dlsym(coreHandle,"cram"),*vsram=dlsym(coreHandle,"vsram"),*regs=dlsym(coreHandle,"reg");
 CHECK(cram&&vsram&&regs);
 CHECK(!memcmp(cram,protectedCRAM,128)&&!memcmp(vsram,protectedVSRAM,128)&&!memcmp(regs,protectedRegs,32));
 checks+=65536*3+128*2+32;uploads+=hostCallbacks;if(!result)refusals++;
 fprintf(eventFile,"%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n",cases,op,a,b,c,result,out,written,hostCallbacks,hostData,cycles);
 cases++;resultWord=out;writtenWords=written;return result;
}
static void reset(void)
{
 state=initialState;failAfter=failCall=0;memset(words,0,sizeof words);memset(nearWords,0,sizeof nearWords);
 memcpy(expectedVRAM,initialVRAM,65536);for(unsigned n=0;n<65536;n++)vram[n^1]=expectedVRAM[n];
}
static unsigned pattern(unsigned ref){return word(resources.refs+ref*4);}
static unsigned hint(unsigned ref){return word(resources.refs+ref*4+2);}
static unsigned attrs(unsigned ref){return ((resources.attributes[ref/2]>>(ref%2?0:4))&15)*2048;}
static unsigned key(unsigned code,unsigned attr)
{
 const uint8_t*p=rom+WILLOW_BG_FAR_STREAM_CATALOG_PAIRS_ADDRESS;
 for(unsigned n=0;n<resources.referenceCount/16;n++)if(lng(p+n*6)==((code<<16)|attr))return n;
 CHECK(0);return 0;
}
static unsigned refFor(unsigned owner)
{for(unsigned n=0;n<resources.referenceCount;n++)if(pattern(n)==owner)return n;CHECK(0);return 0;}
static void checkResolved(unsigned ref,unsigned status)
{
 CHECK(operation(1,ref,hint(ref),0)==status);
 if(status){unsigned p=pattern(ref),tile=resultWord&2047;CHECK((resultWord&0xf800)==(p?attrs(ref):0));CHECK(!memcmp(expectedVRAM+tile*32,resources.patterns+p*32,32));}
 else CHECK(resultWord==65535);
}
static void drain(unsigned budget)
{unsigned limit=0;while(state.phase==2){CHECK(++limit<18);CHECK(operation(3,budget,0,0));}CHECK(state.phase==1);}
static void planeUnion(void)
{
 /* Cover every 16-bit tile/attribute combination on the actual CPU. */
 for(unsigned base=0;base<65536;base+=2048){
  reset();for(unsigned n=0;n<2048;n++){words[n]=base+n;nearWords[n]=65535-(base+n);}
  CHECK(operation(4,2048,0,0));
  CHECK(operation(10,2048,2048,0));
 }

 /* Non-adjacent complete planes; every physical pool slot occurs in both
  * argument orders, including palette/flip bits and distant final cells. */
 for(unsigned swap=0;swap<2;swap++)for(unsigned slot=0;slot<165;slot++){
  reset();unsigned other=(slot+83)%165;
  words[2047]=willowFarTile(slot)|0xf800;
  nearWords[0]=willowFarTile(other)|0x2800;
  nearWords[2047]=words[2047];
  if(swap){uint16_t t=words[2047];words[2047]=nearWords[0];nearWords[0]=t;}
  CHECK(operation(10,2048,2048,0));
  for(unsigned n=0;n<165;n++)CHECK(!!(state.preparedUse[n>>5]&(1u<<(n&31)))==(n==slot||n==other));
  CHECK(!state.missing);CHECK(operation(5,0,0,0));CHECK(operation(7,1,0,0));
  words[2047]=nearWords[0]=nearWords[2047]=0;
  CHECK(operation(10,2048,2048,0));CHECK(operation(5,0,0,0));
  CHECK(operation(7,0,0,0));
  CHECK(state.displayedUse[slot>>5]&(1u<<(slot&31)));
  CHECK(state.displayedUse[other>>5]&(1u<<(other&31)));
  CHECK(operation(7,1,0,0));for(unsigned n=0;n<6;n++)CHECK(!state.displayedUse[n]);
 }
 /* The only unprotected slot must be selected regardless of which plane
  * holds each other slot. A full two-plane union must refuse unchanged. */
 for(unsigned swap=0;swap<2;swap++)for(unsigned freeSlot=0;freeSlot<165;freeSlot++){
  reset();
  for(unsigned slot=0;slot<165;slot++)if(slot!=freeSlot)
   ((slot+swap)&1?nearWords:words)[slot]=willowFarTile(slot)|0x7800;
  CHECK(operation(10,2048,2048,0));
  CHECK(operation(2,key(0xc39,0x26),1,0)==2);CHECK(state.slots[0]==freeSlot);
  drain(1);checkResolved(key(0xc39,0x26)*16,1);
 }
 reset();for(unsigned slot=0;slot<165;slot++)
  (slot&1?nearWords:words)[slot]=willowFarTile(slot);
 CHECK(operation(10,2048,2048,0));
 WillowNativeFarCache full=state;
 CHECK(!operation(2,key(0xc39,0x26),1,0));CHECK(!memcmp(&full,&state,sizeof state));
 for(unsigned plane=0;plane<2;plane++){
  reset();(plane?nearWords:words)[2047]=65535;
  CHECK(operation(10,2048,2048,0));CHECK(state.missing);CHECK(!operation(5,0,0,0));
 }
 for(unsigned invalid=0;invalid<5;invalid++){
  reset();state.preparedUse[0]=123;state.preparedResidencyEpoch=99;
  WillowNativeFarCache before=state;
  CHECK(!operation(10,invalid==0?0:2048,invalid==1?0:2048,invalid>=2?invalid-1:0));
  CHECK(!memcmp(&before,&state,sizeof state));
 }
}
static void lifecycle(void)
{
 reset();CHECK(operation(0,0,0,0));initialState=state;CHECK(operation(8,0,0,0));
 for(unsigned ref=0;ref<resources.referenceCount;ref++){
  unsigned p=pattern(ref),fixed=word(resources.fixed+p*2),present=!p||fixed!=65535;
  for(unsigned n=0;n<165;n++)if(state.owner[n]==p)present=1;
  checkResolved(ref,present?1:0);
 }
 /* Every original far key, all subtiles including both0C39 local flips.
  * Native writes are checked against all64KiB, not just the upload span. */
 for(unsigned k=0;k<resources.referenceCount/16;k++){
  reset();unsigned unique=0;uint16_t missing[16];
  for(unsigned n=0;n<16;n++){
   unsigned p=pattern(k*16+n),found=!p||word(resources.fixed+p*2)!=65535;
   for(unsigned j=0;j<165;j++)if(state.owner[j]==p)found=1;
   for(unsigned j=0;j<unique;j++)if(missing[j]==p)found=1;
   if(!found)missing[unique++]=p;
  }
  unsigned requested=operation(2,k,65535,0);CHECK(requested==(unique?2:1));
  if(unique){CHECK(!operation(5,0,0,0));unsigned before=uploads;drain(1+(k%16));CHECK(uploads-before==unique);}
  for(unsigned n=0;n<16;n++){
   checkResolved(k*16+n,1);words[n]=resultWord;
   uint8_t captured[34];pw(captured,word(sram+0xf20a));
   for(unsigned j=0;j<32;j++)captured[j+2]=vram[(((resultWord&2047)*32+j)^1)];
   CHECK(fwrite(captured,1,34,resolvedFile)==34);
  }
  CHECK(operation(4,16,0,0));CHECK(operation(5,0,0,0));CHECK(operation(6,0,0,0));CHECK(operation(7,1,0,0));
  for(unsigned n=0;n<6;n++)CHECK(state.displayedUse[n]==state.preparedUse[n]);
 }
 unsigned k=key(0xc39,0x26),ref=k*16;
 CHECK(pattern(ref)==3201);
 /* Queued victim cannot be returned as the old canonical pattern, even
  * though its old bytes still exist until the first upload begins. */
 reset();unsigned old=state.owner[0],oldRef=refFor(old),oldWord=hint(oldRef);
 CHECK(operation(2,k,1,0)==2);CHECK(state.slots[0]==0);
 for(unsigned allow=0;allow<2;allow++)for(unsigned h=0;h<2;h++){
  CHECK(operation(1,oldRef,h?65535:oldWord,allow)==0);CHECK(resultWord==65535);
 }
 CHECK(operation(1,ref,hint(ref),1)==2);CHECK((resultWord&2047)==759);drain(16);
 checkResolved(oldRef,0);checkResolved(ref,1);CHECK(state.owner[0]==3201);
 /* Every physical pool boundary and every partially written prefix. */
 for(unsigned slot=0;slot<165;slot++){
  reset();for(unsigned n=0;n<165;n++)if(n!=slot)state.displayedUse[n>>5]|=1u<<(n&31);
  CHECK(operation(2,k,1,0)==2);CHECK(state.slots[0]==slot);drain(1);checkResolved(ref,1);
  CHECK((resultWord&2047)==(slot<117?759+slot:1481+slot-117));boundaryCases++;
 }
 for(unsigned partial=0;partial<=16;partial++){
  reset();CHECK(operation(2,k,1,0)==2);failAfter=partial;failCall=1;
  CHECK(!operation(3,1,0,0));CHECK(!writtenWords&&state.owner[0]==65535&&state.phase==2);
  CHECK(!operation(5,0,0,0));checkResolved(ref,0);checkResolved(oldRef,0);
  failCall=0;drain(1);checkResolved(ref,1);
 }
 /* A failed later callback must keep earlier successful owners committed. */
 for(unsigned partial=0;partial<=16;partial++){
  reset();CHECK(operation(2,k,65535,0)==2);unsigned firstSlot=state.slots[0];
  failAfter=partial;failCall=2;CHECK(!operation(3,16,0,0));CHECK(writtenWords==1);
  CHECK(state.owner[firstSlot]==pattern(ref));checkResolved(ref,1);
  unsigned invalidOwners=0;for(unsigned n=0;n<165;n++)invalidOwners+=state.owner[n]==65535;
  CHECK(invalidOwners==1);CHECK(!operation(5,0,0,0));failCall=0;drain(16);
  for(unsigned n=0;n<16;n++)checkResolved(ref+n,1);
 }
 /* Full protection must reject with byte-identical state and VRAM. */
 reset();for(unsigned n=0;n<165;n++)state.displayedUse[n>>5]|=1u<<(n&31);
 WillowNativeFarCache before=state;CHECK(!operation(2,k,1,0));CHECK(!memcmp(&before,&state,sizeof state));
 reset();for(unsigned n=0;n<165;n++)state.preparedUse[n>>5]|=1u<<(n&31);
 before=state;CHECK(!operation(2,k,1,0));CHECK(!memcmp(&before,&state,sizeof state));
 /* Requested duplicate canonical IDs consume exactly one slot/upload. */
 for(unsigned pair=0;pair<resources.referenceCount/16;pair++){
  unsigned a=16,b=16;
  for(unsigned n=0;n<16&&a==16;n++)for(unsigned j=n+1;j<16;j++)if(pattern(pair*16+n)&&pattern(pair*16+n)==pattern(pair*16+j)&&word(resources.fixed+pattern(pair*16+n)*2)==65535){a=n;b=j;break;}
  if(a==16)continue;
  reset();unsigned p=pattern(pair*16+a);for(unsigned n=0;n<165;n++)if(state.owner[n]==p)state.owner[n]=65535;
  CHECK(operation(2,pair,(1u<<a)|(1u<<b),0)==2);CHECK(state.slots[a]==state.slots[b]);unsigned start=uploads;drain(1);CHECK(uploads==start+1);duplicateCases++;
 }
 CHECK(duplicateCases);
 /* Literal original0A0C/0018 duplicate pattern1430 at the last valid epoch. */
 reset();unsigned duplicateKey=key(0xa0c,0x18);CHECK(duplicateKey==2);
 for(unsigned n=0;n<16;n+=4)CHECK(pattern(duplicateKey*16+n)==1430);
 state.residencyEpoch=UINT32_MAX-2;CHECK(operation(2,duplicateKey,0x1111,0)==2);
 unsigned start=uploads;CHECK(operation(3,1,0,0));CHECK(uploads==start+1&&writtenWords==1);
 CHECK(state.residencyEpoch==UINT32_MAX-1&&state.phase==1);
 for(unsigned n=0;n<16;n+=4)checkResolved(duplicateKey*16+n,1);
 /* A further eviction would consume the invalid terminal epoch. */
 before=state;CHECK(!operation(2,k,1,0));CHECK(!memcmp(&before,&state,sizeof state));
 /* Partial publication protects the union; only complete physical-map and
  * scroll drainage permits retiring the old displayed protection. */
 reset();state.displayedUse[0]=1;words[0]=willowFarTile(164);CHECK(operation(4,1,0,0));
 CHECK(operation(5,0,0,0));CHECK(operation(7,0,0,0));CHECK(state.displayedUse[0]==1&&(state.displayedUse[5]&16));
 CHECK(operation(7,1,0,0));CHECK(!state.displayedUse[0]&&state.displayedUse[5]==16);
 for(unsigned field=0;field<7;field++){
  WillowNativeFarContext saved=context;
  if(field==0)context.sourceEpoch++;else if(field==1)context.paletteEpoch++;else if(field==2)context.generation++;else context.view[field-3]++;
  before=state;CHECK(!operation(6,0,0,0));CHECK(!operation(7,1,0,0));CHECK(!memcmp(&before,&state,sizeof state));context=saved;
 }
 CHECK(operation(6,0,0,0));
 state.residencyEpoch++;CHECK(!operation(6,0,0,0));CHECK(operation(5,0,0,0));CHECK(operation(6,0,0,0));
 words[0]=65535;CHECK(operation(4,1,0,0));CHECK(!operation(5,0,0,0));CHECK(!operation(6,0,0,0));
 for(unsigned field=0;field<3;field++){
  reset();WillowNativeFarContext saved=context;
  if(!field)context.sourceEpoch=0;else if(field==1)context.paletteEpoch=0;else context.generation=0;
  before=state;CHECK(!operation(5,0,0,0));CHECK(!memcmp(&before,&state,sizeof state));context=saved;
 }
 for(unsigned invalid=0;invalid<6;invalid++){
  reset();if(invalid==0)state.phase=0;else if(invalid==1)state.phase=3;else if(invalid==2)state.residencyEpoch=0;else if(invalid==3)state.residencyEpoch=UINT32_MAX;else if(invalid==4)state.reserved=1;else state.displayedUse[5]=32;
  CHECK(!operation(9,0,0,0));CHECK(!operation(2,k,1,0));CHECK(!operation(5,0,0,0));
 }
}
static void loadAt(const char*path,unsigned at)
{FILE*f=fopen(path,"rb");CHECK(f);size_t n=fread(rom+at,1,sizeof rom-at,f);CHECK(n&&feof(f));fclose(f);}
int main(int argc,char**argv)
{
 if(argc!=7)return 2;
 loadAt(argv[2],0x102000);loadAt(argv[3],WILLOW_BG_FAR_STREAM_ORIGIN);loadAt(argv[4],WILLOW_BG_FAR_STREAM_CATALOG_ORIGIN);
 resources=(WillowNativeFarResources){rom+WILLOW_BG_FAR_STREAM_PATTERNS_ADDRESS,rom+WILLOW_BG_FAR_STREAM_ATTRIBUTES_ADDRESS,rom+WILLOW_BG_FAR_STREAM_POOL_ADDRESS,rom+WILLOW_BG_FAR_STREAM_FIXED_MAP_ADDRESS,rom+WILLOW_BG_FAR_STREAM_CATALOG_REFS_ADDRESS,WILLOW_BG_FAR_STREAM_PATTERN_COUNT,WILLOW_BG_FAR_STREAM_REFERENCE_COUNT,165};
 CHECK(willowFarCacheInit(&initialState,&resources));
 for(unsigned n=0;n<65536;n++)initialVRAM[n]=(n*73+(n>>8)*11+0x5a)&255;
 memset(initialVRAM,0,32);FILE*f=fopen(argv[5],"rb");CHECK(f);uint8_t tile[34];
 while(fread(tile,1,34,f)==34)memcpy(initialVRAM+word(tile)*32,tile+2,32);CHECK(feof(f));fclose(f);
 pl(rom,0xffff00);pl(rom+4,0x100004);pw(rom+0x100000,0x60fe);
 const uint8_t boot[]={0x13,0xfc,0,1,0,0xa1,0x30,0xf1,0x33,0xfc,0x81,4,0,0xc0,0,4,0x33,0xfc,0x8f,2,0,0xc0,0,4,0x4e,0xf9,0,0x10,0,0};memcpy(rom+0x100004,boot,sizeof boot);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 rom[0x1b0]='R';rom[0x1b1]='A';rom[0x1b2]=0xf8;rom[0x1b3]=0x20;pl(rom+0x1b4,0x200000);pl(rom+0x1b8,0x20ffff);
 char gamepath[4096];CHECK(snprintf(gamepath,sizeof gamepath,"%s.closed.bin",argv[2])<(int)sizeof gamepath);
 f=fopen(gamepath,"wb");CHECK(f&&fwrite(rom,1,sizeof rom,f)==sizeof rom);fclose(f);
 void*core=dlopen(argv[1],RTLD_NOW);CHECK(core);coreHandle=core;
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void*,retro_get_memory_data,(unsigned));
 setreg=dlsym(core,"m68k_set_reg");getreg=dlsym(core,"m68k_get_reg");run68k=dlsym(core,"m68k_run");CHECK(setreg&&getreg&&run68k);
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 struct retro_game_info game={gamepath,rom,sizeof rom,0};CHECK(retro_load_game(&game));
 ram=dlsym(core,"work_ram");cpu=dlsym(core,"m68k");vram=dlsym(core,"vram");sram=retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);CHECK(ram&&cpu&&vram&&sram);
 retro_run();CHECK(getreg(16)==0x100000);vdpWrite16=cpu->map[0xc0].write16;CHECK(vdpWrite16);cpu->map[0xc0].write16=vdpObserver;
 memcpy(protectedCRAM,dlsym(core,"cram"),128);memcpy(protectedVSRAM,dlsym(core,"vsram"),128);memcpy(protectedRegs,dlsym(core,"reg"),32);
 eventFile=fopen(argv[6],"w");CHECK(eventFile);fputs("case,operation,a,b,c,result,word,written,callbacks,data_words,master_cycles\n",eventFile);
 char resolvedPath[4096];CHECK(snprintf(resolvedPath,sizeof resolvedPath,"%s.resolved.bin",argv[6])<(int)sizeof resolvedPath);
 resolvedFile=fopen(resolvedPath,"wb");CHECK(resolvedFile);
 lifecycle();planeUnion();fclose(eventFile);fclose(resolvedFile);
 printf("{\"cases\":%u,\"byte_checks\":%u,\"native_upload_callbacks\":%u,\"refusals\":%u,\"duplicate_requests\":%u,\"physical_pool_boundaries\":%u,\"state_bytes\":436,\"live_admitted\":false}\n",cases,checks,uploads,refusals,duplicateCases,boundaryCases);return 0;
}
