/* Closed row predicates versus independent original-mask pixel oracle. */
#define WILLOW_CLOSED_HELPERS_ONLY 1
#include "test_native_render_cache_closed.c"
#include "row-fixture.h"
#undef CHECK
static unsigned cases,sourceRows,syntheticRows;static uint64_t pixelChecks,byteChecks;
#define CHECK(x) do{if(!(x)){fprintf(stderr,"row proof case%u line%u: %s\n",cases,__LINE__,#x);exit(1);}}while(0)
static uint8_t initial[65536],beforeRAM[65536],beforeSRAM[65536],savedVRAM[65536],savedCRAM[128],savedVSRAM[128],savedRegs[32];
static unsigned v[4],first[2],top[2],rows[2];
static uint64_t oldTicks[4],newTicks[4],groupCases[4];
static unsigned mask(unsigned l,unsigned x,unsigned y)
{
 unsigned scale=l?4:2,w=l?12:22,extent=l?255:127;x&=extent;y&=extent;
 unsigned dx=((x/scale+64-first[l]/scale)&63),dy=((y/scale+64-top[l]/scale)&63);
 CHECK(dx<w&&dy<(l?8:15));unsigned ref=(dy*w+dx)*scale*scale+(y%scale)*scale+x%scale;
 unsigned offset=(l?13424:2864)+ref*8;
 return offset;
}
static uint64_t oracle(unsigned row)
{
 uint64_t result=0;
 for(unsigned col=0;col<43;col++){
  int sx=(int)col*8-8-(int)(v[2]&7),sy=(int)row*8-(int)(v[3]&7);unsigned needed=0;
  unsigned far=mask(1,first[1]+col,top[1]+row);
  for(unsigned y=0;y<8;y++)for(unsigned x=0;x<8;x++){
   int px=sx+x,py=sy+y;if(px<0||px>=320||py<0||py>=224)continue;
   unsigned nx=(96+v[0]+px)&1023,ny=(16+v[1]+py)&1023;
   unsigned near=mask(0,nx/8,ny/8);pixelChecks++;
   if((initial[far+y]&(128>>x))&&!(initial[near+(ny&7)]&(128>>(nx&7))))needed=1;
  }
  if(needed)result|=1ULL<<col;
 }
 return result;
}
static unsigned invoke(unsigned entry,unsigned seed)
{
 unsigned saved[15],stack=0xff7800;setreg(17,0x2700|(seed&31));setreg(15,stack-4);wl(stack-4,0x100000);
 for(unsigned n=0;n<15;n++){saved[n]=0xabc00000+n*0x107+seed;setreg(n,saved[n]);}
 setreg(16,entry);cpu->cycles=0;cpu->refresh=896;unsigned steps=0;
 while(getreg(16)!=0x100000&&steps++<2000000)run68k(cpu->cycles+1);
 CHECK(steps<2000000&&getreg(15)==stack);
 for(unsigned n=2;n<15;n++)if(n!=8&&n!=9)CHECK(getreg(n)==saved[n]);return cpu->cycles;
}
static void verifyUnchanged(void*core)
{
 for(unsigned n=0;n<65536;n++){
  if(n<0x7000||n>=0x7800)CHECK(ram[n^1]==beforeRAM[n]);
  if(n<0xf804||n>=0xf80c)CHECK(sram[n]==beforeSRAM[n]);
  CHECK(vram[n]==savedVRAM[n]);
 }
 CHECK(!memcmp(savedCRAM,dlsym(core,"cram"),128)&&!memcmp(savedVSRAM,dlsym(core,"vsram"),128)&&!memcmp(savedRegs,dlsym(core,"reg"),32));
 byteChecks+=65536*3-2048-8;
}
static void runFixture(void*core,FILE*file,unsigned group,unsigned index)
{
 for(unsigned n=0;n<4;n++)v[n]=word(initial+n*2);
 for(unsigned l=0;l<2;l++){first[l]=((((96+v[l*2])&65535)>>3)-1)&(l?255:127);top[l]=(((16+v[l*2+1])&65535)>>3)&(l?255:127);rows[l]=28+!!(v[l*2+1]&7);}
 memcpy(sram,initial,65536);memset(ram,0xa5,65536);invoke(ROW_INIT,0);
 for(unsigned n=0;n<65536;n++)beforeRAM[n]=ram[n^1];
 for(unsigned row=0;row<rows[1];row++){
  pw(sram+0xf800,row);memcpy(beforeSRAM,sram,65536);uint64_t expected=oracle(row);
  unsigned a=invoke(ROW_OLD,cases);CHECK((lng(sram+0xf804)|((uint64_t)lng(sram+0xf808)<<32))==expected);verifyUnchanged(core);
  memcpy(sram,beforeSRAM,65536);unsigned b=invoke(ROW_NEW,cases);CHECK((lng(sram+0xf804)|((uint64_t)lng(sram+0xf808)<<32))==expected);verifyUnchanged(core);
  fprintf(file,"%u,%u,%u,%u,%u,%u,%llu\n",cases,group,index,row,a,b,(unsigned long long)expected);
  oldTicks[group]+=a;newTicks[group]+=b;groupCases[group]++;cases++;
  if(group==3)syntheticRows++;else sourceRows++;
 }
}
int main(int argc,char**argv)
{
 if(argc!=7)return 2;FILE*file=fopen(argv[2],"rb");CHECK(file);size_t bytes=fread(rom+0x102000,1,sizeof rom-0x102000,file);CHECK(bytes&&feof(file));fclose(file);
 pl(rom,0xffff00);pl(rom+4,0x100004);pw(rom+0x100000,0x60fe);
 const uint8_t boot[]={0x13,0xfc,0,1,0,0xa1,0x30,0xf1,0x33,0xfc,0x81,4,0,0xc0,0,4,0x4e,0xf9,0,0x10,0,0};memcpy(rom+0x100004,boot,sizeof boot);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);rom[0x1b0]='R';rom[0x1b1]='A';rom[0x1b2]=0xf8;rom[0x1b3]=0x20;pl(rom+0x1b4,0x200000);pl(rom+0x1b8,0x20ffff);
 char gamepath[4096];CHECK(snprintf(gamepath,sizeof gamepath,"%s.closed.bin",argv[2])<(int)sizeof gamepath);file=fopen(gamepath,"wb");CHECK(file&&fwrite(rom,1,sizeof rom,file)==sizeof rom);fclose(file);
 void*core=dlopen(argv[1],RTLD_NOW);CHECK(core);
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void*,retro_get_memory_data,(unsigned));
 setreg=dlsym(core,"m68k_set_reg");getreg=dlsym(core,"m68k_get_reg");run68k=dlsym(core,"m68k_run");CHECK(setreg&&getreg&&run68k);
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();struct retro_game_info game={gamepath,rom,sizeof rom,0};CHECK(retro_load_game(&game));
 ram=dlsym(core,"work_ram");cpu=dlsym(core,"m68k");vram=dlsym(core,"vram");sram=retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);CHECK(ram&&cpu&&vram&&sram);retro_run();CHECK(getreg(16)==0x100000);
 memcpy(savedVRAM,vram,65536);memcpy(savedCRAM,dlsym(core,"cram"),128);memcpy(savedVSRAM,dlsym(core,"vsram"),128);memcpy(savedRegs,dlsym(core,"reg"),32);
 file=fopen(argv[3],"w");CHECK(file);fputs("case,group,index,row,baseline_master,candidate_master,needed_bits\n",file);
 for(unsigned n=0;n<3;n++){memset(initial,0xa5,65536);FILE*input=fopen(argv[4+n],"rb");CHECK(input&&fread(initial,1,25712,input)==25712);CHECK(fgetc(input)==EOF);fclose(input);runFixture(core,file,n,n);}
 /* All64 vertical fine-phase pairs, source/native wraps, and varied horizontal
  * phases, with exact source-shaped flags derived independently from masks. */
 for(unsigned ny=0;ny<8;ny++)for(unsigned fy=0;fy<8;fy++){
  memset(initial,0xa5,65536);pw(initial,(128*8-96+((ny+fy)&7))&65535);pw(initial+2,127*8-16+ny);
  pw(initial+4,253*8-96+((ny*3+fy)&7));pw(initial+6,252*8-16+fy);
  for(unsigned l=0;l<2;l++)for(unsigned ref=0;ref<(l?1536:1320);ref++){
   unsigned at=(l?13424:2864)+ref*8,any=0,full=1;
   for(unsigned y=0;y<8;y++){unsigned b=(ref*31+y*17+ny*19+fy*23+(l?91:49))&255;initial[at+y]=b;any|=b;full&=b==255;}
   initial[(l?1328:8)+ref]=full?1:any?0:2;
  }
  runFixture(core,file,3,ny*8+fy);
 }
 fclose(file);printf("{\"paired_rows\":%u,\"actual_68000_predicate_calls\":%u,\"source_rows\":%u,\"synthetic_alignment_rows\":%u,\"independent_mask_pixels\":%llu,\"whole_memory_byte_checks\":%llu,\"groups\":[",cases,cases*2,sourceRows,syntheticRows,(unsigned long long)pixelChecks,(unsigned long long)byteChecks);
 for(unsigned n=0;n<4;n++)printf("%s{\"group\":%u,\"rows\":%llu,\"baseline_master_sum\":%llu,\"candidate_master_sum\":%llu}",n?",":"",n,(unsigned long long)groupCases[n],(unsigned long long)oldTicks[n],(unsigned long long)newTicks[n]);printf("]}\n");return 0;
}
