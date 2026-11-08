#include "vendor/libretro.h"
#include "../src/willow_native_tile_mirror.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
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

/* Prefix layout from the local core header build/genplus-m68k.h. Validate
 * PC and all DAR fields through the exported register API before using it. */
typedef struct {
 struct {void*base;unsigned(*read8)(unsigned);unsigned(*read16)(unsigned);void(*write8)(unsigned,unsigned);void(*write16)(unsigned,unsigned);} map[256];
 struct {unsigned pc,cycle,detected;} poll;
 int cycles,refresh;unsigned end,dar[16],pc;
} CoreCyclePrefix;

static uint8_t rom[0x200000],expected[0xf600],*ram;
static uint16_t values[WILLOW_TILE_MIRROR_CELLS];
static uint32_t seed=0x717afddb;
static uint32_t rnd(void){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
static unsigned rw(unsigned at){return ram[at]|ram[at+1]<<8;}
static void ww(unsigned at,unsigned value){ram[at]=value;ram[at+1]=value>>8;}
static void ew(unsigned at,unsigned value){expected[at]=value>>8;expected[at+1]=value;}
static void token(unsigned cell,unsigned value){
 /* Independent byte-level oracle, without calling the production codec. */
 unsigned bit=cell*12;
 for(unsigned n=0;n<12;n++){
  unsigned at=0x2000+(bit+n)/8,mask=128u>>((bit+n)%8);
  expected[at]=(expected[at]&~mask)|((value&(1u<<(11-n)))?mask:0);
 }
}
int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/tile_mirror_probe.bin","rb");assert(f);
 assert(fread(rom+0x100010,1,sizeof rom-0x100010,f)>0);fclose(f);
 f=fopen("build/tile_mirror_probe.sym","r");assert(f);
 unsigned entry=0,address;char line[512],kind,name[256];
 while(fgets(line,sizeof line,f))if(sscanf(line,"%x %c %255s",&address,&kind,name)==3&&!strcmp(name,"tileMirrorProbe"))entry=address;
 fclose(f);assert(entry);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);willowMotionPutWord(rom+0x100008,0x4e71);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));FN(void,m68k_run,(unsigned));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/native-tile-mirror-oracle.bin","wb");assert(f&&fwrite(rom,1,sizeof rom,f)==sizeof rom);fclose(f);
 struct retro_game_info game={"build/native-tile-mirror-oracle.bin",rom,sizeof rom,NULL};assert(retro_load_game(&game));
 ram=dlsym(core,"work_ram");CoreCyclePrefix*cpu=dlsym(core,"m68k");assert(ram&&cpu);
 m68k_set_reg(17,0x2700);m68k_set_reg(16,0x100008);assert(cpu->pc==0x100008);
 for(unsigned n=0;n<16;n++)assert(cpu->dar[n]==m68k_get_reg(n));
 cpu->cycles=0;cpu->refresh=128*7;m68k_run(1);assert(cpu->cycles==28&&m68k_get_reg(16)==0x10000a);
 unsigned calls=0,maximumConversion=0,maximumWrite=0,transposeCalls=0,maximumTranspose=0,heldWrites=0,attributeCalls=0,maximumAttributeWrite=0,introCalls=0,introRejects=0,maximumIntroWrite=0;
 for(unsigned scenario=0;scenario<4;scenario++){
  memset(expected,0xa5,sizeof expected);
  for(unsigned i=0;i<WILLOW_TILE_MIRROR_CELLS;i++){values[i]=rnd();ew(0x2000+i*2,values[i]);}
  ew(0x102,0);
  for(unsigned i=0;i<sizeof expected;i++)ram[i^1]=expected[i];
  unsigned converted=0;
  for(unsigned iteration=0;iteration<10000;iteration++){
   unsigned op=iteration%4?iteration%2?1:2:0;
   unsigned cell=rnd()%WILLOW_TILE_MIRROR_CELLS,value=rnd()&65535,result=0;
   if(!op){cell=1+rnd()%65;unsigned end=converted+cell;if(end>WILLOW_TILE_MIRROR_CELLS)end=WILLOW_TILE_MIRROR_CELLS;
    while(converted<end){token(converted,values[converted]&2047);converted++;}result=1;
   }else if(op==1){values[cell]=value;if(cell<converted)token(cell,value&2047);else ew(0x2000+cell*2,value);result=1;
   }else result=values[cell]&2047;
   ww(0x100,op);ww(0x104,cell);ww(0x106,value);ww(0x108,0xface);
   ew(0x100,op);ew(0x102,converted);ew(0x104,cell);ew(0x106,value);ew(0x108,result);
   ww(0xff00,0x0010);ww(0xff02,0);
   m68k_set_reg(17,0x2700);m68k_set_reg(15,0xffff00);m68k_set_reg(16,entry);cpu->cycles=0;cpu->refresh=128*7;
   unsigned instructions=0;
   while(m68k_get_reg(16)!=0x100000){assert(++instructions<100000);m68k_run(cpu->cycles+1);}
   if(!op&&(unsigned)cpu->cycles>maximumConversion)maximumConversion=cpu->cycles;
   if(op==1&&(unsigned)cpu->cycles>maximumWrite)maximumWrite=cpu->cycles;
   for(unsigned i=0;i<sizeof expected;i++)if(ram[i^1]!=expected[i]){fprintf(stderr,"Mirror target mismatch scenario %u iteration %u op %u byte %04x: %02x/%02x converted %u\n",scenario,iteration,op,i,ram[i^1],expected[i],converted);return 1;}
   assert(rw(0x102)==converted);calls++;
   if(converted==WILLOW_TILE_MIRROR_CELLS){
    /* Reclaimed bytes become unrelated live state; later writes must preserve them. */
    for(unsigned i=0x2000+WILLOW_TILE_MIRROR_PACKED_BYTES;i<0x2000+WILLOW_TILE_MIRROR_DENSE_BYTES;i++)ram[i^1]=expected[i]=0x3c;
    if(iteration>1500)break;
   }
  }
  assert(converted==WILLOW_TILE_MIRROR_CELLS);
  unsigned scan=0,start=0,origin=65535,held=0;
  uint8_t visited[2752]={0};
  for(unsigned iteration=0;iteration<3000;iteration++){
   unsigned op=iteration==0?3:iteration%3==1?4:iteration%3==2?6:5;
   unsigned cell=rnd()%2752,value=rnd()&65535,result=0;
   if(op==3){
    memset(expected+0x2000+4128,0,344);result=1;
   }else if(op==4){
    cell=1+rnd()%41;
    for(unsigned n=0;n<cell&&scan<2752;n++){
     if(origin==65535){
      if(visited[scan]){scan++;continue;}
      start=origin=scan;held=values[scan]&2047;
     }
     unsigned dest=(origin/1376*43+origin%43)*32+(origin%1376)/43;
     unsigned displaced=0;
     for(unsigned bit=0;bit<12;bit++)displaced=(displaced<<1)|((expected[0x2000+(dest*12+bit)/8]>>(7-(dest*12+bit)%8))&1);
     token(dest,held);visited[dest]=1;
     expected[0x2000+4128+dest/8]|=1u<<(dest%8);
     if(dest==start){origin=65535;scan++;}else{origin=dest;held=displaced;}
    }
    result=scan==2752;
   }else{
    if(op==6&&origin!=65535){cell=origin;heldWrites++;}
    unsigned dest=(cell/1376*43+cell%43)*32+(cell%1376)/43;
    if(op==5)result=values[cell]&2047;
    else{values[cell]=value&2047;if(cell==origin)held=value&2047;else token(visited[dest]?dest:cell,value&2047);result=1;}
   }
   ew(0x120,scan);ew(0x122,start);ew(0x124,origin);ew(0x126,held);
   ww(0x100,op);ww(0x104,cell);ww(0x106,value);ww(0x108,0xface);
   ew(0x100,op);ew(0x104,cell);ew(0x106,value);ew(0x108,result);
   ww(0xff00,0x0010);ww(0xff02,0);
   m68k_set_reg(17,0x2700);m68k_set_reg(15,0xffff00);m68k_set_reg(16,entry);cpu->cycles=0;cpu->refresh=128*7;
   unsigned instructions=0;
   while(m68k_get_reg(16)!=0x100000){assert(++instructions<100000);m68k_run(cpu->cycles+1);}
   if(op==4&&(unsigned)cpu->cycles>maximumTranspose)maximumTranspose=cpu->cycles;
   for(unsigned i=0;i<sizeof expected;i++)if(ram[i^1]!=expected[i]){fprintf(stderr,"Transpose target mismatch scenario %u iteration %u op %u byte %04x: %02x/%02x scan %u origin %u\n",scenario,iteration,op,i,ram[i^1],expected[i],scan,origin);return 1;}
   transposeCalls++;
   if(scan==2752&&iteration%3==0)break;
  }
  assert(scan==2752&&origin==65535);
  for(unsigned i=0;i<2752;i++){
   values[i]=rnd();token(i,values[i]&4095);
   unsigned at=0x2000+4128+i/2,shift=(i%2)*4;
   expected[at]=(expected[at]&~(15u<<shift))|((values[i]>>12)<<shift);
  }
  for(unsigned i=0;i<sizeof expected;i++)ram[i^1]=expected[i];
  for(unsigned iteration=0;iteration<4096;iteration++){
   if(iteration==2048)for(unsigned i=0;i<1376;i++)ram[(0x2000+4128+i)^1]=expected[0x2000+4128+i]=0x3c;
   unsigned op=iteration<2048?7+iteration%4:9+iteration%2;
   unsigned cell=rnd()%2752,value=rnd()&65535,result=0;
   if(op==7){
    values[cell]=value;token(cell,value&4095);
    unsigned at=0x2000+4128+cell/2,shift=(cell%2)*4;
    expected[at]=(expected[at]&~(15u<<shift))|((value>>12)<<shift);result=1;
   }else if(op==8)result=values[cell];
   else if(op==9){value&=2047;values[cell]=(values[cell]&0xf000)|value;token(cell,value);result=1;}
   else result=values[cell]&2047;
   ww(0x100,op);ww(0x104,cell);ww(0x106,value);ww(0x108,0xface);
   ew(0x100,op);ew(0x104,cell);ew(0x106,value);ew(0x108,result);
   ww(0xff00,0x0010);ww(0xff02,0);
   m68k_set_reg(17,0x2700);m68k_set_reg(15,0xffff00);m68k_set_reg(16,entry);cpu->cycles=0;cpu->refresh=128*7;
   unsigned instructions=0;
   while(m68k_get_reg(16)!=0x100000){assert(++instructions<100000);m68k_run(cpu->cycles+1);}
   if(op==7&&(unsigned)cpu->cycles>maximumAttributeWrite)maximumAttributeWrite=cpu->cycles;
   for(unsigned i=0;i<sizeof expected;i++)if(ram[i^1]!=expected[i]){fprintf(stderr,"Attribute target mismatch scenario %u iteration %u op %u byte %04x: %02x/%02x\n",scenario,iteration,op,i,ram[i^1],expected[i]);return 1;}
   attributeCalls++;
  }


  static const unsigned styles[5]={0,4,6,8,9};
  static const unsigned codes[16]={0,255,255,255,1,255,2,255,3,4,255,255,255,255,255,255};
  memset(expected+0x2000,0xa5,5504);memset(expected+0x2000+4128,0,1032);
  for(unsigned i=0;i<2752;i++){
   unsigned code=rnd()%5;values[i]=(styles[code]<<12)|(rnd()&4095);token(i,values[i]&4095);
   for(unsigned n=0;n<3;n++){unsigned bit=i*3+n,at=0x2000+4128+bit/8,mask=1u<<(bit%8);expected[at]=(expected[at]&~mask)|((code&(1u<<n))?mask:0);}
  }
  for(unsigned i=0;i<sizeof expected;i++)ram[i^1]=expected[i];
  for(unsigned iteration=0;iteration<2048;iteration++){
   if(iteration==1024)for(unsigned i=0;i<1032;i++)ram[(0x2000+4128+i)^1]=expected[0x2000+4128+i]=0x3c;
   unsigned op=iteration<1024?15+iteration%4:17+iteration%2;
   unsigned cell=rnd()%2752,value=rnd()&65535,result=0;
   if(op==15){
    unsigned code=codes[value>>12];
    if(code==255)introRejects++;
    else{values[cell]=value;token(cell,value&4095);result=1;
     for(unsigned n=0;n<3;n++){unsigned bit=cell*3+n,at=0x2000+4128+bit/8,mask=1u<<(bit%8);expected[at]=(expected[at]&~mask)|((code&(1u<<n))?mask:0);}
    }
   }else if(op==16)result=values[cell];
   else if(op==17){value&=2047;values[cell]=(values[cell]&0xf000)|value;token(cell,value);result=1;}
   else result=values[cell]&2047;
   ww(0x100,op);ww(0x104,cell);ww(0x106,value);ww(0x108,0xface);
   ew(0x100,op);ew(0x104,cell);ew(0x106,value);ew(0x108,result);
   ww(0xff00,0x0010);ww(0xff02,0);
   m68k_set_reg(17,0x2700);m68k_set_reg(15,0xffff00);m68k_set_reg(16,entry);cpu->cycles=0;cpu->refresh=128*7;
   unsigned instructions=0;
   while(m68k_get_reg(16)!=0x100000){assert(++instructions<100000);m68k_run(cpu->cycles+1);}
   if(op==15&&(unsigned)cpu->cycles>maximumIntroWrite)maximumIntroWrite=cpu->cycles;
   for(unsigned i=0;i<sizeof expected;i++)if(ram[i^1]!=expected[i]){fprintf(stderr,"Intro attribute target mismatch scenario %u iteration %u op %u byte %04x: %02x/%02x\n",scenario,iteration,op,i,ram[i^1],expected[i]);return 1;}
   introCalls++;
  }

 }
 printf("Native tile mirror: %u compiled conversion/read/write calls match all non-stack RAM; max conversion (budget <=65) %u master cycles, max write %u; reclaimed tail preserved\n",calls,maximumConversion,maximumWrite);
 printf("Native transpose: %u compiled init/step/read/write calls match all non-stack RAM; %u live-held writes, max step (budget <=41) %u master cycles\n",transposeCalls,heldWrites,maximumTranspose);
 printf("Native split attributes: %u full/ownership calls match all non-stack RAM; max full write %u master cycles; reused-tail guard preserved\n",attributeCalls,maximumAttributeWrite);
 printf("Native intro attributes: %u calls and %u atomic rejections match all non-stack RAM; max write %u master cycles; 344-byte saving and retired style guards preserved\n",introCalls,introRejects,maximumIntroWrite);
 return 0;
}
