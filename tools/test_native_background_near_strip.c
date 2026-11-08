#define WILLOW_CLOSED_HELPERS_ONLY 1
#include "test_native_render_cache_closed.c"
#include "willow_native_background_near_strip.h"
#include "render_background_imports.h"
#include "near_columns_imports.h"
#include "strip-fixture.h"
#undef CHECK
static unsigned cases;
#define REQUIRE(x) do{if(!(x)){fprintf(stderr,"Near strip failure%u case%u: %s\n",__LINE__,cases,#x);exit(1);}}while(0)
static WillowNativeBackgroundKeyCache keys;
static WillowNativeBackgroundRing ring;
static WillowNativePlaneSurface surface,expected;
static WillowPresentationPlan plan={0,0,0,WILLOW_PRESENT_BG};
static volatile uint32_t epoch=7;
static WillowPresentationLease lease={&plan,&epoch,7};
static uint16_t refs[WILLOW_BG_CATALOG_LAYER2_REFERENCE_COUNT*2],validIds[1024];
static uint8_t flags[WILLOW_BG_CATALOG_LAYER2_REFERENCE_COUNT],counts[32],expectedCounts[32],beforeRAM[65536],referenceRAM[65536];
static uint32_t columns[2],expectedColumns[2];
static unsigned validCount,x,y,rows,countOffset;static uint32_t leaving;
static WillowNativeBackgroundNearStrip view={&keys,&ring,refs,flags,WILLOW_BG_CATALOG_LAYER2_PAIR_COUNT,0};
static uint32_t randomState=0x1379abcd;
static unsigned rnd(void){randomState^=randomState<<13;randomState^=randomState>>17;randomState^=randomState<<5;return randomState;}
static void setup(unsigned n){
 memset(&surface,0xa5,sizeof surface);surface.lease=&lease;surface.plan=&plan;surface.epoch=&epoch;surface.generation=7;surface.dirty=0;surface.publishedBase=0xc000;
 for(unsigned i=0;i<2048;i++)surface.words[i]=(uint16_t)rnd();
 for(unsigned i=0;i<32;i++){surface.first[i]=255;surface.last[i]=0;counts[i]=20;}
 memset(&keys,0,sizeof keys);for(unsigned i=0;i<330;i++)REQUIRE(willowNativeBackgroundNearKeySet(&keys,i,validIds[rnd()%validCount]));
 ring=(WillowNativeBackgroundRing){n&63,(n>>2)&63,(n/3)%22,(n/5)%15,22,15,1,1};
 x=(ring.x*2+((n/7)%22)*2+(n&1))&127;
 rows=n%3==0?28:n%3==1?29:1+(n%30);
 unsigned parity=(n>>1)&1;if(rows==30)parity=0;
 unsigned used=(parity+rows+1)/2,dy=(n/11)%(16-used);
 y=(ring.y*2+dy*2+parity)&127;
 columns[0]=columns[1]=0;leaving=rnd();expected=surface;memcpy(expectedCounts,counts,32);memcpy(expectedColumns,columns,sizeof columns);
}
static unsigned oracleRef(unsigned px,unsigned py){
 unsigned sourceX=px/2,sourceY=py/2;
 unsigned dx=(sourceX+64-ring.x)%64,dy=(sourceY+64-ring.y)%64;
 REQUIRE(dx<22&&dy<15);
 unsigned slot=((dy+ring.hy)%15)*22+(dx+ring.hx)%22;
 unsigned id=keys.nearLow[slot]+(((keys.nearHigh[slot/4]>>(2*(slot%4)))%4)*256);
 REQUIRE(id<WILLOW_BG_CATALOG_LAYER2_PAIR_COUNT);return id*4+(py%2)*2+px%2;
}
static void variation(unsigned n){
 if(n%4==1){surface.dirty=0xffffffffu;for(unsigned i=0;i<32;i++){surface.first[i]=0;surface.last[i]=63;}columns[0]=0x22222222;columns[1]=0x44444444;}
 if(n%4==2){surface.dirty=0x55555555u;for(unsigned i=0;i<32;i+=2){surface.first[i]=5;surface.last[i]=58;}}
 if(n%4==3)for(unsigned i=0;i<rows;i+=2){unsigned py=(y+i)&127,ref=oracleRef(x,py);surface.words[(py&31)*64+(x&63)]=refs[ref*2+1];}
 expected=surface;memcpy(expectedColumns,columns,sizeof columns);
}
static uint32_t oracle(void){
 uint32_t mask=0;
 for(unsigned row=0;row<rows;row++){
  unsigned py=(y+row)%128,r=py%32,c=x%64,ref=oracleRef(x,py);uint16_t word=refs[ref*2+1];REQUIRE(word!=65535);
  unsigned nonopaque=flags[ref]!=1;mask|=(uint32_t)nonopaque<<r;
  expectedCounts[r]+=(int)nonopaque-(int)((leaving>>r)&1);
  if(expected.words[r*64+c]!=word){expected.words[r*64+c]=word;expectedColumns[c/32]|=1UL<<(c%32);
   if(!(expected.dirty&(1UL<<r))){expected.first[r]=expected.last[r]=c;expected.dirty|=1UL<<r;}
   else{if(c<expected.first[r])expected.first[r]=c;if(c>expected.last[r])expected.last[r]=c;}
  }
 }
 return mask;
}
static void hostCases(void){
 for(cases=0;cases<8000;cases++){
  setup(cases);variation(cases);uint32_t actual=0,mask=oracle();
  REQUIRE(willowNativeBackgroundNearCaptureFlags(&view,x,y,rows,&actual)&&actual==mask);
  REQUIRE(willowNativeBackgroundNearEnter(&view,&surface,columns,counts,x,y,rows,leaving));
  REQUIRE(!memcmp(&surface,&expected,sizeof surface)&&!memcmp(counts,expectedCounts,32)&&!memcmp(columns,expectedColumns,sizeof columns));
 }
 for(unsigned bad=0;bad<12;bad++){
  setup(bad);uint32_t actual=0xabcdef01;
  if(bad==0)ring.hx=22;if(bad==1)ring.hy=15;if(bad==2)ring.w=21;if(bad==3)ring.h=14;if(bad==4)ring.scale=2;if(bad==5)ring.valid=0;
  if(bad==6)x=128;if(bad==7)y=128;if(bad==8)rows=0;if(bad==9)rows=31;if(bad==10)view.pairCount=0;if(bad==11)view.pairCount=1024;
  REQUIRE(!willowNativeBackgroundNearCaptureFlags(&view,x,y,rows,&actual)&&actual==0xabcdef01);
  REQUIRE(!willowNativeBackgroundNearEnter(&view,&surface,columns,counts,x,y,rows,leaving));REQUIRE(!memcmp(&surface,&expected,sizeof surface));view.pairCount=WILLOW_BG_CATALOG_LAYER2_PAIR_COUNT;cases++;
 }
 fprintf(stderr,"HOST PASS %u sanitized cases, independent source-coordinate ID/flag/word/dirty oracle and invalid initial guards.\n",cases);
}
static void seedTarget(void){
 for(unsigned i=0;i<65536;i++){ram[i^1]=(uint8_t)(i*13+7);sram[i]=(uint8_t)(i*17+11);}
 wl(STRIPSURFACE,STRIPLEASE);wl(STRIPSURFACE+4,STRIPPLAN);wl(STRIPSURFACE+8,STRIPEPOCH);wl(STRIPSURFACE+12,7);wl(STRIPSURFACE+16,surface.dirty);
 for(unsigned i=0;i<2048;i++)ww(STRIPSURFACE+20+i*2,surface.words[i]);
 for(unsigned i=0;i<32;i++){ram[((STRIPSURFACE+4116+i)&65535)^1]=surface.first[i];ram[((STRIPSURFACE+4148+i)&65535)^1]=surface.last[i];ram[((STRIPCOUNTS+countOffset+i)&65535)^1]=counts[i];}
 ww(STRIPSURFACE+4180,surface.publishedBase);wl(STRIPEPOCH,7);wl(STRIPLEASE,STRIPPLAN);wl(STRIPLEASE+4,STRIPEPOCH);wl(STRIPLEASE+8,7);
 for(unsigned i=0;i<sizeof keys;i++)ram[((STRIPKEYS+i)&65535)^1]=((const uint8_t*)&keys)[i];
 for(unsigned i=0;i<sizeof ring;i++)ram[((STRIPRING+i)&65535)^1]=((const uint8_t*)&ring)[i];
 wl(STRIPCOLUMNS,columns[0]);wl(STRIPCOLUMNS+4,columns[1]);pw(sram+0xf000,x);pw(sram+0xf002,y);pw(sram+0xf004,rows);pl(sram+0xf006,leaving);pw(sram+0xf00a,countOffset);
 for(unsigned i=0;i<65536;i++)beforeRAM[i]=ram[i^1];
}
static void verifyTarget(void){
 REQUIRE(word(sram+0xf010)==1);REQUIRE(rl(STRIPSURFACE+16)==expected.dirty);
 for(unsigned i=0;i<2048;i++)REQUIRE(rw(STRIPSURFACE+20+i*2)==expected.words[i]);
 for(unsigned i=0;i<32;i++){
  REQUIRE(ram[((STRIPSURFACE+4116+i)&65535)^1]==expected.first[i]);REQUIRE(ram[((STRIPSURFACE+4148+i)&65535)^1]==expected.last[i]);REQUIRE(ram[((STRIPCOUNTS+countOffset+i)&65535)^1]==expectedCounts[i]);
 }
 REQUIRE(rl(STRIPCOLUMNS)==expectedColumns[0]&&rl(STRIPCOLUMNS+4)==expectedColumns[1]);
 for(unsigned i=0;i<65536;i++)if(!((i>=0x4000&&i<0x5056)||(i>=0x5330&&i<0x5361)||(i>=0x7000&&i<0x7800)))REQUIRE(ram[i^1]==beforeRAM[i]);
}
static void nativeCountBoundaries(void){
 unsigned chosen[4][2]={{65535,0},{65535,0},{65535,0},{65535,0}},accepted=0,refused=0;
 for(unsigned id=0;id<WILLOW_BG_CATALOG_LAYER2_PAIR_COUNT;id++)for(unsigned col=0;col<2;col++){
  unsigned bits=(flags[id*4+col]!=1)|((flags[id*4+col+2]!=1)<<1);if(chosen[bits][0]==65535){chosen[bits][0]=id;chosen[bits][1]=col;}
 }
 const uint8_t bounds[]={0,1,42,43,44,255};unsigned tests=0;
 for(unsigned bits=0;bits<4;bits++)if(chosen[bits][0]!=65535)for(unsigned a=0;a<6;a++)for(unsigned b=0;b<6;b++)for(unsigned prior=0;prior<4;prior++)for(unsigned odd=0;odd<2;odd++)for(unsigned start=0;start<3;start++){
  setup(0);x=chosen[bits][1];y=start==0?0:start==1?31:127;rows=2;countOffset=odd;ring=(WillowNativeBackgroundRing){0,y>>1,0,0,22,15,1,1};
  for(unsigned i=0;i<330;i++)REQUIRE(willowNativeBackgroundNearKeySet(&keys,i,chosen[bits][0]));
  counts[y&31]=bounds[a];counts[(y+1)&31]=bounds[b];leaving=((uint32_t)(prior&1)<<(y&31))|((uint32_t)(prior>>1)<<((y+1)&31));
  expected=surface;memcpy(expectedCounts,counts,32);memcpy(expectedColumns,columns,sizeof columns);
  unsigned ok=willowNativeBackgroundNearEnter(&view,&expected,expectedColumns,expectedCounts,x,y,rows,leaving);
  seedTarget();call(STRIPENTERFAST,0xff7800,tests);if(word(sram+0xf010)!=ok)fprintf(stderr,"COUNTER mismatch bits%u counts%u/%u prior%u odd%u y%u got%u expected%u\n",bits,bounds[a],bounds[b],prior,odd,y,word(sram+0xf010),ok);REQUIRE(word(sram+0xf010)==ok);
  if(ok){verifyTarget();accepted++;}else{
   refused++;for(unsigned i=0;i<65536;i++)if(!((i>=0x4000&&i<0x5056)||(i>=0x5330&&i<0x5361)||(i>=0x7000&&i<0x7800)))REQUIRE(ram[i^1]==beforeRAM[i]);
  }
  tests++;
 }
 fprintf(stderr,"COUNT PASS %u native cases: %u accepted, %u refused; all available source opacity pairs, prior flags0..3, counts0/1/42/43/44/255, firstY0/31/127 and even/odd count pointer. Refusal permits discarded partial native RAM, never VDP.\n",tests,accepted,refused);
}
static void patchROMWord(unsigned at,unsigned value){uint8_t*p=cpu->map[0].base;p[at^1]=value>>8;p[(at+1)^1]=value;}
static void nativeRefusals(void){
 unsigned tests=0;
 for(unsigned which=0;which<11;which++){
  setup(0);countOffset=0;rows=28;x=y=0;ring=(WillowNativeBackgroundRing){0,0,0,0,22,15,1,1};
  unsigned oldRef=0,ref=0,table=0,missing=which==4||which==5;
  if(which<4){unsigned slot=(which&1)?13*22:0;REQUIRE(willowNativeBackgroundNearKeySet(&keys,slot,which<2?WILLOW_BG_CATALOG_LAYER2_PAIR_COUNT:1023));}
  if(missing){unsigned slot=which==4?0:13*22,id=keys.nearLow[slot]|(((keys.nearHigh[slot/4]>>(2*(slot%4)))&3)<<8);ref=id*4;oldRef=refs[ref*2+1];refs[ref*2+1]=65535;table=WILLOW_BG_NEAR_COLUMNS_ADDRESS+id*16;patchROMWord(table,65535);}
  if(which==6)rows=31;if(which==7)ring.hx=22;if(which==8)x=128;
  seedTarget();if(which==9)wl(STRIPSURFACE+12,8);if(which==10)wl(STRIPLEASE+8,8);
  call(STRIPENTERFAST,0xff7800,tests);REQUIRE(word(sram+0xf010)==0);
  for(unsigned i=0;i<65536;i++)if(!((i>=0x4000&&i<0x5056)||(i>=0x5330&&i<0x5361)||(i>=0x7000&&i<0x7800)||(i>=0x5314&&i<0x5320)))REQUIRE(ram[i^1]==beforeRAM[i]);
  if(which<4||which==6||which==7||which==8){seedTarget();pl(sram+0xf012,0x12345678);call(STRIPCAPTUREFAST,0xff7800,tests);REQUIRE(word(sram+0xf010)==0&&lng(sram+0xf012)==0x12345678);}
  if(missing){refs[ref*2+1]=oldRef;patchROMWord(table,oldRef);}
  tests++;
 }
 fprintf(stderr,"REFUSAL PASS %u native scenarios: invalid first/late IDs, first/late missing native word, bounds/ring/lease mismatch; failed capture leaves output untouched. No VDP.\n",tests);
}
static void noVDP(unsigned a,unsigned v){(void)a;(void)v;fprintf(stderr,"Unexpected VDP write\n");abort();}
int main(int argc,char**argv){
 if(argc!=4)return 2;char path[4096];FILE*f;
 snprintf(path,sizeof path,"%s/background-resources.bin",argv[3]);f=fopen(path,"rb");if(!f)return 2;size_t n=fread(rom+0x110000,1,0x40000,f);if(!n||!feof(f))return 2;fclose(f);
 for(unsigned i=0;i<WILLOW_BG_CATALOG_LAYER2_REFERENCE_COUNT*2;i++)refs[i]=(uint16_t)word(rom+WILLOW_BG_CATALOG_LAYER2_REFS_ADDRESS+i*2);
 memcpy(flags,rom+WILLOW_BG_CATALOG_LAYER2_FLAGS_ADDRESS,sizeof flags);
 for(unsigned id=0;id<WILLOW_BG_CATALOG_LAYER2_PAIR_COUNT;id++){unsigned valid=1;for(unsigned r=0;r<4;r++)valid&=refs[(id*4+r)*2+1]!=65535;if(valid)validIds[validCount++]=id;}
 REQUIRE(validCount>0);hostCases();
 snprintf(path,sizeof path,"%s/near-columns.bin",argv[2]);f=fopen(path,"rb");if(!f)return 2;n=fread(rom+WILLOW_BG_NEAR_COLUMNS_ADDRESS,1,WILLOW_BG_NEAR_COLUMNS_BYTES,f);if(n!=WILLOW_BG_NEAR_COLUMNS_BYTES)return 2;fclose(f);
 snprintf(path,sizeof path,"%s/strip.bin",argv[2]);f=fopen(path,"rb");if(!f)return 2;n=fread(rom+0x150000,1,0x10000,f);if(!n||!feof(f))return 2;fclose(f);
 pl(rom,0xffff00);pl(rom+4,0x100004);pw(rom+0x100000,0x60fe);const uint8_t boot[]={0x13,0xfc,0,1,0,0xa1,0x30,0xf1,0x60,0xf2};memcpy(rom+0x100004,boot,sizeof boot);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);rom[0x1b0]='R';rom[0x1b1]='A';rom[0x1b2]=0xf8;rom[0x1b3]=0x20;pl(rom+0x1b4,0x200000);pl(rom+0x1b8,0x20ffff);
 snprintf(path,sizeof path,"%s/closed.bin",argv[2]);f=fopen(path,"wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core)return 2;
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void*,retro_get_memory_data,(unsigned));FN(void,m68k_set_irq,(unsigned));
 setreg=dlsym(core,"m68k_set_reg");getreg=dlsym(core,"m68k_get_reg");run68k=dlsym(core,"m68k_run");if(!setreg||!getreg||!run68k)return 2;
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();struct retro_game_info game={path,rom,sizeof rom,0};if(!retro_load_game(&game))return 2;
 ram=dlsym(core,"work_ram");cpu=dlsym(core,"m68k");sram=retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);if(!ram||!cpu||!sram)return 2;retro_run();m68k_set_irq(0);cpu->map[0xc0].write16=noVDP;
 puts("case,rows,x,y,count_offset,capture_original,capture_fast,enter_original,enter_fast");
 for(cases=0;cases<1536;cases++){
  setup(cases);variation(cases);countOffset=cases>=1024;uint32_t mask=oracle();unsigned clocks[4];
  seedTarget();clocks[0]=call(STRIPCAPTUREORIGINAL,0xff7800,cases);REQUIRE(word(sram+0xf010)==1&&lng(sram+0xf012)==mask);
  seedTarget();clocks[1]=call(STRIPCAPTUREFAST,0xff7800,cases);REQUIRE(word(sram+0xf010)==1&&lng(sram+0xf012)==mask);
  seedTarget();clocks[2]=call(STRIPENTERORIGINAL,0xff7800,cases);verifyTarget();for(unsigned i=0;i<65536;i++)referenceRAM[i]=ram[i^1];
  seedTarget();clocks[3]=call(STRIPENTERFAST,0xff7800,cases);verifyTarget();for(unsigned i=0;i<65536;i++)if(!(i>=0x7000&&i<0x7800))REQUIRE(ram[i^1]==referenceRAM[i]);
  printf("%u,%u,%u,%u,%u,%u,%u,%u,%u\n",cases,rows,x,y,countOffset,clocks[0],clocks[1],clocks[2],clocks[3]);
 }
 nativeCountBoundaries();nativeRefusals();
 fprintf(stderr,"NATIVE PASS %u cases/fourcalls, exact flags/nativewords/counts/dirtyspans/columnqueues and memory guards, real C ABI. Every VDPwrite aborts.\n",cases);return 0;
}
