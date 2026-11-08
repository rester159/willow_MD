/* Retained-state differential check of complete original 395D0 behavior. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_stage_object26.h"
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

static uint8_t expected[65536];

static uint16_t rw(uint16_t a){return willowMotionWord(expected+a);}
static uint16_t word(uint32_t a){return willowMotionWord(rom+a);}
static uint8_t rb(uint16_t a){return expected[a];}
static void ww(uint16_t a,uint16_t v){willowMotionPutWord(expected+a,v);}
static void wb(uint16_t a,uint8_t v){expected[a]=v;}


int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
#ifdef WILLOW_TEST_COMPILED_STAGE26
 f=fopen("build/native_stage_object26_probe.bin","rb");if(!f)return 2;
 size_t targetBytes=fread(rom+0x110000,1,0x10000,f);int extra=fgetc(f);fclose(f);
 if(!targetBytes||extra!=EOF)return 2;
 unsigned targetEntry=0,parsed;char symbolLine[256],kind,name[128];
 f=fopen("build/native_stage_object26_probe.sym","r");if(!f)return 2;
 while(fgets(symbolLine,sizeof symbolLine,f))if(sscanf(symbolLine,"%x %c %127s",&parsed,&kind,name)==3&&!strcmp(name,"stageObject26Probe"))targetEntry=parsed;
 fclose(f);if(targetEntry<0x110000||targetEntry>=0x110000+targetBytes)return 2;
#endif
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);
 willowMotionPutLong(rom+0x80,0x15d0);
 willowMotionPutLong(rom+0x9c,0x160e);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/stage-object26-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/stage-object26-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 const WillowVisibilityBus bus={rw,rb,ww,wb};
 unsigned checks=0,spawned=0,failed=0,retired=0,waiting=0,aliases[2]={0},partial=0,full=0,commands=0;

 const uint16_t differences[]={0,1,63,64,65,127,128,129,0xffff,0xffc0};
 for(unsigned sample=0;sample<16384;sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(n*7+sample*31)^0xa5;
  uint16_t r=0x8502,x=sample*509;uint8_t ordinal=(sample/32)%48,mode=(sample/4)%8;
  uint32_t sourceRecord=(sample&1?0xffff0000UL:0xff0000UL)|r;
  wb(r+2,(sample%4)*2);wb(r+16,26);wb(r+17,mode);
  wb(0x8019,(sample/1536)%256);ww(0x8344,(sample/17)%6);wb(0x8395,(sample/19)%2);wb(0x83dd,(sample/31)%2);
  wb(r+20,(sample/9)%3);ww(0x838c,sample*37);
  ww(r+6,x);ww(r+8,x+317);ww(0x984c,x+differences[(sample/16)%10]);ww(0x9850,x+317+differences[(sample/160)%10]);
  ww(r+10,sample*43);ww(r+12,sample*37);uint32_t table=0x39614UL+(mode%7)*8;ww(r+48,table>>16);ww(r+50,table);
  wb(0x8346,sample/13);wb(0x0f5c,(sample/23)%6);wb(0x0f80+mode,(sample/29)%2);ww(0x83d2,(sample/41)%2);
  wb(0x83a6,(sample/43)%2);wb(0x0f87,(sample/47)%2);wb(0x8290,(sample/53)%2);wb(0x8293,(sample/59)%2);ww(0x801e,(sample/61)%64*2);
  wb(0x9801,(sample/37)%5);ww(0x981c,0x912a);for(unsigned n=0;n<4;n++)ww(0x912a+n*2,0x9dc6+n*128);
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  uint32_t a4=0xdeadbeef;
  for(unsigned pass=0;pass<4;pass++){
   uint32_t initialA4=a4;
   unsigned oldState=rb(r+2),oldAvailable=rb(0x9801);
   uint32_t bounds=((uint32_t)rw(r+48)<<16)|rw(r+50);
   unsigned eligible=oldState==2&&(mode==7?rw(r+6)<=rw(0x984c):
    (!rw(0x83d2)&&willowStageObjectRomByte(0x39668UL+(rb(0x8346)>>4),word)>rb(0x0f5c)&&!rb(0x0f80+mode)&&rb(r+20)==1&&
     (uint16_t)(rw(r+6)-rw(0x984c)+word(bounds))<=word(bounds+2)&&(uint16_t)(rw(r+8)-rw(0x9850)+word(bounds+4))<=word(bounds+6)));
   failed+=eligible&&!oldAvailable;
   if(mode==7&&eligible){partial+=oldAvailable>0&&oldAvailable<4;full+=oldAvailable==4;commands+=!rb(0x8290)||!rb(0x8293);}
   if(!willowArcadeStageObject26WithA4(&bus,sourceRecord,ordinal,word,&a4))return 1;
   if(rb(0x9801)<oldAvailable){spawned+=oldAvailable-rb(0x9801);aliases[sourceRecord>>24==255]++;}
   retired+=oldState>=4;waiting+=oldState==2;
   unsigned stop=0x100000;
#ifdef WILLOW_TEST_COMPILED_STAGE26
   static uint8_t originalRam[65536];memcpy(originalRam,ram,sizeof originalRam);
   uint8_t targetStack[16];willowMotionPutLong(targetStack,stop);willowMotionPutLong(targetStack+4,initialA4);willowMotionPutLong(targetStack+8,sourceRecord);willowMotionPutLong(targetStack+12,ordinal);
   for(unsigned j=0;j<16;j++)ram[(0xff00+j)^1]=targetStack[j];
   m68k_set_reg(17,0x2700);m68k_set_reg(15,0xffff00);m68k_set_reg(16,targetEntry);
   for(unsigned slice=0;slice<32&&m68k_get_reg(16)!=stop;slice++)retro_run();
   if(m68k_get_reg(16)!=stop||m68k_get_reg(0)!=a4||m68k_get_reg(15)!=0xffff04){fprintf(stderr,"Compiled boundary mismatch %u/%u\n",sample,pass);return 1;}
   for(unsigned n=0;n<65536;n++)if(!(n>=0xfe80&&n<0xff10)&&ram[n^1]!=expected[n]){
    fprintf(stderr,"Compiled subtype26 mismatch %u/%u at %x\n",sample,pass,n);return 1;
   }
   memcpy(ram,originalRam,sizeof originalRam);
#endif

   ram[0xff00^1]=0;ram[0xff01^1]=0x10;ram[0xff02^1]=0;ram[0xff03^1]=0;
   m68k_set_reg(17,0x2700);m68k_set_reg(13,(sourceRecord&0xffff0000UL)|0x8000UL);m68k_set_reg(15,0xffff00);
   m68k_set_reg(14,sourceRecord);m68k_set_reg(7,ordinal);m68k_set_reg(12,initialA4);m68k_set_reg(0,0);m68k_set_reg(16,0x395d0);
   for(unsigned slice=0;slice<32&&m68k_get_reg(16)!=stop;slice++)retro_run();
   if(m68k_get_reg(16)!=stop||m68k_get_reg(12)!=a4){fprintf(stderr,"Boundary/A4 mismatch %u/%u PC %x A4 %x/%x\n",sample,pass,m68k_get_reg(16),m68k_get_reg(12),a4);return 1;}
   for(unsigned n=0;n<65536;n++)if(!(n>=0xfe80&&n<0xff04)&&ram[n^1]!=expected[n]){
    fprintf(stderr,"Stage object26 mismatch sample %u pass %u at %x actual %x expected %x\n",sample,pass,n,ram[n^1],expected[n]);return 1;
   }
   checks++;
  }
 }
 if(!spawned||!failed||!retired||!waiting||!aliases[0]||!aliases[1]||!partial||!full||!commands)return 1;
 printf("Subtype-twenty-six coverage: %u spawns, %u failed allocations, %u retirements, %u waiting updates\n",spawned,failed,retired,waiting);
 printf("Burst coverage: %u partial, %u full, %u sound commands\n",partial,full,commands);
 printf("Successful allocation address aliases: %u at 00FFxxxx, %u at FFFFxxxx\n",aliases[0],aliases[1]);
 printf("Stage object subtype 26: %u original RAM/A4 boundaries across 16384 retained sequences; phase gates, camera/player bounds, base aliases, sound queue, retry and retirement match\n",checks);
#ifdef WILLOW_TEST_COMPILED_STAGE26
 puts("Compiled 68000 subtype-twenty-six execution matches every retained host/original boundary and A4 result");
#endif
 return 0;
}
