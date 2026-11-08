#include "boot_scheduler_fixture.h"
static int nativeBootContinuationCheck(void*core,unsigned char*ram,const char*symbols,
 void(*runFrame)(void),unsigned(*getReg)(int))
{
 void(*setReg)(int,unsigned)=dlsym(core,"m68k_set_reg");
 void(*setVideo)(retro_video_refresh_t)=dlsym(core,"retro_set_video_refresh");
 if(!setReg||!setVideo)return 2;
 FILE*f=fopen("build/boot-continuation-original.bin","rb");if(!f)return 2;
 unsigned targets[BOOT_SCHEDULER_FIXTURE_BYTES];
 const unsigned bases[]={symbol(symbols,"sourceTaskState"),symbol(symbols,"taskRecords"),
  symbol(symbols,"taskCommands"),symbol(symbols,"sourceBootGlobals")};
 const unsigned sizes[]={84,512,256,sizeof(WillowNativeBootGlobals)};
 unsigned mapped=0;for(unsigned r=0;r<4;r++)for(unsigned n=0;n<sizes[r];n++)targets[mapped++]=bases[r]+n;
 if(mapped!=BOOT_SCHEDULER_FIXTURE_BYTES){fclose(f);return 2;}
 unsigned completion=bases[3]+nativeBootGlobalOffset(0xf891),scene=bases[3]+nativeBootGlobalOffset(0x82a6);
 uint8_t before[BOOT_SCHEDULER_FIXTURE_BYTES],after[BOOT_SCHEDULER_FIXTURE_BYTES],expected[0xf600];
 unsigned checks=0,starts=0,finishes=0;int flags;
 setVideo(nativeRngVideo);
 while((flags=fgetc(f))!=EOF){
  if(flags&~7 || fread(before,1,sizeof before,f)!=sizeof before || fread(after,1,sizeof after,f)!=sizeof after){fclose(f);return 2;}
  if(flags&1){for(unsigned n=0;n<mapped;n++)ram[targets[n]^1]=before[n];starts++;}
  else{
   /* The oracle exposes command/scene completion as controlled external
    * events. Scheduler, task state, ring data and continuations carry over. */
   if(flags&2){ram[completion^1]=0;ram[scene^1]=0;}
   for(unsigned n=0;n<mapped;n++)if(ram[targets[n]^1]!=before[n]){
    fprintf(stderr,"Boot continuation lost state before fixture %u offset %u\n",checks,n);fclose(f);return 1;
   }
  }
  memcpy(expected,ram,sizeof expected);
  for(unsigned n=0;n<mapped;n++)expected[targets[n]^1]=after[n];
  unsigned result=nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceBootDispatch",0,NULL);
  if(result!=0x424f){fprintf(stderr,"Boot continuation fixture %u dispatch status %x\n",checks,result);fclose(f);return 1;}
  if(flags&4)finishes++;
  else if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceTasksWake",0,NULL)==~0u){fclose(f);return 1;}
  if(memcmp(expected,ram,sizeof expected)){
   for(unsigned n=0;n<sizeof expected;n++)if(expected[n]!=ram[n]){
    fprintf(stderr,"Boot continuation fixture %u native byte %04x got %02x want %02x\n",checks,n^1,ram[n],expected[n]);break;
   }
   fclose(f);return 1;
  }
  checks++;
 }
 int bad=ferror(f);fclose(f);if(bad||starts!=1536||finishes!=1364||checks!=18196)return 1;
 /* Invalid manager states and scan inhibition must reject atomically. */
 unsigned manager=bases[3]+nativeBootGlobalOffset(0x8000);
 ram[bases[1]^1]=4;
 ram[(bases[0]+52)^1]=0x7f;ram[(bases[0]+53)^1]=0xf2;
 ram[manager^1]=0;ram[(manager+1)^1]=11;
 ram[(bases[0]+14)^1]=0;memcpy(expected,ram,sizeof expected);
 if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceBootDispatch",0,NULL)!=1 || memcmp(expected,ram,sizeof expected))return 1;
 ram[(bases[0]+14)^1]=255;memcpy(expected,ram,sizeof expected);
 if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceBootDispatch",0,NULL)!=2 || memcmp(expected,ram,sizeof expected))return 1;
 setVideo(video);
 printf("Native boot continuations: %u scheduler/wake boundaries across %u starts and %u terminations; carried state matches original-verified fixtures, unrelated native RAM preserved; invalid manager state and scan inhibition reject atomically\n",checks,starts,finishes);
 return 0;
}
