/* Actual compiled68000 API parity plus independent snapshot/ack properties. */
#define WILLOW_CLOSED_HELPERS_ONLY 1
#include "test_native_render_cache_closed.c"
#include "../src/willow_native_background_columns.h"
#include "columns-api-fixture.h"
#undef CHECK
static unsigned cases,accepted,refused;
#define CHECK(x) do{if(!(x)){fprintf(stderr,"Columns API failure line%u case%u: %s\n",__LINE__,cases,#x);exit(1);}}while(0)
static void encodeState(uint8_t*p,const WillowNativeBackgroundColumns*s)
{
 pl(p,s->rawEpoch);pl(p+4,s->mapGeneration[0]);pl(p+8,s->mapGeneration[1]);
 pw(p+12,s->rawBusy);pw(p+14,s->touchedMask);pw(p+16,s->active);pw(p+18,s->fault);
 pl(p+20,s->completedEpoch);pl(p+24,s->openEpoch);pw(p+28,s->state);pw(p+30,s->reserved);
 for(unsigned l=0;l<2;l++)for(unsigned w=0;w<2;w++)pl(p+32+l*8+w*4,s->pending[l][w]);
}
static void encodeView(uint8_t*p,const WillowNativeBackgroundColumnsView*v)
{
 pl(p,v->owner?0x20f000:0);pl(p+4,v->epoch);pl(p+8,v->mapGeneration[0]);pl(p+12,v->mapGeneration[1]);
 for(unsigned l=0;l<2;l++)for(unsigned w=0;w<2;w++)pl(p+16+l*8+w*4,v->pending[l][w]);
}
static void ready(WillowNativeBackgroundColumns*s,unsigned seed)
{
 memset(s,0,sizeof *s);s->rawEpoch=s->completedEpoch=100+seed;s->mapGeneration[0]=20+seed;s->mapGeneration[1]=30+seed;s->state=1;
 s->touchedMask=seed&3;
 for(unsigned l=0;l<2;l++)for(unsigned w=0;w<2;w++)s->pending[l][w]=(0x80000001u>>(seed&7))^(0x1234567u*(l*2+w+1));
}
static void view(WillowNativeBackgroundColumnsView*v,WillowNativeBackgroundColumns*s)
{
 v->owner=s;v->epoch=s->completedEpoch;
 for(unsigned l=0;l<2;l++){v->mapGeneration[l]=s->mapGeneration[l];for(unsigned w=0;w<2;w++)v->pending[l][w]=s->pending[l][w];}
}
static void one(unsigned op,unsigned variant,unsigned seed,unsigned nulls)
{
 WillowNativeBackgroundColumns s;WillowNativeBackgroundColumnsView v;
 ready(&s,seed);view(&v,&s);
 uint32_t epoch=s.rawEpoch,g2=s.mapGeneration[0],g3=s.mapGeneration[1];
 switch(variant){
 case 1:s.state=0;break;case 2:s.state=3;break;case 3:s.state=2;break;case 4:s.state=0x8001;break;
 case 5:s.rawBusy=3;break;case 6:s.active=1;break;case 7:s.fault=1;break;case 8:s.reserved=1;break;
 case 9:s.openEpoch=1;break;case 10:s.completedEpoch=0;break;case 11:s.rawEpoch++;break;
 case 12:s.rawEpoch=s.completedEpoch=UINT32_MAX;break;case 13:s.mapGeneration[0]=UINT32_MAX;break;case 14:s.mapGeneration[1]=UINT32_MAX;break;
 case 15:v.epoch++;break;case 16:v.pending[seed&1][(seed>>1)&1]^=1u<<(seed+17);break;case 17:v.mapGeneration[seed&1]++;break;
 case 18:v.owner=0;break;case 19:epoch++;break;case 20:g2++;break;case 21:g3++;break;
 case 22:s.rawEpoch=s.completedEpoch=epoch=0;v.epoch=0;break;
 case 23:s.completedEpoch=s.rawEpoch+1;break;
 case 24:s.rawEpoch=s.completedEpoch=epoch=v.epoch=UINT32_MAX-1;break;
 case 25:s.rawEpoch=s.completedEpoch=epoch=v.epoch=UINT32_MAX-2;break;
 }
 uint8_t beforeState[48],beforeView[32],wantState[48],wantView[32];encodeState(beforeState,&s);encodeView(beforeView,&v);
 uint8_t expectedStateReady=s.state==1&&!s.rawBusy&&!s.active&&!s.fault&&!s.reserved&&!s.openEpoch&&s.completedEpoch&&s.completedEpoch==s.rawEpoch&&s.rawEpoch!=UINT32_MAX&&s.mapGeneration[0]!=UINT32_MAX&&s.mapGeneration[1]!=UINT32_MAX;
 uint8_t expectedViewReady=expectedStateReady&&v.owner==&s&&v.epoch==s.completedEpoch&&v.mapGeneration[0]==s.mapGeneration[0]&&v.mapGeneration[1]==s.mapGeneration[1]&&!memcmp(v.pending,s.pending,sizeof s.pending);
 uint8_t baselineReady=!s.active&&!s.rawBusy&&!s.reserved&&epoch&&epoch==s.rawEpoch&&g2==s.mapGeneration[0]&&g3==s.mapGeneration[1]&&epoch<UINT32_MAX-1&&g2!=UINT32_MAX&&g3!=UINT32_MAX&&epoch+1>s.completedEpoch;
 WillowNativeBackgroundColumns*owner=(nulls&1)?0:&s;WillowNativeBackgroundColumnsView*snapshot=(nulls&2)?0:&v;
 uint8_t result=0,expected=0;
 switch(op){
 case 0:willowNativeBackgroundColumnsInit(owner);result=expected=1;break;
 case 1:result=willowNativeBackgroundColumnsBaseline(owner,epoch,g2,g3);expected=owner&&baselineReady;break;
 case 2:result=willowNativeBackgroundColumnsAcquire(owner,snapshot);expected=owner&&snapshot&&expectedStateReady;break;
 case 3:result=willowNativeBackgroundColumnsViewCurrent(snapshot);expected=snapshot&&expectedViewReady;break;
 case 4:result=willowNativeBackgroundColumnsAcknowledge(owner,snapshot);expected=owner&&snapshot&&expectedViewReady;break;
 }
 CHECK(result==expected);encodeState(wantState,&s);encodeView(wantView,&v);
 if(op==3||(!result&&(op==1||op==4))||!owner)CHECK(!memcmp(beforeState,wantState,48));
 if(op==2&&result)CHECK(!memcmp(beforeState,wantState,48));
 if(op==4&&result){CHECK(!memcmp(beforeState,wantState,28));CHECK(word(wantState+28)==1);for(unsigned n=32;n<48;n++)CHECK(!wantState[n]);}
 if(op==1&&result){CHECK(s.rawEpoch==epoch+1&&s.completedEpoch==epoch+1&&!s.openEpoch&&!s.fault&&s.state==1);for(unsigned l=0;l<2;l++)CHECK(!s.pending[l][0]&&!s.pending[l][1]);}
 memset(sram,0xa5,65536);memset(ram,0xa5,65536);memcpy(sram+0xf000,beforeState,48);memcpy(sram+0xf0a0,beforeView,32);
 pw(sram+0xf080,op|((nulls&1)?0x100:0)|((nulls&2)?0x200:0));pw(sram+0xf082,0xbeef);pl(sram+0xf084,epoch);pl(sram+0xf088,g2);pl(sram+0xf08c,g3);
 uint8_t controls[16];memcpy(controls,sram+0xf080,16);pw(controls+2,result);
 call(API_ENTRY,0xff6000,cases);
 CHECK(word(sram+0xf082)==result);CHECK(!memcmp(sram+0xf000,wantState,48));CHECK(!memcmp(sram+0xf0a0,wantView,32));CHECK(!memcmp(sram+0xf080,controls,16));
 for(unsigned n=0;n<65536;n++){
  if(!(n>=0xf000&&n<0xf030)&&!(n>=0xf080&&n<0xf090)&&!(n>=0xf0a0&&n<0xf0c0))CHECK(sram[n]==0xa5);
  if(!(n>=0x5e00&&n<0x6000))CHECK(ram[n^1]==0xa5);
 }
 if(result)accepted++;else refused++;cases++;
}
static void snapshotProperties(void)
{
 for(unsigned seed=0;seed<128;seed++){
  WillowNativeBackgroundColumns s;WillowNativeBackgroundColumnsView old,now;
  ready(&s,seed);CHECK(willowNativeBackgroundColumnsAcquire(&s,&old));
  /* A later completed epoch keeps the union, including a second write to an
   * already-pending column. An old view may never clear that future work. */
  s.state=2;s.rawBusy=3;s.active=1;s.openEpoch=++s.rawEpoch;
  CHECK(!willowNativeBackgroundColumnsViewCurrent(&old));
  s.pending[1][1]|=1u<<(seed&31);s.mapGeneration[1]++;
  s.completedEpoch=s.rawEpoch;s.openEpoch=0;s.active=s.rawBusy=0;s.state=1;
  WillowNativeBackgroundColumns before=s;
  CHECK(!willowNativeBackgroundColumnsAcknowledge(&s,&old));CHECK(!memcmp(&s,&before,sizeof s));
  CHECK(willowNativeBackgroundColumnsAcquire(&s,&now));CHECK(willowNativeBackgroundColumnsAcknowledge(&s,&now));
  for(unsigned l=0;l<2;l++)CHECK(!s.pending[l][0]&&!s.pending[l][1]);
  CHECK(!willowNativeBackgroundColumnsViewCurrent(&old));
  CHECK(!willowNativeBackgroundColumnsAcknowledge(&s,&now));
 }
}
int main(int argc,char**argv)
{
 if(argc!=3)return 2;FILE*f=fopen(argv[2],"rb");if(!f)return 2;
 size_t bytes=fread(rom+0x102000,1,sizeof rom-0x102000,f);if(!bytes||!feof(f))return 2;fclose(f);
 pl(rom,0xffff00);pl(rom+4,0x100004);pw(rom+0x100000,0x60fe);
 const uint8_t boot[]={0x13,0xfc,0,1,0,0xa1,0x30,0xf1,0x60,0xf2};memcpy(rom+0x100004,boot,sizeof boot);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 rom[0x1b0]='R';rom[0x1b1]='A';rom[0x1b2]=0xf8;rom[0x1b3]=0x20;pl(rom+0x1b4,0x200000);pl(rom+0x1b8,0x20ffff);
 char gamepath[4096];if(snprintf(gamepath,sizeof gamepath,"%s.closed.bin",argv[2])>=(int)sizeof gamepath)return 2;
 f=fopen(gamepath,"wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){fprintf(stderr,"Core load failed: %s\n",dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void*,retro_get_memory_data,(unsigned));
 setreg=dlsym(core,"m68k_set_reg");getreg=dlsym(core,"m68k_get_reg");run68k=dlsym(core,"m68k_run");if(!setreg||!getreg||!run68k)return 2;
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 struct retro_game_info game={gamepath,rom,sizeof rom,0};if(!retro_load_game(&game)){fprintf(stderr,"Core rejected API carrier\n");return 2;}
 ram=dlsym(core,"work_ram");cpu=dlsym(core,"m68k");sram=retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);if(!ram||!cpu||!sram){fprintf(stderr,"Missing core memory\n");return 2;}
 retro_run();if(getreg(16)!=0x100000){fprintf(stderr,"API carrier boot PC%x\n",getreg(16));return 2;}
 for(unsigned op=0;op<5;op++)for(unsigned v=0;v<26;v++)for(unsigned seed=0;seed<8;seed++)for(unsigned nulls=0;nulls<4;nulls++)one(op,v,seed,nulls);
 snapshotProperties();
 printf("PASS %u compiled68000 API cases: %u accepted,%u refused; state/view/argument bytes, native C ABI, whole RAM/SRAM canaries;128 stale-epoch/pending-union/exact-ack properties.\n",cases,accepted,refused);return 0;
}
