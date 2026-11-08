/* Projection of original 18D0..1A8C onto the four controller/history bytes.
 * Fixtures are exported only after the entire original IRQ service matches. */
static int nativeControllerHistoryCheck(void*core,unsigned char*ram,const char*symbols,
 void(*runFrame)(void),unsigned(*getReg)(int))
{
 void(*setReg)(int,unsigned)=dlsym(core,"m68k_set_reg");
 void(*setVideo)(retro_video_refresh_t)=dlsym(core,"retro_set_video_refresh");
 if(!setReg||!setVideo)return 2;
 FILE*f=fopen("build/controller-history-original.bin","rb");if(!f)return 2;
 unsigned at=symbol(symbols,"sourceBootGlobals")+nativeBootGlobalOffset(0x8060);
 unsigned char fixture[10],expected[0xf600];unsigned checks=0;size_t count;
 setVideo(nativeRngVideo);
 while((count=fread(fixture,1,sizeof fixture,f))==sizeof fixture){
  for(unsigned n=0;n<4;n++)ram[(at+n)^1]=fixture[n];
  memcpy(expected,ram,sizeof expected);
  for(unsigned n=0;n<4;n++)expected[(at+n)^1]=fixture[6+n];
  unsigned args[2]={fixture[4],fixture[5]};
  if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"normalSourceControllerPublish",2,args)==~0u ||
     memcmp(expected,ram,sizeof expected)){
   fprintf(stderr,"Native controller history mismatch fixture %u\n",checks);fclose(f);return 1;
  }
  checks++;
 }
 int bad=ferror(f);fclose(f);if(bad||count||checks!=4096)return 1;
 setVideo(video);
 puts("Native controller history: 4096 original IRQ projections; both current and previous controller bytes match, unrelated non-stack RAM preserved");
 return 0;
}
