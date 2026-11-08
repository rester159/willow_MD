#!/usr/bin/env python3
"""Compile and sanitize the actual root visible-opacity C with independent rings.
The extraction retains function bodies byte-for-byte and redirects only ROM flags.
"""
from pathlib import Path
import argparse,hashlib,json,re,subprocess
ROOT=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def extract(s,name):
 m=re.search(r'^static[^\n]*\b'+name+r'\(',s,re.M);assert m,name
 begin=s.index('{',m.start());level=1;i=begin+1
 while level:
  if s[i]=='{':level+=1
  elif s[i]=='}':level-=1
  i+=1
 return s[m.start():i]
def main():
 p=argparse.ArgumentParser();p.add_argument('--source',type=Path,default=ROOT/'tools/native_render_background.inc');p.add_argument('--output',type=Path,default=ROOT/'build/native-visible-opacity/root-v1');a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True);source=a.source.resolve();s=source.read_text()
 recheck=re.search(r'uint16_t full=0,recheck=([^;]+);',s).group(1)
 assert 'recheck?bgFarBlank():bg.farBlank' in s
 names=['bgRef','bgFirstX','bgFirstY','bgRows','bgNearVisibleRowFull','bgFarBlank'];funcs='\n'.join(extract(s,n)for n in names)
 funcs+='\nstatic uint16_t extractedRecheck(const uint16_t*old,const uint16_t*next){return '+recheck+';}'
 prefix=r'''
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "willow_native_background_key_cache.h"
#define BG_INVALID 0xffffu
#define WILLOW_BG_CATALOG_LAYER2_PAIR_COUNT 554
#define WILLOW_BG_CATALOG_LAYER3_PAIR_COUNT 248
#define WILLOW_BG_CATALOG_FULL_OPAQUE 1
#define WILLOW_BG_CATALOG_EMPTY 2
static uint8_t nearFlags[554*4],farFlags[248*16];
#define WILLOW_BG_CATALOG_LAYER2_FLAGS_ADDRESS ((uintptr_t)nearFlags)
#define WILLOW_BG_CATALOG_LAYER3_FLAGS_ADDRESS ((uintptr_t)farFlags)
typedef struct{uint8_t x,y,hx,hy,w,h,scale,valid;}BackgroundRing;
static struct{uint16_t view[4];BackgroundRing rings[2];WillowNativeBackgroundKeyCache keys;uint8_t rowCounts[2][32];}bg;
static unsigned long checks,cases;
#define CHECK(c) do{checks++;if(!(c)){fprintf(stderr,"line%d:%s\n",__LINE__,#c);exit(1);}}while(0)
'''
 suffix=r'''
static void setup(unsigned x,unsigned y,unsigned fine,unsigned phase,unsigned hx,unsigned hy){
 memset(&bg,0,sizeof bg);memset(nearFlags,1,sizeof nearFlags);memset(farFlags,2,sizeof farFlags);
 bg.view[0]=(uint16_t)((x+1)*8-96+fine);bg.view[1]=(uint16_t)(y*8-16+phase);
 bg.rings[0]=(BackgroundRing){(uint8_t)(x>>1),(uint8_t)(y>>1),(uint8_t)hx,(uint8_t)hy,22,15,1,1};
 for(unsigned n=0;n<330;n++)CHECK(willowNativeBackgroundNearKeySet(&bg.keys,n,n));
 CHECK(bgFirstX(0)==x);CHECK(bgFirstY(0)==y);
}
static void oneRow(unsigned y,unsigned fine){
 unsigned start=bgFirstX(0);bg.rowCounts[0][y&31]=0;CHECK(bgNearVisibleRowFull(y));
 for(unsigned hole=0;hole<43;hole++){
  unsigned ref=bgRef(0,(start+hole)&127,y);CHECK(ref!=BG_INVALID);nearFlags[ref]=2;bg.rowCounts[0][y&31]=1;
  unsigned expected=hole==0||hole==42||(!fine&&hole==41);
  CHECK(bgNearVisibleRowFull(y)==expected);nearFlags[ref]=1;cases++;
 }
 bg.rowCounts[0][y&31]=0;
}
static void sourceFixture(const char*path,unsigned expected){
 FILE*f=fopen(path,"rb");CHECK(f);uint8_t raw[2864];CHECK(fread(raw,1,sizeof raw,f)==sizeof raw);fclose(f);
 unsigned v[4];for(unsigned n=0;n<4;n++)v[n]=((unsigned)raw[n*2]<<8)|raw[n*2+1];
 unsigned x=(((uint16_t)(96+v[0])>>3)-1)&127,y=((uint16_t)(16+v[1])>>3)&127;
 setup(x,y,v[0]&7,v[1]&7,0,0);for(unsigned n=0;n<4;n++)bg.view[n]=v[n];
 bg.rings[1]=(BackgroundRing){bgFirstX(1)>>2,bgFirstY(1)>>2,0,0,12,8,2,1};
 for(unsigned n=0;n<96;n++)bg.keys.far[n]=n;
 memcpy(nearFlags,raw+8,1320);memcpy(farFlags,raw+1328,1536);
 for(unsigned l=0;l<2;l++)for(unsigned r=0;r<bgRows(l);r++){
  unsigned y=(bgFirstY(l)+r)&(l?255:127),count=0;
  for(unsigned c=0;c<43;c++){unsigned ref=bgRef(l,(bgFirstX(l)+c)&(l?255:127),y);CHECK(ref!=BG_INVALID);count+=(l?farFlags[ref]!=2:nearFlags[ref]!=1);}
  bg.rowCounts[l][y&31]=count;
 }
 CHECK(bgFarBlank()==expected);cases++;
}
int main(int argc,char**argv){
 CHECK(argc==3);sourceFixture(argv[1],1);sourceFixture(argv[2],0);
 /* Every worldX/worldY and fineX, with both native row wraps. Physical heads
  * vary independently in the second loop; raw source map is not assumed. */
 for(unsigned x=0;x<128;x++)for(unsigned y=0;y<128;y++)for(unsigned fine=0;fine<8;fine++){
  setup(x,y,fine,(x+y)&7,(x+fine)%22,(y+fine)%15);oneRow(y,fine);
  unsigned last=(y+27+(((x+y)&7)!=0))&127;oneRow(last,fine);
 }
 for(unsigned hx=0;hx<22;hx++)for(unsigned hy=0;hy<15;hy++)for(unsigned fine=0;fine<8;fine++){
  setup(127,127,fine,7,hx,hy);for(unsigned dy=0;dy<29;dy++)oneRow((127+dy)&127,fine);
 }
 /* Dense/edge mixtures make the subtractive count exercise multiple guards,
  * and keep an in-view hole from vanishing due to double subtraction. */
 for(unsigned fine=0;fine<8;fine++)for(unsigned bits=0;bits<32;bits++){
  setup(127,127,fine,7,21,14);unsigned cols[5]={0,1,40,41,42},count=0,expected=1;
  for(unsigned n=0;n<5;n++)if(bits&(1u<<n)){
   nearFlags[bgRef(0,(127+cols[n])&127,127)]=0;count++;
   if(cols[n]!=0&&cols[n]!=42&&(fine||cols[n]!=41))expected=0;
  }
  bg.rowCounts[0][31]=count;CHECK(bgNearVisibleRowFull(127)==expected);cases++;
 }
 /* Reconstructing world row with nativeY alone would read a different
  * metatile. The actual root helper receives the fullworldY here. */
 setup(26,111,0,0,3,4);nearFlags[bgRef(0,68,111)]=0;bg.rowCounts[0][15]=1;CHECK(bgNearVisibleRowFull(111));
 /* Public proof checks both first and last near rows touched by each far
  * row, including opposite fineY phases and the native31->0 wrap. */
 for(unsigned y=0;y<128;y++)for(unsigned np=0;np<8;np++)for(unsigned fp=0;fp<8;fp++){
  setup(126,y,0,np,0,0);bg.view[2]=17;bg.view[3]=(uint16_t)(252*8-16+fp);
  for(unsigned row=0;row<bgRows(1);row++){
   memset(bg.rowCounts,0,sizeof bg.rowCounts);unsigned far=(bgFirstY(1)+row)&255;bg.rowCounts[1][far&31]=1;
   CHECK(bgFarBlank());int lo=(int)row*8-(int)fp,hi=lo+7;if(lo<0)lo=0;if(hi>223)hi=223;
   unsigned ys[2]={((16+bg.view[1]+lo)>>3)&127,((16+bg.view[1]+hi)>>3)&127};
   for(unsigned n=0;n<2;n++){
    unsigned near=ys[n],ref=bgRef(0,(126+1)&127,near);CHECK(ref!=BG_INVALID);nearFlags[ref]=0;bg.rowCounts[0][near&31]=1;
    CHECK(!bgFarBlank());nearFlags[ref]=1;bg.rowCounts[0][near&31]=0;cases++;
   }
  }
 }
 /* Actual prepare recheck expression, including fine-only transition with
  * unchanged coarse source ring coordinates. */
 for(unsigned oldFine=0;oldFine<8;oldFine++)for(unsigned newFine=0;newFine<8;newFine++){
  uint16_t old[4]={oldFine,0,0,0},next[4]={newFine,0,0,0};CHECK(extractedRecheck(old,next)==(oldFine!=newFine));
 }
 setup(126,127,0,0,0,0);nearFlags[bgRef(0,(126+41)&127,127)]=0;bg.rowCounts[0][31]=1;
 CHECK(bgNearVisibleRowFull(127));uint16_t old[4];memcpy(old,bg.view,sizeof old);bg.view[0]++;
 CHECK(bgFirstX(0)==126);CHECK(extractedRecheck(old,bg.view));CHECK(!bgNearVisibleRowFull(127));
 /* Corrupt subtraction must not turn two bad guards into zero; any count
  * outside0..43 is refused even if guard values could subtract it down. */
 setup(126,127,0,0,0,0);nearFlags[bgRef(0,126,127)]=0;nearFlags[bgRef(0,(126+42)&127,127)]=0;
 bg.rowCounts[0][31]=1;CHECK(!bgNearVisibleRowFull(127));
 for(unsigned n=44;n<256;n++){bg.rowCounts[0][31]=n;CHECK(!bgNearVisibleRowFull(127));}
 /* Cold/unknown descriptor refusal and retained count corruption must not
  * invent coverage; a zero summary remains the existing admitted shortcut. */
 setup(126,127,0,0,0,0);bg.rowCounts[0][31]=1;unsigned slot=0;
 CHECK(willowNativeBackgroundNearKeySet(&bg.keys,slot,1023));CHECK(!bgNearVisibleRowFull(127));
 setup(126,127,0,0,0,0);bg.rowCounts[0][31]=44;CHECK(!bgNearVisibleRowFull(127));
 printf("{\"cases\":%lu,\"checks\":%lu,\"source_body_extraction\":true,\"sanitizers\":true}\n",cases,checks);return 0;
}
'''
 c=out/'actual-root-test.c';c.write_text(prefix+funcs+suffix);exe=out/'actual-root-test';cmd=['clang','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(ROOT/'src'),str(c),'-o',str(exe)];subprocess.run(cmd,check=True);fixtures=[ROOT/'build/native-visible-opacity/v1/source-flags-277.bin',ROOT/'build/native-visible-opacity/v1/source-flags-753.bin'];r=subprocess.run([str(exe),*[str(f)for f in fixtures]],check=True,capture_output=True,text=True);result=json.loads(r.stdout)
 evidence={'scope':__doc__,'source_sha256':sha(source),'extracted_functions':names,'function_bodies_sha256':hashlib.sha256(funcs.encode()).hexdigest(),'compile':cmd,'result':result,'input_hashes':{str(p):sha(p)for p in[Path(__file__),source,ROOT/'src/willow_native_background_key_cache.h',*fixtures]}}
 (out/'evidence.json').write_text(json.dumps(evidence,indent=2)+'\n');print(r.stdout)
if __name__=='__main__':main()
