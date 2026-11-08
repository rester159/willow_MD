/* Terminal storage test: execute compiled fills without advancing the game.
 * This does not stand in for CPS object banks or physical video publication. */
static void nativeMapFillWord(unsigned char*ram,unsigned at,unsigned value)
{ram[at^1]=value>>8;ram[(at+1)^1]=value;}
static int nativeSceneMapFillCheck(void*core,unsigned char*ram,const char*symbols,
 void(*runFrame)(void),unsigned(*getReg)(int))
{
 void(*setReg)(int,unsigned)=dlsym(core,"m68k_set_reg");
 void(*setVideo)(retro_video_refresh_t)=dlsym(core,"retro_set_video_refresh");
 if(!setReg||!setVideo)return 2;
 unsigned map=symbol(symbols,"debugScroll2Map");
 unsigned nearAt=longword(ram,map)&65535;
 unsigned farAt=longword(ram,symbol(symbols,"debugScroll3Cells"))&65535;
 unsigned hudAt=symbol(symbols,"debugHudSurface");
 unsigned nearId=WILLOW_SCROLL2_ZERO_PAIR;
 unsigned farId=WILLOW_SCROLL3_CLEAR_PAIR;
 /* Normal cartridge uses dense Scroll2 IDs and no mutable dictionary entries. */
 if(word(ram,map+14)||ram[(map+16)^1]||nearAt+8192>0xf600||
    farAt+5632>0xf600||hudAt+1188>0xf600)return 1;
 unsigned char expected[0xf600];unsigned fills=0,rejections=0;
 const unsigned rejected[][4]={{0x900000,0,0,512},{0x908000,0,0,512},
  {0x910004,0x7020,0,4096},{0x910000,0x7020,0,4095},
  {0x910000,0x7020,1,4096},{0x910000,0x7120,0,4096},
  {0x918000,1,0,4096},{0x914000,0,0,4096}};
 const unsigned accepted[][4]={{0x910000,0x7020,0,4096},
  {0x918000,0,0,4096},{0x914000,0x0a00,0,4096}};
 setVideo(nativeRngVideo);
 for(unsigned sample=0;sample<16;sample++){
  for(unsigned n=0;n<8192;n++)ram[(nearAt+n)^1]=(n*31+sample*17)&255;
  for(unsigned n=0;n<5632;n++)ram[(farAt+n)^1]=(n*23+sample*19)&255;
  for(unsigned n=0;n<1188;n++)ram[(hudAt+n)^1]=(n*13+sample*37)&255;
  memcpy(expected,ram,sizeof expected);
  for(unsigned n=0;n<8;n++){
   unsigned result=nativeSATCall(ram,symbols,setReg,getReg,runFrame,
     "sourceSceneMapFillProbe",4,rejected[n]);
   if(result||memcmp(expected,ram,sizeof expected)){
    fprintf(stderr,"Native map fill rejection failed sample %u case %u result %u\n",sample,n,result);return 1;
   }
   rejections++;
  }
  for(unsigned region=0;region<3;region++){
   if(region==0){
    for(unsigned n=0;n<288;n++){
     nativeMapFillWord(expected,hudAt+n*4,0x7020);
     nativeMapFillWord(expected,hudAt+n*4+2,0);
    }
    for(unsigned n=0;n<18;n++)nativeMapFillWord(expected,hudAt+1152+n*2,n%3==2?255:65535);
   }else if(region==1){
    for(unsigned n=0;n<4096;n++)nativeMapFillWord(expected,nearAt+n*2,nearId);
   }else{
    /* Independent MSB-first bit stream, not the native packed-fill helper. */
    for(unsigned n=0;n<5632;n++)expected[(farAt+n)^1]=0;
    for(unsigned bit=0;bit<4096*11;bit++)
     expected[(farAt+bit/8)^1]|=((farId>>(10-bit%11))&1)<<(7-bit%8);
   }
   unsigned result=nativeSATCall(ram,symbols,setReg,getReg,runFrame,
     "sourceSceneMapFillProbe",4,accepted[region]);
   if(result!=1||memcmp(expected,ram,sizeof expected)){
    fprintf(stderr,"Native map fill mismatch sample %u region %u result %u\n",sample,region,result);return 1;
   }
   fills++;
  }
 }
 setVideo(video);
 printf("Native ordered map fills: %u fills, %u rejected requests; retained HUD/dense Scroll2/packed Scroll3 match at each boundary, unrelated non-stack RAM preserved\n",fills,rejections);
 return 0;
}
