#include "willow_native_object_grouped.h"
#include "willow_native_object_prepared.h"
#include "grouped_fixture.h"
#ifdef __m68k__
void*memcpy(void*to,const void*from,size_t n){uint8_t*d=to;const uint8_t*s=from;while(n--)*d++=*s++;return to;}
#else
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#endif
static WillowGroupedObjectInstance instances[3];
static WillowGroupedObjectFrame frame;
static WillowGroupedObjectBinding binding;
static struct {uint16_t before;WillowObjectSatBudget value;uint16_t after;} guardedBudget;
static uint16_t satGuard[324],expectedSat[320];
static uint8_t dsGuard[229],duGuard[229];
static WillowObjectSatBudget seed,referenceBudget;
static uint32_t geometryCases,wrapCases,admissionCases,epochCases;
#ifndef __m68k__
static uint32_t pixelCases,pixelsChecked,budgetDifferences;
#endif
static uint16_t failureField;static uint32_t failureCase;
static uint8_t fail(uint16_t field,uint32_t n){failureField=field;failureCase=n;return 0;}
static void bytes(void*p,uint8_t value,unsigned size){uint8_t*q=p;while(size--)*q++=value;}
static uint8_t equal(const void*a,const void*b,unsigned n){const uint8_t*x=a,*y=b;while(n--)if(*x++!=*y++)return 0;return 1;}
static uint8_t seedValidate(const WillowObjectSatBudget*b){
 if(b->count>80)return WILLOW_OBJECT_SAT_TOTAL;
 for(unsigned y=0;y<224;y++){if(b->sprites[y]>20)return WILLOW_OBJECT_SAT_LINE;if(b->pixels[y]>320)return WILLOW_OBJECT_SAT_PIXELS;}
 return 0;
}
/* Independent rectangular reservation oracle. It accumulates full per-line
 * arrays directly; it does not use the consumer's packed endpoint streams. */
static uint8_t oracle(uint16_t capacity,uint16_t*count){
 referenceBudget=seed;*count=0;uint8_t status=seedValidate(&seed);if(status)return status;
 for(uint16_t n=0;n<frame.count;n++){
  const WillowGroupedObjectInstance*i=frame.instances+n;const WillowGroupedObjectTemplate*t=i->shape;
  for(unsigned c=0;c<t->count;c++){
   unsigned size=t->cells[c].offset&15,width=(((size>>2)&3)+1)*8,height=((size&3)+1)*8;
   int x=(i->sourceX+(t->cells[c].offset&0xf0))%512,y=(i->sourceY+(t->cells[c].offset>>8))%512;
   if(x+(int)width>512||y+(int)height>512)return WILLOW_OBJECT_GROUPED_WRAP;
   if(frame.view.flipScreen){x=512-(int)width-x;y=256-(int)height-y;}
   x-=frame.view.left;y-=frame.view.top;
   if(x+(int)width<=0||x>=320||y+(int)height<=0||y>=224)continue;
   if(*count>=capacity||referenceBudget.count>=80)return WILLOW_OBJECT_SAT_TOTAL;
   uint16_t*dst=expectedSat+*count*4;
   dst[0]=(uint16_t)(y+128);dst[1]=(size<<8)|++referenceBudget.count;
   dst[2]=t->cells[c].attribute^(frame.view.flipScreen?0x1800:0);dst[3]=(uint16_t)(x+128);(*count)++;
   for(int line=y<0?0:y;line<y+(int)height&&line<224;line++){referenceBudget.sprites[line]++;referenceBudget.pixels[line]+=width;}
  }
 }
 if(*count)expectedSat[*count*4-3]&=0x0f00;
 return seedValidate(&referenceBudget);
}
static uint8_t buildCheck(uint16_t capacity,uint8_t seedKind){
 uint32_t n=geometryCases+admissionCases;uint16_t wantCount;uint8_t expected=oracle(capacity,&wantCount);
 bytes(satGuard,0xa5,sizeof satGuard);bytes(dsGuard,0x5a,sizeof dsGuard);bytes(duGuard,0x5a,sizeof duGuard);
 guardedBudget.before=0xa55a;guardedBudget.after=0x5aa5;guardedBudget.value=seed;
 unsigned offset=(n&1)?1:2;uint8_t*ds=dsGuard+offset,*du=duGuard+3-offset;
 WillowObjectSatResult result;
 if(seedKind){
  WillowGroupedObjectSeed descriptor;
  if(seedKind==1)willowGroupedObjectSeedEmpty(&descriptor,71);
  else {if(willowGroupedObjectSeedBind(&descriptor,&seed,71)!=expected&&seedValidate(&seed))return fail(1,n);}
  if(seedValidate(&seed))return 1;
  result=willowGroupedObjectBuildSeeded(&binding,&frame.epoch,&descriptor,71,&guardedBudget.value,satGuard+2,capacity,ds,du);
 }else result=willowGroupedObjectBuild(&binding,&frame.epoch,&guardedBudget.value,satGuard+2,capacity,ds,du);
 if(result.status!=expected)return fail(2+(result.status<<4)+(expected<<8),n);
 if(result.pieces!=wantCount)return fail(3,n);
 if(!expected){
  if(!equal(&guardedBudget.value,&referenceBudget,sizeof referenceBudget)||!equal(satGuard+2,expectedSat,wantCount*8))return fail(4,n);
  if(result.records!=frame.sourceRecords)return fail(5,n);
 }
 if(expected==WILLOW_OBJECT_GROUPED_WRAP)wrapCases++;
 if(guardedBudget.before!=0xa55a||guardedBudget.after!=0x5aa5)return fail(6,n);
 if(satGuard[0]!=0xa5a5||satGuard[1]!=0xa5a5)return fail(7,n);
 for(unsigned j=2+capacity*4;j<324;j++)if(satGuard[j]!=0xa5a5)return fail(8,n);
 for(unsigned j=0;j<229;j++){
  if((j<offset||j>=offset+225)&&dsGuard[j]!=0x5a)return fail(9,n);
  if((j<3-offset||j>=3-offset+225)&&duGuard[j]!=0x5a)return fail(10,n);
 }
 return 1;
}
#ifndef __m68k__
static uint8_t originalVram[65536],groupedVram[65536];
typedef struct {uint32_t owner;uint16_t color;} Pixel;
static Pixel originalImage[71680],groupedImage[71680];
static WillowPreparedObjectInstance originalInstances[3];
static uint16_t originalSat[320];static WillowObjectSatBudget originalBudget;static int8_t originalDelta[225];
static uint16_t visibleInstance[80],visibleCell[80];
static uint8_t pen(const uint8_t*vram,uint16_t attr,unsigned w,unsigned h,unsigned x,unsigned y){
 if(attr&0x800)x=w-1-x;if(attr&0x1000)y=h-1-y;
 unsigned tile=(attr&2047)+(x/8)*(h/8)+y/8,at=tile*32+(y%8)*4+(x%8)/2;
 return (vram[at]>>(x&1?0:4))&15;
}
static uint8_t pixelProof(void){
 WillowPreparedObjectFrame f={originalInstances,frame.count,frame.sourceRecords,{1,1,1,1},frame.view};
 for(uint16_t n=0;n<frame.count;n++){
  unsigned templateId=(unsigned)(instances[n].shape-groupedTemplates);
  originalInstances[n]=(WillowPreparedObjectInstance){originalTemplates+templateId,instances[n].sourceX,instances[n].sourceY,instances[n].sourceOrdinal};
 }
 WillowPreparedObjectBinding b;if(willowPreparedObjectBind(&b,&f))return fail(20,pixelCases);
 willowObjectSatBudgetInit(&originalBudget);
 WillowObjectSatResult r=willowPreparedObjectBuild(&b,&f.epoch,&originalBudget,originalSat,80,originalDelta);
 if(r.status)return fail(21+(r.status<<8),pixelCases);
 bytes(originalImage,0,sizeof originalImage);bytes(groupedImage,0,sizeof groupedImage);
 unsigned used=0;
 for(uint16_t n=0;n<frame.count;n++){
  const WillowPreparedObjectTemplate*t=originalInstances[n].shape;
  for(unsigned c=0;c<t->count;c++){
   int x=(instances[n].sourceX+(t->cells[c].offset&255))&511,y=(instances[n].sourceY+(t->cells[c].offset>>8))&511;
   if(frame.view.flipScreen){x=496-x;y=240-y;}x-=frame.view.left;y-=frame.view.top;
   if(x+16<=0||x>=320||y+16<=0||y>=224)continue;
   if(used>=r.pieces)return fail(22,pixelCases);visibleInstance[used]=n;visibleCell[used++]=c;
  }
 }
 if(used!=r.pieces)return fail(23,pixelCases);
 while(used){unsigned entry=--used,n=visibleInstance[entry],c=visibleCell[entry];uint16_t*s=originalSat+entry*4;int x=(int16_t)s[3]-128,y=(int16_t)s[0]-128;
  for(unsigned py=0;py<16;py++)for(unsigned px=0;px<16;px++){
   int dx=x+px,dy=y+py;if(dx<0||dx>=320||dy<0||dy>=224)continue;uint8_t p=pen(originalVram,s[2],16,16,px,py);if(!p)continue;
   unsigned ox=frame.view.flipScreen?15-px:px,oy=frame.view.flipScreen?15-py:py;
   originalImage[dy*320+dx]=(Pixel){((instances[n].sourceOrdinal+1u)<<24)|(c<<8)|(oy<<4)|ox,(s[2]&0xe000)|p};
  }
 }
 for(uint16_t n=0;n<frame.count;n++){
  const WillowGroupedObjectTemplate*t=instances[n].shape;
  for(unsigned c=0;c<t->count;c++){
   unsigned size=t->cells[c].offset&15,w=(((size>>2)&3)+1)*8,h=((size&3)+1)*8;
   int x=(instances[n].sourceX+(t->cells[c].offset&240))&511,y=(instances[n].sourceY+(t->cells[c].offset>>8))&511;
   if(frame.view.flipScreen){x=512-(int)w-x;y=256-(int)h-y;}x-=frame.view.left;y-=frame.view.top;
   if(x+(int)w<=0||x>=320||y+(int)h<=0||y>=224)continue;
   visibleInstance[used]=n;visibleCell[used++]=c;
  }
 }
 if(used!=guardedBudget.value.count)return fail(24,pixelCases);
 while(used){unsigned entry=--used,n=visibleInstance[entry],c=visibleCell[entry];uint16_t*s=satGuard+2+entry*4;
  const WillowGroupedObjectTemplate*t=instances[n].shape;unsigned ti=(unsigned)(t-groupedTemplates);
  unsigned size=(s[1]>>8)&15,w=(((size>>2)&3)+1)*8,h=((size&3)+1)*8;
  int x=(int16_t)s[3]-128,y=(int16_t)s[0]-128;
  for(unsigned py=0;py<h;py++)for(unsigned px=0;px<w;px++){
   int dx=x+px,dy=y+py;if(dx<0||dx>=320||dy<0||dy>=224)continue;uint8_t p=pen(groupedVram,s[2],w,h,px,py);if(!p)continue;
   unsigned ox=frame.view.flipScreen?w-1-px:px,oy=frame.view.flipScreen?h-1-py:py;
   unsigned sx=(t->cells[c].offset&240)+ox,sy=(t->cells[c].offset>>8)+oy,owner=65535;
   for(unsigned child=0;child<originalTemplates[ti].count;child++)if(memberMasks[ti][c]&(1u<<child)){
    unsigned cx=originalTemplates[ti].cells[child].offset&255,cy=originalTemplates[ti].cells[child].offset>>8;
    if(sx>=cx&&sx<cx+16&&sy>=cy&&sy<cy+16){owner=child;ox=sx-cx;oy=sy-cy;break;}
   }
   if(owner==65535)return fail(25,pixelCases);
   groupedImage[dy*320+dx]=(Pixel){((instances[n].sourceOrdinal+1u)<<24)|(owner<<8)|(oy<<4)|ox,(s[2]&0xe000)|p};
  }
 }
 for(unsigned p=0;p<71680;p++)if(originalImage[p].owner!=groupedImage[p].owner||originalImage[p].color!=groupedImage[p].color){fprintf(stderr,"Pixel mismatch case%u position%u flip%u\n",pixelCases,p,frame.view.flipScreen);return fail(26,pixelCases);}
 if(!equal(&originalBudget,&guardedBudget.value,sizeof originalBudget))budgetDifferences++;
 pixelCases++;pixelsChecked+=71680;return 1;
}
#endif
static void setup(uint16_t count){
 frame=(WillowGroupedObjectFrame){instances,count,count,{1,2,3,4},{96,16,320,224,0}};
 for(uint16_t n=0;n<count;n++)instances[n]=(WillowGroupedObjectInstance){groupedTemplates+n,fixtureOrigins[n][0],fixtureOrigins[n][1],n};
 willowObjectSatBudgetInit(&seed);
}
static uint8_t epochs(void){
 setup(3);if(willowGroupedObjectBind(&binding,&frame))return fail(30,0);
 WillowGroupedObjectEpoch e=frame.epoch;uint32_t*word[]={&e.latch,&e.resident,&e.palette,&e.view};
 for(uint16_t n=0;n<4;n++){(*word[n])++;WillowObjectSatResult r=willowGroupedObjectBuild(&binding,&e,&guardedBudget.value,satGuard+2,80,dsGuard+1,duGuard+1);if(r.status!=1)return fail(31,n);(*word[n])--;epochCases++;}
 if(!willowGroupedObjectLatchBegin(&binding)||willowGroupedObjectLatchSeal(&binding,2))return fail(32,0);
 if(willowGroupedObjectBuild(&binding,&frame.epoch,&guardedBudget.value,satGuard+2,80,dsGuard+1,duGuard+1).status!=1)return fail(33,0);
 if(willowGroupedObjectLatchOrigin(&binding,1,0,0,instances[0].shape->sourceCode,instances[0].shape->sourceAttribute))return fail(34,0);
 for(uint16_t n=0;n<3;n++)if(!willowGroupedObjectLatchOrigin(&binding,n,instances[n].sourceX,instances[n].sourceY,instances[n].shape->sourceCode,instances[n].shape->sourceAttribute))return fail(35,n);
 if(willowGroupedObjectLatchSeal(&binding,1)||!willowGroupedObjectLatchSeal(&binding,2))return fail(36,0);
 if(!buildCheck(80,1))return 0;
 WillowGroupedObjectSeed descriptor;willowGroupedObjectSeedEmpty(&descriptor,3);
 if(willowGroupedObjectBuildSeeded(&binding,&frame.epoch,&descriptor,4,&guardedBudget.value,satGuard+2,80,dsGuard+1,duGuard+1).status!=1)return fail(37,0);
 WillowGroupedObjectTemplate t=*instances[0].shape;WillowGroupedObjectCell c=t.cells[0];t.cells=&c;t.count=1;instances[0].shape=&t;
 uint16_t good=c.offset;
 for(uint16_t n=0;n<16;n++){c.offset=(good&0xfff0)|n;uint8_t valid=(n==5||n==7||n==13||n==15);if(!valid&&willowGroupedObjectBind(&binding,&frame)!=1)return fail(38,n);epochCases++;}
 c.offset=good;c.attribute|=0x0800;if(willowGroupedObjectBind(&binding,&frame)!=2)return fail(39,0);
 c.attribute=2047;if(willowGroupedObjectBind(&binding,&frame)!=2)return fail(40,0);
 epochCases+=8;return 1;
}
/* The resource producer may leave a transparent16px half in a32px group.
 * Runtime still charges all32px. Conversely two opaque children consume two
 * native16 line slots while their grouped32px equivalent consumes one. */
static uint8_t differingAdmission(void){
 static const WillowGroupedObjectCell grouped[]={ {13,200} };
 static const WillowGroupedObjectTemplate gt={grouped,1,1,0x0100};
 static const WillowPreparedObjectCell single[]={ {16,100} },pair[]={ {0,100},{16,104} };
 WillowPreparedObjectTemplate ot={single,1,1,0x0100};
 WillowPreparedObjectInstance oi={&ot,200,152,0};
 WillowPreparedObjectFrame of={&oi,1,1,{1,2,3,4},{96,16,320,224,0}};
 WillowPreparedObjectBinding ob;WillowObjectSatBudget other;uint16_t osat[8];int8_t delta[225];
 setup(1);instances[0].shape=&gt;if(willowGroupedObjectBind(&binding,&frame))return fail(46,0);
 seed.count=1;for(unsigned y=0;y<224;y++){seed.sprites[y]=1;seed.pixels[y]=304;}
 if(!buildCheck(80,0))return 0;
 other=seed;if(willowPreparedObjectBind(&ob,&of)||willowPreparedObjectBuild(&ob,&of.epoch,&other,osat,2,delta).status)return fail(47,0);
 uint16_t count;if(oracle(80,&count)!=WILLOW_OBJECT_SAT_PIXELS)return fail(48,0);
 admissionCases++;
 ot.cells=pair;ot.count=2;seed.count=19;
 for(unsigned y=0;y<224;y++){seed.sprites[y]=19;seed.pixels[y]=152;}
 if(!buildCheck(80,0))return 0;
 other=seed;if(willowPreparedObjectBind(&ob,&of)||willowPreparedObjectBuild(&ob,&of.epoch,&other,osat,2,delta).status!=WILLOW_OBJECT_SAT_LINE)return fail(49,0);
 admissionCases++;return 1;
}
static uint8_t run(void){
 geometryCases=wrapCases=admissionCases=epochCases=0;
 static const uint16_t xs[]={0,1,15,16,64,65,95,96,97,100,319,320,383,400,415,416,431,432,447,448,463,464,479,480,495,496,497,500,511,512,65535};
 static const uint16_t ys[]={0,1,15,16,17,31,32,80,152,208,223,224,239,240,241,255,256,288,480,495,496,497,511,65535};
 for(unsigned ti=0;ti<3;ti++)for(unsigned flip=0;flip<2;flip++)for(unsigned x=0;x<sizeof xs/sizeof*xs;x++)for(unsigned y=0;y<sizeof ys/sizeof*ys;y++){
  setup(1);instances[0].shape=groupedTemplates+ti;instances[0].sourceX=xs[x];instances[0].sourceY=ys[y];frame.view.flipScreen=flip;
  if(willowGroupedObjectBind(&binding,&frame))return fail(41,geometryCases);
  if(!buildCheck(80,((uint16_t)geometryCases)%3))return 0;
  geometryCases++;
#ifndef __m68k__
  uint16_t count;if(!oracle(80,&count)&&!pixelProof())return 0;
#endif
 }
 for(unsigned flip=0;flip<2;flip++)for(uint16_t n=0;n<128;n++){
  setup(3);frame.view.flipScreen=flip;frame.view.left=(n%3)*96;frame.view.top=(n%2)*16;
  for(unsigned i=0;i<3;i++){instances[i].sourceX=(instances[i].sourceX+n*13-320)&511;instances[i].sourceY=(instances[i].sourceY+n*7-96)&511;}
  if(willowGroupedObjectBind(&binding,&frame)||!buildCheck(80,0))return fail(42,geometryCases);
  geometryCases++;
#ifndef __m68k__
  uint16_t count;if(!oracle(80,&count)&&!pixelProof())return 0;
#endif
 }
 for(uint16_t n=0;n<4096;n++){
  setup(3);frame.view.flipScreen=n&1;if(willowGroupedObjectBind(&binding,&frame))return fail(43,n);
  seed.count=(n%5==0)?79:(n%5==1)?80:(n%5==2)?81:0;
  for(unsigned y=0;y<224;y++){seed.sprites[y]=(n>>4)%22;seed.pixels[y]=(n&32)?304:(n&64)?320:(n&128)?321:0;}
  if(!buildCheck((n>>2)%81,(n&256)&&!seedValidate(&seed)?2:0))return 0;
  admissionCases++;
 }
 /* 80 wide groups share one endpoint:160 units must not sign-wrap to -96. */
 static WillowGroupedObjectCell many[80];static WillowGroupedObjectTemplate manyTemplate;
 setup(1);for(uint16_t n=0;n<80;n++)many[n]=(WillowGroupedObjectCell){13,100};manyTemplate=(WillowGroupedObjectTemplate){many,80,1,0x0100};instances[0].shape=&manyTemplate;
 if(willowGroupedObjectBind(&binding,&frame)||!buildCheck(80,0))return fail(44,0);
  admissionCases++;
 unsigned offset=((geometryCases+admissionCases-1)&1)?1:2;
 if(dsGuard[offset+136]!=80||dsGuard[offset+152]!=176||duGuard[3-offset+136]!=160||duGuard[3-offset+152]!=96)return fail(45,0);
 return epochs()&&differingAdmission();
}
#ifdef __m68k__
uint16_t nativeObjectPresentation(void){
 uint8_t ok=run();*(volatile uint32_t*)0xff0008=ok?geometryCases+admissionCases:failureCase;*(volatile uint16_t*)0xff000c=ok?0:failureField;return ok;
}
#else
int main(int argc,char**argv){
 if(argc!=3)return 2;FILE*f=fopen(argv[1],"rb");if(!f||fread(originalVram,1,65536,f)!=65536)return 2;fclose(f);f=fopen(argv[2],"rb");if(!f||fread(groupedVram,1,65536,f)!=65536)return 2;fclose(f);
 if(!run()){fprintf(stderr,"FAIL field%u case%u\n",failureField,failureCase);return 1;}
 printf("PASS grouped consumer: %u geometry cases, %u explicit source-wrap failures, %u pixel/owner comparisons (%u pixels), %u budget/capacity cases, %u epoch/metadata checks. %u pixel-identical cases have different native16 versus grouped budgets.\n",geometryCases,wrapCases,pixelCases,pixelsChecked,admissionCases,epochCases,budgetDifferences);
 f=fopen("host-result.json","w");if(!f)return 2;fprintf(f,"{\"geometry_cases\":%u,\"wrap_failures\":%u,\"pixel_provenance_cases\":%u,\"pixels_compared\":%u,\"budget_capacity_cases\":%u,\"epoch_metadata_checks\":%u,\"different_native16_budget_cases\":%u,\"shipping_integration\":false}\n",geometryCases,wrapCases,pixelCases,pixelsChecked,admissionCases,epochCases,budgetDifferences);fclose(f);return 0;
}
#endif
