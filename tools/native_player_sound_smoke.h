/* Production player callback versus independently executed original 786A.
 * The sound consumer cursor and all unrelated native RAM must be preserved. */
static unsigned nativeSoundWord(const unsigned char*ram,unsigned at)
{return ((unsigned)ram[at^1]<<8)|ram[(at+1)^1];}
static int nativePlayerSoundCheck(void*core,unsigned char*ram,const char*symbols,
 void(*runFrame)(void),unsigned(*getReg)(int))
{
 void(*setReg)(int,unsigned)=dlsym(core,"m68k_set_reg");if(!setReg)return 2;
 FILE*f=fopen("build/player-sound-original.bin","rb");if(!f)return 2;
 unsigned ring=symbol(symbols,"soundCommands"),cursor=symbol(symbols,"sourceTaskState")+84;
 unsigned globals=symbol(symbols,"sourceBootGlobals");
 WillowNativeBootGlobals layout;
 uint8_t*g0=willowNativeBootGlobalsResolve(&layout,0x8290,1),*g1=willowNativeBootGlobalsResolve(&layout,0x8293,1);
 if(!g0||!g1){fclose(f);return 1;}
 unsigned gate0=globals+(g0-layout.bytes),gate1=globals+(g1-layout.bytes);
 unsigned char fixture[264],expected[0xf600];unsigned checks=0;
 while(fread(fixture,1,sizeof fixture,f)==sizeof fixture){
  ram[cursor^1]=fixture[2];ram[(cursor+1)^1]=fixture[3];
  ram[gate0^1]=fixture[4];ram[gate1^1]=fixture[5];
  for(unsigned n=0;n<128;n++)ram[(ring+n)^1]=fixture[6+n];
  memcpy(expected,ram,sizeof expected);
  expected[cursor^1]=fixture[134];expected[(cursor+1)^1]=fixture[135];
  for(unsigned n=0;n<128;n++)expected[(ring+n)^1]=fixture[136+n];
  unsigned arg=willowMotionWord(fixture);
  if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"normalSoundQueue",1,&arg)==~0u){fclose(f);return 1;}
  if(memcmp(expected,ram,sizeof expected)){
   for(unsigned n=0;n<sizeof expected;n++)if(expected[n]!=ram[n]){
    fprintf(stderr,"Native sound fixture %u mismatch at %04x: got %02x expected %02x\n",checks,n^1,ram[n],expected[n]);break;
   }
   fclose(f);return 1;
  }
  checks++;
 }
 int bad=ferror(f);fclose(f);if(bad||checks!=256)return 1;
 /* Exercise the installed callbacks through the actual player dispatcher. */
 unsigned player=symbol(symbols,"debugPlayerRecord"),health=symbol(symbols,"debugPlayerGateGlobals");
 unsigned charge=symbol(symbols,"hudCharge"),overflow=symbol(symbols,"debugIntroOverflow");
 unsigned beforeOverflow=nativeSoundWord(ram,overflow),gate=0;
 ram[gate0^1]=ram[gate1^1]=0;ram[cursor^1]=0;ram[(cursor+1)^1]=124;
 ram[health^1]=4;ram[(health+1)^1]=0;ram[(health+2)^1]=5;ram[(health+3)^1]=0;
 for(unsigned n=0;n<2;n++){ram[(player+54+n)^1]=0;ram[(player+80+n)^1]=0;}
 nativeSATCall(ram,symbols,setReg,getReg,runFrame,"stepDispatchedPlayer",1,&gate);
 if(nativeSoundWord(ram,cursor)!=0 || nativeSoundWord(ram,ring+124)!=0x82 ||
    nativeSoundWord(ram,ring+126)!=255 || nativeSoundWord(ram,overflow)!=beforeOverflow){
  fprintf(stderr,"Player damage: cursor=%u cmd=%04x term=%04x overflow=%u->%u state=%u sub=%u hp=%u cached=%u timer=%u\n",nativeSoundWord(ram,cursor),nativeSoundWord(ram,ring+124),nativeSoundWord(ram,ring+126),beforeOverflow,nativeSoundWord(ram,overflow),ram[(player+2)^1],ram[(player+3)^1],ram[health^1],ram[(health+2)^1],nativeSoundWord(ram,player+54));return 1;
 }
 ram[(charge+2)^1]=1;ram[(player+49)^1]=0;
 nativeSATCall(ram,symbols,setReg,getReg,runFrame,"stepDispatchedPlayer",1,&gate);
 if(nativeSoundWord(ram,cursor)!=4 || nativeSoundWord(ram,ring)!=244 ||
    nativeSoundWord(ram,ring+2)!=255 || ram[(charge+2)^1] ||
    nativeSoundWord(ram,overflow)!=beforeOverflow){
  fprintf(stderr,"Player charge cancellation did not enqueue source command F4/FF\n");return 1;
 }
 puts("Native player dispatch: damage produces 82/FF with cursor wrap; charge cancellation produces F4/FF, without unavailable-service overflow");
 puts("Native player sound: 256 original-CPU fixtures; all 64 producer cursors and four gate combinations; command/FF ordering, wrap, consumer cursor and unrelated RAM preserved");
 return 0;
}
