/* Probe observed source bus addresses at the verified demo-start endpoint.
 * Resolver checks cover starts, not unknown extents of direct pointer access. */
static int nativeDemoAccessCheck(void*core,uint8_t*ram,const char*symbols,
 void(*runFrame)(void),unsigned(*getReg)(int))
{
 void(*setReg)(int,unsigned)=dlsym(core,"m68k_set_reg");
 void*(*memory)(unsigned)=dlsym(core,"retro_get_memory_data");
 if(!setReg||!memory)return 2;
 uint8_t*sram=memory(RETRO_MEMORY_SAVE_RAM);if(!sram)return 2;
 uint8_t access[65536],saved[65536],savedSRAM[65536];
 FILE*f=fopen("build/demo-update-bus-access.bin","rb");
 if(!f||fread(access,1,sizeof access,f)!=sizeof access||fgetc(f)!=EOF)return 2;fclose(f);
 memcpy(saved,ram,sizeof saved);memcpy(savedSRAM,sram,sizeof savedSRAM);
 unsigned overflow=symbol(symbols,"debugIntroOverflow"),missing=0,buses=0,resolvers=0;
 FILE*out=fopen("build/demo-update-native-access.csv","w");if(!out)return 2;
 fputs("source,kind,missing\n",out);
 for(unsigned at=0;at<65536;at++){
  /* 2000..37FF and 4000..7FFF are relocated fixture graphics pages. */
  if(at>=0x2000&&at<0x8000)continue;
  for(unsigned kind=0;kind<2;kind++){
   if(!(access[at]&(kind?4:3)))continue;
   unsigned args[]={at};
   unsigned result=nativeSATCall(ram,symbols,setReg,getReg,runFrame,kind?"sourceOpeningResolve":"sourceOpeningBusByte",1,args);
   unsigned absent=result==~0u||(uint16_t)word(ram,overflow)!=(uint16_t)word(saved,overflow);
   if(kind){resolvers++;unsigned pointer=getReg(0),physical=pointer&0xffffff;absent|=!pointer||!((physical>=0xff0000&&physical<0xfff600)||(physical>=0x200000&&physical<0x210000));}
   else buses++;
   fprintf(out,"%04x,%s,%u\n",at,kind?"resolver":"bus",absent);
   if(absent){fprintf(stderr,"Unbound demo-update %s %04x\n",kind?"resolver":"bus",at);missing++;}
   /* Calls may only touch diagnostic scratch and overflow, not retained state. */
   sceneCommitPut(ram,overflow,(uint16_t)word(saved,overflow));
   if(memcmp(ram,saved,0xf600)||memcmp(sram,savedSRAM,sizeof savedSRAM)){fclose(out);return 1;}
  }
 }
 fclose(out);memcpy(ram,saved,sizeof saved);memcpy(sram,savedSRAM,sizeof savedSRAM);
 printf("Compiled demo-update address audit: %u bus bytes, %u resolver starts, %u unbound; direct pointer extents and write semantics remain separate\n",buses,resolvers,missing);
 return missing?1:0;
}
