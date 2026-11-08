/* Read-only call-stack and source-IRQ timing. No target instrumentation. */
static struct {unsigned sp,ret,callee,inIRQ;uint64_t begin,irqBegin;} farProfileStack[64];
static unsigned farProfileDepth,farProfileSerial;
static uint64_t farProfileIRQTotal,farProfileIRQStart;
static FILE*farProfileCalls;
static int farProfileFunction(unsigned target){
 for(unsigned n=0;n<sizeof farProfileFunctions/sizeof*farProfileFunctions;n++)if(farProfileFunctions[n].begin==target)return (int)n;
 return -1;
}
static void farProfileInit(void){farProfileCalls=outputFile("far-function-calls.csv","w");if(!farProfileCalls)abort();fputs("sample,frame,callee,call_return,stack,begin_master,end_master,irq_master,remaining_depth,entered_in_irq\n",farProfileCalls);}
static void farProfileAfterWork(unsigned at,unsigned width){
 if(renderPath||width!=2||!renderActive())return;
 unsigned pc=sourcePC(),sp=cpu->dar[15]&65535;at&=65535;
 if(sp<0x7000||sp>=0x7800||(at!=sp&&at!=sp+2))return;
 unsigned low=0,high=sizeof farProfileCallSites/sizeof*farProfileCallSites;
 while(low<high){unsigned mid=(low+high)/2;if(farProfileCallSites[mid].ret<pc)low=mid+1;else high=mid;}
 if(low==sizeof farProfileCallSites/sizeof*farProfileCallSites||farProfileCallSites[low].ret!=pc)return;
 const FarProfileCallSite*s=farProfileCallSites+low;
 if(rawLong(sp)!=pc)return; /* The second bus word has not arrived yet. */
 unsigned callee=s->reg<16?cpu->dar[s->reg]&0xffffff:s->callee;
 if(farProfileFunction(callee)<0)return;
 if(farProfileDepth&&farProfileStack[farProfileDepth-1].sp==sp&&farProfileStack[farProfileDepth-1].ret==pc)return;
 if(farProfileDepth==64)abort();
 farProfileStack[farProfileDepth].sp=sp;farProfileStack[farProfileDepth].ret=pc;farProfileStack[farProfileDepth].callee=callee;
 farProfileStack[farProfileDepth].inIRQ=farProfileIRQStart!=0;farProfileStack[farProfileDepth].begin=stamp();farProfileStack[farProfileDepth].irqBegin=farProfileIRQTotal+(farProfileIRQStart?stamp()-farProfileIRQStart:0);farProfileDepth++;
}
static void farProfileRead(unsigned at,unsigned width){
 if(renderPath||width!=2||!farProfileDepth)return;at&=65535;
 unsigned i=farProfileDepth-1,pc=sourcePC();
 if(at!=farProfileStack[i].sp+2||rawLong(farProfileStack[i].sp)!=farProfileStack[i].ret)return;
 unsigned low=0,high=sizeof farProfileReturnSites/sizeof*farProfileReturnSites;
 while(low<high){unsigned mid=(low+high)/2;if(farProfileReturnSites[mid]<pc)low=mid+1;else high=mid;}
 if(low==sizeof farProfileReturnSites/sizeof*farProfileReturnSites||farProfileReturnSites[low]!=pc)return;
 uint64_t irq=farProfileIRQTotal+(farProfileIRQStart?stamp()-farProfileIRQStart:0)-farProfileStack[i].irqBegin;
 fprintf(farProfileCalls,"%u,%u,%x,%x,%x,%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%u,%u\n",farProfileSerial++,frameNumber,farProfileStack[i].callee,farProfileStack[i].ret,farProfileStack[i].sp,farProfileStack[i].begin,stamp(),irq,i,farProfileStack[i].inIRQ);
 farProfileDepth--;
}
static void farProfileIRQ(unsigned ending,unsigned at){
 if(renderPath)return;
 if(!ending){if(farProfileIRQStart)abort();farProfileIRQStart=stamp();return;}
 BackgroundObserverPath*p=backgroundPaths;
 if(p->irqDepth&&backgroundRTEPC(sourcePC())&&(0xff0000|(at&65535))==p->irqFrames[p->irqDepth-1]+5){
  if(!farProfileIRQStart)abort();farProfileIRQTotal+=stamp()-farProfileIRQStart;farProfileIRQStart=0;
 }
}

static void farProfileFinish(void){
 FILE*f=outputFile("far-profile-open.json","w");if(!f)abort();fprintf(f,"{\"open\":[");
 for(unsigned n=0;n<farProfileDepth;n++)fprintf(f,"%s{\"callee\":%u,\"ret\":%u,\"sp\":%u,\"begin\":%" PRIu64 "}",n?",":"",farProfileStack[n].callee,farProfileStack[n].ret,farProfileStack[n].sp,farProfileStack[n].begin);
 fprintf(f,"]}\n");fclose(f);fflush(farProfileCalls);
}
