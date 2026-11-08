#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "willow_native_background_key_cache.h"
#include "willow_native_plane_surface.h"
#define BG_INVALID 0xffffu
#define WILLOW_BG_CATALOG_LAYER2_PAIR_COUNT 554
#define WILLOW_BG_CATALOG_LAYER3_PAIR_COUNT 248
#define WILLOW_BG_CATALOG_FULL_OPAQUE 1
#define WILLOW_BG_CATALOG_EMPTY 2
static uint8_t nearFlags[554*4],farFlags[248*16],nearMasks[554*4*8],farMasks[248*16*8];
static uint16_t nearRefs[554*8],farRefs[248*32];
#define WILLOW_BG_CATALOG_LAYER2_FLAGS_ADDRESS ((uintptr_t)nearFlags)
#define WILLOW_BG_CATALOG_LAYER3_FLAGS_ADDRESS ((uintptr_t)farFlags)
#define WILLOW_BG_CATALOG_LAYER2_OPACITY_ADDRESS ((uintptr_t)nearMasks)
#define WILLOW_BG_CATALOG_LAYER3_OPACITY_ADDRESS ((uintptr_t)farMasks)
#define WILLOW_BG_CATALOG_LAYER2_REFS_ADDRESS ((uintptr_t)nearRefs)
#define WILLOW_BG_CATALOG_LAYER3_REFS_ADDRESS ((uintptr_t)farRefs)
typedef struct{uint8_t x,y,hx,hy,w,h,scale,valid;}BackgroundRing;
static struct{uint16_t view[4];BackgroundRing rings[2];WillowNativeBackgroundKeyCache keys;uint8_t rowCounts[2][32];WillowNativePlaneSurface planes[2];uint32_t columns[2][2];}bg;
static unsigned long checks,cases,overlapCases,rowClearCases;
#define CHECK(c) do{checks++;if(!(c)){fprintf(stderr,"line%d:%s\n",__LINE__,#c);exit(1);}}while(0)
#include "actual-functions.h"
static unsigned independentRef(unsigned layer,unsigned x,unsigned y){
 unsigned scale=layer?4:2;BackgroundRing*r=bg.rings+layer;
 unsigned dx=((x/scale+64-r->x)%64),dy=((y/scale+64-r->y)%64);
 CHECK(dx<r->w&&dy<r->h);unsigned slot=((dy+r->hy)%r->h)*r->w+(dx+r->hx)%r->w;
 /* Fixtures assign each physical ring slot its own immutable descriptorID. */
 return slot*scale*scale+(y%scale)*scale+x%scale;
}
static unsigned flag(const uint8_t*m){unsigned any=0,full=1;for(unsigned n=0;n<8;n++){any|=m[n];full&=m[n]==255;}return full?1:any?0:2;}
static void recount(void){
 for(unsigned l=0;l<2;l++)for(unsigned row=0;row<bgRows(l);row++){
  unsigned y=(bgFirstY(l)+row)&(l?255:127),count=0;
  for(unsigned col=0;col<43;col++){unsigned ref=independentRef(l,(bgFirstX(l)+col)&(l?255:127),y);count+=(l?farFlags[ref]!=2:nearFlags[ref]!=1);}
  bg.rowCounts[l][y&31]=count;
 }
}
static void setup(unsigned x,unsigned y,unsigned nf,unsigned nyf,unsigned ff,unsigned fyf,unsigned hx,unsigned hy){
 memset(&bg,0,sizeof bg);memset(nearMasks,255,sizeof nearMasks);memset(farMasks,255,sizeof farMasks);memset(nearFlags,1,sizeof nearFlags);memset(farFlags,1,sizeof farFlags);
 bg.view[0]=(uint16_t)((x+1)*8-96+nf);bg.view[1]=(uint16_t)(y*8-16+nyf);
 bg.view[2]=(uint16_t)(253*8-96+ff);bg.view[3]=(uint16_t)(252*8-16+fyf);
 bg.rings[0]=(BackgroundRing){x>>1,y>>1,hx,hy,22,15,1,1};bg.rings[1]=(BackgroundRing){bgFirstX(1)>>2,bgFirstY(1)>>2,(hx%12),(hy%8),12,8,2,1};
 for(unsigned n=0;n<330;n++)CHECK(willowNativeBackgroundNearKeySet(&bg.keys,n,n));for(unsigned n=0;n<96;n++)bg.keys.far[n]=n;
 for(unsigned n=0;n<554*4;n++)nearRefs[n*2+1]=(n%2047)+1;
 for(unsigned n=0;n<248*16;n++)farRefs[n*2+1]=(n%2047)+1;
 recount();
}
static unsigned nearPixel(int sx,int sy){
 unsigned x=(96+bg.view[0]+sx)&1023,y=(16+bg.view[1]+sy)&1023;
 return (nearMasks[independentRef(0,x/8,y/8)*8+y%8]>>(7-x%8))&1;
}
static unsigned oracle(unsigned ref,unsigned x,unsigned y){
 int sx=(int)(((x-bgFirstX(1))&255)*8)-8-(bg.view[2]&7),sy=(int)(((y-bgFirstY(1))&255)*8)-(bg.view[3]&7);
 for(int yy=0;yy<8;yy++)for(int xx=0;xx<8;xx++)if(sx+xx>=0&&sx+xx<320&&sy+yy>=0&&sy+yy<224&&(farMasks[ref*8+yy]&(128>>xx))&&!nearPixel(sx+xx,sy+yy))return 1;
 return 0;
}
static void verifyCells(void){
 for(unsigned row=0;row<bgRows(1);row++)for(unsigned col=0;col<43;col++){
  unsigned x=(bgFirstX(1)+col)&255,y=(bgFirstY(1)+row)&255,ref=independentRef(1,x,y);CHECK(bgFarNeeded(ref,x,y)==oracle(ref,x,y));overlapCases++;
 }
}
static void sourceFixture(const char*path,unsigned expected){
 FILE*f=fopen(path,"rb");CHECK(f);uint8_t raw[25712];CHECK(fread(raw,1,sizeof raw,f)==sizeof raw);fclose(f);unsigned v[4];for(unsigned n=0;n<4;n++)v[n]=((unsigned)raw[n*2]<<8)|raw[n*2+1];
 unsigned x=(((uint16_t)(96+v[0])>>3)-1)&127,y=((uint16_t)(16+v[1])>>3)&127;setup(x,y,v[0]&7,v[1]&7,v[2]&7,v[3]&7,0,0);
 for(unsigned n=0;n<4;n++)bg.view[n]=v[n];bg.rings[1]=(BackgroundRing){bgFirstX(1)>>2,bgFirstY(1)>>2,0,0,12,8,2,1};
 memcpy(nearFlags,raw+8,1320);memcpy(farFlags,raw+1328,1536);memcpy(nearMasks,raw+2864,10560);memcpy(farMasks,raw+13424,12288);recount();CHECK(bgFarBlank()==expected);verifyCells();
 /* Row fastpath must write exactly zero for hidden far cells, including stale
  * resident words, and keep all other rows untouched. No physical VDP here. */
 for(unsigned row=0;row<bgRows(1);row++){
  memset(bg.planes,0,sizeof bg.planes);memset(bg.columns,0,sizeof bg.columns);for(unsigned n=0;n<2048;n++)bg.planes[1].words[n]=0x1234;
  unsigned y=(bgFirstY(1)+row)&255,first=bgFirstX(1);CHECK(bgResolveRow(1,row));
  uint64_t changed=0;
  for(unsigned col=0;col<43;col++){
   unsigned x=(first+col)&255,ref=independentRef(1,x,y),expectedWord=oracle(ref,x,y)?farRefs[ref*2+1]:0;CHECK(bg.planes[1].words[(y&31)*64+(x&63)]==expectedWord);changed|=1ULL<<(x&63);
  }
  for(unsigned iy=0;iy<32;iy++)for(unsigned ix=0;ix<64;ix++)if(iy!=(y&31)||!(changed&(1ULL<<ix)))CHECK(bg.planes[1].words[iy*64+ix]==0x1234);
  unsigned low=0,high=63;while(!(changed&(1ULL<<low)))low++;while(!(changed&(1ULL<<high)))high--;CHECK(bg.planes[1].first[y&31]==low);CHECK(bg.planes[1].last[y&31]==high);CHECK(bg.planes[1].dirty==(1UL<<(y&31)));CHECK(bg.columns[1][0]==(uint32_t)changed);CHECK(bg.columns[1][1]==(uint32_t)(changed>>32));rowClearCases++;
 }
 /* Native residency is consulted only if original far pixels are demanded. */
 for(unsigned n=0;n<248*16;n++)farRefs[n*2+1]=0xffff;
 unsigned failed=0;for(unsigned row=0;row<bgRows(1);row++)failed+=!bgResolveRow(1,row);
 CHECK(failed==(expected?0:1));cases++;
}
int main(int argc,char**argv){
 CHECK(argc==4);sourceFixture(argv[1],1);sourceFixture(argv[2],1);sourceFixture(argv[3],0);
 /* Every transparent source pixel in every retained column, all fineX,
  * source wrap, native31->0, both metatile rows, and all visible mask bits. */
 for(unsigned fine=0;fine<8;fine++)for(unsigned col=0;col<43;col++)for(unsigned py=0;py<8;py++)for(unsigned px=0;px<8;px++){
  setup(127,127,fine,7,0,0,21,14);unsigned ref=independentRef(0,(127+col)&127,127);nearMasks[ref*8+py]^=128>>px;nearFlags[ref]=0;bg.rowCounts[0][31]=1;
  int sx=(int)col*8-8-(int)fine+(int)px;CHECK(bgNearVisibleRowFull(127)==(sx<0||sx>=320));cases++;
 }
 /* Fine horizontal/vertical alignments force0..7 byte join shifts, adjacent
  * source rows, clipping on both horizontal and vertical sides, and wraps. */
 for(unsigned nf=0;nf<8;nf++)for(unsigned ff=0;ff<8;ff++)for(unsigned nyf=0;nyf<8;nyf++){
  unsigned fyf=(nf+ff+nyf)&7;setup(127,127,nf,nyf,ff,fyf,(nf+nyf)%22,(ff+nyf)%15);
  for(unsigned n=0;n<1320;n++){for(unsigned y=0;y<8;y++)nearMasks[n*8+y]=(uint8_t)((n*31+y*17+nf*19)^0x9b);nearFlags[n]=flag(nearMasks+n*8);}
  for(unsigned n=0;n<1536;n++){for(unsigned y=0;y<8;y++)farMasks[n*8+y]=(uint8_t)((n*11+y*33+ff*23)^0x59);farFlags[n]=flag(farMasks+n*8);}
  recount();verifyCells();
 }
 /* Fully covered far cells must also traverse the byte fallback where a
  * different in-view cell makes the whole near row nonFULL. */
 for(unsigned nf=0;nf<8;nf++)for(unsigned ff=0;ff<8;ff++){
  setup(127,127,nf,3,ff,6,7,9);for(unsigned r=0;r<29;r++){unsigned y=(127+r)&127,ref=independentRef(0,(127+15)&127,y);nearMasks[ref*8+3]&=0xfe;nearFlags[ref]=0;}recount();verifyCells();
 }
 /* Count corruption never receives partial-edge subtraction permission. */
 setup(127,127,3,0,0,0,21,14);for(unsigned n=44;n<256;n++){bg.rowCounts[0][31]=n;CHECK(!bgNearVisibleRowFull(127));}
 unsigned guard0=independentRef(0,127,127),guard42=independentRef(0,(127+42)&127,127);nearFlags[guard0]=nearFlags[guard42]=2;memset(nearMasks+guard0*8,0,8);memset(nearMasks+guard42*8,0,8);bg.rowCounts[0][31]=1;CHECK(!bgNearVisibleRowFull(127));
 /* Invalid source descriptor must return conservative needed, not hidden. */
 setup(126,127,3,0,0,0,0,0);memset(bg.rowCounts[0],43,32);CHECK(willowNativeBackgroundNearKeySet(&bg.keys,1,1023));unsigned x=(bgFirstX(1)+2)&255,y=bgFirstY(1);CHECK(bgFarNeeded(independentRef(1,x,y),x,y));
 /* All-zero far byte/EMPTY must remain free of near dependency. */
 farFlags[independentRef(1,x,y)]=2;CHECK(!bgFarNeeded(independentRef(1,x,y),x,y));
 printf("{\"cases\":%lu,\"byte_overlap_cells\":%lu,\"source_fixture_row_clears\":%lu,\"assertions\":%lu,\"sanitizers\":true}\n",cases,overlapCases,rowClearCases,checks);return 0;
}
