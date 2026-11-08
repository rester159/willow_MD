#ifndef WILLOW_TEST_NATIVE_BACKGROUND_FAR_PUBLICATION_H
#define WILLOW_TEST_NATIVE_BACKGROUND_FAR_PUBLICATION_H
/* Host observation only. Target bytes and execution are never modified. */
#define FAR_O(n) RENDER_BG_FAR_CACHE_##n##_OFFSET
#define FAR_BASE (RENDER_BG_FAR_CACHE_BASE&65535u)
#define FAR_NONE 65535u
#define FAR_SLOTS 165u
#define FAR_BYTES 436u
static struct FarObserver {
 unsigned owner[FAR_SLOTS],epoch,initialized,open,steps,done,words,controls;
 unsigned slot[16],pattern[16],reference[16],oldOwner[16];
 unsigned address,code,pending,latch,increment,lastAddress,firstControl;
 unsigned batches,transfers,deadlineFailures;
 uint64_t begin,lastRetire;
 uint8_t expected[FAR_SLOTS*32],cacheBefore[FAR_BYTES];
 uint8_t vramBefore[65536],cramBefore[128],vsramBefore[128],regsBefore[32];
} farPaths[2];
static FILE *farEvents,*farTransferData,*farBatchStates,*farPublications,*farStates,*farOwners;
static unsigned farBatchSample,farTransferSample,farPublicationSample;
static unsigned farCapturePending,farCaptureFrame,farCaptureSample,farCaptured;
static unsigned farLatchPending,farLatchPendingFrame;
static FILE*farLatchBoundaries;
static unsigned farLatchCounterCurrent(unsigned observed,const unsigned*original,unsigned nativePC){
 if(observed==latchCaptures){farLatchPending=0;return 1;}
 unsigned originalPC=original[16];
 if(observed+1!=latchCaptures||originalPC!=FAR_LATCH_LOOP_BOUNDARY||nativePC!=originalPC||
    farLatchPending||!latchWords||latchWords>latchExpectedWords||latchWords%32)return 0;
 if(willowMotionWord(platforms[0]+(originalPC-0x100000))!=0xb5c1||
    memcmp(platforms[0]+(originalPC-0x100000),platforms[1]+(originalPC-0x100000),4))return 0;
 unsigned chunks=latchWords/32,offset=0x2660+(chunks-1)*128;
 unsigned table=FAR_LATCH_CHUNK_TABLE,sourceBase=latchSelector==0x9000?0x900000:0x908000;
 unsigned nativeBase=latchSelector==0x9000?0x20c000:0x20d000;
 if(latchSelector!=0x9000&&latchSelector!=0x9080)return 0;
 if(original[8]!=0xff0000+offset+64||cpu->dar[8]!=original[8]||
    original[9]!=sourceBase+latchWords*2||cpu->dar[9]!=nativeBase+latchWords*2||
    original[10]!=table+chunks*2||cpu->dar[10]!=original[10]||
    original[0]!=original[10]||cpu->dar[0]!=original[0]||
    original[2]!=offset||cpu->dar[2]!=offset||
    original[1]!=table+(latchExpectedWords/32)*2||cpu->dar[1]!=original[1])return 0;
 unsigned sp=cpu->dar[15]&65535;
 if(rawWord(sp+12)!=latchExpectedWords/4||rawLong(sp+16)!=latchExpectedWords/32||
    rawWord(sp+14)!=(latchExpectedWords/4)%8)return 0;
 for(unsigned n=0;n<1024;n++)if(latchSeenWords[n]!=(n<latchWords))return 0;
 farLatchPending=1;farLatchPendingFrame=frameNumber;
 fprintf(farLatchBoundaries,"%u,%u,%u,%x,%u,%u\n",frameNumber,observed,latchCaptures,originalPC,latchWords,latchExpectedWords);
 return 1;
}

static unsigned farSlot(unsigned tile){if(tile>=759&&tile<=875)return tile-759;if(tile>=1481&&tile<=1528)return tile-1481+117;return FAR_NONE;}
static unsigned farTile(unsigned slot){return slot<117?759+slot:1481+slot-117;}
static unsigned farWord(unsigned at){return rawWord(FAR_BASE+at);}
static unsigned farLong(unsigned at){return rawLong(FAR_BASE+at);}
static unsigned farByte(unsigned at){return ram[(FAR_BASE+at)^1];}
static void farPutWord(uint8_t*p,unsigned v){p[0]=v>>8;p[1]=v;}
static void farPutLong(uint8_t*p,unsigned v){farPutWord(p,v>>16);farPutWord(p+2,v);}
static unsigned farUsed(unsigned offset,unsigned slot){return(farLong(offset+(slot>>5)*4)>>(slot&31))&1;}
static void farDumpCache(FILE*f){for(unsigned n=0;n<FAR_BYTES;n++)if(fputc(farByte(n),f)==EOF)abort();}
static void farFail(const char*why){fprintf(stderr,"Far-cache observation path%u frame%u PC%x: %s\n",renderPath,frameNumber,sourcePC(),why);abort();}
static unsigned farCodePC(unsigned pc){return pc>=RENDER_BG_FAR_CODE_BEGIN&&pc<RENDER_BG_FAR_CODE_END;}
static unsigned farOpen(void){return farPaths[renderPath].open;}
static void farProtection(void){
 if(farLong(FAR_O(DISPLAYED_USE)+20)&~31u||farLong(FAR_O(PREPARED_USE)+20)&~31u||farWord(FAR_O(RESERVED)))farFail("corrupt ownership bitmaps");
 for(unsigned n=0;n<2048;n++){
  unsigned word=renderVRAM[(0xe000+n*2)^1]<<8|renderVRAM[(0xe001+n*2)^1],slot=farSlot(word&2047);
  if(slot!=FAR_NONE&&!farUsed(FAR_O(DISPLAYED_USE),slot))farFail("physical planeB reference is not eviction protected");
  word=rawWord((RENDER_BG_FAR_PREPARED_WORDS&65535)+n*2);slot=farSlot(word&2047);
  if(word!=FAR_NONE&&slot!=FAR_NONE&&!farUsed(FAR_O(PREPARED_USE),slot))farFail("prepared native far reference is not eviction protected");
#ifdef FAR_SHARED_NEAR_WORDS
  word=renderVRAM[(0xc000+n*2)^1]<<8|renderVRAM[(0xc001+n*2)^1];slot=farSlot(word&2047);
  if(slot!=FAR_NONE&&!farUsed(FAR_O(DISPLAYED_USE),slot))farFail("physical planeA reference is not eviction protected");
  word=rawWord((FAR_SHARED_NEAR_WORDS&65535)+n*2);slot=farSlot(word&2047);
  if(word!=FAR_NONE&&slot!=FAR_NONE&&!farUsed(FAR_O(PREPARED_USE),slot))farFail("prepared near reference is not eviction protected");
#endif
 }
}
static void farOwnersCurrent(void){
 struct FarObserver*p=farPaths+renderPath;
 if(!p->initialized)farFail("cache used before observed post-source-clear initialization");
 if(farWord(FAR_O(PHASE))==3)farFail("unclosed copying phase outside upload");
 if(farLong(FAR_O(RESIDENCY_EPOCH))!=p->epoch){fprintf(stderr,"Far epoch expected%u actual%u phase%u owner0=%u expected%u\n",p->epoch,farLong(FAR_O(RESIDENCY_EPOCH)),farWord(FAR_O(PHASE)),farWord(FAR_O(OWNER)),p->owner[0]);farFail("unobserved residency epoch change");}
 for(unsigned n=0;n<FAR_SLOTS;n++)if(farWord(FAR_O(OWNER)+n*2)!=p->owner[n])farFail("target owner differs from independently committed VRAM owner");
 farProtection();
}
static unsigned farPatternByte(unsigned at){unsigned slot=farSlot(at>>5);return slot==FAR_NONE?backgroundExpectedPatterns[at]:farPaths[renderPath].expected[slot*32+(at&31)];}
static void farValidatePublication(const BackgroundSpan*s){
 if(farOpen())farFail("plane publication overlaps pattern upload");
 farOwnersCurrent();
 if(farWord(FAR_O(PHASE))!=1||farWord(FAR_O(MISSING))||
    farLong(FAR_O(PREPARED_RESIDENCY_EPOCH))!=farLong(FAR_O(RESIDENCY_EPOCH))||
    farLong(FAR_O(SOURCE_EPOCH))!=rawLong(RENDER_BG_FAR_SOURCE_EPOCH&65535)||
    farLong(FAR_O(PALETTE_EPOCH))!=s->palette||farLong(FAR_O(PREPARATION_GENERATION))!=s->generation)farFail("stale/uncommitted far plane certificate");
 for(unsigned n=0;n<4;n++)if(farWord(FAR_O(VIEW)+n*2)!=s->view[n])farFail("far certificate view differs from published planes");
}
static void farSnapshot(unsigned sample){
 if(sample!=farPublicationSample)farFail("cache/publication sample ordering changed");
 if(!farCaptured&&!farCapturePending&&farPaths[0].epoch>1&&backgroundSourceWord(0x800110)>=132&&backgroundSourceWord(0x800110)<512){farCapturePending=1;farCaptureFrame=frameNumber;farCaptureSample=sample;}
 fprintf(farPublications,"%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n",sample,frameNumber,farLong(FAR_O(RESIDENCY_EPOCH)),farLong(FAR_O(SOURCE_EPOCH)),farLong(FAR_O(PALETTE_EPOCH)),farLong(FAR_O(PREPARATION_GENERATION)),farWord(FAR_O(VIEW)),farWord(FAR_O(VIEW)+2),farWord(FAR_O(VIEW)+4),farWord(FAR_O(VIEW)+6),farPaths[0].transfers,farPaths[0].batches);
 farDumpCache(farStates);for(unsigned n=0;n<FAR_SLOTS;n++){unsigned v=farPaths[0].owner[n];if(fputc(v>>8,farOwners)==EOF||fputc(v&255,farOwners)==EOF)abort();}farPublicationSample++;
}
static void farEvent(unsigned phase,unsigned status){
 struct FarObserver*p=farPaths+renderPath;uint64_t deadline=(uint64_t)renderPaths[renderPath].irqFrame*RENDER_FRAME_TICKS+RENDER_SAT_FETCH_TICKS;
 fprintf(farEvents,"%u,%u,%u,%u,%u,%" PRIu64 ",%x,%u,%u,%u,%u,%u,%u,%u,%u,%" PRIu64 ",%" PRIu64 "\n",farBatchSample++,renderPath,frameNumber,phase,status,stamp(),sourcePC(),p->epoch,farWord(FAR_O(KEY)),farWord(FAR_O(REQUESTED)),farWord(FAR_O(COMPLETED)),farWord(FAR_O(PHASE)),p->done,p->words,p->controls,p->lastRetire,deadline);farDumpCache(farBatchStates);
}
static unsigned farPhase(unsigned encoded){
 unsigned phase=encoded&255,status=encoded>>8;if(phase!=8&&phase!=9&&phase!=10&&phase!=11)return 0;
 struct FarObserver*p=farPaths+renderPath;BackgroundObserverPath*b=backgroundPaths+renderPath;
 if(phase==11){
  if(status||p->initialized||p->open||b->prepare.open||b->publish.open||!farCodePC(sourcePC())||renderPaths[renderPath].ownershipArmed)farFail("invalid/repeated post-clear initialization marker");
#ifdef RENDER_FAR_INIT_BEGIN
  if(sourcePC()<RENDER_FAR_INIT_BEGIN||sourcePC()>=RENDER_FAR_INIT_END)farFail("initialization marker outside exact native initializer");
#endif
  uint8_t expected[FAR_BYTES]={0};
  for(unsigned n=0;n<FAR_SLOTS;n++)farPutWord(expected+FAR_O(OWNER)+n*2,p->owner[n]);
  memset(expected+FAR_O(SLOTS),255,16);farPutWord(expected+FAR_O(KEY),FAR_NONE);
  farPutWord(expected+FAR_O(PHASE),1);farPutLong(expected+FAR_O(RESIDENCY_EPOCH),1);
  for(unsigned n=0;n<FAR_BYTES;n++)if(farByte(n)!=expected[n])farFail("post-clear initialization metadata differs");
  for(unsigned n=0;n<FAR_SLOTS*32;n++)if(renderVRAM[(farTile(n/32)*32+(n&31))^1]!=p->expected[n])farFail("initial physical pool differs from WBR1");
  for(unsigned n=0;n<4096;n++)if(renderVRAM[(0xe000+n)^1])farFail("post-clear initialization has nonempty physical far plane");
  for(unsigned n=0;n<2048;n++)if(rawWord((RENDER_BG_FAR_PREPARED_WORDS&65535)+n*2))farFail("post-clear initialization has nonempty prepared far plane");
  p->initialized=1;p->epoch=1;farEvent(phase,status);farOwnersCurrent();return 1;
 }

 if(phase==8){
  if(status||!b->prepare.open||b->publish.open||p->open||!backgroundCodePC(sourcePC()))farFail("unpaired deferred far preparation");
  if(rawWord(RENDER_BG_FAR_MISSING_REF&65535)>=FAR_ORACLE_REFERENCE_COUNT&&!farWord(FAR_O(MISSING))&&farWord(FAR_O(PHASE))==1)farFail("deferred far preparation has no pending resource work");
  b->prepareTicks+=stamp()-b->prepare.begin;backgroundEvent(phase,status,&b->prepare);b->prepare.open=0;farEvent(phase,status);return 1;
 }
 if(!farCodePC(sourcePC()))farFail("cache phase outside exact auxiliary code");
 if(phase==9){
  if(status||p->open||b->publish.open||renderPaths[renderPath].publish.open||!b->irqDepth||b->prepare.open)farFail("cache upload lacks exclusive completed-source-IRQ owner");
  if((rawLong(RENDER_BG_IRQ_SP&65535)&0xffffff)+60!=b->irqFrames[b->irqDepth-1])farFail("cache wrapper IRQ frame differs");
  farOwnersCurrent();
  if(farWord(FAR_O(PHASE))!=2||farWord(FAR_O(KEY))>=FAR_ORACLE_KEY_COUNT||!farWord(FAR_O(REQUESTED)))farFail("upload has no valid queued source key");
  if(renderRegs[15]!=2||*renderDMALength)farFail("cache upload begins with DMA/wrong increment");
  p->open=1;p->begin=stamp();p->steps=p->done=p->words=p->controls=0;p->lastRetire=0;p->firstControl=1;
  backgroundContext(&p->address,&p->code,&p->pending,&p->latch);p->increment=2;if(p->pending)farFail("cache begins with incomplete VDP command");
  for(unsigned n=0;n<FAR_BYTES;n++)p->cacheBefore[n]=farByte(n);
  memcpy(p->vramBefore,renderVRAM,65536);memcpy(p->cramBefore,renderCRAM,128);memcpy(p->vsramBefore,renderVSRAM,128);memcpy(p->regsBefore,renderRegs,32);
  unsigned key=farWord(FAR_O(KEY)),request=farWord(FAR_O(REQUESTED));
  for(unsigned n=0;n<16;n++)if(request&(1u<<n)){
   unsigned pattern=farOracleReferences[key*16+n],slot=farByte(FAR_O(SLOTS)+n);
   if(!pattern||farOracleFixed[pattern]!=FAR_NONE){if(slot!=255)farFail("fixed pattern has dynamic assignment");continue;}
   if(slot>=FAR_SLOTS)farFail("request slot outside declared pool");
   if(p->owner[slot]==pattern)continue;
   if(farUsed(FAR_O(DISPLAYED_USE),slot)||farUsed(FAR_O(PREPARED_USE),slot))farFail("victim is referenced by displayed/prepared words");
   unsigned repeat=0;for(unsigned i=0;i<p->steps;i++)if(p->slot[i]==slot){if(p->pattern[i]!=pattern)farFail("slot assigned conflicting canonical patterns");repeat=1;}
   if(!repeat){unsigned i=p->steps++;p->slot[i]=slot;p->pattern[i]=pattern;p->reference[i]=n;p->oldOwner[i]=p->owner[slot];}
  }
  farEvent(phase,status);
 }else{
  if(!p->open||status&128||status>16||status!=p->done||p->words!=16*p->done||p->controls!=2*p->done||p->pending)farFail("incomplete/failed upload batch");
  unsigned address,code,pending,latch;backgroundContext(&address,&code,&pending,&latch);
  if(pending||address!=p->address||code!=p->code||latch!=p->latch||renderRegs[15]!=2||*renderDMALength)farFail("cache command state differs from observed stream");
  if(memcmp(renderCRAM,p->cramBefore,128)||memcmp(renderVSRAM,p->vsramBefore,128)||memcmp(renderRegs,p->regsBefore,32))farFail("cache changed palette/scroll/registers");
  for(unsigned at=0;at<65536;at++)if(renderVRAM[at^1]!=p->vramBefore[at^1]){
   unsigned found=0;for(unsigned i=0;i<p->done;i++)if(at/32==farTile(p->slot[i]))found=1;
   if(!found)farFail("cache changed undeclared VRAM byte");
  }
  uint8_t expected[FAR_BYTES];memcpy(expected,p->cacheBefore,FAR_BYTES);
  unsigned request=willowMotionWord(expected+FAR_O(REQUESTED)),key=willowMotionWord(expected+FAR_O(KEY)),completed=willowMotionWord(expected+FAR_O(COMPLETED)),copies=0,cursor=willowMotionWord(expected+FAR_O(CURSOR));
  for(unsigned n=0;n<16;n++)if(request&(1u<<n)){
   unsigned slot=expected[FAR_O(SLOTS)+n],pattern=farOracleReferences[key*16+n];
   if(slot==255||willowMotionWord(expected+FAR_O(OWNER)+slot*2)==pattern){completed|=1u<<n;continue;}
   if(copies==p->done)break;
   if(slot!=p->slot[copies]||pattern!=p->pattern[copies])farFail("upload order differs from queued source references");
   farPutWord(expected+FAR_O(OWNER)+slot*2,pattern);completed|=1u<<n;cursor=n+1;copies++;
  }
  if(copies!=p->done)farFail("unexplained completed transfer");
  farPutWord(expected+FAR_O(COMPLETED),completed);farPutWord(expected+FAR_O(CURSOR),cursor);farPutWord(expected+FAR_O(PHASE),completed==request?1:2);
  farPutLong(expected+FAR_O(RESIDENCY_EPOCH),p->epoch);if(p->done)farPutLong(expected+FAR_O(PREPARED_RESIDENCY_EPOCH),0);
  for(unsigned n=0;n<FAR_BYTES;n++)if(farByte(n)!=expected[n])farFail("cache commit bytes differ from independent transfer result");
  uint64_t deadline=(uint64_t)renderPaths[renderPath].irqFrame*RENDER_FRAME_TICKS+RENDER_SAT_FETCH_TICKS;
  if(renderActive()&&(stamp()>deadline||p->lastRetire>deadline))p->deadlineFailures++;
  p->batches++;farEvent(phase,status);p->open=0;farOwnersCurrent();
 }
 return 1;
}
static unsigned farVDPBefore(unsigned at,unsigned width,unsigned value){
 struct FarObserver*p=farPaths+renderPath;
 if(!p->open||width!=2||sourcePC()<RENDER_BG_FAR_UPLOAD_BEGIN||sourcePC()>RENDER_BG_FAR_UPLOAD_END||backgroundPaths[renderPath].publish.open||renderPaths[renderPath].publish.open)farFail("unowned cache VDP store");
 if(p->done>=p->steps||farWord(FAR_O(PHASE))!=3)farFail("transfer has no copying plan");
 unsigned i=p->done,slot=p->slot[i],pattern=p->pattern[i];
 if(farWord(FAR_O(OWNER)+slot*2)!=FAR_NONE||farLong(FAR_O(RESIDENCY_EPOCH))!=p->epoch+1||farLong(FAR_O(PREPARED_RESIDENCY_EPOCH)))farFail("victim owner not invalidated before first data write");
 if((at&0x1c)==4){
  if(p->words%16)farFail("command splits a pattern");
  if(!p->pending){if((value&0xc000)!=0x4000)farFail("cache register/read command");p->address=p->latch|(value&0x3fff);p->code=(p->code&0x3c)|((value>>14)&3);p->pending=1;}
  else{if(value&~3u)farFail("cache DMA/unsupported command");p->latch=(value&3)<<14;p->address=p->latch|(p->address&0x3fff);p->code=(p->code&3)|((value>>2)&0x3c);p->pending=0;p->firstControl=0;if(p->code!=1||p->address!=farTile(slot)*32)farFail("cache command has wrong planned destination");}
 }else if((at&0x1c)==0){
  unsigned word=p->words%16;
  if(p->pending||p->firstControl||p->code!=1||p->address!=farTile(slot)*32+word*2||value!=willowMotionWord(farOraclePatterns+pattern*32+word*2))farFail("cache data differs from exact canonical pattern/order");
  p->lastAddress=p->address;p->address+=2;
 }else farFail("unsupported cache VDP port");
 return 1;
}
static void farVDPAfter(unsigned at,unsigned value){
 struct FarObserver*p=farPaths+renderPath;
 if((at&0x1c)==0){
  unsigned slot=p->slot[p->done],pattern=p->pattern[p->done],word=p->words%16;
  if((unsigned)(renderVRAM[p->lastAddress^1]<<8|renderVRAM[(p->lastAddress+1)^1])!=(value&65535))farFail("actual cache VRAM differs from bus word");
  p->expected[slot*32+word*2]=value>>8;p->expected[slot*32+word*2+1]=value;p->words++;
  if(!(p->words%16)){
   for(unsigned n=0;n<32;n++)if(fputc(renderVRAM[(farTile(slot)*32+n)^1],farTransferData)==EOF)abort();
   p->owner[slot]=pattern;p->epoch++;p->done++;p->transfers++;farTransferSample++;
  }
 }else{if(renderRegs[15]!=2)farFail("cache changed increment");p->controls++;}
 uint32_t tail=(uint32_t)cpu->cycles;for(unsigned n=0;n<4;n++)if(renderFIFO[n]>tail)tail=renderFIFO[n];
 if(*renderDMALength||(*renderStatus&2))farFail("cache unexpectedly started DMA");
 uint64_t retire=(uint64_t)frameNumber*RENDER_FRAME_TICKS+tail;if(retire>p->lastRetire)p->lastRetire=retire;
}
static void farObserveFrame(unsigned path){
 if(!renderPaths[path].ownershipArmed)return;
 if(farPaths[path].open)farFail("upload crossed a physical frame boundary");
 farOwnersCurrent();
}
static unsigned farInit(void){
 if(RENDER_BG_FAR_CACHE_BYTES!=FAR_BYTES)farFail("unreviewed cache ABI size");
 for(unsigned path=0;path<2;path++){
  struct FarObserver*p=farPaths+path;p->epoch=0;
  for(unsigned n=0;n<FAR_SLOTS;n++){
   if(farOraclePool[n*2]!=farTile(n))farFail("resource pool geometry differs");
   unsigned pattern=farOraclePool[n*2+1];p->owner[n]=pattern;memcpy(p->expected+n*32,farOraclePatterns+pattern*32,32);
  }
 }
 farLatchBoundaries=outputFile("latch-boundaries.csv","w");if(!farLatchBoundaries)return 0;fputs("frame,committed,captured,pc,copied_words,expected_words\n",farLatchBoundaries);
 farEvents=outputFile("far-batches.csv","w");farTransferData=outputFile("far-transfers.bin","wb");farBatchStates=outputFile("far-batches.bin","wb");
 farPublications=outputFile("far-publications.csv","w");farStates=outputFile("far-publication-states.bin","wb");farOwners=outputFile("far-publication-owners.bin","wb");
 if(!farEvents||!farTransferData||!farBatchStates||!farPublications||!farStates||!farOwners)return 0;
 fputs("sample,path,frame,phase,status,master_cycles,pc,residency_epoch,key,requested,completed,cache_phase,patterns,words,controls,retire_upper_master,deadline_master\n",farEvents);
 fputs("sample,frame,residency_epoch,source_epoch,palette_epoch,generation,s2x,s2y,s3x,s3y,total_patterns,total_batches\n",farPublications);return 1;
}
static void farVideo(const void*d,unsigned w,unsigned h,size_t pitch){
 if(renderPath||!farCapturePending)return;
 if(frameNumber!=farCaptureFrame||!d||w!=320||h!=224||farPixelFormat>2)farFail("actual frame does not match selected streamed publication");
 FILE*f=outputFile("far-first-streamed-frame.ppm","wb");if(!f)abort();fprintf(f,"P6\n%u %u\n255\n",w,h);
 for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++){
  const uint8_t*p=(const uint8_t*)d+y*pitch+x*(farPixelFormat==1?4:2);uint8_t rgb[3];
  if(farPixelFormat==1){uint32_t v;memcpy(&v,p,4);rgb[0]=v>>16;rgb[1]=v>>8;rgb[2]=v;}
  else{uint16_t v;memcpy(&v,p,2);rgb[0]=((v>>(farPixelFormat==2?11:10))&31)*255/31;rgb[1]=((v>>5)&(farPixelFormat==2?63:31))*255/(farPixelFormat==2?63:31);rgb[2]=(v&31)*255/31;}
  if(fwrite(rgb,1,3,f)!=3)abort();
 }
 fclose(f);f=outputFile("far-first-streamed-frame.json","w");if(!f)abort();
 fprintf(f,"{\"frame\":%u,\"background_sample\":%u,\"residency_epoch\":%u,\"pixel_format\":%u,\"width\":320,\"height\":224,\"actual_core_video_callback\":true}\n",frameNumber,farCaptureSample,farPaths[0].epoch,farPixelFormat);fclose(f);
 farCaptured++;farCapturePending=0;
}
#endif
