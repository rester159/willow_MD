/* Execute the cartridge's DMA/SAT routines with interrupts masked. The test
 * inspects hardware VRAM independently of the software publication metadata. */
static unsigned nativeSATCallBounded(unsigned limit,unsigned*elapsed,unsigned char*ram,const char*symbols,
 void(*setReg)(int,unsigned),unsigned(*getReg)(int),void(*runFrame)(void),
 const char*name,unsigned argc,const unsigned*args)
{
 unsigned pc=0xf800;
#define SAT_W(v) do{unsigned q=(v);ram[pc++^1]=q>>8;ram[pc++^1]=q;}while(0)
#define SAT_L(v) do{unsigned ql=(v);SAT_W(ql>>16);SAT_W(ql);}while(0)
 for(unsigned n=argc;n;n--){SAT_W(0x2f3c);SAT_L(args[n-1]);}
 SAT_W(0x4eb9);SAT_L(fullSymbol(symbols,name));
 if(argc){SAT_W(0x4fef);SAT_W(argc*4);}
 unsigned stop=0xff0000+pc;SAT_W(0x60fe);
 setReg(17,0x2700);setReg(15,0xffff00);setReg(16,0xfff800);
 unsigned frames=0;do{runFrame();}while(getReg(16)!=stop&&++frames<limit);
 if(elapsed)*elapsed=frames+1;
 if(getReg(16)!=stop){fprintf(stderr,"SAT probe routine did not return: %s\n",name);return ~0u;}
 return getReg(0)&65535;
#undef SAT_L
#undef SAT_W
}
/* Existing service checks retain their eight-slice bound. */
static unsigned nativeSATCall(unsigned char*ram,const char*symbols,
 void(*setReg)(int,unsigned),unsigned(*getReg)(int),void(*runFrame)(void),
 const char*name,unsigned argc,const unsigned*args)
{return nativeSATCallBounded(8,NULL,ram,symbols,setReg,getReg,runFrame,name,argc,args);}
static int nativeSATPublicationCheck(void*core,unsigned char*ram,unsigned char*vram,
 const char*symbols,void(*runFrame)(void),unsigned(*getReg)(int))
{
 void(*setReg)(int,unsigned)=dlsym(core,"m68k_set_reg");if(!setReg)return 2;
 unsigned sat=symbol(symbols,"willowSAT"),flagAt=symbol(symbols,"flag"),limitAt=symbol(symbols,"maxTransferPerFrame");
 unsigned args[6],overflowAt=symbol(symbols,"debugIntroOverflow"),overflowBefore;unsigned char displayed[640];
#define SAT_CALL(name,n) nativeSATCall(ram,symbols,setReg,getReg,runFrame,name,n,args)
#define SAT_REQUIRE(expr) do{if(!(expr)){fprintf(stderr,"SAT publication failed at line %d\n",__LINE__);return 1;}}while(0)
#define SAT_PUT(at,v) do{unsigned a_=(at),v_=(v);ram[a_^1]=v_>>8;ram[(a_+1)^1]=v_;}while(0)
#define SAT_UPLOAD(count,prefix,method) (overflowBefore=word(ram,overflowAt),args[0]=(count),args[1]=(prefix),args[2]=(method),SAT_CALL("willowSATUpload",3),word(ram,overflowAt)==overflowBefore)
#define SAT_LAYOUT() SAT_CALL("willowSATPublishedLayout",0)
 /* Existing final-frame DMA can still be pending when the smoke loop stops. */
 SAT_CALL("DMA_flushQueue",0);
 for(unsigned n=0;n<640;n++)ram[(sat+n)^1]=(n*29+7)&255;
 SAT_REQUIRE(SAT_UPLOAD(5,2,1)==1);SAT_REQUIRE(SAT_LAYOUT()==0x0502);
 memcpy(displayed,vram+0xf400,640);
 for(unsigned n=0;n<56;n++)ram[(sat+n)^1]^=0x55;
 SAT_REQUIRE(SAT_UPLOAD(7,4,2)==1);SAT_REQUIRE(SAT_LAYOUT()==0x0502);
 SAT_REQUIRE(!memcmp(displayed,vram+0xf400,640));
 SAT_REQUIRE(SAT_UPLOAD(8,3,2)==0); /* Never overwrite pending metadata. */
 SAT_CALL("DMA_flushQueue",0);SAT_REQUIRE(SAT_LAYOUT()==0x0704);
 SAT_REQUIRE(!memcmp(vram+0xf400,ram+sat,56));
 memcpy(displayed,vram+0xf400,640);
 SAT_REQUIRE(SAT_UPLOAD(6,1,2)==1);
 SAT_CALL("sourceSATQueueClearProbe",0);SAT_CALL("DMA_flushQueue",0);
 SAT_REQUIRE(SAT_LAYOUT()==0x0704 && !memcmp(displayed,vram+0xf400,640));
 /* Put an earlier entry within budget, then make SAT exceed that budget.
  * The SDK uses zero as its no-limit sentinel, so first-entry overflow is a
  * different behavior and is covered separately below. */
 SAT_PUT(flagAt,word(ram,flagAt)|2);SAT_PUT(limitAt,8);
 args[0]=0;args[1]=0xff0000+sat;args[2]=0xf300;args[3]=4;args[4]=2;
 SAT_REQUIRE((SAT_CALL("DMA_queueDmaFast",5)&255)==1);
 SAT_REQUIRE(SAT_UPLOAD(6,1,2)==1);SAT_CALL("DMA_flushQueue",0);
 SAT_REQUIRE(SAT_LAYOUT()==0x0704 && !memcmp(displayed,vram+0xf400,640));
 /* With SAT first, this SDK executes it despite the limit-zero sentinel. */
 SAT_REQUIRE(SAT_UPLOAD(6,1,2)==1);SAT_CALL("DMA_flushQueue",0);
 SAT_REQUIRE(SAT_LAYOUT()==0x0601);
 SAT_PUT(flagAt,word(ram,flagAt)&~2);SAT_PUT(limitAt,65535);
 /* Queue full: rejection must leave both displayed and pending metadata. */
 for(unsigned n=0;n<32;n++){
  args[0]=0;args[1]=0xff0000+sat;args[2]=0xf300;args[3]=1;args[4]=2;
  SAT_REQUIRE((SAT_CALL("DMA_queueDmaFast",5)&255)==1);
 }
 SAT_REQUIRE(SAT_UPLOAD(8,3,2)==0);SAT_CALL("DMA_flushQueue",0);
 SAT_REQUIRE(SAT_LAYOUT()==0x0601);
 SAT_REQUIRE(SAT_UPLOAD(81,0,1)==0 && SAT_UPLOAD(2,3,1)==0);
 SAT_REQUIRE(SAT_LAYOUT()==0x0601);
 SAT_REQUIRE(SAT_UPLOAD(80,80,2)==1);SAT_CALL("DMA_flushQueue",0);
 SAT_REQUIRE(SAT_LAYOUT()==0x5050 && !memcmp(vram+0xf400,ram+sat,640));
 SAT_REQUIRE(SAT_UPLOAD(1,0,0)==1);SAT_REQUIRE(SAT_LAYOUT()==0x0100);
 /* SAT is within budget, but the later entry is dropped. */
 SAT_PUT(flagAt,word(ram,flagAt)|2);SAT_PUT(limitAt,16);
 SAT_REQUIRE(SAT_UPLOAD(2,1,2)==1);
 args[0]=0;args[1]=0xff0000+sat;args[2]=0xf300;args[3]=8;args[4]=2;
 SAT_CALL("DMA_queueDmaFast",5);SAT_CALL("DMA_flushQueue",0);
 SAT_REQUIRE(SAT_LAYOUT()==0x0201);
 SAT_PUT(flagAt,word(ram,flagAt)&~2);SAT_PUT(limitAt,65535);
 puts("Compiled SAT publication: immediate/queued uploads, pending rejection, cancellation, budget drops, first-entry limit sentinel, full queue and invalid bounds match hardware");
#undef SAT_UPLOAD
#undef SAT_LAYOUT
#undef SAT_PUT
#undef SAT_REQUIRE
#undef SAT_CALL
 return 0;
}
