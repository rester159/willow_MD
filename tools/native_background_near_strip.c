/* Isolated native68000 candidate and the existing coordinate-per-cell loop.
 * No game behavior or VDP access. Diagnostic owner addresses only. */
#include "willow_native_background_near_strip.h"
#include "render_background_imports.h"
#include "near_columns_imports.h"
extern WillowNativePlaneSurface stripSurface;
extern WillowNativeBackgroundKeyCache stripKeys;
extern WillowNativeBackgroundRing stripRing;
extern volatile uint32_t stripEpoch;
extern WillowPresentationLease stripLease;
extern uint32_t stripColumns[2];
extern uint8_t stripCounts[32];
const WillowPresentationPlan stripPlan={0,0,0,WILLOW_PRESENT_BG};
const WillowNativeBackgroundNearStrip stripView={&stripKeys,&stripRing,
 (const uint16_t*)WILLOW_BG_CATALOG_LAYER2_REFS_ADDRESS,
 (const uint8_t*)WILLOW_BG_CATALOG_LAYER2_FLAGS_ADDRESS,
 WILLOW_BG_CATALOG_LAYER2_PAIR_COUNT,(const uint8_t*)WILLOW_BG_NEAR_COLUMNS_ADDRESS};
#define W(at) (*(volatile uint16_t*)(at))
#define L(at) (*(volatile uint32_t*)(at))
static inline __attribute__((always_inline)) uint16_t originalRef(uint16_t x,uint16_t y)
{
 const WillowNativeBackgroundRing*r=&stripRing;
 uint16_t dx=((x>>1)-r->x)&63,dy=((y>>1)-r->y)&63;
 if(dx>=r->w||dy>=r->h)return 0xffff;
 dx+=r->hx;if(dx>=r->w)dx-=r->w;
 dy+=r->hy;if(dy>=r->h)dy-=r->h;
 uint16_t slot=dy*r->w+dx;
 uint16_t id=stripKeys.nearLow[slot]|((uint16_t)((stripKeys.nearHigh[slot>>2]>>((slot&3)*2))&3)<<8);
 if(id>=WILLOW_BG_CATALOG_LAYER2_PAIR_COUNT)return 0xffff;
 return (id<<2)+((y&1)<<1)+(x&1);
}
static __attribute__((noinline)) void originalSet(uint16_t x,uint16_t y,uint16_t word)
{
 WillowNativePlaneSurface*s=&stripSurface;x&=63;y&=31;
 uint16_t*p=s->words+y*64+x;if(*p==word)return;*p=word;
 uint32_t bit=1UL<<y;
 if(!(s->dirty&bit)){s->first[y]=s->last[y]=x;s->dirty|=bit;}
 else{if(x<s->first[y])s->first[y]=x;if(x>s->last[y])s->last[y]=x;}
 stripColumns[x>>5]|=1UL<<(x&31);
}
static __attribute__((noinline)) uint16_t originalResolve(uint16_t x,uint16_t y)
{
 uint16_t ref=originalRef(x,y);if(ref==0xffff)return 0;
 uint16_t word=stripView.references[ref*2+1];if(word==0xffff)return 0;
 originalSet(x,y,word);return 1;
}
void stripCaptureOriginal(void)
{
 uint16_t x=W(0x20f000),y=W(0x20f002),rows=W(0x20f004);uint32_t flags=0;
 for(uint16_t row=0;row<rows;row++){
  uint16_t py=(y+row)&127,ref=originalRef(x,py);if(ref==0xffff){W(0x20f010)=0;return;}
  if(stripView.flags[ref]!=1)flags|=1UL<<(py&31);
 }
 L(0x20f012)=flags;W(0x20f010)=1;
}
void stripCaptureFast(void)
{
 uint32_t flags=0;uint8_t ok=willowNativeBackgroundNearCaptureFlags(&stripView,W(0x20f000),W(0x20f002),W(0x20f004),&flags);
 if(ok)L(0x20f012)=flags;
 W(0x20f010)=ok;
}
void stripEnterOriginal(void)
{
 uint16_t x=W(0x20f000),y=W(0x20f002),rows=W(0x20f004);uint32_t leaving=L(0x20f006);
 uint8_t*counts=stripCounts+(W(0x20f00a)&1);
 for(uint16_t row=0;row<rows;row++){
  uint16_t py=(y+row)&127,ref=originalRef(x,py);if(ref==0xffff){W(0x20f010)=0;return;}
  counts[py&31]+=(stripView.flags[ref]!=1)-((leaving>>(py&31))&1);
  if(!originalResolve(x,py)){W(0x20f010)=0;return;}
 }
 W(0x20f010)=1;
}
void stripEnterFast(void)
{
 W(0x20f010)=willowNativeBackgroundNearEnter(&stripView,&stripSurface,stripColumns,stripCounts+(W(0x20f00a)&1),
 W(0x20f000),W(0x20f002),W(0x20f004),L(0x20f006));
}
