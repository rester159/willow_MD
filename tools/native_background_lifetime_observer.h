/* Read-only host observation of the frozen continuous source/native pair.
 * Not target code. Source RAM/video, registers, cycles and scheduling are never
 * changed here. Callback stamps are core bus timing, not physical pin timing. */
static FILE *bgEvents,*bgIRQs,*bgMaps;
static unsigned bgDirty[2][4096],bgWrites[2],bgChanged[2],bgReads[2],bgIrqCount;
static uint8_t bgReplay[32768];
static void bgInit(void){
 bgEvents=outputFile("background-events.csv","w");
 bgIRQs=outputFile("background-irqs.csv","w");
 bgMaps=outputFile("background-active-maps.bin","wb");
 if(!bgEvents||!bgIRQs||!bgMaps)abort();
 fputs("kind,frame,irq,master,callback_pc,address,width,value,previous,manager,scene,a0,a1,d0,d1,return_address\n",bgEvents);
 fputs("frame,irq,master,saved_pc,manager,scene,active_index,s2_writes,s2_changed,s2_reads,s2_dirty,s3_writes,s3_changed,s3_reads,s3_dirty,port_s2x,port_s2y,port_s3x,port_s3y,staged_s2x,staged_s2y,staged_s3x,staged_s3y,logical_s2x,logical_s2y,logical_s3x,logical_s3y\n",bgIRQs);
}
static void bgTrace(unsigned kind,unsigned at,unsigned width,unsigned value){
 unsigned pc=sourcePC(),layer=at>=0x918000&&at<0x91c000?0:at>=0x914000&&at<0x918000?1:2;
 unsigned previous=value,emit=0;
 if((kind==2||kind==3)&&layer<2&&pc<0x100000){
  unsigned offset=at-(layer?0x914000:0x918000),base=layer*16384;
  previous=width==2?willowMotionWord(cps+at-0x900000):cps[at-0x900000];
  if(kind==2)bgReads[layer]++;
  else{
   bgWrites[layer]++;if(previous!=(value&(width==2?65535:255))){bgChanged[layer]++;bgDirty[layer][offset/4]=1;}
   if(width==2)willowMotionPutWord(bgReplay+base+offset,value);else bgReplay[base+offset]=value;
  }
  emit=1;
 }
 if(kind==5&&at>=0x800100&&at<=0x800170)emit=1;
 if(kind==1&&((at>=0x8032&&at<0x803a)||(at>=0xf442&&at<0xf456)))emit=1;
 /* Actual RTS stack reads give the final helper return boundary, including
  * tail branches, without inserting source instructions or instruction hooks. */
 if(kind==0&&width==2&&pc>=2&&pc<0x100000&&at==(cpu->dar[15]&65535)&&
    willowMotionWord(programs[0]+pc-2)==0x4e75&&
    ((pc>=0x42602&&pc<=0x43ef4)||(pc>=0x548c&&pc<=0x5570)))emit=1;
 if(emit)fprintf(bgEvents,"%u,%u,%u,%" PRIu64 ",%x,%x,%u,%x,%x,%u,%u,%x,%x,%x,%x,%x\n",kind,frameNumber,observedIRQs,stamp(),pc,at,width,value,previous,rawWord(0x8004),rawWord(0x8008),cpu->dar[8],cpu->dar[9],cpu->dar[0],cpu->dar[1],rawLong(cpu->dar[15]));
}
static void bgIRQ(unsigned savedPC){
 unsigned dirty[2]={0},active=rawWord(0x8004)==2&&rawWord(0x8008)==2,index=0;
 for(unsigned layer=0;layer<2;layer++){
  unsigned source=layer?0x14000:0x18000;
  if(memcmp(bgReplay+layer*16384,cps+source,16384)){fprintf(stderr,"Background replay missed a source store, frame%u layer%u\n",frameNumber,layer+2);abort();}
  for(unsigned n=0;n<4096;n++)dirty[layer]+=bgDirty[layer][n]!=0;
 }
 if(active){index=++bgIrqCount;if(fwrite(bgReplay,1,sizeof bgReplay,bgMaps)!=sizeof bgReplay)abort();}
 fprintf(bgIRQs,"%u,%u,%" PRIu64 ",%x,%u,%u,%u",frameNumber,observedIRQs,stamp(),savedPC,rawWord(0x8004),rawWord(0x8008),index);
 for(unsigned n=0;n<2;n++)fprintf(bgIRQs,",%u,%u,%u,%u",bgWrites[n],bgChanged[n],bgReads[n],dirty[n]);
 for(unsigned n=0;n<4;n++)fprintf(bgIRQs,",%x",willowMotionWord(ports+0x110+n*2));
 for(unsigned n=0;n<4;n++)fprintf(bgIRQs,",%x",rawWord(0xf446+n*2));
 for(unsigned n=0;n<4;n++)fprintf(bgIRQs,",%x",rawWord(0x8032+n*2));
 fputc('\n',bgIRQs);memset(bgDirty,0,sizeof bgDirty);memset(bgWrites,0,sizeof bgWrites);memset(bgChanged,0,sizeof bgChanged);memset(bgReads,0,sizeof bgReads);
}
static void bgFinish(void){fclose(bgEvents);fclose(bgIRQs);fclose(bgMaps);printf("Read-only background lifetime: %u active PRE-source IRQ maps, exact incremental source-write replay.\n",bgIrqCount);}
