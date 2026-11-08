/* Headless video callback capture of native VDP publication. No target writes. */
#include "vendor/libretro.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <dlfcn.h>
static int format;
static unsigned frame,captured;
static const char*output;
#ifdef WILLOW_PRESENTATION_PHASES
/* Optional diagnostic bus observation. This handles only actual native word
 * stores using the core's original RAM semantics; no values are injected and
 * neither CPU registers nor cycle accounting are modified. */
typedef struct {
 struct {void*base;unsigned(*read8)(unsigned);unsigned(*read16)(unsigned);void(*write8)(unsigned,unsigned);void(*write16)(unsigned,unsigned);} map[256];
 struct {unsigned pc,cycle,detected;} poll;
 int cycles,refresh;unsigned end,dar[16],pc;
} PresentationCPU;
static PresentationCPU*profileCPU;
static uint8_t*profileRAM;
static void(*originalWrite16)(unsigned,unsigned);
static uint64_t phaseCycles[6];
static unsigned phaseCount;
static void profileWrite16(unsigned address,unsigned value){
 if((address&0xffffff)==0xff0004){
  if(value!=phaseCount+1||phaseCount>=6){fprintf(stderr,"Invalid native phase order\n");exit(1);}
  phaseCycles[phaseCount++]=(uint64_t)frame*896040+(int64_t)profileCPU->cycles;
 }
 if(originalWrite16)originalWrite16(address,value);
 else{address&=65535;profileRAM[address^1]=value>>8;profileRAM[(address+1)^1]=value;}
}
#endif
static bool env(unsigned cmd,void*data){
 if(cmd==RETRO_ENVIRONMENT_SET_PIXEL_FORMAT){format=*(int*)data;return true;}
 if(cmd==RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY||cmd==RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY){*(const char**)data="/tmp";return true;}
 if(cmd==RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE){*(bool*)data=false;return true;}
 if(cmd==RETRO_ENVIRONMENT_GET_VARIABLE){struct retro_variable*v=data;if(!strcmp(v->key,"genesis_plus_gx_addr_error")){v->value="enabled";return true;}if(!strcmp(v->key,"genesis_plus_gx_system_hw")){v->value="auto";return true;}return false;}
 if(cmd==RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION){*(unsigned*)data=0;return true;}
 return cmd==RETRO_ENVIRONMENT_SET_VARIABLES||cmd==RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS||cmd==RETRO_ENVIRONMENT_SET_CONTROLLER_INFO;
}
static void video(const void*d,unsigned w,unsigned h,size_t pitch){
 if(frame!=30)return;
 if(!d||w!=320||h!=224||format<0||format>2){fprintf(stderr,"Unexpected video frame %ux%u format%d\n",w,h,format);exit(1);}
 FILE*f=fopen(output,"wb");if(!f){perror(output);exit(2);}fprintf(f,"P6\n%u %u\n255\n",w,h);
 for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++){
  const uint8_t*p=(const uint8_t*)d+y*pitch+x*(format==1?4:2);uint8_t rgb[3];
  if(format==1){uint32_t v;memcpy(&v,p,4);rgb[0]=v>>16;rgb[1]=v>>8;rgb[2]=v;}
  else{uint16_t v;memcpy(&v,p,2);rgb[0]=((v>>(format==2?11:10))&31)*255/31;rgb[1]=((v>>5)&(format==2?63:31))*255/(format==2?63:31);rgb[2]=(v&31)*255/31;}
  if(fwrite(rgb,1,3,f)!=3)exit(2);
 }
 fclose(f);captured++;
}
static void audio(int16_t l,int16_t r){(void)l;(void)r;}
static size_t batch(const int16_t*d,size_t n){(void)d;return n;}
static void poll(void){}
static int16_t input(unsigned p,unsigned d,unsigned i,unsigned id){(void)p;(void)d;(void)i;(void)id;return 0;}
#define FN(ret,n,args) ret(*n)args=dlsym(core,#n);if(!n){fprintf(stderr,"Missing core symbol %s\n",#n);return 2;}
int main(int argc,char**argv){
 if(argc!=4){fprintf(stderr,"Usage: %s CORE ROM OUTPUT.ppm\n",argv[0]);return 2;}output=argv[3];
 FILE*f=fopen(argv[2],"rb");if(!f)return 2;fseek(f,0,SEEK_END);long size=ftell(f);rewind(f);if(size<=0)return 2;
 uint8_t*rom=malloc((size_t)size);if(!rom||fread(rom,1,(size_t)size,f)!=(size_t)size)return 2;fclose(f);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,retro_unload_game,(void));FN(void,retro_deinit,(void));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 struct retro_game_info game={argv[2],rom,(size_t)size,NULL};if(!retro_load_game(&game))return 2;
#ifdef WILLOW_PRESENTATION_PHASES
 profileCPU=dlsym(core,"m68k");profileRAM=dlsym(core,"work_ram");
 FN(unsigned,m68k_get_reg,(int));
 uint8_t*pal=dlsym(core,"vdp_pal");uint16_t*lines=dlsym(core,"lines_per_frame");
 if(!profileCPU||!profileRAM||!pal||!lines||*pal||*lines!=262||profileCPU->pc!=m68k_get_reg(16))return 2;
 for(unsigned n=0;n<16;n++)if(profileCPU->dar[n]!=m68k_get_reg(n))return 2;
 originalWrite16=profileCPU->map[255].write16;
 profileCPU->map[255].write16=profileWrite16;
#endif
 for(frame=0;frame<=30;frame++)retro_run();
 const uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 uint32_t ready=0;for(unsigned n=0;n<4;n++)ready=ready<<8|ram[n^1];
 if(ready!=0x56445031||captured!=1){fprintf(stderr,"Native publication incomplete: marker%x captured%u\n",ready,captured);return 1;}
 printf("Native VDP buffers published; captured320x224 pixels after31 emulated frames; format%d; no host target writes or GUI.\n",format);
#ifdef WILLOW_PRESENTATION_PHASES
 if(phaseCount!=6){fprintf(stderr,"Incomplete native timing phases %u\n",phaseCount);return 1;}
 const char*names[]={"text_budget","raw_capture","object_build_and_sat","diagnostic_assertions","static_vdp_upload"};
 for(unsigned n=0;n<5;n++){
  if(phaseCycles[n+1]<=phaseCycles[n])return 1;
  unsigned long long elapsed=phaseCycles[n+1]-phaseCycles[n];
  printf("PHASE %s %llu %.9f\n",names[n],elapsed,(double)elapsed/53693.175);
 }
 profileCPU->map[255].write16=originalWrite16;
#endif
#ifdef WILLOW_PRESENTATION_VRAM_DUMP
 const uint8_t*vr=dlsym(core,"vram");char dumpPath[4096];
 if(!vr||snprintf(dumpPath,sizeof dumpPath,"%s.vram",output)>=(int)sizeof dumpPath)return 2;
 FILE*dump=fopen(dumpPath,"wb");if(!dump)return 2;
 for(unsigned n=0;n<65536;n++)if(fputc(vr[n^1],dump)==EOF)return 2;
 if(fclose(dump))return 2;
#endif
 retro_unload_game();retro_deinit();free(rom);return 0;
}
