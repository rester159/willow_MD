#include "../src/willow_native_object_catalog.h"
#include CATALOG_TRACE_HEADER
#ifndef __m68k__
#include <stdio.h>
#else
void*memcpy(void*to,const void*from,unsigned count){uint8_t*d=to;const uint8_t*s=from;while(count--)*d++=*s++;return to;}
void*memset(void*to,int value,unsigned count){uint8_t*d=to;while(count--)*d++=(uint8_t)value;return to;}
#endif
static WillowPreparedObjectSelection selected[2][256],coldItems[256],streamItems[2][256];
static WillowPreparedObjectStream stream;
static WillowPreparedObjectSelectionPacket packets[2],cold;
static WillowPreparedObjectInstance instances[256];
static const WillowPreparedObjectTemplate*unresolved[TRACE_KEY_COUNT];
static uint16_t mutableRecord[4],wideRecords[256][4];
static uint8_t readTrace(void*context,uint16_t n,uint16_t*out){
 const TraceFrame*f=context;if(n>=f->count)return 0;
 const uint16_t*record=traceRecords[f->offset+n];for(unsigned k=0;k<4;k++)out[k]=record[k];return 1;
}
static uint8_t readMutable(void*context,uint16_t n,uint16_t*out){(void)context;if(n)return 0;for(unsigned k=0;k<4;k++)out[k]=mutableRecord[k];return 1;}
static int fail(unsigned test,unsigned field){
#ifdef __m68k__
 *(volatile uint32_t*)0xff0008=test;*(volatile uint16_t*)0xff000c=field;
#else
 fprintf(stderr,"catalog test%u field%u\n",test,field);
#endif
 return 0;
}
static void mark(unsigned phase){
#ifdef __m68k__
 *(volatile uint16_t*)0xff0004=(uint16_t)phase;
#else
 (void)phase;
#endif
}
static uint8_t selectTrace(const WillowPreparedObjectKeyCatalog*c,const WillowPreparedObjectSourceInput*s,
 const WillowPreparedObjectSelectionPacket*p,WillowPreparedObjectSelectionPacket*out,
 WillowPreparedObjectSelection*items,uint16_t capacity)
{
#ifdef CATALOG_DIRECT_WORDS
 const TraceFrame*f=s->context;
 return willowPreparedObjectSelectWords(c,s,traceRecords[f->offset],p,out,items,capacity);
#else
 return willowPreparedObjectSelect(c,s,p,out,items,capacity);
#endif
}
static int run(void){
 WillowPreparedObjectKeyCatalog catalog;
 if(willowPreparedObjectKeyCatalogBind(&catalog,traceKeys,TRACE_KEY_COUNT,72))return fail(0,1);
 if(willowPreparedObjectStreamBind(&stream,&catalog,streamItems[0],streamItems[1],256))return fail(0,30);
 const WillowPreparedObjectSelectionPacket*previous=0;
 unsigned records=0,transitions=0,adversarial=0;
 for(unsigned n=0;n<TRACE_FRAME_COUNT;n++){
  const TraceFrame*f=traceFrames+n;
  WillowPreparedObjectSourceInput source={readTrace,(void*)f,f->epoch,f->count,256,f->bank,1};
  mark(1);uint8_t status=selectTrace(&catalog,&source,0,&cold,coldItems,256);mark(2);
  if(status)return fail(n,2);
  const WillowPreparedObjectSelectionPacket*out;
#ifdef CATALOG_STREAM
  mark(3);status=willowPreparedObjectStreamSelect(&stream,&source,traceRecords[f->offset],&out);mark(4);
#else
  WillowPreparedObjectSelectionPacket*mutableOut=packets+(n&1);
  mark(3);status=selectTrace(&catalog,&source,previous,mutableOut,selected[n&1],256);mark(4);out=mutableOut;
#endif
  if(status||!out->ready||out->count!=f->count||out->bank!=f->bank||out->latchEpoch!=f->epoch||out->sourceRecords!=256)return fail(n,3);
  uint8_t topologyChanged=previous&&previous->count!=out->count;
  for(unsigned k=0;k<f->count;k++){
   const uint16_t*r=traceRecords[f->offset+k];
   const WillowPreparedObjectSelection*s=out->items+k,*c=cold.items+k;
   uint16_t expected=WILLOW_PREPARED_OBJECT_IGNORED;
   for(unsigned id=0;id<TRACE_KEY_COUNT;id++)if(traceKeys[id].value==willowPreparedObjectKey(r[2],r[3]))expected=id;
   if(s->templateId!=expected||s->sourceX!=r[0]||s->sourceY!=r[1]||s->sourceOrdinal!=k||
      c->templateId!=s->templateId||c->sourceX!=s->sourceX||c->sourceY!=s->sourceY||c->sourceOrdinal!=s->sourceOrdinal)return fail(n,4);
   if(previous&&(k>=previous->count||previous->items[k].templateId!=s->templateId))topologyChanged=1;
   records++;
  }
  transitions+=topologyChanged;previous=out;
 }
 if(transitions!=TRACE_TOPOLOGY_CHANGES)return fail(0,5);
 /* Actual source keys remain unbound until matching palette/resident/view data
  * is admitted. This test never supplies invented colors or patterns. */
 WillowPreparedObjectResolvedCatalog resolved;
 WillowPreparedObjectEpoch epoch={previous->latchEpoch,77,78,79};
 WillowObjectSatView view={96,16,320,224,0};
 if(willowPreparedObjectResolvedCatalogBind(&resolved,&catalog,unresolved,&epoch,&view))return fail(0,6);
 WillowPreparedObjectFrame frame;WillowPreparedObjectBinding binding;
 if(willowPreparedObjectPublishSelection(&resolved,previous,&epoch,instances,256,&frame,&binding)!=WILLOW_OBJECT_SAT_ASSET||binding.ready||binding.frame)return fail(0,7);
 adversarial++;
 /* Stream failure never publishes its partial staging packet. */
 const WillowPreparedObjectSelectionPacket*streamResult=0;
 WillowPreparedObjectSourceInput streamBad={readTrace,(void*)traceFrames,traceFrames[0].epoch,traceFrames[0].count,256,traceFrames[0].bank,0};
 if(willowPreparedObjectStreamSelect(&stream,&streamBad,traceRecords[0],&streamResult)!=WILLOW_OBJECT_SAT_INVALID||streamResult)return fail(0,31);
 adversarial++;
#ifdef CATALOG_STREAM
 streamBad.metadataValid=1;
 if(willowPreparedObjectStreamSelect(&stream,&streamBad,traceRecords[0],&streamResult)!=WILLOW_OBJECT_SAT_INVALID||streamResult)return fail(0,32);
 adversarial++;
#endif
 /* Bad source/cached-state guards. */
 WillowPreparedObjectSourceInput good={readTrace,(void*)traceFrames,traceFrames[0].epoch,traceFrames[0].count,256,traceFrames[0].bank,1};
 for(unsigned test=0;test<6;test++){
  WillowPreparedObjectSourceInput bad=good;
  if(test==0)bad.metadataValid=0;
  if(test==1)bad.bank=2;
  if(test==2)bad.emittedRecords=257;
  if(test==3)bad.sourceRecords=0;
  if(test==4)bad.latchEpoch=0;
  if(test==5)bad.read=0;
  if(willowPreparedObjectSelect(&catalog,&bad,0,&cold,coldItems,256)!=WILLOW_OBJECT_SAT_INVALID||cold.ready)return fail(test,8);
  adversarial++;
 }
 if(willowPreparedObjectSelect(&catalog,&good,0,&cold,coldItems,0)!=WILLOW_OBJECT_SAT_TOTAL||cold.ready)return fail(0,9);
 adversarial++;
 if(willowPreparedObjectSelect(&catalog,&good,0,&cold,coldItems,257)!=WILLOW_OBJECT_SAT_INVALID||cold.ready)return fail(0,10);
 adversarial++;
 if(willowPreparedObjectSelect(&catalog,&good,0,&packets[0],selected[0],256))return fail(0,11);
 for(unsigned test=0;test<7;test++){
  WillowPreparedObjectSelectionPacket bad=packets[0];WillowPreparedObjectSelection saved=bad.items[0];
  if(test==0)bad.ready=0;
  if(test==1)bad.catalogEpoch++;
  if(test==2)bad.latchEpoch=good.latchEpoch+1;
  if(test==3)bad.bank=2;
  if(test==4)bad.count=257;
  if(test==5)bad.items[0].sourceOrdinal=1;
  if(test==6)bad.items[0].templateId=TRACE_KEY_COUNT;
  uint8_t status=willowPreparedObjectSelect(&catalog,&good,&bad,&cold,coldItems,256);
  bad.items[0]=saved;
  if(status!=WILLOW_OBJECT_SAT_INVALID||cold.ready)return fail(test,12);
  adversarial++;
 }
 WillowPreparedObjectKey badKeys[2]={{1},{0}};WillowPreparedObjectKeyCatalog rejected;
 if(willowPreparedObjectKeyCatalogBind(&rejected,badKeys,2,1)!=WILLOW_OBJECT_SAT_INVALID||rejected.ready)return fail(0,13);
 adversarial++;
 badKeys[0].value=badKeys[1].value=0;
 if(willowPreparedObjectKeyCatalogBind(&rejected,badKeys,2,1)!=WILLOW_OBJECT_SAT_INVALID)return fail(0,14);
 adversarial++;
 for(unsigned k=0;k<4;k++)mutableRecord[k]=traceRecords[0][k];
 WillowPreparedObjectSourceInput mutable={readMutable,0,1,1,256,0,1};
 mutableRecord[2]=0x27ff;mutableRecord[3]=0;
 if(willowPreparedObjectSelect(&catalog,&mutable,0,&cold,coldItems,256)!=WILLOW_OBJECT_SAT_ASSET||cold.ready)return fail(0,15);
 adversarial++;
 mutableRecord[2]=0;mutableRecord[3]=0xff00;
 if(willowPreparedObjectSelect(&catalog,&mutable,0,&cold,coldItems,256)!=WILLOW_OBJECT_SAT_INVALID||cold.ready)return fail(0,16);
 adversarial++;
 mutableRecord[2]=0x2800;mutableRecord[3]=0;
 if(willowPreparedObjectSelect(&catalog,&mutable,0,&cold,coldItems,256)||cold.items[0].templateId!=WILLOW_PREPARED_OBJECT_IGNORED)return fail(0,17);
 adversarial++;
 if(willowPreparedObjectSelectWords(&catalog,&good,0,0,&cold,coldItems,256)!=WILLOW_OBJECT_SAT_INVALID||cold.ready)return fail(0,28);
 adversarial++;
 if(willowPreparedObjectSelectWords(&catalog,&good,(const uint16_t*)((const uint8_t*)traceRecords+1),0,&cold,coldItems,256)!=WILLOW_OBJECT_SAT_INVALID||cold.ready)return fail(0,29);
 adversarial++;
 /* Real frozen1200 descriptors: exact source identity and position publication,
  * no colors/patterns generated by this test. */
 WillowPreparedObjectKeyCatalog realKeys;
 if(willowPreparedObjectKeyCatalogBind(&realKeys,realTemplateKeys,3,90))return fail(0,18);
 epoch.latch=1;epoch.resident=77;epoch.palette=77;epoch.view=1;
 if(willowPreparedObjectResolvedCatalogBind(&resolved,&realKeys,realTemplates,&epoch,&view))return fail(0,19);
 WillowPreparedObjectSourceInput realInput={readTrace,(void*)&realFrame,1,3,256,0,1};
 if(willowPreparedObjectSelect(&realKeys,&realInput,0,&cold,coldItems,256))return fail(0,20);
 mark(5);uint8_t published=willowPreparedObjectPublishSelection(&resolved,&cold,&epoch,instances,256,&frame,&binding);mark(6);
 if(published||!binding.ready||frame.count!=3||frame.sourceRecords!=256)return fail(0,21);
 for(unsigned n=0;n<3;n++){
  const uint16_t*r=traceRecords[realFrame.offset+n];const WillowPreparedObjectInstance*i=instances+n;
  if(i->sourceX!=r[0]||i->sourceY!=r[1]||i->sourceOrdinal!=n||i->shape->sourceCode!=r[2]||i->shape->sourceAttribute!=r[3])return fail(n,22);
 }
 for(unsigned test=0;test<4;test++){
  WillowPreparedObjectEpoch stale=epoch;
  if(test==0)stale.latch++;
  if(test==1)stale.resident++;
  if(test==2)stale.palette++;
  if(test==3)stale.view++;
  if(willowPreparedObjectPublishSelection(&resolved,&cold,&stale,instances,256,&frame,&binding)!=WILLOW_OBJECT_SAT_INVALID||binding.ready)return fail(test,23);
  adversarial++;
 }
 view.left=0;
 if(willowPreparedObjectResolvedCatalogBind(&resolved,&realKeys,realTemplates,&epoch,&view))return fail(0,25);
 if(willowPreparedObjectPublishSelection(&resolved,&cold,&epoch,instances,256,&frame,&binding)!=WILLOW_OBJECT_SAT_INVALID||binding.ready)return fail(0,26);
 adversarial++;
 view.left=96;
 if(willowPreparedObjectResolvedCatalogBind(&resolved,&realKeys,realTemplates,&epoch,&view))return fail(0,27);
 realKeys.epoch++;
 if(willowPreparedObjectPublishSelection(&resolved,&cold,&epoch,instances,256,&frame,&binding)!=WILLOW_OBJECT_SAT_INVALID||binding.ready)return fail(0,24);
 adversarial++;
 /* Native long-index boundaries and cold/cached prefix counts. These are
  * synthetic lookup keys only; no synthetic color or render assets exist. */
 WillowPreparedObjectKeyCatalog wideCatalog;
 if(willowPreparedObjectKeyCatalogBind(&wideCatalog,wideKeys,65534,200))return fail(0,33);
 const uint16_t counts[7]={0,1,7,8,9,255,256};
 const uint16_t ids[10]={0,1,8191,8192,16383,16384,32767,32768,65532,65533};
 unsigned wideCases=0;
 for(unsigned test=0;test<7;test++){
  if(willowPreparedObjectStreamBind(&stream,&wideCatalog,streamItems[0],streamItems[1],256))return fail(test,34);
  unsigned index=0;
  for(unsigned n=0;n<counts[test];n++){
   uint16_t id=ids[index];wideRecords[n][0]=(uint16_t)(0x8000+n*17);wideRecords[n][1]=(uint16_t)(0xffff-n*13);
   wideRecords[n][2]=(uint16_t)(wideKeys[id].value>>16);wideRecords[n][3]=(uint16_t)wideKeys[id].value;
   if(++index==10)index=0;
  }
  for(unsigned pass=0;pass<2;pass++){
   WillowPreparedObjectSourceInput input={0,0,pass+1,counts[test],256,0,1};
   if(willowPreparedObjectStreamSelect(&stream,&input,wideRecords[0],&streamResult)||!streamResult||streamResult->count!=counts[test])return fail(test,35);
   index=0;
   for(unsigned n=0;n<counts[test];n++){
    const WillowPreparedObjectSelection*item=streamResult->items+n;
    if(item->templateId!=ids[index]||item->sourceX!=wideRecords[n][0]||item->sourceY!=wideRecords[n][1]||item->sourceOrdinal!=n)return fail(test,36);
    if(++index==10)index=0;
   }
   wideCases++;
  }
 }
 WillowPreparedObjectSourceInput wideInput={0,0,3,1,256,0,1};
 wideRecords[0][2]=0;wideRecords[0][3]=0xff00;
 if(willowPreparedObjectStreamSelect(&stream,&wideInput,wideRecords[0],&streamResult)!=WILLOW_OBJECT_SAT_INVALID||streamResult)return fail(0,37);
 adversarial++;
 wideRecords[0][2]=0x27ff;wideRecords[0][3]=0;
 if(willowPreparedObjectStreamSelect(&stream,&wideInput,wideRecords[0],&streamResult)!=WILLOW_OBJECT_SAT_ASSET||streamResult)return fail(0,38);
 adversarial++;
 wideRecords[0][2]=0x2800;
 if(willowPreparedObjectStreamSelect(&stream,&wideInput,wideRecords[0],&streamResult)||!streamResult||streamResult->items[0].templateId!=WILLOW_PREPARED_OBJECT_IGNORED)return fail(0,39);
 adversarial++;
 for(unsigned test=0;test<3;test++){
  WillowPreparedObjectSourceInput stale=wideInput;
  if(test==0)stale.bank=1;
  if(test==1)stale.emittedRecords=2;
  if(test==2)stale.sourceRecords=255;
  if(willowPreparedObjectStreamSelect(&stream,&stale,wideRecords[0],&streamResult)!=WILLOW_OBJECT_SAT_INVALID||streamResult)return fail(test,40);
  adversarial++;
 }
 wideCatalog.epoch++;
 if(willowPreparedObjectStreamSelect(&stream,&wideInput,wideRecords[0],&streamResult)!=WILLOW_OBJECT_SAT_INVALID||streamResult)return fail(0,41);
 adversarial++;
 if(willowPreparedObjectStreamBind(&stream,&catalog,streamItems[0],streamItems[0]+1,256)!=WILLOW_OBJECT_SAT_INVALID||stream.ready)return fail(0,42);
 adversarial++;
#ifdef __m68k__
 *(volatile uint32_t*)0xff0008=records;*(volatile uint16_t*)0xff000c=0;
 *(volatile uint16_t*)0xff000e=TRACE_FRAME_COUNT;*(volatile uint16_t*)0xff0010=transitions;*(volatile uint16_t*)0xff0012=adversarial;
 *(volatile uint16_t*)0xff0014=sizeof(WillowPreparedObjectSelection);*(volatile uint16_t*)0xff0016=sizeof(WillowPreparedObjectSelectionPacket);
 *(volatile uint16_t*)0xff0018=wideCases;
#else
 printf("{\"captures\":%u,\"source_records_exact\":%u,\"cold_and_cached_passes\":2,\"topology_changes\":%u,\"adversarial_cases\":%u,\"wide_selection_cases\":%u,\"real1200_published_instances\":3,\"trace_palette_binding\":\"unresolved; explicit publication refusal tested\"}\n",TRACE_FRAME_COUNT,records,transitions,adversarial,wideCases);
#endif
 return 1;
}
#ifdef __m68k__
uint16_t nativeObjectPresentation(void){return (uint16_t)run();}
#else
int main(void){return run()?0:1;}
#endif
