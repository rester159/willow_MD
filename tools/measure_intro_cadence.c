/* Read-only normal-play cadence measurement of an existing intro cartridge.
 * No target RAM/register mutation, snapshot handshake, forced poses or GUI. */
#include "vendor/libretro.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include "native_symbol_lookup.h"
static uint8_t *ram;
static unsigned tickAt,videoCalls,duplicateCalls,walk;
static unsigned word(unsigned at){return ram[at^1]*256u+ram[(at+1)^1];}
static bool env(unsigned cmd,void *data){
 if(cmd==RETRO_ENVIRONMENT_SET_PIXEL_FORMAT)return true;
 if(cmd==RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY||cmd==RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY){*(const char**)data="/tmp";return true;}
 if(cmd==RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE){*(bool*)data=false;return true;}
 if(cmd==RETRO_ENVIRONMENT_GET_VARIABLE){struct retro_variable*v=data;if(!strcmp(v->key,"genesis_plus_gx_addr_error")){v->value="enabled";return true;}if(!strcmp(v->key,"genesis_plus_gx_system_hw")){v->value="auto";return true;}return false;}
 if(cmd==RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION){*(unsigned*)data=0;return true;}
 return cmd==RETRO_ENVIRONMENT_SET_VARIABLES||cmd==RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS||cmd==RETRO_ENVIRONMENT_SET_CONTROLLER_INFO;
}
static void video(const void*d,unsigned w,unsigned h,size_t p){(void)w;(void)h;(void)p;videoCalls++;if(!d)duplicateCalls++;}
static void audio(int16_t l,int16_t r){(void)l;(void)r;}
static size_t batch(const int16_t*d,size_t n){(void)d;return n;}
static void poll(void){}
static int16_t input(unsigned p,unsigned d,unsigned i,unsigned id){(void)i;return walk&&ram&&!p&&d==RETRO_DEVICE_JOYPAD&&id==RETRO_DEVICE_ID_JOYPAD_RIGHT&&word(tickAt)>=935&&word(tickAt)<975;}
#define FN(ret,n,args) ret(*n)args=dlsym(core,#n);if(!n){fprintf(stderr,"Missing core symbol %s\n",#n);return 2;}
int main(int argc,char**argv){
 if(argc!=5||(strcmp(argv[4],"--idle")&&strcmp(argv[4],"--walk"))){fprintf(stderr,"Usage: %s CORE ROM SYMBOLS --idle|--walk\n",argv[0]);return 2;}
 walk=!strcmp(argv[4],"--walk");tickAt=fullSymbol(argv[3],"debugIntroTick")&65535;
 unsigned overflowAt=fullSymbol(argv[3],"debugIntroOverflow")&65535;
 FILE*f=fopen(argv[2],"rb");if(!f)return 2;fseek(f,0,SEEK_END);long length=ftell(f);rewind(f);if(length<=0)return 2;
 uint8_t*rom=malloc((size_t)length);if(!rom||fread(rom,1,(size_t)length,f)!=(size_t)length)return 2;fclose(f);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_get_system_av_info,(struct retro_system_av_info*));FN(void,retro_run,(void));FN(void,retro_unload_game,(void));FN(void,retro_deinit,(void));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 struct retro_game_info game={argv[2],rom,(size_t)length,NULL};if(!retro_load_game(&game))return 2;
 struct retro_system_av_info av;memset(&av,0,sizeof av);retro_get_system_av_info(&av);
 ram=dlsym(core,"work_ram");if(!ram||av.timing.fps<50||av.timing.fps>61)return 2;
 unsigned firstFrame=0,previous=0,elapsed[2]={0},updates[2]={0},stalls[2]={0},skips[2]={0},maxGap[2]={0},gap=0,last=0;
 printf("frame,tick,delta,phase\n");
 for(unsigned frame=0;frame<4000;frame++){
  unsigned calls=videoCalls;retro_run();if(videoCalls!=calls+1){fprintf(stderr,"Unexpected video callback count at frame%u\n",frame);return 1;}
  unsigned tick=word(tickAt);if(word(overflowAt)){fprintf(stderr,"Target overflow at frame%u tick%u\n",frame,tick);return 1;}
  if(!previous){if(tick){if(tick!=1)return 1;firstFrame=frame;previous=tick;printf("%u,%u,0,start\n",frame,tick);}continue;}
  unsigned phase=previous<930?0:1,delta=(tick-previous)&65535;elapsed[phase]++;updates[phase]+=delta;gap++;
  if(!delta)stalls[phase]++;if(delta>1)skips[phase]++;
  if(delta){if(gap>maxGap[phase])maxGap[phase]=gap;gap=0;}
  printf("%u,%u,%u,%s\n",frame,tick,delta,phase?"early_player":"opening");previous=tick;last=frame;
  if(tick>=1530)break;
 }
 if(previous!=1530){fprintf(stderr,"Did not complete cadence sample; tick%u\n",previous);return 1;}
 fprintf(stderr,"Current intro cartridge; %s input. Core nominal %.9fHz, first update after%u frames. Opening: %u updates/%u frames = %.9f updates/s, %u stalled frames, %u multi-update frames, maximum gap%u. Early player: %u updates/%u frames = %.9f updates/s, %u stalled frames, %u multi-update frames, maximum gap%u. %u video callbacks through frame%u, %u null-frame callbacks. Headless emulated cadence only; no host VSync/presentation or complete-game claim.\n",walk?"RIGHT during ticks935..974":"idle",av.timing.fps,firstFrame,updates[0],elapsed[0],av.timing.fps*updates[0]/elapsed[0],stalls[0],skips[0],maxGap[0],updates[1],elapsed[1],av.timing.fps*updates[1]/elapsed[1],stalls[1],skips[1],maxGap[1],videoCalls,last,duplicateCalls);
 retro_unload_game();retro_deinit();free(rom);return 0;
}
