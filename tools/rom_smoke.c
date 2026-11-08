// Integration test against the installed Genesis Plus GX libretro core.
// clang -O2 tools/rom_smoke.c -o build/rom_smoke
#include "vendor/libretro.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
static int frame,fmt=0,two_player=0;
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
 if(!data || (frame!=120 && frame!=170 && frame!=240 && frame!=600 && frame!=720 && frame!=900 && frame!=2870 && frame!=1040 && frame!=3600))return;
 char name[128];snprintf(name,sizeof(name),"build/probe-%d.ppm",frame);FILE*f=fopen(name,"wb");fprintf(f,"P6\n%d %d\n255\n",w,h);
 for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++){unsigned c;unsigned char rgb[3];if(fmt==1){c=*(uint32_t*)((char*)data+y*pitch+x*4);rgb[0]=c>>16;rgb[1]=c>>8;rgb[2]=c;}else{c=*(uint16_t*)((char*)data+y*pitch+x*2);rgb[0]=((c>>(fmt==2?11:10))&31)*255/31;rgb[1]=((c>>5)&(fmt==2?63:31))*255/(fmt==2?63:31);rgb[2]=(c&31)*255/31;}fwrite(rgb,1,3,f);}fclose(f);
}
static void audio(int16_t l,int16_t r){}
static size_t batch(const int16_t*d,size_t n){return n;}
static void poll(void){}
static int16_t input(unsigned p,unsigned d,unsigned i,unsigned id){if(p==1)return two_player && ((id==RETRO_DEVICE_ID_JOYPAD_START && frame>=260 && frame<270)||(id==RETRO_DEVICE_ID_JOYPAD_RIGHT && ((frame>=600 && frame<700)||(frame>=1300 && frame<2500)))||(id==RETRO_DEVICE_ID_JOYPAD_Y && frame>=700 && frame<710)); return p==0 && ((id==RETRO_DEVICE_ID_JOYPAD_START && ((frame>=180 && frame<190)||(frame>=1000 && frame<1010)||(frame>=1200 && frame<1210))) || (id==RETRO_DEVICE_ID_JOYPAD_RIGHT && ((frame>=600 && frame<700)||(frame>=1050 && frame<1100)||(frame>=1300 && frame<2500))) || (id==RETRO_DEVICE_ID_JOYPAD_Y && frame>=700 && frame<710) || (id==RETRO_DEVICE_ID_JOYPAD_B && ((frame>=880 && frame<890)||(frame>=990 && frame<999)||(frame>=2700 && frame<2850))));}
#define FN(ret,n,args) ret(*n)args=dlsym(core,#n)
static unsigned symbol(const char *path,const char *name){
 FILE*f=fopen(path,"r");char line[512],sym[256],type;unsigned address;
 if(!f){perror(path);exit(2);}
 while(fgets(line,sizeof(line),f)) if(sscanf(line,"%x %c %255s",&address,&type,sym)==3 && !strcmp(sym,name)){fclose(f);return address&65535;}
 fprintf(stderr,"missing symbol: %s\n",name);exit(2);
}
static int word(const unsigned char*ram,unsigned address){return (int16_t)(ram[address]|(ram[address+1]<<8));}
int main(int argc,char**argv){
 if(argc!=4 && argc!=5){fprintf(stderr,"usage: %s CORE ROM SYMBOLS [2p]\n",argv[0]);return 2;}
 two_player=argc==5;
 unsigned bank=symbol(argv[3],"debugBgBankSwitches");
 unsigned px=symbol(argv[3],"debugPlayerX"),py=symbol(argv[3],"debugPlayerY"),ov=symbol(argv[3],"debugBgOverflow"),pa=symbol(argv[3],"debugPaused");
 unsigned shots=symbol(argv[3],"debugMagicShots"),live=symbol(argv[3],"debugMagicActive"),age=symbol(argv[3],"debugMagicAge");
 int pauseAge=0;
 int x0=0,x1=0,minY=300,pauseX=0,failed=0,fell=0,recovered=0;
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 1;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 FILE*f=fopen(argv[2],"rb");fseek(f,0,SEEK_END);size_t n=ftell(f);rewind(f);void*data=malloc(n);fread(data,1,n,f);fclose(f);struct retro_game_info game={argv[2],data,n,NULL};if(!retro_load_game(&game)){puts("load failed");return 2;}
 printf("system=%x\n",*(unsigned char*)dlsym(core,"system_hw"));
 unsigned char*ram=dlsym(core,"work_ram");
 unsigned char companion_palette[32], effect_palette[32];
 for(frame=0;frame<=3600;frame++){retro_run();
 int x=word(ram,px),y=word(ram,py);
 if(frame==280 && two_player)memcpy(companion_palette,(char*)dlsym(core,"cram")+32,32);
 if(frame==950 && two_player && memcmp(companion_palette,(char*)dlsym(core,"cram")+32,32)){fprintf(stderr,"companion palette changed across scenery banks\n");failed++;}
 if(frame==950)memcpy(effect_palette,(char*)dlsym(core,"cram")+32,32);
 if(frame==1150 && memcmp(effect_palette,(char*)dlsym(core,"cram")+32,32)){fprintf(stderr,"pause changed effect/companion palette\n");failed++;}
 if(frame==599)x0=x;
 if(frame==699)x1=x;
 if(frame>=710 && frame<=770 && y<minY)minY=y;
 if(frame==930 && (word(ram,shots)!=1 || word(ram,live)!=1)){fprintf(stderr,"basic magic did not fire\n");failed++;}
 if(frame==1040){pauseX=x;pauseAge=word(ram,age);if(!word(ram,pa)||!word(ram,live))failed++;}
 if(frame==1150 && (x!=pauseX || word(ram,age)!=pauseAge))failed++;
 if(frame==2870 && (word(ram,shots)<2 || !word(ram,live))){fprintf(stderr,"charged magic did not fire\n");failed++;}
 if(frame==1250 && word(ram,pa))failed++;
 if(frame>=1300 && y>240)fell=1;
 if(fell && x==160 && y==200)recovered=1;
 if(word(ram,ov)){fprintf(stderr,"VRAM cache overflow at frame %d\n",frame);return 1;}
if(frame%600==0)printf("frame=%d pc=%x sp=%x sr=%x vdp1=%x\n",frame,m68k_get_reg(16),m68k_get_reg(15),m68k_get_reg(17),((unsigned char*)dlsym(core,"reg"))[1]);}
 FILE* vd=fopen("build/probe-vram.bin","wb");fwrite(dlsym(core,"vram"),1,65536,vd);fclose(vd);FILE* cd=fopen("build/probe-cram.bin","wb");fwrite(dlsym(core,"cram"),1,128,cd);fclose(cd);
 FILE* dump=fopen("build/probe-ram.bin","wb");fwrite(dlsym(core,"work_ram"),1,65536,dump);fclose(dump);
 if(x1<=x0+100 || minY>=180 || !fell || !recovered || !word(ram,bank))failed++;
 printf("movement=%d->%d jump_min_y=%d pause_x=%d fall=%d respawn=%d cache_overflows=%d\n",x0,x1,minY,pauseX,fell,recovered,word(ram,ov));
 printf("background_bank_switches=%d two_player=%d\n",word(ram,bank),two_player);
 printf("ROM smoke: %s (3601 emulated frames, strict address errors)\n",failed?"FAIL":"PASS");
 return failed?1:0;
}
