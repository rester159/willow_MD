#include "native_frame_profile.h"
/* Headless renderer stress with live source-controlled player input. */
#include "vendor/libretro.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <dlfcn.h>
#include "native_background_fixture.h"
#include "../src/willow_native_column_stream.h"
static bool env(unsigned cmd,void*p){
 if(cmd==RETRO_ENVIRONMENT_SET_PIXEL_FORMAT)return true;
 if(cmd==RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY || cmd==RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY){*(const char**)p="/tmp";return true;}
 if(cmd==RETRO_ENVIRONMENT_GET_VARIABLE){struct retro_variable*v=p;if(!strcmp(v->key,"genesis_plus_gx_addr_error")){v->value="enabled";return true;}return false;}
 if(cmd==RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE){*(bool*)p=false;return true;}
 if(cmd==RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION){*(unsigned*)p=0;return true;}
 return cmd==RETRO_ENVIRONMENT_SET_VARIABLES || cmd==RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME || cmd==RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS || cmd==RETRO_ENVIRONMENT_SET_CONTROLLER_INFO;
}
static void video(const void*p,unsigned w,unsigned h,size_t pitch){(void)p;(void)w;(void)h;(void)pitch;}
static void audio(int16_t a,int16_t b){(void)a;(void)b;}
static size_t batch(const int16_t*p,size_t n){(void)p;return n;}
static void poll(void){}
static uint8_t *inputRam;static unsigned inputTick;static unsigned leg=84;
static int16_t input(unsigned p,unsigned d,unsigned i,unsigned id){
 (void)i;if(!inputRam || p || d!=RETRO_DEVICE_JOYPAD)return 0;
 unsigned t=inputRam[inputTick]|inputRam[inputTick+1]<<8;
 if(t<915 || t>=1500)return 0;
 unsigned direction=t<915+leg?RETRO_DEVICE_ID_JOYPAD_RIGHT:t<915+leg*2?RETRO_DEVICE_ID_JOYPAD_LEFT:((t-915-leg*2)/16)%2?RETRO_DEVICE_ID_JOYPAD_LEFT:RETRO_DEVICE_ID_JOYPAD_RIGHT;
 return id==direction;
}
static unsigned symbol(const char*path,const char*name){
 FILE*f=fopen(path,"r");assert(f);char line[512],sym[256],type;unsigned at;
 while(fgets(line,sizeof line,f))if(sscanf(line,"%x %c %255s",&at,&type,sym)==3 && !strcmp(sym,name)){fclose(f);return at&65535;}
 fprintf(stderr,"Missing symbol %s\n",name);exit(2);
}
static unsigned mirrorBase,compactAt;
static unsigned mirrorTile(const uint8_t*p,unsigned base,unsigned cell){
 if(!(p[compactAt]|p[compactAt+1]<<8))return (p[base+cell*2]|p[base+cell*2+1]<<8)&2047;
 unsigned index=((base-mirrorBase)/2752*43+cell%43)*32+cell/43,bit=index*12,value=0;
 for(unsigned n=0;n<12;n++)value=(value<<1)|((p[(mirrorBase+(bit+n)/8)^1]>>(7-(bit+n)%8))&1);
 return value;
}
static uint16_t word(const uint8_t*p,unsigned at){return p[at]|p[at+1]<<8;}
static uint8_t*readFile(const char*path,size_t*size){FILE*f=fopen(path,"rb");assert(f);assert(!fseek(f,0,SEEK_END));*size=ftell(f);rewind(f);uint8_t*p=malloc(*size);assert(p&&fread(p,1,*size,f)==*size);fclose(f);return p;}
#define FN(ret,name,args) ret(*name)args=dlsym(core,#name);assert(name)
int main(int argc,char**argv){
 if(argc!=4 && (argc!=5 || strcmp(argv[4],"--long")))return 2;
 if(argc==5)leg=176;
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 size_t romSize,bankSize;uint8_t*gameRom=readFile(argv[2],&romSize),*bank=readFile("native/intro_probe/res/background_pairs.bin",&bankSize);
 assert(bankSize==(NATIVE_BACKGROUND_ID_LIMIT-NATIVE_BACKGROUND_EXTENSION_BASE)*32);
 struct retro_game_info game={argv[2],gameRom,romSize,NULL};assert(retro_load_game(&game));
 uint8_t*ram=dlsym(core,"work_ram"),*vram=dlsym(core,"vram");assert(ram&&vram);
 unsigned tick=symbol(argv[3],"debugIntroTick"),overflow=symbol(argv[3],"debugIntroOverflow"),ids=symbol(argv[3],"ids");
 unsigned streamsAt=symbol(argv[3],"columnStreams"),planeA=symbol(argv[3],"ownershipMirror"),planeB=planeA+2752;
 mirrorBase=planeA;compactAt=symbol(argv[3],"debugMirrorCompact");
 unsigned refsAt=symbol(argv[3],"refsByTile"),ownedAt=symbol(argv[3],"rowOwned"),freeAt=symbol(argv[3],"freeTiles");
 unsigned hudAt=symbol(argv[3],"debugHudSprites"),hudCountAt=symbol(argv[3],"debugHudSpriteCount"),pinsAt=symbol(argv[3],"debugForestRestorePins"),overlayAt=symbol(argv[3],"overlayLoaded");
 unsigned ownershipChecks=0;
 inputRam=ram;inputTick=tick;
 FILE*f=fopen("build/reference_maincpu.bin","rb");assert(f&&fread(rom,1,sizeof rom,f)==sizeof rom);fclose(f);
 size_t introSize;uint8_t*intro=readFile("native/intro_probe/res/layers.bin",&introSize);
 unsigned checks=0,maximumX=0;int previousX=0;unsigned reversals=0;
 for(unsigned frame=0;frame<1800;frame++){
  unsigned before=word(ram,tick);
  if(before==949){
   /* Isolate compact steady-state cost. This host transition is NOT a
    * production conversion or a proof of DMA lifetime/transition timing. */
   uint16_t tiles[2752];for(unsigned i=0;i<2752;i++)tiles[i]=word(ram,planeA+i*2)&2047;
   for(unsigned i=0;i<2752;i++)for(unsigned n=0;n<12;n++){
    unsigned index=(i/1376*43+i%43)*32+(i%1376)/43;
    unsigned bit=index*12+n,at=(planeA+bit/8)^1,mask=128u>>(bit%8);
    ram[at]=(ram[at]&~mask)|((tiles[i]&(1u<<(11-n)))?mask:0);
   }
   for(unsigned i=4128;i<5504;i++)ram[(planeA+i)^1]=0x3c;
   ram[compactAt]=1;ram[compactAt+1]=0;
  }
  retro_run();unsigned t=word(ram,tick);
  if(t>=950)for(unsigned i=4128;i<5504;i++)assert(ram[(planeA+i)^1]==0x3c);
  if(before>=915 && t!=before+1){unsigned ph=profileSymbol(argv[3],"debugFrameLines"),pr=profileSymbol(argv[3],"debugColumnLines");fprintf(stderr,"Phases %u %u %u %u %u; columns %u %u %u %u\n",profileWord(ram,ph,0),profileWord(ram,ph,2),profileWord(ram,ph,4),profileWord(ram,ph,6),profileWord(ram,ph,8),profileWord(ram,pr,0),profileWord(ram,pr,2),profileWord(ram,pr,4),profileWord(ram,pr,6));fprintf(stderr,"Movement stress missed update %u -> %u, CPU PC %x\n",before,t,m68k_get_reg(16));return 1;}
  if(word(ram,overflow)){fprintf(stderr,"Movement stress overflow tick %u count %u\n",t,word(ram,overflow));return 1;}
  if(t<916)continue;
  int nearX=-(int16_t)word(vram,0xf000);if(nearX>previousX && (unsigned)nearX>maximumX)maximumX=nearX;
  if(nearX<previousX)reversals++;previousX=nearX;
  for(unsigned layer=2;layer<=3;layer++){
   int cx=-(int16_t)word(vram,0xf000+(layer-2)*2);
   assert(cx>=0);
   for(unsigned row=0;row<28;row++)for(int col=cx/8;col<=(cx+319)/8;col++){
    uint16_t x=32+col*8,y=row*8-240;uint32_t pair=sourcePair(layer,x,y);
    unsigned pairId=idOf(pair,layer==2?nativeScroll2Pairs:nativeScroll3Pairs,layer==2?WILLOW_SCROLL2_PAIR_COUNT:WILLOW_SCROLL3_PAIR_COUNT);
    unsigned offset=(layer==2?nativeBackground2Offsets:nativeBackground3Offsets)[pairId],size=layer==2?16:32;
    assert(offset!=65535);
    unsigned entry=(layer==2?nativeBackground2Entries:nativeBackground3Entries)[offset+(y%size/8)*(size/8)+x%size/8];
    unsigned attr=word(vram,(layer==2?0xc000:0xe000)+(row*64+(col&63))*2),tile=attr&2047,id=entry&32767;
    if(tile<16 || tile>=1312 || word(ram,ids+(tile-16)*2)!=id || attr!=(((2+(entry>>15))<<13)|tile)){
     fprintf(stderr,"Visible map mismatch tick %u layer %u column %d row %u camera %d\n",t,layer,col,row,cx);return 1;
    }
    assert(id<(introSize/32) || (id>=NATIVE_BACKGROUND_EXTENSION_BASE && id<NATIVE_BACKGROUND_ID_LIMIT));
    const uint8_t*bytes=id<(introSize/32)?intro+id*32:bank+(id-NATIVE_BACKGROUND_EXTENSION_BASE)*32;
    for(unsigned b=0;b<32;b++)if(vram[(tile*32+b)^1]!=bytes[b]){fprintf(stderr,"Visible pattern mismatch tick %u ID %u\n",t,id);return 1;}
    checks++;
   }
  }
  if(t>=950){
   assert(!word(ram,pinsAt) && !word(ram,overlayAt));
   uint16_t expectedRefs[1296]={0};
   unsigned streams=((unsigned)word(ram,streamsAt)<<16|word(ram,streamsAt+2))&65535;
   for(unsigned layer=0;layer<2;layer++){
    unsigned base=layer?planeB:planeA,stride=WILLOW_COLUMN_WINDOW,record=streams+layer*sizeof(WillowColumnStream);
    for(unsigned slot=0;slot<43;slot++){
     int col=(int16_t)word(ram,record+slot*2);unsigned rows=ram[(record+86+slot)^1];
     assert(rows<=28 && (col!=WILLOW_COLUMN_EMPTY || !rows));
     if(col==WILLOW_COLUMN_EMPTY)continue;
     assert(willowColumnSlot(col)==slot);
     for(unsigned row=0;row<rows;row++){
      unsigned attr=mirrorTile(ram,base,row*stride+slot),tile=attr&2047;
      assert(tile>=16 && tile<1312);expectedRefs[tile-16]++;
     }
    }
    for(unsigned row=28;row<32;row++)if(((ram[(ownedAt+layer*4+(row>>3))^1]>>(row&7))&1))for(unsigned col=0;col<40;col++){
     unsigned tile=mirrorTile(ram,base,row*stride+col)&2047;assert(tile>=16 && tile<1312);expectedRefs[tile-16]++;
    }
   }
   unsigned count=word(ram,hudCountAt);assert(count<=40);
   for(unsigned n=0;n<count;n++){
    unsigned at=hudAt+n*6,tile=word(ram,at+4),size=ram[(at+2)^1]*ram[(at+3)^1];
    if(tile>=16 && tile<1312)for(unsigned i=0;i<size;i++){assert(tile+i<1312);expectedRefs[tile+i-16]++;}
   }
   for(unsigned slot=0;slot<1296;slot++){
    if(word(ram,refsAt+(slot+16)*2)!=expectedRefs[slot]){fprintf(stderr,"Ownership mismatch tick %u slot %u\n",t,slot);return 1;}
    assert(!!(word(ram,freeAt+(slot/16)*2)&(1u<<(slot&15)))==!expectedRefs[slot]);
    ownershipChecks++;
   }
  }
  if(t>=1530)break;
 }
 assert(word(ram,tick)==1530 && maximumX>(leg==176?320:80) && reversals>0);
 printf("Native movement stress: immediate input, forward/backward and repeated reversals; %u visible cell/pattern checks, maximum camera X %u, %u leftward updates, no missed updates or overflow. Renderer stress only; no new original-game state trace.\n",checks,maximumX,reversals);
 printf("Independent ownership audit: %u cache-reference and free-slot checks after temporary pins retire\n",ownershipChecks);
 unsigned dmaPeak=word(ram,symbol(argv[3],"debugDMAQueuePeak"));
 unsigned dmaCapacity=word(ram,symbol(argv[3],"queueSize"));
 assert(dmaPeak<dmaCapacity);
 printf("DMA queue headroom: peak %u of %u operations through update 1530\n",dmaPeak,dmaCapacity);
 unsigned copyPeak=word(ram,symbol(argv[3],"debugDMACopyPeak"));
 unsigned copyCapacity=2*word(ram,symbol(argv[3],"dataBufferSize"));
 assert(copyPeak<copyCapacity);
 printf("DMA copy-buffer headroom: peak %u of %u bytes through update 1530\n",copyPeak,copyCapacity);
 free(intro);free(bank);free(gameRom);return 0;
}
