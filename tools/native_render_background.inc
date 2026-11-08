/* Live, opt-in retained background experiment. The original maps and staged
 * IRQ camera words are authoritative. No trace coordinates enter this code.
 * Priority overlays/Scroll1 are separate work; this is plane publication only. */
#include "willow_native_background_plane_publish.h"
#include "willow_native_background_key_cache.h"
#ifdef RENDER_BACKGROUND_COLUMNS
#include "willow_native_background_columns.h"
typedef WillowNativeBackgroundColumns BackgroundSourceWriter;
#define backgroundSourceWriter willowNativeBackgroundColumnsState
#else
#include "willow_native_background_writer.h"
typedef WillowNativeBackgroundWriter BackgroundSourceWriter;
#define backgroundSourceWriter willowNativeBackgroundWriterState
#endif
#include "render_background_imports.h"
#ifdef RENDER_BACKGROUND_LOOKUP
#include "render_background_lookup_imports.h"
#endif
#ifdef RENDER_BACKGROUND_NEAR_STRIP
#include "willow_native_background_near_strip.h"
#include "render_background_near_columns_imports.h"
#endif
#ifdef RENDER_BACKGROUND_FAR_STREAM
#include "willow_native_far_cache.h"
#include "render_background_far_stream_imports.h"
#ifdef RENDER_BACKGROUND_SHARED_STREAM
#include "render_background_shared_stream_imports.h"
#define BG_STREAM_ATTRIBUTES WILLOW_BG_SHARED_ATTRIBUTES_ADDRESS
#define BG_STREAM_REFS WILLOW_BG_SHARED_REFS_ADDRESS
#define BG_STREAM_REF_COUNT WILLOW_BG_SHARED_REFERENCE_COUNT
#else
#define BG_STREAM_ATTRIBUTES WILLOW_BG_FAR_STREAM_ATTRIBUTES_ADDRESS
#define BG_STREAM_REFS WILLOW_BG_CATALOG_LAYER3_REFS_ADDRESS
#define BG_STREAM_REF_COUNT WILLOW_BG_FAR_STREAM_REFERENCE_COUNT
#endif
#define BG_FAR_CODE __attribute__((noinline,section(".willow_far_stream")))
WillowNativeFarCache backgroundFarCache;
uint16_t backgroundFarMissingRef;
static const WillowNativeFarResources backgroundFarResources={
 (const uint8_t*)WILLOW_BG_FAR_STREAM_PATTERNS_ADDRESS,
 (const uint8_t*)BG_STREAM_ATTRIBUTES,
 (const uint8_t*)WILLOW_BG_FAR_STREAM_POOL_ADDRESS,
 (const uint8_t*)WILLOW_BG_FAR_STREAM_FIXED_MAP_ADDRESS,
 (const uint8_t*)BG_STREAM_REFS,
 WILLOW_BG_FAR_STREAM_PATTERN_COUNT,BG_STREAM_REF_COUNT,
 WILLOW_BG_FAR_STREAM_POOL_COUNT};
#endif
#ifdef RENDER_ORIGINAL_BUS
#define BG_MAP2 0x918000u
#define BG_MAP3 0x914000u
#else
#define BG_MAP2 0x204000u
#define BG_MAP3 0x208000u
#endif
#define BG_INVALID 0xffffu
#define BG_PHASE(n,s) do{WORD(0x20f592)=(s);WORD(0x20f590)=(uint16_t)(((s)<<8)|(n));}while(0)
#ifdef RENDER_BACKGROUND_NEAR_STRIP
typedef WillowNativeBackgroundRing BackgroundRing;
#else
typedef struct {uint8_t x,y,hx,hy,w,h,scale,valid;} BackgroundRing;
#endif
BackgroundSourceWriter backgroundSourceWriter;
uint32_t renderBackgroundIRQSP;
static const WillowPresentationPlan backgroundPlan={renderColors,0,0,WILLOW_PRESENT_BG};
static const WillowNativePlaneTarget backgroundTargets[2]={{0x40000003UL,0xc000},{0x60000003UL,0xe000}};
static struct {
 WillowNativePlaneSurface planes[2];
 WillowNativeBackgroundKeyCache keys;
 WillowPresentationLease lease;
 BackgroundRing rings[2];
 uint32_t columns[2][2];
 uint32_t mapGeneration[2],preparedEpoch,generation;
 uint16_t view[4];
 uint16_t mode,cursor,ready,busy,fault,farBlank,scrollDirty;
 uint8_t rowCounts[2][32];
} bg;
#ifdef RENDER_BACKGROUND_FAR_STREAM
const uint32_t backgroundFarPreparedWordsAddress __attribute__((section(".willow_far_stream.abi")))=
 (uint32_t)&bg.planes[1].words[0];
#endif
#ifdef RENDER_BACKGROUND_NEAR_STRIP
typedef char BackgroundNearResourceShape[(WILLOW_BG_NEAR_COLUMNS_COUNT==WILLOW_BG_CATALOG_LAYER2_PAIR_COUNT&&
 WILLOW_BG_NEAR_COLUMNS_BYTES==WILLOW_BG_NEAR_COLUMNS_COUNT*16&&WILLOW_BG_NEAR_COLUMNS_STRIDE==16)?1:-1];
static const WillowNativeBackgroundNearStrip backgroundNearStrip={&bg.keys,bg.rings,
 (const uint16_t*)WILLOW_BG_CATALOG_LAYER2_REFS_ADDRESS,
 (const uint8_t*)WILLOW_BG_CATALOG_LAYER2_FLAGS_ADDRESS,WILLOW_BG_CATALOG_LAYER2_PAIR_COUNT,
 (const uint8_t*)WILLOW_BG_NEAR_COLUMNS_ADDRESS};
#endif
/* Mode0 invalid;1 resolving source pairs;2 resolving rows;3 draining cold
 * transfers;4 live. Interrupts can publish only immutable prepared state. */
static uint16_t bgCell(uint16_t layer,uint16_t x,uint16_t y)
{return layer?((y&7)|((x&63)<<3)|((y&56)<<6)):((y&15)|((x&63)<<4)|((y&48)<<6));}
static uint16_t bgLookup(uint16_t layer,uint32_t key)
{
#ifdef RENDER_BACKGROUND_LOOKUP
 return willowNativeBackgroundLookupFindAdmitted(willowBackgroundLookupIndexes+layer,key);
#else
 const uint8_t*p=(const uint8_t*)(layer?WILLOW_BG_CATALOG_LAYER3_PAIRS_ADDRESS:WILLOW_BG_CATALOG_LAYER2_PAIRS_ADDRESS);
 uint16_t low=0,high=layer?WILLOW_BG_CATALOG_LAYER3_PAIR_COUNT:WILLOW_BG_CATALOG_LAYER2_PAIR_COUNT;
 while(low<high){uint16_t mid=low+((high-low)>>1);uint32_t found=*(const uint32_t*)(p+mid*6);
  if(found<key)low=mid+1;else if(found>key)high=mid;else return mid;}
 return BG_INVALID;
#endif
}
static uint16_t bgSlot(const BackgroundRing*r,uint16_t x,uint16_t y)
{
 uint16_t dx=(x-r->x)&63,dy=(y-r->y)&63;
 if(dx>=r->w||dy>=r->h)return BG_INVALID;
 dx+=r->hx;if(dx>=r->w)dx-=r->w;
 dy+=r->hy;if(dy>=r->h)dy-=r->h;
 return dy*r->w+dx;
}
static uint16_t bgPair(uint16_t layer,uint16_t x,uint16_t y,uint32_t key)
{
 uint16_t slot=bgSlot(bg.rings+layer,x,y);if(slot==BG_INVALID)return 1;
 uint16_t id=bgLookup(layer,key);if(id==BG_INVALID)return 0;
 return layer?willowNativeBackgroundFarKeySet(&bg.keys,slot,id):willowNativeBackgroundNearKeySet(&bg.keys,slot,id);
}
static uint16_t bgReadPair(uint16_t layer,uint16_t x,uint16_t y)
{
 const volatile uint32_t*map=(const volatile uint32_t*)(layer?BG_MAP3:BG_MAP2);
 return bgPair(layer,x,y,map[bgCell(layer,x,y)]);
}
#ifdef RENDER_BACKGROUND_COLUMNS
/* Dirty source columns are conservative. Compare the complete resolved ID
 * before invalidating row summaries; unchanged writes do not force a rebuild.
 * Return0 on refusal,1 unchanged/outside,3 when the retained ID changed. */
static uint16_t bgRefreshPair(uint16_t layer,uint16_t x,uint16_t y)
{
 uint16_t slot=bgSlot(bg.rings+layer,x,y);if(slot==BG_INVALID)return 1;
 const volatile uint32_t*map=(const volatile uint32_t*)(layer?BG_MAP3:BG_MAP2);
 uint16_t id=bgLookup(layer,map[bgCell(layer,x,y)]),old;
 if(id==BG_INVALID)return 0;
 if(layer){
  if(!willowNativeBackgroundFarKeyGet(&bg.keys,slot,&old))return 0;
  if(old==id)return 1;
  return willowNativeBackgroundFarKeySet(&bg.keys,slot,id)?3:0;
 }
 if(!willowNativeBackgroundNearKeyGet(&bg.keys,slot,&old))return 0;
 if(old==id)return 1;
 return willowNativeBackgroundNearKeySet(&bg.keys,slot,id)?3:0;
}
#endif
static inline __attribute__((always_inline)) uint16_t bgRef(uint16_t layer,uint16_t x,uint16_t y)
{
 const BackgroundRing*r=bg.rings+layer;uint16_t scale=layer?2:1;
 uint16_t dx=((x>>scale)-r->x)&63,dy=((y>>scale)-r->y)&63;
 if(dx>=r->w||dy>=r->h)return BG_INVALID;
 dx+=r->hx;if(dx>=r->w)dx-=r->w;
 dy+=r->hy;if(dy>=r->h)dy-=r->h;
 uint16_t slot=dy*r->w+dx,id;
 if(layer){id=bg.keys.far[slot];if(id>=WILLOW_BG_CATALOG_LAYER3_PAIR_COUNT)return BG_INVALID;}
 else{
  id=bg.keys.nearLow[slot]|((uint16_t)((bg.keys.nearHigh[slot>>2]>>((slot&3)*2))&3)<<8);
  if(id>=WILLOW_BG_CATALOG_LAYER2_PAIR_COUNT)return BG_INVALID;
 }
 uint16_t mask=layer?3:1;
 return (id<<(scale*2))+((y&mask)<<scale)+(x&mask);
}
static uint16_t bgFirstX(uint16_t layer)
{return (((uint16_t)(96+bg.view[layer*2])>>3)-1)&(layer?255:127);}
static uint16_t bgFirstY(uint16_t layer)
{return ((uint16_t)(16+bg.view[layer*2+1])>>3)&(layer?255:127);}
static uint16_t bgRows(uint16_t layer){return 28+((bg.view[layer*2+1]&7)!=0);}
#ifdef RENDER_BACKGROUND_FAR_STREAM
/* Protection may conservatively retain retired words until the next miss.
 * A miss rebuilds the complete prepared set before selecting any victim. */
static BG_FAR_CODE void bgFarProtectWord(uint16_t word)
{
 if(word==BG_INVALID){backgroundFarCache.missing=1;return;}
 uint16_t slot=willowFarSlot(word&0x7ff);
 if(slot!=WILLOW_FAR_CACHE_NONE)willowFarProtect(backgroundFarCache.preparedUse,slot);
}
#endif
static void bgSet(uint16_t layer,uint16_t x,uint16_t y,uint16_t word)
{
#ifdef RENDER_BACKGROUND_FAR_STREAM
#ifdef RENDER_BACKGROUND_SHARED_STREAM
 bgFarProtectWord(word);
#else
 if(layer)bgFarProtectWord(word);
#endif
#endif
 WillowNativePlaneSurface*s=bg.planes+layer;x&=63;y&=31;
 uint16_t*p=s->words+y*64+x;if(*p==word)return;*p=word;
 uint32_t bit=1UL<<y;
 if(!(s->dirty&bit)){s->first[y]=s->last[y]=x;s->dirty|=bit;}
 else{if(x<s->first[y])s->first[y]=x;if(x>s->last[y])s->last[y]=x;}
 bg.columns[layer][x>>5]|=1UL<<(x&31);
}
#ifdef RENDER_BACKGROUND_FAR_STREAM
/* Clear exactly one wrapped native row span. Most retained far words are
 * already blank, so scan those words directly and update metadata only for
 * actual changes. Each native scan ends at its declared contiguous span. */
static BG_FAR_CODE void bgFarClearRow(uint16_t x,uint16_t y,uint16_t count)
{
 uint16_t*base=bg.planes[1].words+(y&31)*64;x&=63;
 while(count){
  uint16_t span=64-x;if(span>count)span=count;
  uint16_t*start=base+x,*scan=start,word;
#ifdef __m68k__
  uint16_t left=span-1;
  __asm__ volatile("1: move.w (%0)+,%2\n\tdbne %1,1b"
   : "+&a"(scan),"+&d"(left),"=&d"(word) : : "cc","memory");
#else
  uint16_t left=span;
  do{word=*scan++;}while(!word&&--left);
#endif
  uint16_t consumed=(uint16_t)(scan-start);
  if(word)bgSet(1,x+consumed-1,y,0);
  count-=consumed;x=(x+consumed)&63;
 }
}
#endif
/* A far source cell supplies four consecutive8px rows. Resolve its retained
 * byte ID once, then walk the already converted flags at stride4. The owner
 * captures the retiring column before moving this ring; refusal invalidates
 * any partially edited preparation through bgFail while busy is set. */
typedef struct {uint16_t slot,y,rows;} BackgroundFarCursor;
static uint16_t bgFarStripStart(uint16_t x,uint16_t y,uint16_t rows,BackgroundFarCursor*c)
{
 const BackgroundRing*r=bg.rings+1;
 if(x>=256||y>=256||!rows||rows>29||!r->valid||r->w!=12||r->h!=8||r->scale!=2||
    r->x>=64||r->y>=64||r->hx>=12||r->hy>=8)return 0;
 uint16_t dy=((y>>2)-r->y)&63;
 if(dy+(((y&3)+rows-1)>>2)>=8)return 0;
 c->slot=bgSlot(r,x>>2,y>>2);if(c->slot==BG_INVALID)return 0;
 c->y=y;c->rows=rows;return 1;
}
static uint16_t bgFarCaptureFlags(uint16_t x,uint16_t y,uint16_t rows,uint32_t*mask)
{
 BackgroundFarCursor c;if(!bgFarStripStart(x,y,rows,&c))return 0;
 const uint8_t*flags=(const uint8_t*)WILLOW_BG_CATALOG_LAYER3_FLAGS_ADDRESS;
 uint32_t result=0;
 do{
  uint16_t id=bg.keys.far[c.slot];if(id>=WILLOW_BG_CATALOG_LAYER3_PAIR_COUNT)return 0;
  uint16_t ref=id*16+(c.y&3)*4+(x&3),n=4-(c.y&3);if(n>c.rows)n=c.rows;
  do{
   if(flags[ref]!=WILLOW_BG_CATALOG_EMPTY)result|=1UL<<(c.y&31);
   c.y++;ref+=4;c.rows--;
  }while(--n);
  c.slot+=12;if(c.slot>=96)c.slot-=96;
 }while(c.rows);
 *mask=result;return 1;
}
static uint16_t bgFarEnter(uint16_t x,uint16_t y,uint16_t rows,uint32_t leaving)
{
 BackgroundFarCursor c;if(!bgFarStripStart(x,y,rows,&c))return 0;
 const uint8_t*flags=(const uint8_t*)WILLOW_BG_CATALOG_LAYER3_FLAGS_ADDRESS;
 do{
  uint16_t id=bg.keys.far[c.slot];if(id>=WILLOW_BG_CATALOG_LAYER3_PAIR_COUNT)return 0;
  uint16_t ref=id*16+(c.y&3)*4+(x&3),n=4-(c.y&3);if(n>c.rows)n=c.rows;
  do{
   uint16_t row=c.y&31;
   int16_t count=(int16_t)bg.rowCounts[1][row]+(flags[ref]!=WILLOW_BG_CATALOG_EMPTY)-((leaving>>row)&1);
   if(count<0||count>43)return 0;
   bg.rowCounts[1][row]=(uint8_t)count;
   if(bg.farBlank&&bg.planes[1].words[row*64+(x&63)])bgSet(1,x,c.y,0);
   c.y++;ref+=4;c.rows--;
  }while(--n);
  c.slot+=12;if(c.slot>=96)c.slot-=96;
 }while(c.rows);
 return 1;
}
#ifdef RENDER_BACKGROUND_FAR_STREAM
typedef struct {uint16_t row[2],full[2];} BackgroundNearRowProof;
#define BG_ROW_PROOF_ARGUMENT ,BackgroundNearRowProof*proof
#define BG_ROW_PROOF_PASS(p) ,p
#define BG_ROW_FULL(y) rowProofFull(proof,y)
#define BG_SHARED_FAR_CODE BG_FAR_CODE
#else
#define BG_ROW_PROOF_ARGUMENT
#define BG_ROW_PROOF_PASS(p)
#define BG_ROW_FULL(y) bgNearVisibleRowFull(y)
#define BG_SHARED_FAR_CODE
#endif
static uint16_t bgNearVisibleRowFull(uint16_t y);
static uint16_t bgFarRowBlank(uint16_t row BG_ROW_PROOF_ARGUMENT);
#ifdef RENDER_BACKGROUND_FAR_STREAM
/* Fresh stack-local memo for one serialized far-row call. Prepared near view,
 * rings, keys and counts cannot change while this caller owns bg.busy. Source
 * map/staged-view/epoch changes still pass through the existing final guards. */
static void rowProofInit(BackgroundNearRowProof*p)
{p->row[0]=p->row[1]=BG_INVALID;p->full[0]=p->full[1]=0;}
static uint16_t rowProofFull(BackgroundNearRowProof*p,uint16_t y)
{
 if(!p)return bgNearVisibleRowFull(y);
 uint16_t slot=y&1;
 if(p->row[slot]!=y){p->full[slot]=bgNearVisibleRowFull(y);p->row[slot]=y;}
 return p->full[slot];
}

#endif
static __attribute__((noinline)) uint16_t bgNearOpacityRef(uint16_t x,uint16_t y)
{return bgRef(0,x,y);}
/* Compare original displayed-opacity bytes. A far8px row overlaps at most
 * two near cells; resolve those IDs once per near row, never once per pixel.
 * Unknown source descriptors cannot certify hidden pixels. */
static BG_SHARED_FAR_CODE uint16_t bgFarNeeded(uint16_t ref,uint16_t x,uint16_t y BG_ROW_PROOF_ARGUMENT)
{
 if(*((const uint8_t*)WILLOW_BG_CATALOG_LAYER3_FLAGS_ADDRESS+ref)==WILLOW_BG_CATALOG_EMPTY)return 0;
 const uint8_t*mask=(const uint8_t*)WILLOW_BG_CATALOG_LAYER3_OPACITY_ADDRESS+ref*8;
 int16_t sx=(int16_t)(((x-bgFirstX(1))&255)*8)-8-(bg.view[2]&7);
 int16_t sy=(int16_t)(((y-bgFirstY(1))&255)*8)-(bg.view[3]&7);
 if(sx>=320||sx+8<=0||sy>=224||sy+8<=0)return 0;
 uint16_t visible=0xff;
 if(sx<0)visible>>=-sx;
 if(sx+8>320)visible&=0xffu<<(sx+8-320);
 uint16_t nx=(96+bg.view[0]+sx)&1023,shift=nx&7,lastY=BG_INVALID,full=0;
 const uint8_t*left=0,*right=0;
 for(uint16_t yy=0;yy<8;yy++)if(sy+yy>=0&&sy+yy<224){
  uint16_t candidate=mask[yy]&visible;if(!candidate)continue;
  uint16_t ny=(16+bg.view[1]+sy+yy)&1023;
  if((ny>>3)!=lastY){
   lastY=ny>>3;full=BG_ROW_FULL(lastY);
   if(!full){
    uint16_t a=bgNearOpacityRef(nx>>3,lastY);if(a==BG_INVALID)return 1;
    full=*((const uint8_t*)WILLOW_BG_CATALOG_LAYER2_FLAGS_ADDRESS+a)==WILLOW_BG_CATALOG_FULL_OPAQUE;
    left=(const uint8_t*)WILLOW_BG_CATALOG_LAYER2_OPACITY_ADDRESS+a*8;
    if(shift){
     uint16_t b=bgNearOpacityRef(((nx>>3)+1)&127,lastY);if(b==BG_INVALID)return 1;
     full=full&&*((const uint8_t*)WILLOW_BG_CATALOG_LAYER2_FLAGS_ADDRESS+b)==WILLOW_BG_CATALOG_FULL_OPAQUE;
     right=(const uint8_t*)WILLOW_BG_CATALOG_LAYER2_OPACITY_ADDRESS+b*8;
    }
   }
  }
  /* A FULL source tile certifies every opacity byte through the end of this
   * near cell. Skip only that covered span; another overlapping near row
   * still needs its own proof, including at the wrapped world boundary. */
  if(full){yy+=7-(ny&7);continue;}
  uint16_t covered=left[ny&7];
  if(shift)covered=(covered<<shift)|(right[ny&7]>>(8-shift));
  if(candidate&((uint16_t)~covered&0xff))return 1;
 }
 return 0;
}
static BG_SHARED_FAR_CODE uint16_t bgResolveRef(uint16_t layer,uint16_t x,uint16_t y,uint16_t ref BG_ROW_PROOF_ARGUMENT)
{
 if(ref==BG_INVALID)return 0;
 const uint16_t*refs=(const uint16_t*)(layer?WILLOW_BG_CATALOG_LAYER3_REFS_ADDRESS:WILLOW_BG_CATALOG_LAYER2_REFS_ADDRESS);
 uint16_t word=refs[ref*2+1];
 if(layer&&!bgFarNeeded(ref,x,y BG_ROW_PROOF_PASS(proof)))word=0;
#ifdef RENDER_BACKGROUND_FAR_STREAM
#ifdef RENDER_BACKGROUND_SHARED_STREAM
 else if(layer||word==BG_INVALID||willowFarSlot(word&0x7ff)!=WILLOW_FAR_CACHE_NONE){
  if(!layer)ref=(WILLOW_BG_SHARED_NEAR_KEY_BASE+(ref>>2))*16+(ref&3);
#else
 else if(layer){
#endif
  uint16_t hint=bg.planes[layer].words[(y&31)*64+(x&63)];
  if(willowFarCacheResolve(&backgroundFarCache,&backgroundFarResources,ref,hint,0,&word)!=WILLOW_FAR_CACHE_COMMITTED){
   if(backgroundFarMissingRef==BG_INVALID)backgroundFarMissingRef=ref;
   /* Missing art is never published. Keep logical rings/counts current while
    * the bounded VBlank owner completes the pending pattern transfer. */
   bgSet(layer,x,y,BG_INVALID);return 1;
  }
 }
#endif
 if(word==BG_INVALID)return 0;
 bgSet(layer,x,y,word);return 1;
}
static uint16_t bgResolveCell(uint16_t layer,uint16_t x,uint16_t y)
{return bgResolveRef(layer,x,y,bgRef(layer,x,y) BG_ROW_PROOF_PASS(0));}
static uint16_t bgResolveRow(uint16_t layer,uint16_t row)
{
#ifdef RENDER_BACKGROUND_FAR_STREAM
 BackgroundNearRowProof proof;rowProofInit(&proof);
#endif
 uint16_t x=bgFirstX(layer),y=(bgFirstY(layer)+row)&(layer?255:127);
 if(layer&&bgFarRowBlank(row BG_ROW_PROOF_PASS(&proof))){
#ifdef RENDER_BACKGROUND_FAR_STREAM
  bgFarClearRow(x,y,43);
#else
  for(uint16_t n=0;n<43;n++)if(bg.planes[1].words[(y&31)*64+((x+n)&63)])bgSet(1,x+n,y,0);
#endif
  return 1;
 }
 #ifdef RENDER_BACKGROUND_FAR_STREAM
 for(uint16_t n=0;n<43;n++){
  uint16_t worldX=(x+n)&(layer?255:127);
  if(layer){if(!bgResolveRef(1,worldX,y,bgRef(1,worldX,y),&proof))return 0;}
  else if(!bgResolveCell(0,worldX,y))return 0;
 }
#else
 for(uint16_t n=0;n<43;n++)if(!bgResolveCell(layer,(x+n)&(layer?255:127),y))return 0;
#endif
 return 1;
}
#ifdef RENDER_BACKGROUND_SHARED_STREAM
/* Source/view changes have already rebuilt entering or changed cells before
 * this retry. Fixed words cannot change identity during a pool upload. Only
 * retained dynamic words and unresolved markers need fresh source resolution. */
static BG_FAR_CODE uint16_t bgNearRetry(void)
{
 uint16_t x=bgFirstX(0),y=bgFirstY(0),rows=bgRows(0);
 for(uint16_t row=0;row<rows;row++)for(uint16_t col=0;col<43;col++){
  uint16_t nx=(x+col)&127,ny=(y+row)&127;
  uint16_t word=bg.planes[0].words[(ny&31)*64+(nx&63)];
  if((word==BG_INVALID||willowFarSlot(word&0x7ff)!=WILLOW_FAR_CACHE_NONE)&&
     !bgResolveCell(0,nx,ny))return 0;
 }
 return 1;
}
#endif
#if defined(RENDER_BACKGROUND_SHARED_STREAM) && defined(RENDER_BACKGROUND_NEAR_STRIP)
/* The prepared writer bypasses bgSet. Admit only immutable non-pool words;
 * any missing or dynamic reference sends the entire column through resolution
 * before row summaries or shadow words are changed. */
static BG_FAR_CODE uint16_t bgNearFixedColumn(uint16_t x,uint16_t y,uint16_t rows)
{
 const uint16_t*refs=(const uint16_t*)WILLOW_BG_CATALOG_LAYER2_REFS_ADDRESS;
 for(uint16_t row=0;row<rows;row++){
  uint16_t ref=bgRef(0,x,(y+row)&127);
  if(ref==BG_INVALID)return 0;
  uint16_t word=refs[ref*2+1];
  if(word==BG_INVALID||willowFarSlot(word&0x7ff)!=WILLOW_FAR_CACHE_NONE)return 0;
 }
 return 1;
}
#endif
static uint16_t bgCountRow(uint16_t layer,uint16_t row)
{
 uint16_t count=0,x=bgFirstX(layer),y=(bgFirstY(layer)+row)&(layer?255:127);
 const uint8_t*flags=(const uint8_t*)(layer?WILLOW_BG_CATALOG_LAYER3_FLAGS_ADDRESS:WILLOW_BG_CATALOG_LAYER2_FLAGS_ADDRESS);
 for(uint16_t col=0;col<43;col++){
  uint16_t ref=bgRef(layer,(x+col)&(layer?255:127),y);
  if(ref==BG_INVALID)return 0;
  count+=flags[ref]!=(layer?WILLOW_BG_CATALOG_EMPTY:WILLOW_BG_CATALOG_FULL_OPAQUE);
 }
 bg.rowCounts[layer][y&31]=count;return 1;
}
/* The retained row has43 columns, but its outer guards never reach the320px
 * display. Remove complete offscreen cells from the exact current nonopaque
 * count; partial edge cells must be opaque at every on-screen source pixel.
 * y is the wrapped world row, not the aliased32-row native plane index. */
static uint16_t bgNearVisibleRowFull(uint16_t y)
{
 int16_t count=bg.rowCounts[0][y&31];
 if(!count)return 1;
 if(count>43)return 0;
 uint16_t first=bgFirstX(0),fine=bg.view[0]&7,guards=fine?2:3;
 const uint8_t*flags=(const uint8_t*)WILLOW_BG_CATALOG_LAYER2_FLAGS_ADDRESS;
 for(uint16_t n=0;n<guards;n++){
  uint16_t col=n?43-n:0,ref=bgNearOpacityRef((first+col)&127,y);
  if(ref==BG_INVALID)return 0;
  count-=flags[ref]!=WILLOW_BG_CATALOG_FULL_OPAQUE;
 }
 if(count<=0||!fine)return count==0;
 for(uint16_t n=0;n<2;n++){
  uint16_t ref=bgNearOpacityRef((first+(n?41:1))&127,y);
  if(ref==BG_INVALID)return 0;
  if(flags[ref]==WILLOW_BG_CATALOG_FULL_OPAQUE)continue;
  uint16_t visible=n?(0xffu<<(8-fine))&0xff:0xffu>>fine,opaque=1;
  const uint8_t*mask=(const uint8_t*)WILLOW_BG_CATALOG_LAYER2_OPACITY_ADDRESS+ref*8;
  for(uint16_t row=0;row<8;row++)if((mask[row]&visible)!=visible){opaque=0;break;}
  count-=opaque;
 }
 return count==0;
}
/* Every nonempty far row must be covered by opaque current near rows within
 * the visible width. The proof is invalidated on near fine-X changes too:
 * retained column41 enters the right edge when fineX changes from0 to1. */
static uint16_t bgFarRowBlank(uint16_t row BG_ROW_PROOF_ARGUMENT)
{
 uint16_t y=(bgFirstY(1)+row)&255;
 if(!bg.rowCounts[1][y&31])return 1;
 int16_t top=row*8-(bg.view[3]&7),bottom=top+7;
 if(top<0)top=0;
 if(bottom>223)bottom=223;
 uint16_t ny=((16+bg.view[1]+top)>>3)&127;
 uint16_t end=((16+bg.view[1]+bottom)>>3)&127;
 return BG_ROW_FULL(ny)&&(end==ny||BG_ROW_FULL(end));
}
static uint16_t bgFarBlank(void)
{
 /* Equal vertical fine phases align each clipped far row with exactly one
  * near row. Empty far rows and wholly opaque43-cell near rows need no
  * descriptor/edge work. Keep the original proof for every other row. */
 if(!((bg.view[1]^bg.view[3])&7)){
  uint16_t near=bgFirstY(0)&31,far=bgFirstY(1)&31,row=0,rows=bgRows(1);
  while(row<rows){
#ifdef __m68k__
   __asm__ volatile(
    "1: tst.b (%3,%0.w)\n\tbeq.s 2f\n\t"
    "tst.b (%4,%1.w)\n\tbne.s 3f\n\t"
    "2: addq.w #1,%0\n\tandi.w #31,%0\n\t"
    "addq.w #1,%1\n\tandi.w #31,%1\n\t"
    "addq.w #1,%2\n\tcmp.w %5,%2\n\tbcs.s 1b\n\t3:"
    : "+&d"(near),"+&d"(far),"+&d"(row)
    : "a"(bg.rowCounts[0]),"a"(bg.rowCounts[1]),"d"(rows)
    : "cc","memory");
   if(row==rows)return 1;
#else
   if(!bg.rowCounts[0][near]||!bg.rowCounts[1][far]){
    near=(near+1)&31;far=(far+1)&31;row++;continue;
   }
#endif
   if(!bgFarRowBlank(row BG_ROW_PROOF_PASS(0)))return 0;
   near=(near+1)&31;far=(far+1)&31;row++;
  }
  return 1;
 }
 for(uint16_t row=0;row<bgRows(1);row++)if(!bgFarRowBlank(row BG_ROW_PROOF_PASS(0)))return 0;
 return 1;
}
#ifdef RENDER_BACKGROUND_FAR_STREAM
static BG_FAR_CODE void bgFarContext(WillowNativeFarContext*c)
{
 c->sourceEpoch=bg.preparedEpoch;c->paletteEpoch=bg.lease.generation;c->generation=bg.generation;
 for(uint16_t i=0;i<4;i++)c->view[i]=bg.view[i];
}
static BG_FAR_CODE uint16_t bgFarRetry(void)
{
 return backgroundFarCache.missing||backgroundFarCache.phase!=WILLOW_FAR_CACHE_IDLE||
  backgroundFarCache.preparedResidencyEpoch!=backgroundFarCache.residencyEpoch;
}
/* Complete resolution precedes victim selection. Clear only off-view shadow
 * words; old physical words remain protected until their actual publication.
 * A full pool refuses safely, never overwriting still-displayed references. */
static BG_FAR_CODE uint16_t bgFarFinish(void)
{
 WillowNativeFarCache*s=&backgroundFarCache;
 if(s->missing||backgroundFarMissingRef!=BG_INVALID){
  uint16_t firstX=bgFirstX(1)&63,firstY=bgFirstY(1)&31,rows=bgRows(1);
  for(uint16_t y=0;y<32;y++){
   /* Retained rows have exactly21 off-view columns. Rows outside the
    * wrapped vertical span have64. Visit only those words; the resulting
    * shadow, dirty ranges and ownership rebuild are unchanged. */
   uint16_t retained=((y-firstY)&31)<rows;
   uint16_t x=retained?(firstX+43)&63:0,count=retained?21:64;
   bgFarClearRow(x,y,count);
  }
#ifdef RENDER_BACKGROUND_SHARED_STREAM
  uint16_t nx=bgFirstX(0)&63,ny=bgFirstY(0)&31,nrows=bgRows(0);
  for(uint16_t y=0;y<32;y++){
   uint16_t retained=((y-ny)&31)<nrows;
   uint16_t x=retained?(nx+43)&63:0,count=retained?21:64;
   for(uint16_t n=0;n<count;n++){
    uint16_t word=bg.planes[0].words[y*64+((x+n)&63)];
    /* Fixed art consumes no dynamic ownership. Keep those off-view words;
     * only missing markers and pool references need retirement before the
     * complete two-plane protection rebuild. */
    if(word==BG_INVALID||willowFarSlot(word&0x7ff)!=WILLOW_FAR_CACHE_NONE)bgSet(0,x+n,y,0);
   }
  }
  if(!willowFarCachePreparedPlanes(s,bg.planes[0].words,2048,bg.planes[1].words,2048))return 0;
#else
  if(!willowFarCachePreparedUse(s,bg.planes[1].words,2048))return 0;
#endif
  if(backgroundFarMissingRef!=BG_INVALID&&s->phase==WILLOW_FAR_CACHE_IDLE)
   willowFarCacheRequest(s,&backgroundFarResources,backgroundFarMissingRef>>4,
#ifdef RENDER_BACKGROUND_SHARED_STREAM
    (backgroundFarMissingRef>>4)>=WILLOW_BG_SHARED_NEAR_KEY_BASE?0x000f:0xffff
#else
    0xffff
#endif
   );
 }
 WillowNativeFarContext context;bgFarContext(&context);
 return willowFarCacheBindPrepared(s,&context);
}
/* One callback is one complete32-byte native pattern. Cache preflight protects
 * every displayed slot; this caller additionally owns the serialized VDP. */
static BG_FAR_CODE uint16_t bgFarUpload(void*context,uint16_t tile,const uint8_t*bytes)
{
 (void)context;
 LONG(0xc00004)=willowNativePlaneCommand(tile*32);
 willowNativePlaneCopy((const uint16_t*)bytes,16);return 1;
}
static BG_FAR_CODE void bgFarPublishPatterns(void)
{
 uint16_t written=0;
 BG_PHASE(9,0);
 uint16_t okay=willowFarCacheUpload(&backgroundFarCache,&backgroundFarResources,4,bgFarUpload,0,&written);
 WORD(0x20f5b4)=written;BG_PHASE(10,written|(okay?0:0x80));
}
static BG_FAR_CODE uint16_t bgFarPublishCurrent(void)
{
 WillowNativeFarContext context;bgFarContext(&context);
 return willowFarCachePreparedCurrent(&backgroundFarCache,&context);
}
static BG_FAR_CODE uint16_t bgFarPublished(void)
{
 WillowNativeFarContext context;bgFarContext(&context);
 return willowFarCachePublished(&backgroundFarCache,&context,!bg.planes[0].dirty&&!bg.planes[1].dirty);
}
#endif
static uint16_t bgIdentity(void)
{
 return viewReady()&&WORD(PORT+0x104)==0x9180&&WORD(PORT+0x106)==0x9140&&
  WORD(PORT+0x170)==0x12f8;
}
static void bgDescribe(uint16_t publish)
{
 uint32_t at=publish?0x20f5a4:0x20f594;
 LONG(at)=bg.generation;LONG(at+4)=bg.lease.generation;
 for(uint16_t i=0;i<4;i++)WORD(at+8+i*2)=bg.view[i];
}
static void bgFail(uint16_t status)
{
 bg.ready=0;
 /* Preparation mutates rings/words in place. A refused partial update cannot
  * become a new baseline on retry; every descriptor must be reconstructed. */
 if(bg.busy)bg.mode=0;
 bg.fault=status;BG_PHASE(5,status);
}
static void bgReset(const uint16_t*next)
{
 bg.ready=0;bg.mode=0;bg.cursor=0;bg.scrollDirty=3;
 bg.lease.plan=&backgroundPlan;bg.lease.epoch=(const volatile uint32_t*)0x20f568;bg.lease.generation=PALETTE_EPOCH;
 for(uint16_t i=0;i<4;i++)bg.view[i]=next[i];
 /* Cache slots are all replaced before admission. Defer the8KiB plane clear
  * until a complete stable source view exists; moving intro views must not
  * repeatedly clear both native planes. */
 for(uint16_t layer=0;layer<2;layer++){
  BackgroundRing*r=bg.rings+layer;r->scale=layer?2:1;
  r->x=bgFirstX(layer)>>r->scale;r->y=bgFirstY(layer)>>r->scale;r->hx=r->hy=0;
  r->w=layer?12:22;r->h=layer?8:15;r->valid=1;
  bg.mapGeneration[layer]=backgroundSourceWriter.mapGeneration[layer];
  bg.columns[layer][0]=bg.columns[layer][1]=0xffffffffUL;
 }
 bg.mode=1;bg.fault=0;
}
/* Shift the descriptor ring by at most one metatile in either direction. A
 * larger source jump requires cold reconstruction, never stale wrapped IDs. */
static uint16_t bgMoveRing(uint16_t layer)
{
 BackgroundRing*r=bg.rings+layer;uint16_t x=bgFirstX(layer)>>r->scale,y=bgFirstY(layer)>>r->scale;
 int16_t dx=(x-r->x)&63,dy=(y-r->y)&63;if(dx==63)dx=-1;if(dy==63)dy=-1;
 if(dx>1||dy>1)return 0;
 if(dx){r->x=x;if(dx>0){if(++r->hx==r->w)r->hx=0;}else r->hx=r->hx?r->hx-1:r->w-1;
  uint16_t col=(x+(dx>0?r->w-1:0))&63;
  for(uint16_t row=0;row<r->h;row++)if(!bgReadPair(layer,col,(r->y+row)&63))return 0;
 }
 if(dy){r->y=y;if(dy>0){if(++r->hy==r->h)r->hy=0;}else r->hy=r->hy?r->hy-1:r->h-1;
  uint16_t row=(y+(dy>0?r->h-1:0))&63;
  for(uint16_t col=0;col<r->w;col++)if(!bgReadPair(layer,(r->x+col)&63,row))return 0;
 }
 return 1;
}
static __attribute__((noinline)) void nativeBackgroundPrepare(void)
{
#ifdef RENDER_BACKGROUND_FAR_STREAM
 /* The queued immutable request owns its reserved slots until upload finishes.
  * Re-resolving the same misses here only blocks the IRQ's transfer while busy
  * is set. Keep source-column marks pending; the next complete preparation
  * rereads the authoritative view and changed keys after the queue drains. */
 if(backgroundFarCache.phase==WILLOW_FAR_CACHE_QUEUED)return;
#endif
 BackgroundSourceWriter*w=&backgroundSourceWriter;
#ifdef RENDER_BACKGROUND_COLUMNS
 WillowNativeBackgroundColumnsView journal;
#else
 WillowNativeBackgroundJournalView journal;
#endif
 if(bg.busy||w->rawBusy||w->active||!w->rawEpoch||!bgIdentity()||!PALETTE_READY)return;
 bg.busy=1;bg.ready=0;BARRIER();
#ifdef RENDER_BACKGROUND_FAR_STREAM
 backgroundFarMissingRef=BG_INVALID;
#endif
 uint16_t next[4];
#if defined(RENDER_BACKGROUND_SHARED_STREAM) && defined(__m68k__)
 /* A single unprivileged MOVEM reads the staged quartet. A 68000 takes
  * interrupts between instructions, so the four source words stay coherent.
  * Following stores only copy those saved registers into native locals. */
 __asm__ volatile("movem.w 0xfff446,%%d0-%%d3\n\t"
                  "move.w %%d0,(%0)\n\tmove.w %%d1,2(%0)\n\t"
                  "move.w %%d2,4(%0)\n\tmove.w %%d3,6(%0)"
                  ::"a"(next):"d0","d1","d2","d3","cc","memory");
#else
 for(uint16_t i=0;i<4;i++)next[i]=WORD(0xfff446u+i*2);
#endif
 bg.generation++;LONG(0x20f594)=bg.generation;LONG(0x20f598)=PALETTE_EPOCH;
 for(uint16_t i=0;i<4;i++)WORD(0x20f59cu+i*2)=next[i];
 BG_PHASE(1,0);
 uint16_t changed=0;for(uint16_t i=0;i<4;i++)changed|=next[i]!=bg.view[i];
 if(!bg.mode||!willowPresentationCurrent(&bg.lease)||
    (bg.mode<4&&(w->mapGeneration[0]!=bg.mapGeneration[0]||w->mapGeneration[1]!=bg.mapGeneration[1]||changed)))bgReset(next);
 if(bg.mode==1){
  uint16_t layer=bg.cursor>=15,row=bg.cursor-(layer?15:0);BackgroundRing*r=bg.rings+layer;
  for(uint16_t col=0;col<r->w;col++)if(!bgReadPair(layer,(r->x+col)&63,(r->y+row)&63)){bgFail(2);goto done;}
  if(++bg.cursor==23){
#ifdef RENDER_BACKGROUND_COLUMNS
   if(!willowNativeBackgroundColumnsBaseline(w,w->rawEpoch,bg.mapGeneration[0],bg.mapGeneration[1])){bgFail(3);goto done;}
#else
   if(!willowNativeBackgroundWriterBaseline(w,w->rawEpoch,bg.mapGeneration[0],bg.mapGeneration[1])){bgFail(3);goto done;}
#endif
   if(!willowNativePlaneLoad(bg.planes,&bg.lease,0)||!willowNativePlaneLoad(bg.planes+1,&bg.lease,0)){bgFail(1);goto done;}
#ifdef RENDER_BACKGROUND_FAR_STREAM
#ifdef RENDER_BACKGROUND_SHARED_STREAM
   if(!willowFarCachePreparedPlanes(&backgroundFarCache,bg.planes[0].words,2048,bg.planes[1].words,2048)){bgFail(24);goto done;}
#else
   if(!willowFarCachePreparedUse(&backgroundFarCache,bg.planes[1].words,2048)){bgFail(24);goto done;}
#endif
#endif
   bg.mode=2;bg.cursor=0;
  }
  BG_PHASE(6,0);goto done;
 }
 if(bg.mode==2){
  uint16_t nearCells=bgRows(0)*43,total=nearCells+bgRows(1)*43;
  /* Bounded preparation keeps source tasks running during cold construction.
   * Resolve each key once; opacity summary and native word share that ref. */
  for(uint16_t budget=0;budget<16&&bg.cursor<total;budget++,bg.cursor++){
   uint16_t layer=bg.cursor>=nearCells,offset=bg.cursor-(layer?nearCells:0);
   uint16_t row=offset/43,col=offset%43;
   uint16_t x=(bgFirstX(layer)+col)&(layer?255:127),y=(bgFirstY(layer)+row)&(layer?255:127);
   uint16_t ref=bgRef(layer,x,y);
   if(ref==BG_INVALID){bgFail(4);goto done;}
   if(!col)bg.rowCounts[layer][y&31]=0;
   const uint8_t*flags=(const uint8_t*)(layer?WILLOW_BG_CATALOG_LAYER3_FLAGS_ADDRESS:WILLOW_BG_CATALOG_LAYER2_FLAGS_ADDRESS);
   bg.rowCounts[layer][y&31]+=flags[ref]!=(layer?WILLOW_BG_CATALOG_EMPTY:WILLOW_BG_CATALOG_FULL_OPAQUE);
   if(!bgResolveRef(layer,x,y,ref BG_ROW_PROOF_PASS(0))){bgFail(4);goto done;}
  }
  if(bg.cursor==total){bg.mode=3;bg.cursor=0;bg.farBlank=bgFarBlank();}
  goto prepared;
 }
 if(bg.mode==3){
#ifdef RENDER_BACKGROUND_FAR_STREAM
  if(bgFarRetry()){
#ifdef RENDER_BACKGROUND_SHARED_STREAM
   if(!bgNearRetry()){bgFail(11);goto done;}
#endif
   for(uint16_t row=0;row<bgRows(1);row++)if(!bgResolveRow(1,row)){bgFail(11);goto done;}
  }
#endif
  if(bg.planes[0].dirty||bg.planes[1].dirty)goto prepared;
  bg.mode=4;
 }
 if(bg.mode==4){
#ifdef RENDER_BACKGROUND_COLUMNS
  if(!willowNativeBackgroundColumnsAcquire(w,&journal)){bgFail(5);goto done;}
  if(!changed&&!(journal.pending[0][0]|journal.pending[0][1]|journal.pending[1][0]|journal.pending[1][1])
#ifdef RENDER_BACKGROUND_FAR_STREAM
     &&!bgFarRetry()
#endif
    )goto acknowledged;
#else
  if(!willowNativeBackgroundJournalAcquire(&w->journal,&journal)){bgFail(5);goto done;}
  if(!changed&&!journal.count)goto acknowledged;
#endif
  /* Retain departing opacity BEFORE advancing/reusing descriptor ring slots.
   * A retired slot cannot be reconstructed from the new ring's coordinates. */
  uint32_t leaving[2]={0,0};
  for(uint16_t layer=0;layer<2;layer++){
   uint16_t extent=layer?255:127,ox=bgFirstX(layer),oy=bgFirstY(layer);
   uint16_t nx=((((uint16_t)(96+next[layer*2])>>3)-1)&extent);
   int16_t dx=(nx-ox)&extent;if(dx==extent)dx=-1;
   if((dx==1||dx==-1)&&bg.view[layer*2+1]==next[layer*2+1]){
    uint16_t leave=(ox+(dx>0?0:42))&extent;
    if(layer){if(!bgFarCaptureFlags(leave,oy,bgRows(1),leaving+1)){bgFail(9);goto done;}continue;}
#ifdef RENDER_BACKGROUND_NEAR_STRIP
    if(!willowNativeBackgroundNearCaptureFlags(&backgroundNearStrip,leave,oy,bgRows(0),leaving)){bgFail(9);goto done;}
#else
    const uint8_t*flags=(const uint8_t*)WILLOW_BG_CATALOG_LAYER2_FLAGS_ADDRESS;
    for(uint16_t row=0;row<bgRows(layer);row++){
     uint16_t y=(oy+row)&extent,ref=bgRef(layer,leave,y);
     if(ref==BG_INVALID){bgFail(9);goto done;}
     if(flags[ref]!=(layer?WILLOW_BG_CATALOG_EMPTY:WILLOW_BG_CATALOG_FULL_OPAQUE))leaving[layer]|=1UL<<(y&31);
    }
#endif
   }
  }
  uint16_t old[4];for(uint16_t i=0;i<4;i++){
   old[i]=bg.view[i];bg.view[i]=next[i];if(old[i]!=next[i])bg.scrollDirty|=(i&1)?2:1;
  }
  if(!bgMoveRing(0)||!bgMoveRing(1)){bgFail(6);goto done;}
  uint16_t full=0,recheck=((old[0]^next[0])&7)!=0;
#ifdef RENDER_BACKGROUND_COLUMNS
  for(uint16_t layer=0;layer<2;layer++){
   if(!(journal.pending[layer][0]|journal.pending[layer][1]))continue;
   const BackgroundRing*r=bg.rings+layer;
   for(uint16_t col=0;col<r->w;col++){
    uint16_t x=(r->x+col)&63;
    if(!(journal.pending[layer][x>>5]&(1UL<<(x&31))))continue;
    for(uint16_t row=0;row<r->h;row++){
     uint16_t result=bgRefreshPair(layer,x,(r->y+row)&63);
     if(!result){bgFail(7);goto done;}
     full|=result>>1;
    }
   }
  }
#else
  for(uint16_t n=0;n<journal.count;n++){
   const WillowNativeBackgroundJournalEntry*e=journal.entries+n;
   uint16_t layer=e->key>>12,cell=e->key&4095;
   uint16_t x=layer?(cell>>3)&63:(cell>>4)&63;
   uint16_t y=layer?((cell&7)|((cell>>6)&56)):((cell&15)|((cell>>6)&48));
   if(bgSlot(bg.rings+layer,x,y)!=BG_INVALID){
    if(!bgPair(layer,x,y,((uint32_t)e->code<<16)|e->attribute)){bgFail(7);goto done;}
    full=1;
   }
  }
#endif
  for(uint16_t layer=0;layer<2;layer++){
   uint16_t extent=layer?255:127;
   uint16_t ox=((((uint16_t)(96+old[layer*2])>>3)-1)&extent),oy=((uint16_t)(16+old[layer*2+1])>>3)&extent;
   uint16_t nx=bgFirstX(layer),ny=bgFirstY(layer),rows=bgRows(layer);
   int16_t dx=(nx-ox)&extent;if(dx==extent)dx=-1;
   if(full||ny!=oy||(old[layer*2+1]&7)!=(next[layer*2+1]&7)||dx>1){
    recheck=1;
    for(uint16_t row=0;row<rows;row++){
     if(!bgCountRow(layer,row)||(!layer&&!bgResolveRow(layer,row))){bgFail(8);goto done;}
     if(layer&&bg.farBlank)for(uint16_t col=0;col<43;col++)bgSet(1,nx+col,ny+row,0);
    }
   }else if(dx){
    recheck=1;
    uint16_t enter=(nx+(dx>0?42:0))&extent;
    if(layer){if(!bgFarEnter(enter,ny,rows,leaving[1])){bgFail(9);goto done;}continue;}
#if defined(RENDER_BACKGROUND_SHARED_STREAM) && defined(RENDER_BACKGROUND_NEAR_STRIP)
    if(bgNearFixedColumn(enter,ny,rows)){
     if(!willowNativeBackgroundNearEnter(&backgroundNearStrip,bg.planes,bg.columns[0],bg.rowCounts[0],enter,ny,rows,leaving[0])){bgFail(10);goto done;}
     continue;
    }
#endif
#if defined(RENDER_BACKGROUND_NEAR_STRIP) && !defined(RENDER_BACKGROUND_SHARED_STREAM)
    if(!willowNativeBackgroundNearEnter(&backgroundNearStrip,bg.planes,bg.columns[0],bg.rowCounts[0],enter,ny,rows,leaving[0])){bgFail(10);goto done;}
#else
    const uint8_t*flags=(const uint8_t*)WILLOW_BG_CATALOG_LAYER2_FLAGS_ADDRESS;
    for(uint16_t row=0;row<rows;row++){
     uint16_t y=(ny+row)&extent,b=bgRef(layer,enter,y);
     if(b==BG_INVALID){bgFail(9);goto done;}
     bg.rowCounts[layer][y&31]+=(flags[b]!=(layer?2:1))-((leaving[layer]>>(y&31))&1);
     if(!layer&&!bgResolveCell(layer,enter,y)){bgFail(10);goto done;}
     if(layer&&bg.farBlank)bgSet(1,enter,y,0);
    }
#endif
   }
  }
#ifdef RENDER_BACKGROUND_SHARED_STREAM
  /* Any upload changes owner identity; rebuild near words from current keys
   * before binding the preparation, including cells outside a changed column. */
  if(bgFarRetry())if(!bgNearRetry()){bgFail(11);goto done;}
#endif
  /* Hidden far layers cost only the current conservative row proof. Any
   * exposed pixel takes the exact mask/ref path, including missing residency. */
  uint16_t blank=recheck?bgFarBlank():bg.farBlank;
  if(!blank){for(uint16_t row=0;row<bgRows(1);row++)if(!bgResolveRow(1,row)){bgFail(11);goto done;}}
  else if(!bg.farBlank)for(uint16_t row=0;row<bgRows(1);row++)for(uint16_t col=0;col<43;col++)bgSet(1,bgFirstX(1)+col,bgFirstY(1)+row,0);
  bg.farBlank=blank;
acknowledged:
#ifdef RENDER_BACKGROUND_COLUMNS
  if(!willowNativeBackgroundColumnsAcknowledge(w,&journal)){bgFail(12);goto done;}
#else
  if(!willowNativeBackgroundJournalAcknowledge(&w->journal,&journal)){bgFail(12);goto done;}
#endif
 }
prepared:
 bg.preparedEpoch=w->rawEpoch;
#ifdef RENDER_BACKGROUND_FAR_STREAM
 if(!bgFarFinish()){BG_PHASE(8,0);goto done;}
#endif
 bgDescribe(0);BARRIER();bg.ready=1;BG_PHASE(2,0);
done:
 BARRIER();bg.busy=0;
}
/* Dedicated post-source-IRQ publisher. It never modifies original source
 * maps, staged camera values, object geometry or palette ownership. */
void nativeBackgroundPublish(void)
{
#ifdef RENDER_BACKGROUND_FAR_STREAM
 if(bg.busy)return;
 if(backgroundFarCache.phase==WILLOW_FAR_CACHE_QUEUED){bgFarPublishPatterns();return;}
#endif
 if(bg.busy||!bg.ready||bg.mode<2)return;
 bgDescribe(1);WORD(0x20f5b4)=WORD(0x20f5b6)=WORD(0x20f5b8)=WORD(0x20f5ba)=0;
 WORD(0x20f5bc)=bg.mode<4;BG_PHASE(3,0);
 if(active()&&bg.mode<4){bgFail(23);return;}
 if(backgroundSourceWriter.rawBusy||backgroundSourceWriter.active||
    !bgIdentity()||!PALETTE_READY||!willowPresentationCurrent(&bg.lease)||
    bg.preparedEpoch!=backgroundSourceWriter.rawEpoch){bgFail(20);return;}
 for(uint16_t i=0;i<4;i++)if(WORD(PORT+0x110u+i*2)!=bg.view[i]){bgFail(21);return;}
#ifdef RENDER_BACKGROUND_FAR_STREAM
 if(!bgFarPublishCurrent()){bgFail(24);return;}
#endif
 WillowPresentationFrame frame={&bg.lease,bg.lease.generation,WILLOW_PRESENT_BG};
 for(uint16_t layer=0;layer<2;layer++){
  WillowNativePlaneSurface*s=bg.planes+layer;
  /* No plane field is consumed when its queue is empty. Resource/view
   * certification above still gates scroll and the retained display. */
  if(!s->dirty)continue;
  if(!willowNativeBackgroundPlaneAdmit(s,backgroundTargets+layer,&frame)||
     (bg.mode<4&&!willowNativePlaneSpansValid(s,s->dirty))||
     (bg.mode==4&&(s->publishedBase!=backgroundTargets[layer].base||
      (s->dirty&&!(bg.columns[layer][0]|bg.columns[layer][1]))))){bgFail(22);return;}
 }
 for(uint16_t layer=0;layer<2;layer++){
  WillowNativePlaneSurface*s=bg.planes+layer;
  if(!s->dirty)continue;
  WillowNativeBackgroundPlaneTransfer transfer;
  if(bg.mode<4)willowNativeBackgroundPlanePublishRows(s,backgroundTargets+layer,&frame,1,&transfer);
  else willowNativeBackgroundPlanePublishColumns(s,backgroundTargets+layer,&frame,bg.columns[layer],64,&transfer);
  if(!s->dirty)bg.columns[layer][0]=bg.columns[layer][1]=0;
  WORD(0x20f5b4u+layer*2)=transfer.words;
 }
 /* The retained dirty bits survive refused publication and are cleared only
  * after the corresponding physical scalar-scroll stores complete. */
 if(bg.scrollDirty&1){
  LONG(0xc00004)=willowNativePlaneCommand(0xf000);
  WORD(0xc00000)=-(96+bg.view[0]);WORD(0xc00000)=-(96+bg.view[2]);WORD(0x20f5b8)=2;
 }
 if(bg.scrollDirty&2){
  LONG(0xc00004)=0x40000010;
  WORD(0xc00000)=(16+bg.view[1])&255;WORD(0xc00000)=(16+bg.view[3])&255;WORD(0x20f5ba)=2;
 }
 bg.scrollDirty=0;
#ifdef RENDER_BACKGROUND_FAR_STREAM
 if(!bgFarPublished()){bgFail(24);return;}
#endif
 BG_PHASE(4,0);
}
