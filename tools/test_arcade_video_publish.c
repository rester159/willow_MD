/* Differential verification of shared services against original instructions. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_video_publish.h"
#include "../src/willow_native_boot_globals.h"
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
static uint8_t expected[65536],accessed[65536];
static uint16_t rw(uint16_t a){accessed[a]=accessed[(uint16_t)(a+1)]=1;return willowMotionWord(expected+a);}
static uint8_t rb(uint16_t a){accessed[a]=1;return expected[a];}
static void ww(uint16_t a,uint16_t v){accessed[a]=accessed[(uint16_t)(a+1)]=1;willowMotionPutWord(expected+a,v);}
static void wb(uint16_t a,uint8_t v){accessed[a]=1;expected[a]=v;}
static uint8_t in(uint16_t at){return rb(0x4000+at);}
static void outByte(uint16_t at,uint8_t v){wb(0x4000+at,v);}
static void outWord(uint16_t at,uint16_t v){ww(0x4000+at,v);}
int main(int argc,char**argv){
 if(argc!=2 && (argc!=3 || (strcmp(argv[2],"--compiled") && strcmp(argv[2],"--controller-fixtures") && strcmp(argv[2],"--irq-prefix") && strcmp(argv[2],"--compiled-irq-prefix"))))return 2;
 int irqPrefix=argc==3&&(!strcmp(argv[2],"--irq-prefix")||!strcmp(argv[2],"--compiled-irq-prefix"));
 int exportControllers=argc==3&&!strcmp(argv[2],"--controller-fixtures");
 FILE*fixtures=exportControllers?fopen("build/controller-history-original.bin","wb"):NULL;
 if(exportControllers&&!fixtures)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 /* Relocate only absolute hardware operands in this bounded instruction
  * sequence. No branches, source RAM accesses or arithmetic are changed. */
 unsigned relocated=0;
 for(unsigned at=0x18d0;at<0x1a8c;at+=2){
  uint32_t a=willowMotionLong(rom+at);
  if(a>=0x800000&&a<=0x800170){willowMotionPutLong(rom+at,0xff4000+a-0x800000);relocated++;at+=2;}
 }
 if(relocated!=26){fprintf(stderr,"Unexpected hardware operand count %u\n",relocated);return 2;}
 if(irqPrefix){
  if(willowMotionWord(rom+0x1558)!=0x33ed||willowMotionLong(rom+0x155c)!=0x800100||willowMotionWord(rom+0x1564)!=0x6100)return 2;
  /* End before the next IRQ service. Keep the original 18D0 BSR intact. */
  willowMotionPutLong(rom+0x155c,0xff4100);willowMotionPutWord(rom+0x1564,0x4e75);
 }
 unsigned target=0;
 if(argc==3&&!exportControllers&&strcmp(argv[2],"--irq-prefix")){
  f=fopen("build/video_publish_probe.bin","rb");if(!f)return 2;
  if(!fread(rom+0x100010,1,sizeof rom-0x100010,f))return 2;fclose(f);
  f=fopen("build/video_publish_probe.sym","r");if(!f)return 2;
  unsigned address;char line[512],kind,name[256];
  while(fgets(line,sizeof line,f))if(sscanf(line,"%x %c %255s",&address,&kind,name)==3&&!strcmp(name,irqPrefix?"videoIrqPrefixProbe":"videoPublishProbe"))target=address;
  fclose(f);if(!target)return 2;
 }
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/video-publish-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/video-publish-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 const WillowVisibilityBus bus={rw,rb,ww,wb};uint32_t random=0x315927ab;
 for(unsigned sample=0;sample<4096;sample++){
  for(unsigned n=0;n<65536;n++){random^=random<<13;random^=random>>17;random^=random<<5;expected[n]=random;}
  static uint8_t before[65536];memcpy(before,expected,sizeof before);
  if(irqPrefix)willowArcadeObjectBasePublish(&bus,outWord);
  willowArcadeVideoPublish(&bus,in,outByte,outWord);
  for(unsigned mode=0;mode<(target?2u:1u);mode++){
  for(unsigned n=0;n<65536;n++)ram[n^1]=before[n];
  putRamWord(ram,0xff00,0x0010);putRamWord(ram,0xff02,0);
  m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00);
  m68k_set_reg(0,random);m68k_set_reg(1,~random);m68k_set_reg(16,mode?target:irqPrefix?0x1558:0x18d0);retro_run();
  if(m68k_get_reg(16)!=0x100000)return 1;
  for(unsigned n=0;n<65536;n++)if(n<(mode?0xfe00u:irqPrefix?0xfefcu:0xff00u)||n>=0xff04){
   if(ram[n^1]!=expected[n]){fprintf(stderr,"Publication mismatch mode %u sample %u address %04x got %02x want %02x\n",mode,sample,n,ram[n^1],expected[n]);return 1;}
  }
  if(fixtures && mode==0){
   uint8_t record[10];memcpy(record,before+0x8060,4);
   record[4]=(uint8_t)~before[0x4001];record[5]=(uint8_t)~before[0x4000];
   for(unsigned n=0;n<4;n++)record[6+n]=ram[(0x8060+n)^1];
   if(fwrite(record,1,sizeof record,fixtures)!=sizeof record)return 2;
  }
 }
 }
 if(fixtures){if(fclose(fixtures))return 2;puts("Exported 4096 original controller-history projections from the full IRQ service");}
 if(irqPrefix)puts("Original IRQ prefix 1558..1564: object base publishes before video/input service; 4096 states match full hardware fixture and source RAM");
 if(target)puts("Compiled 68000 publication: 4096 states match the same original-instruction oracle, including all unrelated non-stack RAM");
 puts("Original IRQ video/input publication: 4096 randomized states match hardware registers, staged scrolls, flip, input history and unrelated RAM");
 WillowNativeBootGlobals globals;unsigned missing=0,observed=0;
 f=fopen("build/video-publish-storage.csv","w");if(!f)return 2;
 fputs("source_address,mapped_in_native_boot_bus\n",f);
 for(unsigned a=0;a<65536;a++)if(accessed[a] && !(a>=0x4000 && a<0x4200)){
  unsigned mapped=willowNativeBootGlobalsResolve(&globals,a,1)!=NULL;
  fprintf(f,"%04x,%u\n",a,mapped);observed++;missing+=!mapped;
 }
 fclose(f);printf("Publication source state: %u touched bytes; %u absent from current native boot bus (hardware fixture excluded)\n",observed,missing);
 return 0;
}
