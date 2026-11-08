/* Differential calls into frozen old/new actual68000 BG blank predicates. */
#define WILLOW_CLOSED_HELPERS_ONLY 1
#include "test_native_render_cache_closed.c"
#include "aligned-fixture.h"
#undef CHECK
#define MUST(v) do{if(!(v)){fprintf(stderr,"aligned-row failure line%d case%u path%u pc%x: %s\n",__LINE__,caseID,path,getreg(16),#v);exit(3);}}while(0)
static uint8_t code[2][65536],seed[4][BG_SIZE],inputState[BG_SIZE];
static unsigned codeSize[2],caseID,path,abi[15];
static unsigned execute(unsigned variant,unsigned*cost){
 path=variant;uint8_t*base=cpu->map[0x10].base;
 for(unsigned i=0;i<codeSize[path];i++)base[(0x2000+i)^1]=code[path][i];
 memset(ram,0x5a,65536);for(unsigned i=0;i<BG_SIZE;i++)ram[((BG_BASE&65535)+i)^1]=inputState[i];
 setreg(17,0x2700);setreg(15,0xff75fc);wl(0xff75fc,0x1f0008);
 for(unsigned i=0;i<15;i++){abi[i]=0x413b0000+i*0x111+caseID;setreg(i,abi[i]);}
 setreg(16,path?NEW_PC:OLD_PC);cpu->cycles=0;cpu->refresh=0x7fffffff;
 unsigned steps=0;while(getreg(16)!=0x1f0008&&steps++<100000)run68k(cpu->cycles+1);
 MUST(steps<100000&&getreg(15)==0xff7600);
 for(unsigned i=2;i<15;i++)if(i!=8&&i!=9)MUST(getreg(i)==abi[i]);
 for(unsigned i=0;i<BG_SIZE;i++)MUST(ram[((BG_BASE&65535)+i)^1]==inputState[i]);
 *cost=cpu->cycles;return getreg(0)&65535;
}
static void pair(FILE*f,const char*kind){unsigned oldCost,newCost;unsigned a=execute(0,&oldCost),b=execute(1,&newCost);MUST(a==b);fprintf(f,"%u,%s,%u,%u,%u\n",caseID,kind,a,oldCost,newCost);caseID++;}
static void setword(unsigned off,unsigned value){pw(inputState+off,value);}
static int read(const char*dir,const char*name,uint8_t*dst,unsigned max){char p[4096];snprintf(p,sizeof p,"%s/%s",dir,name);FILE*f=fopen(p,"rb");if(!f)return -1;unsigned n=fread(dst,1,max,f);if(!feof(f))return -1;fclose(f);return n;}
int main(int argc,char**argv){
 if(argc!=3)return 2;
 int a=read(argv[2],"old.bin",code[0],65536),b=read(argv[2],"new.bin",code[1],65536);if(a<1||b<1)return 2;codeSize[0]=a;codeSize[1]=b;memcpy(rom+0x102000,code[0],a);
 if(read(argv[2],"resources.bin",rom+0x110000,sizeof rom-0x110000)<1)return 2;
 for(unsigned n=0;n<4;n++){char name[30];snprintf(name,sizeof name,"state%u.bin",n);if(read(argv[2],name,seed[n],BG_SIZE+1)!=BG_SIZE)return 2;}
 pl(rom,0xffff00);pl(rom+4,0x1f0000);const uint8_t boot[]={0x13,0xfc,0,1,0,0xa1,0x30,0xf1,0x60,0xfe};memcpy(rom+0x1f0000,boot,sizeof boot);memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);rom[0x1b0]='R';rom[0x1b1]='A';rom[0x1b2]=0xf8;rom[0x1b3]=0x20;pl(rom+0x1b4,0x200000);pl(rom+0x1b8,0x20ffff);
 char gamepath[4096];snprintf(gamepath,sizeof gamepath,"%s/closed.bin",argv[2]);FILE*f=fopen(gamepath,"wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core)return 2;
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void*,retro_get_memory_data,(unsigned));FN(void,m68k_set_irq,(unsigned));
 setreg=dlsym(core,"m68k_set_reg");getreg=dlsym(core,"m68k_get_reg");run68k=dlsym(core,"m68k_run");if(!setreg||!getreg||!run68k)return 2;
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();struct retro_game_info game={gamepath,rom,sizeof rom,0};if(!retro_load_game(&game))return 2;
 ram=dlsym(core,"work_ram");cpu=dlsym(core,"m68k");sram=retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);if(!ram||!cpu||!sram)return 2;retro_run();m68k_set_irq(0);
 char csvPath[4096];snprintf(csvPath,sizeof csvPath,"%s/calls.csv",argv[2]);f=fopen(csvPath,"w");if(!f)return 2;fputs("case,kind,result,old_master,new_master\n",f);
 for(unsigned n=0;n<4;n++){memcpy(inputState,seed[n],BG_SIZE);pair(f,n==0?"source433":n==1?"source122":n==2?"source130":"source132");}
 for(unsigned rotation=0;rotation<32;rotation++)for(unsigned nearFine=0;nearFine<8;nearFine++)for(unsigned farFine=0;farFine<8;farFine++){
  memcpy(inputState,seed[rotation&3],BG_SIZE);unsigned nearY=(rotation*7+97)&127,farY=(rotation*11+225)&255;
  setword(VIEW+2,nearY*8-16+nearFine);setword(VIEW+6,farY*8-16+farFine);
  inputState[RINGS+1]=nearY>>1;inputState[RINGS+8+1]=farY>>2;
  /* Preserve the full source-derived descriptor cache, vary admitted wrapped
   * counts to exercise skip prefixes, fallback/resume and clipped last rows. */
  for(unsigned y=0;y<32;y++){inputState[COUNTS+y]=((y+rotation)%9)?0:1;inputState[COUNTS+32+y]=((y+rotation)%7)?1:0;}
  pair(f,nearFine==farFine?"aligned_wrap_fallback":"unequal_fallback");
 }
 for(unsigned rotation=0;rotation<32;rotation++)for(unsigned poison=0;poison<4;poison++){
  memcpy(inputState,seed[0],BG_SIZE);setword(VIEW+2,rotation*8-16+(rotation&7));setword(VIEW+6,(rotation+192)*8-16+(rotation&7));
  inputState[RINGS+1]=rotation>>1;inputState[RINGS+9]=(rotation+192)>>2;
  memset(inputState+COUNTS,0,32);memset(inputState+COUNTS+32,1,32);
  if(poison==0){inputState[COUNTS+rotation]=44;inputState[COUNTS+((rotation+1)&31)]=255;}
  if(poison==1){memset(inputState+COUNTS,1,32);memset(inputState+KEYS+NEARLOW,255,330);memset(inputState+KEYS+NEARHIGH,255,83);}
  if(poison==2){memset(inputState+COUNTS,1,32);inputState[RINGS+4]=0;}
  if(poison==3){memset(inputState+COUNTS,255,32);memset(inputState+COUNTS+32,0,32);}
  pair(f,"refusal_or_empty");
 }
 fclose(f);printf("PASS %u actual68000 old/new predicate pairs; exact result, unchanged input BG bytes and preserved C ABI. No hardware frame/FPS claim.\n",caseID);return 0;
}
