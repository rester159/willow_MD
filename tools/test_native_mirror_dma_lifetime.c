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
 unsigned streamsAt=symbol(argv[3],"columnStreams"),planeA=symbol(argv[3],"planeAAttrs"),planeB=symbol(argv[3],"planeBAttrs");
 unsigned refsAt=symbol(argv[3],"refsByTile"),ownedAt=symbol(argv[3],"rowOwned"),freeAt=symbol(argv[3],"freeTiles");
 unsigned hudAt=symbol(argv[3],"debugHudSprites"),hudCountAt=symbol(argv[3],"debugHudSpriteCount"),pinsAt=symbol(argv[3],"debugForestRestorePins"),overlayAt=symbol(argv[3],"overlayLoaded");
 unsigned ownershipChecks=0;
 unsigned queuePointer=symbol(argv[3],"dmaQueues"),queueCount=symbol(argv[3],"queueIndex");
 unsigned bufferPointer=symbol(argv[3],"dmaDataBuffer"),nextPointer=symbol(argv[3],"nextDataBuffer");
 unsigned attributeCounts[16]={0};
 unsigned maxStaging=0,maxStagingTick=0,lastMirrorTick=0,mirrorTransfers=0;

 inputRam=ram;inputTick=tick;
 FILE*f=fopen("build/reference_maincpu.bin","rb");assert(f&&fread(rom,1,sizeof rom,f)==sizeof rom);fclose(f);
 size_t introSize;uint8_t*intro=readFile("native/intro_probe/res/layers.bin",&introSize);
 unsigned checks=0,maximumX=0;int previousX=0;unsigned reversals=0;
 for(unsigned frame=0;frame<1800;frame++){
  unsigned before=word(ram,tick);retro_run();unsigned t=word(ram,tick);
  if(t)for(unsigned layer=0;layer<2;layer++)for(unsigned i=0;i<1376;i++){
   unsigned attribute=word(ram,(layer?planeB:planeA)+i*2),high=attribute>>12;
   if(high!=0&&high!=4&&high!=6&&high!=8&&high!=9){fprintf(stderr,"Unsupported intro mirror attribute %04x at update %u layer %u cell %u\n",attribute,t,layer,i);return 1;}
   attributeCounts[high]++;
  }
  unsigned queue=((unsigned)word(ram,queuePointer)<<16|word(ram,queuePointer+2))&65535;
  unsigned count=word(ram,queueCount),mirrorBytes=0,mirrorOps=0;
  assert(count<=32);
  for(unsigned n=0;n<count;n++){
   unsigned at=queue+n*16,len=(word(ram,at)&255)|((word(ram,at+2)&255)<<8);
   unsigned source=((word(ram,at+4)&255)<<9)|((word(ram,at+8)&127)<<17)|((word(ram,at+10)&255)<<1);
   if((source&0xff0000)!=0xff0000)continue;
   unsigned offset=source&65535;
   if((offset>=planeA&&offset<planeA+2752)||(offset>=planeB&&offset<planeB+2752)){
    unsigned end=offset+len*2;
    assert((offset>=planeA&&end<=planeA+2752)||(offset>=planeB&&end<=planeB+2752));
    mirrorBytes+=len*2;mirrorOps++;
   }
  }
  if(mirrorOps){
   unsigned begin=((unsigned)word(ram,bufferPointer)<<16)|word(ram,bufferPointer+2);
   unsigned next=((unsigned)word(ram,nextPointer)<<16)|word(ram,nextPointer+2);
   unsigned need=next-begin+mirrorBytes;assert(next>=begin&&next-begin<=256);
   if(need>maxStaging){maxStaging=need;maxStagingTick=t;printf("Decoded mirror staging peak: tick %u needs %u bytes (%u mirror bytes across %u transfers, %u existing copy bytes)\n",t,need,mirrorBytes,mirrorOps,next-begin);}
   lastMirrorTick=t;mirrorTransfers+=mirrorOps;
  }

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
      unsigned attr=word(ram,base+(row*stride+slot)*2),tile=attr&2047;
      assert(tile>=16 && tile<1312);expectedRefs[tile-16]++;
     }
    }
    for(unsigned row=28;row<32;row++)if(((ram[(ownedAt+layer*4+(row>>3))^1]>>(row&7))&1))for(unsigned col=0;col<40;col++){
     unsigned tile=word(ram,base+(row*stride+col)*2)&2047;assert(tile>=16 && tile<1312);expectedRefs[tile-16]++;
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
 printf("Mirror DMA audit: %u transfers observed; last queued at update %u; decoded-row copy requirement peaks at %u bytes at update %u\n",mirrorTransfers,lastMirrorTick,maxStaging,maxStagingTick);
 for(unsigned n=0;n<16;n++)if(attributeCounts[n])printf("Observed mirror high nibble %x: %u cell samples\n",n,attributeCounts[n]);
 free(intro);free(bank);free(gameRom);return 0;
}
