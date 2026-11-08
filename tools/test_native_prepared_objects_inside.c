/* Explicit complete-block/crop/wrap boundaries for the optional native loop. */
#define WILLOW_PREPARED_NATIVE_CELLS 1
#define main preparedRandomMain
#define nativeObjectPresentation preparedRandomNative
#include "test_native_prepared_objects.c"
#undef main
#undef nativeObjectPresentation
static uint8_t boundaryResolve(void*context,uint16_t code,uint16_t source,uint16_t*attribute)
{return context?WILLOW_OBJECT_ASSET_TRANSPARENT:resolve(0,code,source,attribute);}
static uint8_t boundaryAsset(void*context,uint16_t code,uint16_t source,int16_t x,int16_t y,uint16_t*attribute)
{(void)x;(void)y;return boundaryResolve(context,code,source,attribute);}
static int boundaryRun(void)
{
 static const uint16_t dimensions[5][2]={{1,1},{2,2},{4,7},{16,14},{16,16}};
 static const uint16_t crops[3][2]={{0,0},{96,16},{192,288}};
 unsigned comparisons=0,successes=0,statuses[6]={0};
 for(unsigned dimension=0;dimension<5;dimension++)for(unsigned local=0;local<4;local++)
 for(unsigned transparent=0;transparent<2;transparent++){
  const uint16_t width=dimensions[dimension][0]*16,height=dimensions[dimension][1]*16;
  const uint16_t source=(uint16_t)(((dimensions[dimension][0]-1)<<8)|((dimensions[dimension][1]-1)<<12)|(local<<5));
  if((source&0xff00)==0xff00)continue; /* source marker, not a 16x16-cell block */
  void*context=transparent?(void*)&a:0;
  if(willowPreparedObjectCompile(0x1af,source,boundaryResolve,context,cells[0],256,shapes))return failure(comparisons,31);
  for(unsigned crop=0;crop<3;crop++)for(unsigned flip=0;flip<2;flip++)for(unsigned location=0;location<16;location++){
   const int16_t xs[16]={-16,-15,-1,0,1,(int16_t)(320-width-1),(int16_t)(320-width),(int16_t)(321-width),319,320,511,512,0,0,0,0};
   const int16_t ys[16]={0,0,0,0,1,(int16_t)(224-height-1),(int16_t)(224-height),(int16_t)(225-height),223,224,511,512,-16,-15,-1,1};
   WillowPreparedObjectFrame frame={0};frame.instances=instances;frame.count=1;frame.sourceRecords=1;
   frame.epoch.latch=1;frame.epoch.resident=2;frame.epoch.palette=3;frame.epoch.view=4;
   frame.view.left=crops[crop][0];frame.view.top=crops[crop][1];frame.view.width=320;frame.view.height=224;frame.view.flipScreen=(uint8_t)flip;
   instances[0].shape=shapes;instances[0].sourceOrdinal=0;
   instances[0].sourceX=(uint16_t)(frame.view.left+xs[location]+0x8000);
   instances[0].sourceY=(uint16_t)(frame.view.top+ys[location]+0x8000);
   bank[0]=instances[0].sourceX;bank[1]=instances[0].sourceY;bank[2]=0x1af;bank[3]=source;bank[7]=0xff00;
   WillowPreparedObjectBinding binding;
   if(willowPreparedObjectBind(&binding,&frame))return failure(comparisons,32);
   WillowPreparedObjectSeed certificate;willowPreparedObjectSeedEmpty(&certificate,1);
   willowObjectSatBudgetInit(&a);b=a;
   for(unsigned n=0;n<320;n++)actual[n]=expected[n]=0x5a5a;
   const uint16_t capacity=(uint16_t)(location==1?0:location==2?1:80);
   WillowObjectSatResult ra=willowObjectSatBuildReaderDeferred(readBank,bank,2,&frame.view,&a,pieces,capacity,boundaryAsset,context,da);
   if(!ra.status)for(unsigned n=0;n<ra.pieces;n++)if(!willowObjectSatWords(pieces+n,n+1<ra.pieces?(uint8_t)(n+1):0,expected+n*4))return failure(comparisons,33);
   WillowObjectSatResult rb=willowPreparedObjectBuildSeeded(&binding,&frame.epoch,&certificate,1,&b,actual,capacity,db);
   if(ra.status!=rb.status)return failure(comparisons,34);
   if(ra.status==WILLOW_OBJECT_SAT_LINE||ra.status==WILLOW_OBJECT_SAT_PIXELS)if(!equalBytes(&a,&b,sizeof a))return failure(comparisons,35);
   if(!ra.status){
    if(ra.pieces!=rb.pieces||ra.records!=rb.records||!equalBytes(&a,&b,sizeof a)||!equalBytes(actual,expected,sizeof actual))return failure(comparisons,36);
    successes++;
   }
   statuses[ra.status]++;comparisons++;
  }
 }
#if defined(__m68k__)
 *(volatile uint32_t*)0xff0008=comparisons;*(volatile uint16_t*)0xff000c=0;
 *(volatile uint16_t*)0xff0010=(uint16_t)successes;
 for(unsigned n=0;n<6;n++)((volatile uint16_t*)0xff0012)[n]=(uint16_t)statuses[n];
 *(volatile uint16_t*)0xff001e=0;*(volatile uint16_t*)0xff0020=(uint16_t)comparisons;
#else
 printf("{\"comparisons\":%u,\"certified_seed_comparisons\":0,\"empty_seed_comparisons\":%u,\"successes_exact_sat_budgets\":%u,\"status_counts\":[%u,%u,%u,%u,%u,%u],\"explicit_boundary_suite\":true,\"all_transparent_templates\":true,\"source_high_bits\":true}\n",comparisons,comparisons,successes,statuses[0],statuses[1],statuses[2],statuses[3],statuses[4],statuses[5]);
#endif
 return 1;
}
#ifdef __m68k__
uint16_t nativeObjectPresentation(void){return (uint16_t)boundaryRun();}
#else
int main(void){return boundaryRun()?0:1;}
#endif
