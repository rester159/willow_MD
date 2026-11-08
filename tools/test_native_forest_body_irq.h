/* Read-only disambiguation of a real BSR/JSR return push from an IRQ that
 * interrupts at the same PC. No state comparison or target byte is altered. */
static unsigned mainBodyIRQStage,mainBodyIRQBase,mainBodyIRQPC,mainBodyIRQSR;
static FILE*mainBodyMarkers;
static void mainBodyMarker(const char*event,unsigned at,unsigned width,unsigned value){
 if(!mainBodyMarkers){mainBodyMarkers=outputFile("main-body-marker-events.csv","w");if(!mainBodyMarkers)abort();
  fputs("event,frame,master_cycles,body,open,pc,sp,sr,address,width,value,task,task_stack_top\n",mainBodyMarkers);}
 fprintf(mainBodyMarkers,"%s,%u,%" PRIu64 ",%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n",event,frameNumber,stamp(),mainBodyNumber,mainBodyOpen,sourcePC(),cpu->dar[15]&0xffffff,renderGetRegister(17),at,width,value,rawLong(0x204)&0xffffff,rawLong(0x6c)&0xffffff);
}
static unsigned mainBodyIRQMarker(unsigned kind,unsigned at,unsigned width,unsigned value){
 unsigned pc=sourcePC(),sp=cpu->dar[15]&0xffffff;
 if(mainBodyIRQStage||((pc==0x92f2||pc==0x92f6)&&sp>=0xfff000&&kind==1)){
  if(kind!=1)return 0;
  if(!renderGetRegister||width!=2||(renderGetRegister(17)&0x2700)!=0x2600)goto invalid;
  if(!mainBodyIRQStage){
   if(sp>0xfffffa||at!=(sp&65535)||value!=(pc>>16))goto invalid;
   mainBodyIRQPC=pc;mainBodyIRQBase=sp;mainBodyIRQStage=1;mainBodyMarker("irq_pc_high",at,width,value);return 1;
  }
  if(mainBodyIRQStage==1){
   if(pc!=mainBodyIRQPC||sp!=mainBodyIRQBase||at!=((sp+2)&65535)||value!=(pc&65535))goto invalid;
   mainBodyIRQStage=2;mainBodyMarker("irq_pc_low",at,width,value);return 1;
  }
  if(mainBodyIRQStage==2){
   if(pc!=mainBodyIRQPC||sp+2!=mainBodyIRQBase||at!=(sp&65535)||(value&~0x1fu))goto invalid;
   mainBodyIRQSR=value;mainBodyIRQStage=3;mainBodyMarker("irq_old_user_sr",at,width,value);return 1;
  }
  if(pc!=CADENCE_IRQ+2||sp+4!=mainBodyIRQBase||at!=(sp&65535)||rawWord(at+2)!=mainBodyIRQSR||rawLong(at+4)!=mainBodyIRQPC||(value&0x2700)!=0x2600)goto invalid;
  mainBodyMarker("irq_frame_confirmed",at,width,value);mainBodyIRQStage=0;return 0;
 }
 return 0;
invalid:
 mainBodyMarker("INVALID_IRQ_FRAME",at,width,value);fflush(mainBodyMarkers);
 fprintf(stderr,"Invalid main-body IRQ marker sequence stage%u PC%x SP%x at%x value%x\n",mainBodyIRQStage,pc,sp,at,value);abort();
}
static void mainBodyCallStack(unsigned at,unsigned value){
 unsigned sp=cpu->dar[15]&0xffffff,top=rawLong(0x6c)&0xffffff;
 if(!renderGetRegister||(renderGetRegister(17)&0x2000)||(rawLong(0x204)&0xffffff)!=0xff0060||top<0xff0290||top>0xff0a10||sp<top-128||sp+4>top||(at&65535)!=((sp+2)&65535)){
  mainBodyMarker("INVALID_CALL_STACK",at,2,value);fflush(mainBodyMarkers);
  fprintf(stderr,"Original body call escaped exact user task stack PC%x SP%x top%x at%x\n",sourcePC(),sp,top,at);abort();
 }
 mainBodyMarker(sourcePC()==0x92f2?"body_call_return":"body_complete_call_return",at,2,value);
}
