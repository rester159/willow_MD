/* Closed instruction trace from a genuine serialized original-bus state.
 * No m68k_set_reg, injected interrupt, opcode patch or logical normalization.
 * This diagnostic never feeds state back to the continuous timing observer. */
#include "vendor/libretro.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
static uint8_t rom[0x400000],source[123392],cps[0x22800],ports[512];
static uint8_t*ram;
static FILE*accesses;
static unsigned instructionPC,step;
static bool env(unsigned cmd,void*data){
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
#define FN(ret,n,args) ret(*n)args=dlsym(core,#n);if(!n)return 2
typedef struct{
 struct{void*base;unsigned(*read8)(unsigned);unsigned(*read16)(unsigned);void(*write8)(unsigned,unsigned);void(*write16)(unsigned,unsigned);}map[256];
 struct{unsigned pc,cycle,detected;}poll;
 int cycles,refresh;unsigned end,dar[16],pc;
}Core;
static Core*cpu;
static unsigned word(const uint8_t*p){return p[0]*256u+p[1];}
static uint8_t*busByte(unsigned at){
 at&=0xffffff;
 if(at>=0xff0000)return ram+((at&65535)^1);
 if(at>=0x800000&&at<0x800200)return ports+at-0x800000;
 if(at>=0x900000&&at<0x922800)return cps+at-0x900000;
 fprintf(stderr,"Unrepresented source address%x\n",at);abort();
}
static unsigned read8(unsigned at){unsigned v=*busByte(at);fprintf(accesses,"%u,%x,R,1,%x,%x,%x\n",step,instructionPC,at,v,cpu->dar[0]);return v;}
static unsigned read16(unsigned at){unsigned v=*busByte(at)*256u+*busByte(at+1);fprintf(accesses,"%u,%x,R,2,%x,%x,%x\n",step,instructionPC,at,v,cpu->dar[0]);return v;}
static void write8(unsigned at,unsigned v){fprintf(accesses,"%u,%x,W,1,%x,%x,%x\n",step,instructionPC,at,v&255,cpu->dar[0]);*busByte(at)=v;}
static void write16(unsigned at,unsigned v){fprintf(accesses,"%u,%x,W,2,%x,%x,%x\n",step,instructionPC,at,v&65535,cpu->dar[0]);*busByte(at)=v>>8;*busByte(at+1)=v;}
static unsigned readFile(const char*path,void*data,unsigned maximum){FILE*f=fopen(path,"rb");if(!f)abort();unsigned bytes=fread(data,1,maximum,f);if(fgetc(f)!=EOF)abort();fclose(f);return bytes;}
int main(int argc,char**argv){
 if(argc!=9)return 2;
 unsigned romBytes=readFile(argv[2],rom,sizeof rom);if(romBytes!=sizeof rom)return 2;
 if(readFile(argv[4],source,sizeof source)!=sizeof source)return 2;
 memcpy(cps+0x18000,source,16384);memcpy(cps+0x14000,source+16384,16384);memcpy(cps+0x10000,source+32768,16384);
 memcpy(cps+0x1c000,source+49152,6144);memcpy(ports,source+55296,512);memcpy(cps+0x22000,source+55808,2048);
 uint8_t exactCPS[sizeof cps];if(readFile(argv[6],exactCPS,sizeof exactCPS)!=sizeof exactCPS)return 2;
 const unsigned offsets[]={0x18000,0x14000,0x10000,0x1c000,0x22000},sizes[]={16384,16384,16384,6144,2048};
 for(unsigned n=0;n<5;n++)if(memcmp(cps+offsets[n],exactCPS+offsets[n],sizes[n]))return 2;
 memcpy(cps,exactCPS,sizeof cps);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core)return 2;
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(bool,retro_unserialize,(const void*,size_t));FN(size_t,retro_serialize_size,(void));FN(void*,retro_get_memory_data,(unsigned));FN(unsigned,m68k_get_reg,(int));FN(void,m68k_run,(unsigned));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 struct retro_game_info game={argv[2],rom,romBytes,NULL};if(!retro_load_game(&game))return 2;
 size_t stateBytes=retro_serialize_size();void*state=malloc(stateBytes);if(readFile(argv[3],state,stateBytes)!=stateBytes||!retro_unserialize(state,stateBytes))return 2;
 cpu=dlsym(core,"m68k");ram=dlsym(core,"work_ram");uint8_t*sram=retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);if(!cpu||!ram||!sram)return 2;
 if(readFile(argv[5],sram,65536)!=65536)return 2;
 uint8_t registers[84];if(readFile(argv[7],registers,sizeof registers)!=sizeof registers)return 2;
 for(unsigned n=0;n<21;n++){unsigned expected=word(registers+n*4)*65536u+word(registers+n*4+2);if(m68k_get_reg(n)!=expected){fprintf(stderr,"Restored register%u differs%x/%x\n",n,m68k_get_reg(n),expected);return 2;}}
 for(unsigned n=0;n<65536;n++)if(ram[n^1]!=source[57856+n]){fprintf(stderr,"Restored RAM differs%x\n",n);return 2;}
 const unsigned pages[]={0x80,0x90,0x91,0x92,0xff};for(unsigned n=0;n<5;n++){unsigned page=pages[n];cpu->map[page].read8=read8;cpu->map[page].read16=read16;cpu->map[page].write8=write8;cpu->map[page].write16=write16;}
 char path[4096];snprintf(path,sizeof path,"%s.instructions.csv",argv[8]);FILE*trace=fopen(path,"w");snprintf(path,sizeof path,"%s.accesses.csv",argv[8]);accesses=fopen(path,"w");if(!trace||!accesses)return 2;
 fputs("step,pc,opcode,extension,d0_before,d0_after,d1_before,d1_after,next_pc,sr_before,sr_after,sp_before,sp_after,cycles\n",trace);fputs("step,pc,kind,width,address,value,current_d0\n",accesses);
 unsigned initial=cpu->dar[0],ended=0;
 for(step=0;step<10000;step++){
  instructionPC=m68k_get_reg(16);if(instructionPC+4>sizeof rom){fprintf(stderr,"Trace left ROM%x\n",instructionPC);return 2;}
  unsigned d0=cpu->dar[0],d1=cpu->dar[1],sp=cpu->dar[15],sr=m68k_get_reg(17),before=cpu->cycles;
  m68k_run(cpu->cycles+1);
  fprintf(trace,"%u,%x,%04x,%04x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%u\n",step,instructionPC,word(rom+instructionPC),word(rom+instructionPC+2),d0,cpu->dar[0],d1,cpu->dar[1],m68k_get_reg(16),sr,m68k_get_reg(17),sp,cpu->dar[15],(unsigned)cpu->cycles-before);
  if((cpu->dar[0]&0xffff0000)!=(initial&0xffff0000)){ended=1;break;}
 }
 fclose(trace);fclose(accesses);printf("Closed source continuation steps%u, initialD0%x, finalD0%x, finalPC%x, upperword_changed%u\n",step+1,initial,cpu->dar[0],m68k_get_reg(16),ended);
 return ended?0:1;
}
