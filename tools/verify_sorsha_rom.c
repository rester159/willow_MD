#include "../build/libretro.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
static int frame,fmt=0,scenario=0;
static bool env(unsigned cmd,void *data){
 if(cmd==RETRO_ENVIRONMENT_SET_PIXEL_FORMAT){fmt=*(int*)data;return true;}
 if(cmd==RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY || cmd==RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY){*(const char**)data="/tmp";return true;}
 if(cmd==RETRO_ENVIRONMENT_GET_VARIABLE){struct retro_variable*v=data; if(!strcmp(v->key,"genesis_plus_gx_addr_error")){v->value="enabled";return true;} if(!strcmp(v->key,"genesis_plus_gx_system_hw")){v->value="auto";return true;} return false;}
 if(cmd==RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE){*(bool*)data=false;return true;}
 if(cmd==RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION){*(unsigned*)data=0;return true;}
 if(cmd==RETRO_ENVIRONMENT_SET_VARIABLES || cmd==RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME || cmd==RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS || cmd==RETRO_ENVIRONMENT_SET_CONTROLLER_INFO) return true;
 return false;
}
static void video(const void*data,unsigned w,unsigned h,size_t pitch){
 if(!data || (frame!=170 && frame!=240 && frame!=280 && frame!=360 && frame!=420 && frame!=600 && frame!=720 && frame!=900 && frame!=1200 && frame!=1500))return;
 char name[128];snprintf(name,sizeof(name),"build/sorsha-probe-%d-%d.ppm",scenario,frame);FILE*f=fopen(name,"wb");fprintf(f,"P6\n%d %d\n255\n",w,h);
 for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++){unsigned c;unsigned char rgb[3];if(fmt==1){c=*(uint32_t*)((char*)data+y*pitch+x*4);rgb[0]=c>>16;rgb[1]=c>>8;rgb[2]=c;}else{c=*(uint16_t*)((char*)data+y*pitch+x*2);rgb[0]=((c>>(fmt==2?11:10))&31)*255/31;rgb[1]=((c>>5)&(fmt==2?63:31))*255/(fmt==2?63:31);rgb[2]=(c&31)*255/31;}fwrite(rgb,1,3,f);}fclose(f);
}
static void audio(int16_t l,int16_t r){}
static size_t batch(const int16_t*d,size_t n){return n;}
static void poll(void){}
static int16_t input(unsigned p,unsigned d,unsigned i,unsigned id){
 if(p)return 0;
 if(scenario==3){
  if(id==RETRO_DEVICE_ID_JOYPAD_START)return (frame>=180&&frame<190)||(frame>=950&&frame<955);
  if(id==RETRO_DEVICE_ID_JOYPAD_Y)return frame>=180&&frame<190;
  return 0;
 }
 if(scenario==2){
  if(id==RETRO_DEVICE_ID_JOYPAD_START)return (frame>=180&&frame<190);
  return 0;
 }
 if(scenario==1){
  if(id==RETRO_DEVICE_ID_JOYPAD_START)return (frame>=180&&frame<190)||(frame>=250&&frame<255)||(frame>=300&&frame<305)||(frame>=320&&frame<325)||(frame>=380&&frame<385)||(frame>=450&&frame<455);
  if(id==RETRO_DEVICE_ID_JOYPAD_RIGHT)return (frame>=270&&frame<275)||(frame>=340&&frame<345);
  if(id==RETRO_DEVICE_ID_JOYPAD_Y)return (frame>=180&&frame<190)||(frame>=470&&frame<475);
  if(id==RETRO_DEVICE_ID_JOYPAD_A)return (frame>=470&&frame<475);
  return 0;
 }
 if(id==RETRO_DEVICE_ID_JOYPAD_START)return (frame>=180&&frame<190)||(frame>=1000&&frame<1005)||(frame>=1080&&frame<1085)||(frame>=1350&&frame<1355)||(frame>=1430&&frame<1435);
 if(id==RETRO_DEVICE_ID_JOYPAD_Y)return (frame>=180&&frame<190)||(frame>=290&&frame<950&&frame%30<2);
 if(id==RETRO_DEVICE_ID_JOYPAD_RIGHT)return (frame>=245&&frame<275)||(frame>=1040&&frame<1045)||(frame>=1390&&frame<1395);
 if(id==RETRO_DEVICE_ID_JOYPAD_B)return frame>=386&&frame<389;
 if(id==RETRO_DEVICE_ID_JOYPAD_LEFT)return frame>=644&&frame<659;
 if(id==RETRO_DEVICE_ID_JOYPAD_DOWN)return frame>=570&&frame<630;
 if(id==RETRO_DEVICE_ID_JOYPAD_A)return frame>=730&&frame<810;
 return 0;
}
#define FN(ret,n,args) ret(*n)args=dlsym(core,#n)
int main(int argc,char**argv){if(argc<3){puts("usage: probe CORE ROM [0=combat,1=menus,2=willow,3=retry]");return 1;} if(argc>3)scenario=atoi(argv[3]); void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 1;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 FILE*f=fopen(argv[2],"rb");fseek(f,0,SEEK_END);size_t n=ftell(f);rewind(f);void*data=malloc(n);fread(data,1,n,f);fclose(f);struct retro_game_info game={argv[2],data,n,NULL};if(!retro_load_game(&game)){puts("load failed");return 2;}
 printf("system=%x\n",*(unsigned char*)dlsym(core,"system_hw"));
 for(frame=0;frame<=1500;frame++){retro_run();if(frame%600==0)printf("frame=%d pc=%x sp=%x sr=%x vdp1=%x\n",frame,m68k_get_reg(16),m68k_get_reg(15),m68k_get_reg(17),((unsigned char*)dlsym(core,"reg"))[1]);}
 FILE* vd=fopen("build/sorsha-probe-vram.bin","wb");fwrite(dlsym(core,"vram"),1,65536,vd);fclose(vd);FILE* cd=fopen("build/sorsha-probe-cram.bin","wb");fwrite(dlsym(core,"cram"),1,128,cd);fclose(cd);
 FILE* dump=fopen("build/sorsha-probe-ram.bin","wb");fwrite(dlsym(core,"work_ram"),1,65536,dump);fclose(dump);
 return 0;
}
