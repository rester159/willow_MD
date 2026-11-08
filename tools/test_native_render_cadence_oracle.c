/* Negative tests of host retained-SAT proof; no target execution claim. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define RENDER_RETAINED_SAT 1
#define RENDER_CONTENT_STREAM 1
#define RENDER_CONTENT_ID_ADDRESS 0xff4800u
#define RENDER_CONTENT_PALETTE_ADDRESS 0xff4804u
#define RENDER_CONTENT_COUNT_ADDRESS 0xff4808u
#define RENDER_BSS_BEGIN 0xff4000u
#define RENDER_BSS_END 0xff5000u
#define RENDER_RETAINED_BASE 0xff4000u
#define RENDER_RETAINED_BYTES 662u
#define RENDER_RETAINED_PLAN 0x103764u
#define RENDER_RETAINED_WORDS_OFFSET 0u
#define RENDER_RETAINED_GENERATION_OFFSET 640u
#define RENDER_RETAINED_COMMAND_OFFSET 644u
#define RENDER_RETAINED_PLAN_OFFSET 648u
#define RENDER_RETAINED_EPOCH_OFFSET 652u
#define RENDER_RETAINED_COUNT_OFFSET 656u
#define RENDER_RETAINED_CAPACITY_OFFSET 658u
#define RENDER_RETAINED_READY_OFFSET 660u
#define RENDER_ORACLE_FIRST_BYTE 0x8000u
typedef struct {unsigned bank,records,epoch,palette,view;} RenderSpan;
typedef struct {unsigned code,group,status,attribute;} RenderOracleEntry;
static const RenderOracleEntry renderOracleEntries[]={{1,0,0,0x123},{2,0,0,0x456}};
static const uint8_t renderOraclePatterns[32]={0},renderOraclePalettes[6144]={0},renderOracleColors[128]={0};
static unsigned renderPath,frameNumber,latchCaptures=1;
static uint8_t ram[65536],sram[65536],vram[65536],cram[128],ports[512],cps[0x22800],independentLatch[2048];
static uint8_t*metadataSRAM=sram,*renderVRAM=vram,*renderCRAM=cram;
static uint16_t satbase=0xf400,*renderSatBase=&satbase;
static unsigned willowMotionWord(const uint8_t*p){return (unsigned)p[0]<<8|p[1];}
static unsigned rawByte(unsigned at){return ram[(at&65535)^1];}
static unsigned rawWord(unsigned at){return rawByte(at)<<8|rawByte(at+1);}
static unsigned rawLong(unsigned at){return rawWord(at)<<16|rawWord(at+2);}
static unsigned renderWord(unsigned at){return willowMotionWord(sram+at);}
static unsigned renderLong(unsigned at){return renderWord(at)<<16|renderWord(at+2);}
#include "test_native_render_cadence_oracle.h"
static void require(unsigned yes,const char*why){if(!yes){fprintf(stderr,"Oracle test failed: %s\n",why);exit(1);}}
static void be(uint8_t*p,unsigned word){p[0]=word>>8;p[1]=word;}
static void nativeWord(unsigned at,unsigned word){ram[(at&65535)^1]=word>>8;ram[((at+1)&65535)^1]=word;}
static void nativeLong(unsigned at,unsigned value){nativeWord(at,value>>16);nativeWord(at+2,value);}
static void satWord(unsigned n,unsigned word){vram[(0xf400+n*2)^1]=word>>8;vram[(0xf401+n*2)^1]=word;nativeWord(0x4000+n*2,word);}
int main(void){
 RenderSpan span={1,1,42,77,1};unsigned pieces=0;
 nativeLong(0x4000+640,77);nativeLong(0x4000+644,0x74000003);nativeLong(0x4000+648,RENDER_RETAINED_PLAN);nativeLong(0x4000+652,0x20f568);
 nativeWord(0x4000+656,1);nativeWord(0x4000+658,80);ram[(0x4000+660)^1]=1;
 be(sram+0xf568+2,77);be(sram+0xf56c,1);be(sram+0xf51e,1);be(sram+0xf530,1);be(sram+0xf534+2,42);be(sram+0xf532,1);
 be(ports+0x10a,0x91c0);be(ports+0x122,0x003e);
 be(independentLatch,96);be(independentLatch+2,16);be(independentLatch+4,1);
 satWord(0,128);satWord(1,0x500);satWord(2,0x123);satWord(3,128);
 require(renderRetainedProof(&span,&pieces)&&pieces==1&&renderCurrentSourceOracle(&span,pieces),"correct current source, shadow, lease, atlas and hardware");
 nativeLong(0x4800,3);nativeLong(0x4804,77);nativeWord(0x4808,1);be(sram+0xf58e,3);
 require(renderContentProof(&span,&pieces)&&renderCurrentSourceOracle(&span,pieces),"exact physical content certificate and current original geometry");
 nativeLong(0x4800,2);require(!renderContentProof(&span,&pieces),"old physical content ID");nativeLong(0x4800,3);
 nativeLong(0x4804,76);require(!renderContentProof(&span,&pieces),"content certificate palette stale");nativeLong(0x4804,77);
 nativeWord(0x4808,81);require(!renderContentProof(&span,&pieces),"content certificate capacity invalid");nativeWord(0x4808,1);
 nativeLong(0x4000+640,76);require(!renderRetainedProof(&span,&pieces),"stale generation");nativeLong(0x4000+640,77);
 be(sram+0xf56e,1);require(!renderRetainedProof(&span,&pieces),"in-progress source palette");be(sram+0xf56e,0);
 be(ports+0x122,0x003f);require(!renderRetainedProof(&span,&pieces),"changed view");be(ports+0x122,0x003e);
 vram[0x8000]=1;require(!renderRetainedProof(&span,&pieces),"changed resident pattern");vram[0x8000]=0;
 cram[0]=1;require(!renderRetainedProof(&span,&pieces),"changed physical CRAM");cram[0]=0;
 vram[0xf600]=1;require(!renderRetainedProof(&span,&pieces),"unreachable physical tail diverged from shadow");vram[0xf600]=0;
 cps[0x1c000]=1;require(!renderRetainedProof(&span,&pieces),"numeric source palette changed");cps[0x1c000]=0;
 be(independentLatch,97);require(renderRetainedProof(&span,&pieces)&&!renderCurrentSourceOracle(&span,pieces),"internally consistent stale retained geometry fails source oracle");be(independentLatch,96);
 be(independentLatch+8,1);require(!renderCurrentSourceOracle(&span,pieces),"omitted original tail became nonempty");be(independentLatch+8,0);
 be(independentLatch+4,3);require(!renderCurrentSourceOracle(&span,pieces),"unprepared child fails");be(independentLatch+4,1);
 /* A two-cell local horizontal flip reverses child codes while retaining
  * source display order; the oracle must compare exact native flip words. */
 be(independentLatch+6,0x120);nativeWord(0x4000+656,2);
 satWord(1,0x501);satWord(2,0x456|0x800);satWord(4,128);satWord(5,0x500);satWord(6,0x123|0x800);satWord(7,144);
 require(renderRetainedProof(&span,&pieces)&&pieces==2&&renderCurrentSourceOracle(&span,pieces),"source block/local flip and child order");
 puts("Retained live oracle: exact geometry, full shadow, lease, view, source palette, CRAM, atlas, stale geometry, omitted tail and local flip tests passed.");
 return 0;
}
