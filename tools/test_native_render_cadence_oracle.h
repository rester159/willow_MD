/* Host-only independent current-source geometry and retained hardware oracle.
 * Prepared assets are numeric palette/tile bindings, never captured positions.
 * This code has no target bus writes, CPU stepping or cycle modification. */
#ifndef WILLOW_RENDER_CADENCE_ORACLE_H
#define WILLOW_RENDER_CADENCE_ORACLE_H
#if (defined(RENDER_RETAINED_SAT) && RENDER_RETAINED_SAT == 1) || (defined(RENDER_CONTENT_STREAM) && RENDER_CONTENT_STREAM == 1)
#define RENDER_ZERO_TRANSFER_PROOF 1
#if !defined(RENDER_ORACLE_FIRST_BYTE)
#error Zero-transfer publication requires host resource evidence
#endif
static unsigned renderOracleFailed(const char*reason){
 fprintf(stderr,"Live retained SAT proof failed: %s path%u frame%u capture%u\n",reason,renderPath,frameNumber,latchCaptures);
 return 0;
}
static unsigned renderOracleVRAMWord(unsigned at){return (unsigned)renderVRAM[at^1]<<8|renderVRAM[(at+1)^1];}
static unsigned renderResourceProof(const RenderSpan*end){
 if(!end->palette||end->palette!=renderLong(0xf568)||!renderWord(0xf56c)||renderWord(0xf56e)||end->view!=1||*renderSatBase!=0xf400)
  return renderOracleFailed("current palette/view/SAT lease differs");
 const uint8_t*livePorts=renderPath?metadataSRAM+0xf800:ports;
 if(willowMotionWord(livePorts+0x10a)!=0x91c0||willowMotionWord(livePorts+0x122)!=0x003e)
  return renderOracleFailed("source palette/view binding changed");
 const uint8_t*livePalette=renderPath?metadataSRAM+0xd800:cps+0x1c000;
 if(memcmp(livePalette,renderOraclePalettes,sizeof renderOraclePalettes))return renderOracleFailed("source numeric palette changed");
 for(unsigned n=0;n<sizeof renderOraclePatterns;n++)if(renderVRAM[(RENDER_ORACLE_FIRST_BYTE+n)^1]!=renderOraclePatterns[n])
  return renderOracleFailed("resident pattern changed");
 for(unsigned n=0;n<128;n+=2){uint16_t word;memcpy(&word,renderCRAM+n,2);
  unsigned bus=((word&0x1c0)<<3)|((word&0x38)<<2)|((word&7)<<1);
  if(bus!=willowMotionWord(renderOracleColors+n))return renderOracleFailed("physical CRAM changed");
 }
 return 1;
}
#if defined(RENDER_RETAINED_SAT) && RENDER_RETAINED_SAT == 1
static unsigned renderRetainedProof(const RenderSpan*end,unsigned*pieces){
 unsigned base=RENDER_RETAINED_BASE&65535;
 if(RENDER_RETAINED_BASE<RENDER_BSS_BEGIN||RENDER_RETAINED_BASE+RENDER_RETAINED_BYTES>RENDER_BSS_END)
  return renderOracleFailed("shadow lies outside owned BSS");
 unsigned count=rawWord(base+RENDER_RETAINED_COUNT_OFFSET);
 if(count>80||rawWord(base+RENDER_RETAINED_CAPACITY_OFFSET)!=80||rawByte(base+RENDER_RETAINED_READY_OFFSET)!=1||
    rawLong(base+RENDER_RETAINED_GENERATION_OFFSET)!=end->palette||
    rawLong(base+RENDER_RETAINED_COMMAND_OFFSET)!=0x74000003||
    rawLong(base+RENDER_RETAINED_PLAN_OFFSET)!=RENDER_RETAINED_PLAN||
    rawLong(base+RENDER_RETAINED_EPOCH_OFFSET)!=0x20f568)
  return renderOracleFailed("shadow identity/count differs");
 /* Check all 80 physical slots, including the deliberately unreachable tail. */
 for(unsigned n=0;n<320;n++)if(rawWord(base+RENDER_RETAINED_WORDS_OFFSET+n*2)!=renderOracleVRAMWord(0xf400+n*2))
  return renderOracleFailed("full retained shadow differs from physical SAT");
 *pieces=count;
 return renderResourceProof(end);
}
#endif
#if defined(RENDER_CONTENT_STREAM) && RENDER_CONTENT_STREAM == 1
static unsigned renderContentProof(const RenderSpan*end,unsigned*pieces){
 const unsigned addresses[]={RENDER_CONTENT_ID_ADDRESS,RENDER_CONTENT_PALETTE_ADDRESS,RENDER_CONTENT_COUNT_ADDRESS};
 for(unsigned n=0;n<3;n++)if(addresses[n]<RENDER_BSS_BEGIN||addresses[n]+(n==2?2:4)>RENDER_BSS_END)
  return renderOracleFailed("content certificate lies outside owned BSS");
 unsigned id=rawLong(RENDER_CONTENT_ID_ADDRESS),count=rawWord(RENDER_CONTENT_COUNT_ADDRESS);
 if(!id||id!=renderLong(0xf58c)||rawLong(RENDER_CONTENT_PALETTE_ADDRESS)!=end->palette||count>80)
  return renderOracleFailed("physical content certificate differs from requested content");
 *pieces=count;
 return renderResourceProof(end);
}
#endif
static unsigned renderCurrentSourceOracle(const RenderSpan*end,unsigned pieces){
 if(end->records>8||!latchCaptures||!renderWord(0xf532)||end->bank!=renderWord(0xf51e)||
    end->records!=renderWord(0xf530)||end->epoch!=renderLong(0xf534))
  return renderOracleFailed("uncertified current original latch");
 unsigned expected[320]={0},sprites[224]={0},fetch[224]={0},count=0;
 for(unsigned record=0;record<256;record++){
  const uint8_t*input=independentLatch+record*8;
  unsigned x=willowMotionWord(input),y=willowMotionWord(input+2),code=willowMotionWord(input+4),attribute=willowMotionWord(input+6);
  if(record>=end->records){if(x||attribute)return renderOracleFailed("nonempty omitted original tail");continue;}
  if(code>0x27ff||(attribute&0xff00)==0xff00)continue;
  unsigned columns=((attribute>>8)&15)+1,rows=(attribute>>12)+1;
  for(unsigned row=0;row<rows;row++)for(unsigned column=0;column<columns;column++){
   int sx=(int)((x+column*16)&511)-96,sy=(int)((y+row*16)&511)-16;
   if(sx<=-16||sx>=320||sy<=-16||sy>=224)continue;
   unsigned cx=attribute&32?columns-1-column:column,cy=attribute&64?rows-1-row:row;
   unsigned child=(code&0xfff0)+((code+cx)&15)+cy*16;
   const RenderOracleEntry*entry=NULL;
   for(unsigned n=0;n<sizeof renderOracleEntries/sizeof *renderOracleEntries;n++)
    if(renderOracleEntries[n].code==child&&renderOracleEntries[n].group==(attribute&31)){entry=renderOracleEntries+n;break;}
   if(!entry)return renderOracleFailed("unprepared visible child");
   if(entry->status==2)continue;
   if(count>=80)return renderOracleFailed("source exceeds SAT capacity");
   unsigned*item=expected+count++*4;
   item[0]=(unsigned)(sy+128);item[1]=0x500;item[2]=entry->attribute|((attribute<<6)&0x1800);item[3]=(unsigned)(sx+128);
   for(int line=sy<0?0:sy;line<224&&line<sy+16;line++)if(++sprites[line]>20||(fetch[line]+=16)>320)
    return renderOracleFailed("source exceeds native line budget");
  }
 }
 if(pieces!=count)return renderOracleFailed("retained piece count differs from source");
 for(unsigned n=0;n<count;n++)expected[n*4+1]|=n+1<count?n+1:0;
 for(unsigned n=0;n<(count?count*4:4);n++)if(renderOracleVRAMWord(0xf400+n*2)!=expected[n])
  return renderOracleFailed("physical SAT differs from current original geometry");
 return 1;
}
#endif
#endif
