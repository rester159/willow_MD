/* Independent expected ownership, including words spanning native allocations. */
static uint8_t*demoStateByte(uint8_t*ram,uint8_t*sram,unsigned globals,unsigned at)
{
 if(at>=0x0faa&&at<0x0fae)return sram+0x8517+(at-0x0faa);
 if(at==0x0e96)return sram+0x8516;
 if(at>=0xf892&&at<0xf898)return sram+0x850f+(at-0xf892);
 if(at==0xf99e)return sram+0x8515;
 if(at>=0x0a32&&at<0x0a36)return sram+0x8500+(at-0x0a32);
 if(at>=0x0a3a&&at<0x0a3c)return sram+0x8504+(at-0x0a3a);
 if(at>=0x0e92&&at<0x0e95)return sram+0x8506+(at-0x0e92);
 if(at>=0x0f08&&at<0x0f0a)return sram+0x8509+(at-0x0f08);
 if(at>=0x0fb0&&at<0x0fb4)return sram+0x850b+(at-0x0fb0);
 if(at>=0x0a30&&at<0x0a32)return sram+0x8400+(at-0x0a30);
 if(at==0x0a3c)return sram+0x8402;
 if(at>=0x0a42&&at<0x0a4a)return sram+0x8403+(at-0x0a42);
 if(at>=0x0e68&&at<0x0e6c)return sram+0x840b+(at-0x0e68);
 if(at>=0x0f40&&at<0x0f80)return sram+0x840f+(at-0x0f40);
 if(at>=0x0f88&&at<0x0fa8)return sram+0x844f+(at-0x0f88);
 if(at>=0x8289&&at<=0x828d)return sram+0x8200+(at-0x8289);
 if(at>=0x82c2&&at<=0x8342)return sram+0x8205+(at-0x82c2);
 if(at>=0x8346&&at<=0x83c2)return sram+0x8286+(at-0x8346);
 if(at>=0x83c4&&at<=0x83ca)return sram+0x8303+(at-0x83c4);
 if(at>=0x83cc&&at<=0x8401)return sram+0x830a+(at-0x83cc);
 if(at==0xf9a2)return sram+0x8340;
 if(at==0x8288)return ram+((globals+102)^1);
 if(at==0x8343)return ram+((globals+145)^1);
 if(at==0xf9a3)return ram+((globals+170)^1);
 if(at==0x8344||at==0x8345)return sram+0x8036+(at-0x8344);
 if(at==0x83c3)return sram+0x8035;
 if(at==0x83cb)return sram+0x8000;
 return NULL;
}
static uint8_t*retainedStateAuditByte(uint8_t*ram,uint8_t*sram,unsigned globals,unsigned at)
{
 if(at>=0x9356&&at<0x93a6)return sram+0x8800+at-0x9356;
 if(at>=0x946e&&at<0x95ce)return sram+0x8850+at-0x946e;
 if(at>=0x96be&&at<0x97fe)return sram+0x89b0+at-0x96be;
 if(at>=0xf466&&at<0xf486)return sram+0x8af0+at-0xf466;
 return demoStateByte(ram,sram,globals,at);
}
static int nativeDemoStateCheck(void*core,uint8_t*ram,const char*symbols,
 void(*runFrame)(void),unsigned(*getReg)(int))
{
 void(*setReg)(int,unsigned)=dlsym(core,"m68k_set_reg");
 void(*setVideo)(retro_video_refresh_t)=dlsym(core,"retro_set_video_refresh");
 void*(*memory)(unsigned)=dlsym(core,"retro_get_memory_data");
 if(!setReg||!setVideo||!memory)return 2;
 uint8_t*sram=memory(RETRO_MEMORY_SAVE_RAM);if(!sram)return 2;
 uint8_t original[65536],originalSRAM[65536],expected[0xf600],expectedSRAM[65536];
 memcpy(original,ram,sizeof original);memcpy(originalSRAM,sram,sizeof originalSRAM);
 unsigned globals=symbol(symbols,"sourceBootGlobals"),checks=0;
 const char*names[2][4]={{"introQueueByte","introQueuePutByte","introQueueWord","introQueuePutWord"},
  {"sourceOpeningBusByte","sourceOpeningBusPutByte","sourceOpeningBusWord","sourceOpeningBusPutWord"}};
 setVideo(nativeRngVideo);
 for(unsigned n=0;n<321;n++)sram[0x8200+n]=n*73+19;
 for(unsigned n=0;n<27;n++)sram[0x8500+n]=n*39+17;
 for(unsigned n=0;n<784;n++)sram[0x8800+n]=n*29+47;
 for(unsigned n=0;n<111;n++)sram[0x8400+n]=n*51+37;
 for(unsigned pass=0;pass<2;pass++)for(unsigned at=0;at<65536;at++){
  uint8_t*high=retainedStateAuditByte(ram,sram,globals,at);if(!high)continue;
  for(unsigned op=0;op<4;op++){
   uint8_t*low=retainedStateAuditByte(ram,sram,globals,at+1);
   if(op>=2&&((at&1)||!low))continue;
   memcpy(expected,ram,sizeof expected);memcpy(expectedSRAM,sram,sizeof expectedSRAM);
   unsigned value=(at*311+pass*977+op*73)&65535,args[]={at,value};
   unsigned wanted=op<2?*high:((unsigned)*high<<8)|*low;
   if(op==1)*retainedStateAuditByte(expected,expectedSRAM,globals,at)=value;
   if(op==3){*retainedStateAuditByte(expected,expectedSRAM,globals,at)=value>>8;*retainedStateAuditByte(expected,expectedSRAM,globals,at+1)=value;}
   unsigned result=nativeSATCall(ram,symbols,setReg,getReg,runFrame,names[pass][op],op&1?2:1,args);
   if(result==~0u||(!(op&1)&&(result&(op?65535:255))!=wanted)||memcmp(expected,ram,sizeof expected)||memcmp(expectedSRAM,sram,sizeof expectedSRAM)){
    fprintf(stderr,"Demo state bus mismatch pass %u address %04x op %u result %x expected %x\n",pass,at,op,result,wanted);return 1;
   }
   checks++;
  }
 }
 memcpy(ram,original,sizeof original);memcpy(sram,originalSRAM,sizeof originalSRAM);setVideo(video);
 printf("Native demo state: %u compiled accesses through both buses; 1243 supplemental/visibility bytes and existing aliases coherent; full unrelated RAM/SRAM preserved\n",checks);
 return 0;
}
