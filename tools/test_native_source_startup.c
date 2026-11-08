/* Source5B6/13FC bootstrap, real task identities, and native TRAP/RTE IRQ.
 * Original CPS buses provide the independent reference; native uses SRAM. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_native_video_layout.h"
#include "../build/native-video-relocation/io-test-ports.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
static uint8_t rom[0x400000],images[2][0x100000],cps[0x20000],ports[512];
static bool env(unsigned cmd,void *data){
 if(cmd==RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY||cmd==RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY){*(const char**)data="/tmp";return true;}
 if(cmd==RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE){*(bool*)data=false;return true;}
 if(cmd==RETRO_ENVIRONMENT_GET_VARIABLE){((struct retro_variable*)data)->value=NULL;return false;}
 if(cmd==RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION){*(unsigned*)data=0;return true;}
 return cmd==RETRO_ENVIRONMENT_SET_PIXEL_FORMAT||cmd==RETRO_ENVIRONMENT_SET_VARIABLES||cmd==RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS||cmd==RETRO_ENVIRONMENT_SET_CONTROLLER_INFO;
}
static void video(const void*d,unsigned w,unsigned h,size_t p){(void)d;(void)w;(void)h;(void)p;}
static void audio(int16_t l,int16_t r){(void)l;(void)r;}
static size_t batch(const int16_t*d,size_t n){(void)d;return n;}
static void poll(void){}
static int16_t input(unsigned p,unsigned d,unsigned i,unsigned id){(void)p;(void)d;(void)i;(void)id;return 0;}
#define FN(ret,n,args) ret(*n)args=dlsym(core,#n);if(!n){fprintf(stderr,"Missing core symbol %s\n",#n);return 2;}
typedef struct {
 struct {void*base;unsigned(*read8)(unsigned);unsigned(*read16)(unsigned);void(*write8)(unsigned,unsigned);void(*write16)(unsigned,unsigned);} map[256];
 struct {unsigned pc,cycle,detected;} poll;
 int cycles,refresh;unsigned end,dar[16],pc;
} ForestCoreCycles;
static const struct {unsigned source,target,size;} regions[]={
 {0x910000,WILLOW_VIDEO_SCROLL1,0x4000},{0x918000,WILLOW_VIDEO_SCROLL2,0x4000},
 {0x914000,WILLOW_VIDEO_SCROLL3,0x4000},{0x900000,WILLOW_VIDEO_OBJECT0,0x800},
 {0x908000,WILLOW_VIDEO_OBJECT1,0x800},{0x91c000,WILLOW_VIDEO_PALETTES,0x1800}};
typedef struct {unsigned pc,address,width,write,value;} IOEvent;
static IOEvent events[2][256];static unsigned eventCounts[2],path,currentPC;
static uint8_t states[2][65536],referenceVideo[0x20000],referencePorts[512];
static unsigned registers[2][21],provenValues[65536],provenPC[65536],pointerPairs;
static unsigned sleepTraps,readyTraps,taskDepth,irqEntries,irqReturns;
static void portEvent(unsigned at,unsigned width,unsigned write,unsigned value){
 if(eventCounts[path]>=256||at<0x800000||at+width>0x800200)abort();
 events[path][eventCounts[path]++]=(IOEvent){currentPC,at,width,write,value};
}
static unsigned portRead8(unsigned at){if(at<0x800000||at>=0x800200)abort();unsigned v=ports[at-0x800000];portEvent(at,1,0,v);return v;}
static unsigned portRead16(unsigned at){if(at<0x800000||at>0x8001fe||(at&1))abort();unsigned v=willowMotionWord(ports+(at-0x800000));portEvent(at,2,0,v);return v;}
static void portWrite8(unsigned at,unsigned v){portEvent(at,1,1,v&255);ports[at-0x800000]=v;}
static void portWrite16(unsigned at,unsigned v){if(at&1)abort();portEvent(at,2,1,v&65535);willowMotionPutWord(ports+(at-0x800000),v);}
static void validVideo(unsigned at,unsigned width){for(unsigned n=0;n<6;n++)if(at>=regions[n].source&&at+width<=regions[n].source+regions[n].size)return;fprintf(stderr,"Unrepresented source video%x/%u PC%x\n",at,width,currentPC);abort();}
static unsigned videoRead8(unsigned at){validVideo(at,1);return cps[at-0x900000];}
static unsigned videoRead16(unsigned at){validVideo(at,2);if(at&1)abort();return willowMotionWord(cps+(at-0x900000));}
static void videoWrite8(unsigned at,unsigned v){validVideo(at,1);cps[at-0x900000]=v;}
static void videoWrite16(unsigned at,unsigned v){validVideo(at,2);if(at&1)abort();willowMotionPutWord(cps+(at-0x900000),v);}
static unsigned ramLong(const uint8_t*r,unsigned at){unsigned v=0;for(unsigned n=0;n<4;n++)v=v<<8|r[((at+n)&65535)^1];return v;}
static int samePointer(unsigned a,unsigned b){for(unsigned n=0;n<6;n++)if(a>=regions[n].source&&a<=regions[n].source+regions[n].size&&b==regions[n].target+a-regions[n].source)return 1;return 0;}
static unsigned char savedMaps[4][sizeof(((ForestCoreCycles*)0)->map[0])];
static void selectProgram(ForestCoreCycles*cpu,unsigned selected){
 for(unsigned page=0;page<16;page++){uint8_t*b=cpu->map[page].base;if(!b)abort();for(unsigned n=0;n<65536;n++)b[n^1]=images[selected][page*65536+n];}
 const unsigned pages[]={0x80,0x90,0x91,0x92};for(unsigned n=0;n<4;n++)memcpy(cpu->map+pages[n],savedMaps[n],sizeof savedMaps[n]);
 if(!selected){
  cpu->map[0x80].base=ports;cpu->map[0x80].read8=portRead8;cpu->map[0x80].read16=portRead16;cpu->map[0x80].write8=portWrite8;cpu->map[0x80].write16=portWrite16;
  for(unsigned p=0x90;p<=0x91;p++){cpu->map[p].base=cps+(p-0x90)*65536;cpu->map[p].read8=videoRead8;cpu->map[p].read16=videoRead16;cpu->map[p].write8=videoWrite8;cpu->map[p].write16=videoWrite16;}
  /* Source92 accesses must fail until that additional hardware region is
   * represented. Do not let the Genesis open bus hide missing CPS storage. */
  cpu->map[0x92].base=cps;cpu->map[0x92].read8=videoRead8;cpu->map[0x92].read16=videoRead16;cpu->map[0x92].write8=videoWrite8;cpu->map[0x92].write16=videoWrite16;
 }
}
static void observeSourceStores(unsigned(*get)(int)){
 if(currentPC==0x1750||currentPC==0x17a4||currentPC==0x17c0){
  if(willowMotionLong(images[0]+currentPC)!=0x48e7fffe)abort();
  unsigned sp=get(15)&0xffffff;if(sp<0xff024c||sp>0xff0a10)abort();
  for(unsigned n=0;n<15;n++){unsigned at=(sp-60+n*4)&65535;provenValues[at]=get(n);provenPC[at]=currentPC;}
 }else if(currentPC==0xc42||currentPC==0x127e){
  if(willowMotionLong(images[0]+currentPC)!=0x2d490030)abort();unsigned a=get(14)&0xffffff;
  if(a<0xfff042||a>=0xfff442||(a-0xfff042)%64)abort();provenValues[(a+48)&65535]=get(9);provenPC[(a+48)&65535]=currentPC;
 }
}
static int compare(unsigned checkpoint,const uint8_t*sram){
 for(unsigned n=0;n<65536;n++){
  if(!(n&1)&&n<=65532){unsigned a=willowMotionLong(states[0]+n),b=willowMotionLong(states[1]+n);if(a!=b&&samePointer(a,b)){
   if(!provenPC[n]||provenValues[n]!=a){fprintf(stderr,"Unproven RAM pair check%u at%x %x/%x\n",checkpoint,n,a,b);return 1;}pointerPairs++;n+=3;continue;
  }}
  if(states[0][n]!=states[1][n]){fprintf(stderr,"Source startup RAM mismatch check%u at%x %x/%x\n",checkpoint,n,states[0][n],states[1][n]);return 1;}
 }
 for(unsigned n=0;n<21;n++)if(registers[0][n]!=registers[1][n]&&!samePointer(registers[0][n],registers[1][n])){fprintf(stderr,"Source startup register mismatch check%u r%u %x/%x\n",checkpoint,n,registers[0][n],registers[1][n]);return 1;}
 for(unsigned r=0;r<6;r++)if(memcmp(referenceVideo+(regions[r].source-0x900000),sram+(regions[r].target-0x200000),regions[r].size)){fprintf(stderr,"Source startup video mismatch check%u region%u\n",checkpoint,r);return 1;}
 if(memcmp(referencePorts,sram+(IO_MIRROR-0x200000),512)){fprintf(stderr,"Source startup port mismatch check%u\n",checkpoint);return 1;}
 if(eventCounts[0]!=eventCounts[1]||memcmp(events[0],events[1],eventCounts[0]*sizeof(IOEvent))){fprintf(stderr,"Source startup event mismatch check%u %u/%u\n",checkpoint,eventCounts[0],eventCounts[1]);return 1;}
 return 0;
}
int main(int argc,char**argv){
 if(argc!=2&&(argc!=3||strcmp(argv[2],"--unmapped-row-scroll")))return 2;
 unsigned unmappedRowScroll=argc==3;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(images[0],1,0x100000,f)!=0x100000)return 2;fclose(f);
 f=fopen("build/native-video-relocation/io-test-input.bin","rb");if(!f||fread(images[1],1,0x100000,f)!=0x100000)return 2;fclose(f);
 if(willowMotionLong(images[0]+0xa0)!=0x16be||willowMotionLong(images[0]+0x68)!=0x1554||willowMotionLong(images[0]+0xb4)!=0x402||willowMotionLong(images[0]+0x15b4)!=0x4cdf7fff||willowMotionWord(images[0]+0x15b8)!=0x4e73)return 2;
 for(unsigned n=0;n<2;n++){
  uint8_t*p=images[n];willowMotionPutLong(p,0xffff00);willowMotionPutLong(p+4,0x100000);willowMotionPutLong(p+0xb4,0x100016);
  memcpy(p+0x100,"SEGA GENESIS    ",16);memcpy(p+0x1f0,"U               ",16);p[0x1b0]='R';p[0x1b1]='A';p[0x1b2]=0xf8;p[0x1b3]=0x20;willowMotionPutLong(p+0x1b4,0x200000);willowMotionPutLong(p+0x1b8,0x20ffff);
 }
 memcpy(rom,images[0],0x100000);willowMotionPutWord(rom+0x100000,0x60fe);willowMotionPutWord(rom+0x10000e,0x4e71);
 f=fopen("build/native-video-relocation/source-irq-probe.bin","rb");if(!f||fread(rom+0x100010,1,0x100,f)!=18)return 2;fclose(f);
 willowMotionPutWord(rom+0x100100,0x13fc);willowMotionPutWord(rom+0x100102,1);willowMotionPutLong(rom+0x100104,0xa130f1);willowMotionPutWord(rom+0x100108,0x4e75);
 f=fopen("build/native-video-relocation/source-startup-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));FN(void,m68k_run,(unsigned));FN(void*,retro_get_memory_data,(unsigned));FN(size_t,retro_get_memory_size,(unsigned));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 struct retro_game_info game={"build/native-video-relocation/source-startup-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram"),*sram=retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);ForestCoreCycles*cpu=dlsym(core,"m68k");if(!ram||!sram||!cpu||retro_get_memory_size(RETRO_MEMORY_SAVE_RAM)<65536)return 2;
 const unsigned pages[]={0x80,0x90,0x91,0x92};for(unsigned n=0;n<4;n++)memcpy(savedMaps[n],cpu->map+pages[n],sizeof savedMaps[n]);
 ram[0xff00^1]=0;ram[0xff01^1]=0x10;ram[0xff02^1]=ram[0xff03^1]=0;
 m68k_set_reg(17,0x2700);m68k_set_reg(15,0xffff00);m68k_set_reg(16,0x100100);retro_run();if(m68k_get_reg(16)!=0x100000)return 2;
 m68k_set_reg(16,0x10000e);if(cpu->pc!=0x10000e)return 2;for(unsigned n=0;n<16;n++)if(cpu->dar[n]!=m68k_get_reg(n))return 2;
 cpu->cycles=0;cpu->refresh=896;m68k_run(1);if(cpu->cycles!=28||m68k_get_reg(16)!=0x100010)return 2;
 memset(states,0xa5,sizeof states);memset(cps,0,sizeof cps);memset(ports,255,sizeof ports);memset(sram,0,65536);memset(sram+(IO_MIRROR-0x200000),255,512);
 unsigned totalChecks=0;unsigned long long totalCycles=0;
 /* Each pass is an observation checkpoint, NOT a frame or VBlank cadence.
  * Original A0 task intentionally yields ready, so the scheduler keeps sweeping. */
 for(unsigned checkpoint=0;checkpoint<18;checkpoint++){
  unsigned costs[2];
  for(path=0;path<2;path++){
   selectProgram(cpu,path);for(unsigned n=0;n<65536;n++)ram[n^1]=states[path][n];eventCounts[path]=0;
   if(!checkpoint){
    for(unsigned n=0;n<15;n++)m68k_set_reg(n,0x71510000+n*0x1111);
    m68k_set_reg(12,0x5b6);m68k_set_reg(17,0x2700);m68k_set_reg(19,0xff0f00);m68k_set_reg(15,0xffff00);m68k_set_reg(16,0x6f2);
   }else{
    m68k_set_reg(17,0x2700);m68k_set_reg(19,registers[path][19]);for(unsigned n=0;n<16;n++)m68k_set_reg(n,registers[path][n]);m68k_set_reg(17,registers[path][17]);m68k_set_reg(16,0x14cc);
   }
   cpu->cycles=0;cpu->refresh=896;unsigned steps=0;
   while((m68k_get_reg(16)!=0x14cc||!steps)&&steps++<2000000){
    currentPC=m68k_get_reg(16);if(!path)observeSourceStores(m68k_get_reg);
    unsigned index=0;for(;index<sizeof ioAllPorts/sizeof ioAllPorts[0];index++)if(ioAllPorts[index].pc==currentPC)break;
    unsigned value=0;if(path&&index<sizeof ioAllPorts/sizeof ioAllPorts[0]&&!ioAllPorts[index].write){const uint8_t*p=sram+(ioAllPorts[index].target-0x200000);value=ioAllPorts[index].width==1?*p:willowMotionWord(p);}
    if(path&&!(m68k_get_reg(17)&0x2000)){
     unsigned slot=ramLong(ram,0x204)&0xffffff,sp=m68k_get_reg(15)&0xffffff;
     if(slot<0xff0000||slot>=0xff0200||(slot&31))return 1;unsigned top=ramLong(ram,(slot&65535)+12);
     if(top!=willowMotionLong(images[0]+0x17dc+(slot-0xff0000)/8)||sp>top||sp<top-128){fprintf(stderr,"Original128-byte task stack escaped slot%x SP%x top%x PC%x\n",slot,sp,top,currentPC);return 1;}
     if(top-sp>taskDepth)taskDepth=top-sp;
    }
    if(path&&currentPC==0x1754)sleepTraps++;if(path&&currentPC==0x1784)readyTraps++;
    m68k_run(cpu->cycles+1);
    if(path&&index<sizeof ioAllPorts/sizeof ioAllPorts[0]){if(ioAllPorts[index].write){const uint8_t*p=sram+(ioAllPorts[index].target-0x200000);value=ioAllPorts[index].width==1?*p:willowMotionWord(p);}portEvent(ioAllPorts[index].source,ioAllPorts[index].width,ioAllPorts[index].write,value);}
   }
   if(m68k_get_reg(16)!=0x14cc){fprintf(stderr,"Source scheduler did not reach boundary check%u PC%x\n",checkpoint,m68k_get_reg(16));return 1;}
   for(unsigned n=0;n<65536;n++)states[path][n]=ram[n^1];for(unsigned n=0;n<21;n++)registers[path][n]=m68k_get_reg(n);costs[path]=cpu->cycles;
   if(!path){memcpy(referenceVideo,cps,sizeof cps);memcpy(referencePorts,ports,512);}
  }
  if(costs[0]!=costs[1]){fprintf(stderr,"Startup cycle mismatch check%u %u/%u\n",checkpoint,costs[0],costs[1]);return 1;}
  if(compare(checkpoint,sram))return 1;
  if(!checkpoint){
   for(unsigned n=0;n<16;n++)if(willowMotionLong(states[1]+n*32+12)!=willowMotionLong(images[0]+0x17dc+n*4))return 1;
   if(willowMotionLong(states[1]+4)!=0x7fda||willowMotionLong(states[1]+0xa4)!=0x1ddc||states[1][0]!=12||states[1][0xa0]!=12)return 1;
   printf("Source6F2 clear +5B6 defaults +13FC startup: original tasks0/7FDA,A0/1DDC and all16 native128-byte task stacks match; %u cycles\n",costs[1]);
  }
  totalChecks++;totalCycles+=costs[1];
 }
 printf("Original scheduler: %u observation checkpoints, %u sleep traps, %u ready-yield traps, task stack peak%u/128 bytes, %u proven RAM pointer pairs; exact cycles and ordered I/O match. These are scheduler observations, NOT frames. Total%llu mastercycles\n",totalChecks,sleepTraps,readyTraps,taskDepth,pointerPairs,totalCycles);
 static uint8_t bootStates[2][65536],bootVideo[0x20000];memcpy(bootStates,states,sizeof states);memcpy(bootVideo,cps,sizeof cps);
 unsigned irqPeak=0,irqStackPeak=0;unsigned long long irqCycles=0,irqEvents=0;
 for(unsigned sample=0;sample<(unmappedRowScroll?1u:256u);sample++){
  unsigned costs[2];
  for(path=0;path<2;path++){
   selectProgram(cpu,path);memcpy(states[path],bootStates[path],65536);
   /* Boundary fixtures vary IRQ inputs; source instructions own all results. */
   states[path][0x8349]=(sample&128)?0x80:0;
   states[path][0x83d6]=unmappedRowScroll?0x80:(sample&32)?1:0;
   states[path][0x828f]=(sample&16)?1:0;states[path][0x8293]=0;
   for(unsigned n=0;n<16;n++){states[path][n*32]=1;states[path][n*32+1]=(unsigned[]){0,1,255}[(sample+n)%3];}
   for(unsigned n=0;n<65536;n++)ram[n^1]=states[path][n];
   memcpy(cps,bootVideo,sizeof cps);for(unsigned n=0;n<6;n++)memcpy(sram+(regions[n].target-0x200000),bootVideo+(regions[n].source-0x900000),regions[n].size);
   memset(ports,255,512);const unsigned inputPorts[]={0,1,0x11,0x18,0x19,0x1a,0x1c,0x1e};
   for(unsigned n=0;n<8;n++)ports[inputPorts[n]]=(uint8_t)(sample*(n*2+1)+n*17);
   memcpy(sram+(IO_MIRROR-0x200000),ports,512);eventCounts[path]=0;
   unsigned initial[21],initialSR=(sample&31)|((sample&32)?0:0x2000)|((sample&64)?0x0700:0);
   m68k_set_reg(17,0x2700);m68k_set_reg(15,0xffffc0);m68k_set_reg(19,0xff0f00);
   for(unsigned n=0;n<15;n++)m68k_set_reg(n,0x51710000+n*0x1111+sample);m68k_set_reg(13,0xffff8000);
   m68k_set_reg(17,initialSR);m68k_set_reg(16,0x100010);for(unsigned n=0;n<21;n++)initial[n]=m68k_get_reg(n);
   cpu->cycles=0;cpu->refresh=896;unsigned steps=0,frameObserved=0,entryObserved=0,returnObserved=0;
   while(m68k_get_reg(16)!=0x100012&&steps++<200000){
    currentPC=m68k_get_reg(16);
    if(currentPC==0x100016){
     unsigned sp=m68k_get_reg(15)&0xffffff;
     if(sp!=0xffffba||(((unsigned)ram[(sp&65535)^1]<<8)|ram[((sp+1)&65535)^1])!=initialSR||ramLong(ram,(sp+2)&65535)!=0x100012){fprintf(stderr,"Incorrect realTRAP exception frame sample%u SP%x SR%x\n",sample,sp,initialSR);return 1;}frameObserved++;
    }
    if(currentPC==0x1554){if((m68k_get_reg(17)&0x2700)!=0x2200)return 1;entryObserved++;irqEntries++;}
    if(currentPC==0x15b8){returnObserved++;irqReturns++;}
    if(m68k_get_reg(17)&0x2000){unsigned sp=m68k_get_reg(15)&0xffffff;if(sp<0xfffc00||sp>0xffffc0){fprintf(stderr,"IRQ stack bounds sample%u SP%x PC%x\n",sample,sp,currentPC);return 1;}if(0xffffc0-sp>irqStackPeak)irqStackPeak=0xffffc0-sp;}
    unsigned index=0;for(;index<sizeof ioAllPorts/sizeof ioAllPorts[0];index++)if(ioAllPorts[index].pc==currentPC)break;
    unsigned value=0;if(path&&index<sizeof ioAllPorts/sizeof ioAllPorts[0]&&!ioAllPorts[index].write){const uint8_t*p=sram+(ioAllPorts[index].target-0x200000);value=ioAllPorts[index].width==1?*p:willowMotionWord(p);}
    m68k_run(cpu->cycles+1);
    if(path&&index<sizeof ioAllPorts/sizeof ioAllPorts[0]){if(ioAllPorts[index].write){const uint8_t*p=sram+(ioAllPorts[index].target-0x200000);value=ioAllPorts[index].width==1?*p:willowMotionWord(p);}portEvent(ioAllPorts[index].source,ioAllPorts[index].width,ioAllPorts[index].write,value);}
   }
   if(m68k_get_reg(16)!=0x100012||frameObserved!=1||entryObserved!=1||returnObserved!=1){fprintf(stderr,"FullIRQ failed return sample%u PC%x observations%u/%u/%u\n",sample,m68k_get_reg(16),frameObserved,entryObserved,returnObserved);return 1;}
   for(unsigned n=0;n<21;n++)if(n!=16&&m68k_get_reg(n)!=initial[n]){fprintf(stderr,"FullIRQ failed restored register sample%u r%u got%x wanted%x\n",sample,n,m68k_get_reg(n),initial[n]);return 1;}
   for(unsigned n=0;n<65536;n++)states[path][n]=ram[n^1];for(unsigned n=0;n<21;n++)registers[path][n]=m68k_get_reg(n);costs[path]=cpu->cycles;
   if(!path){memcpy(referenceVideo,cps,sizeof cps);memcpy(referencePorts,ports,512);}
  }
  if(costs[0]!=costs[1]){fprintf(stderr,"FullIRQ cycle mismatch sample%u %u/%u\n",sample,costs[0],costs[1]);return 1;}
  if(compare(18+sample,sram))return 1;
  irqCycles+=costs[1];irqEvents+=eventCounts[1];if(costs[1]>irqPeak)irqPeak=costs[1];
 }
 printf("Full source1554 IRQ: 256 nativeTRAP/realRTE cases, supervisor/user interrupted contexts, allCCRbits and masks0/7, sleep0/1/255, zero/positive transitions and dirty foreground clears; every register/SR/SP/USP/SSP restored, fullRAM/sixvideo/ports match, %llu ordered events and exact cycles; mean/peak%.3f/%u mastercycles, stack peak%u bytes, %u entries/%u RTEs across both paths. PhysicalVBlank cadence and negative transition row-scroll remain unadmitted.\n",irqEvents,(double)irqCycles/256,irqPeak,irqStackPeak,irqEntries,irqReturns);
 return 0;
}
