/* Read-only host snapshots at the existing independent object acquisition.
 * No target bus calls, writes, execution or cycle adjustments are permitted. */
#ifndef WILLOW_TEST_NATIVE_BACKGROUND_CONTEXT_H
#define WILLOW_TEST_NATIVE_BACKGROUND_CONTEXT_H
static FILE *backgroundContextCSV,*backgroundContextBinary;
static unsigned backgroundContextRecords;
static uint8_t backgroundContextWork[65536];
static int backgroundContextInit(void){
 backgroundContextCSV=outputFile("source-background-contexts.csv","w");
 backgroundContextBinary=outputFile("source-background-contexts.bin","wb");
 if(!backgroundContextCSV||!backgroundContextBinary)return 0;
 fputs("sample,frame,capture,master_cycles,manager,scene,bank,selector,source_epoch,source_count,source_valid,native_epoch,native_count,native_valid,callback_pc\n",backgroundContextCSV);
 return 1;
}
static void backgroundContextCapture(void){
 /* publicationLatch has just independently verified the source prefix and
  * acquired metadata. latchCaptures increments only after this callback. */
 if(renderPath||backgroundContextRecords!=latchCaptures)abort();
 unsigned bank=latchSelector==0x9080;
 const PublicationBank*b=publicationBanks+bank;
 const uint8_t*m=metadataSRAM+METADATA_SRAM_OFFSET;
 const struct {const uint8_t*data;unsigned size;} blocks[]={
  {cps+0x18000,16384},{cps+0x14000,16384},{cps+0x10000,16384},
  {cps+0x1c000,6144},{ports,512},{cps+0x22000,2048}};
 fprintf(backgroundContextCSV,"%u,%u,%u,%" PRIu64 ",%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n",
  backgroundContextRecords,frameNumber,latchCaptures+1,stamp(),rawWord(0x8004),rawWord(0x8008),bank,latchSelector,
  b->epoch,b->count,b->valid,willowMotionLong(m+24),willowMotionWord(m+28),willowMotionWord(m+32),sourcePC());
 for(unsigned n=0;n<sizeof blocks/sizeof *blocks;n++)
  if(fwrite(blocks[n].data,1,blocks[n].size,backgroundContextBinary)!=blocks[n].size)abort();
 for(unsigned n=0;n<65536;n++)backgroundContextWork[n]=ram[n^1];
 if(fwrite(backgroundContextWork,1,65536,backgroundContextBinary)!=65536)abort();
 backgroundContextRecords++;
}
static void backgroundContextFinish(void){
 if(backgroundContextRecords!=latchCaptures)abort();
 if(fclose(backgroundContextCSV)||fclose(backgroundContextBinary))abort();
}
#endif
