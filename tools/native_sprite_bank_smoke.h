/* Verify the retained bank-control fields, not hardware object latching. */
static int nativeSpriteBankCheck(void*core,unsigned char*ram,const char*symbols,
 void(*runFrame)(void),unsigned(*getReg)(int))
{
 void(*setReg)(int,unsigned)=dlsym(core,"m68k_set_reg");
 void(*setVideo)(retro_video_refresh_t)=dlsym(core,"retro_set_video_refresh");
 if(!setReg||!setVideo)return 2;
 unsigned globals=symbol(symbols,"sourceBootGlobals");
 unsigned bank=globals+nativeBootGlobalOffset(0x8022);
 unsigned budget=globals+nativeBootGlobalOffset(0x829a);
 unsigned mode=globals+nativeBootGlobalOffset(0x82a0);
 unsigned char expected[0xf600];unsigned checks=0;
 setVideo(nativeRngVideo);
 for(unsigned single=0;single<2;single++)for(unsigned flags=0;flags<256;flags++){
  ram[mode^1]=single;
  ram[bank^1]=0x90;ram[(bank+1)^1]=flags;
  ram[budget^1]=flags;ram[(budget+1)^1]=255-flags;
  /* Multiple cycles must retain source flags and alternate bank selection. */
  for(unsigned cycle=0;cycle<4;cycle++){
   unsigned old=ram[(bank+1)^1];
   unsigned selected=single||(old&128)?0x9000:0x9080;
   memcpy(expected,ram,sizeof expected);
   expected[budget^1]=1;expected[(budget+1)^1]=0;
   unsigned result=nativeSATCall(ram,symbols,setReg,getReg,runFrame,
    "sourceSpriteBankBeginProbe",0,NULL);
   if(result!=selected||memcmp(expected,ram,sizeof expected)){
    fprintf(stderr,"Sprite bank begin mismatch mode %u flags %u cycle %u\n",single,flags,cycle);return 1;
   }
   if(single){expected[bank^1]=0x90;expected[(bank+1)^1]=0;}
   else expected[(bank+1)^1]=old^128;
   result=nativeSATCall(ram,symbols,setReg,getReg,runFrame,
    "sourceSpriteBankPublishProbe",0,NULL);
   if(result==~0u||memcmp(expected,ram,sizeof expected)){
    fprintf(stderr,"Sprite bank publish mismatch mode %u flags %u cycle %u\n",single,flags,cycle);return 1;
   }
   checks++;
  }
 }
 setVideo(video);
 printf("Native sprite bank controls: %u sequential begin/publication pairs; selected bank, budget, flags and unrelated non-stack RAM match source rules\n",checks);
 return 0;
}
