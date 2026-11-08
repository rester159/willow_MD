/* Prove the exact native four-word source-view read before a phase1 marker.
 * The only permitted stale case is one real source IRQ after the last read,
 * at the verified next instruction, followed by the four original staged
 * camera writes. Mixed or unknown read/IRQ sequences remain fatal. */
typedef struct{unsigned count,used,irq,writes,bad;unsigned initial[4],value[4],current[4];uint64_t begin;} ForestViewAcquisition;
static ForestViewAcquisition forestViewAcquisitions[2];
static FILE*forestViewLog;
static void forestViewEvent(const char*event,unsigned at,unsigned value){
 ForestViewAcquisition*a=forestViewAcquisitions+renderPath;
 if(!forestViewLog){forestViewLog=outputFile("view-acquisition-events.csv","w");if(!forestViewLog)abort();
  fputs("event,path,frame,master_cycles,pc,address,value,reads,irq,writes,bad,v0,v1,v2,v3,current0,current1,current2,current3\n",forestViewLog);}
 fprintf(forestViewLog,"%s,%u,%u,%" PRIu64 ",%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n",event,renderPath,frameNumber,stamp(),sourcePC(),at,value,a->count,a->irq,a->writes,a->bad,a->value[0],a->value[1],a->value[2],a->value[3],rawWord(0xf446),rawWord(0xf448),rawWord(0xf44a),rawWord(0xf44c));
}
static void forestViewFailure(const char*why){forestViewEvent("INVALID",0,0);fflush(NULL);fprintf(stderr,"Unproved source view acquisition: %s path%u frame%u PC%x\n",why,renderPath,frameNumber,sourcePC());abort();}
static void forestBGRead(unsigned at,unsigned width,unsigned value){
 if(width!=2)return;at&=65535;
#ifdef FOREST_VIEW_MOVEM
 unsigned n=(at>=0xf446&&at<=0xf44c&&!(at&1)&&sourcePC()==forestViewLoadNextPC[0])?(at-0xf446)/2:4;
#else
 unsigned n=0;while(n<4&&sourcePC()!=forestViewLoadNextPC[n])n++;
#endif
 if(n==4)return;
 if(at!=0xf446+n*2)forestViewFailure("source read effective address");
 ForestViewAcquisition*a=forestViewAcquisitions+renderPath;
 if(!n){
  if(a->count&&!a->used)forestViewFailure("unconsumed read tuple");
  memset(a,0,sizeof*a);a->begin=stamp();
  for(unsigned i=0;i<4;i++)a->initial[i]=a->current[i]=rawWord(0xf446+i*2);
 }
 if(a->count!=n||value!=a->initial[n]||a->irq||a->bad)forestViewFailure("mixed or reordered source reads");
 a->value[n]=value;a->count++;forestViewEvent("read",at,value);
}
static void forestBGReferenceRead(unsigned at,unsigned width,unsigned value){if(!renderPath)forestBGRead(at,width,value);}
static void forestBGBeforeWrite(unsigned at,unsigned width,unsigned value){
 ForestViewAcquisition*a=forestViewAcquisitions+renderPath;if(!a->count||a->used)return;
 unsigned pc=sourcePC();at&=65535;
 if(width==2&&pc==CADENCE_IRQ+2){
  unsigned saved=rawLong(at+4)&0xffffff;
  if(a->count!=4||a->irq||saved!=FOREST_VIEW_ACQUISITION_END||(value&0x2700)!=0x2600){
   a->bad=1;forestViewEvent("unsupported_irq_position",at,saved);return;
  }
  for(unsigned i=0;i<4;i++)if(rawWord(0xf446+i*2)!=a->value[i])a->bad=1;
  a->irq=1;forestViewEvent("irq_after_complete_read",at,saved);return;
 }
 if(at<0xf446||at>0xf44d)return;
 static const unsigned order[]={0,2,1,3},nextPC[]={0x19a2,0x1a20,0x19b4,0x1a34};
 if(width!=2||(at&1)||!a->irq||a->writes>=4){a->bad=1;forestViewEvent("unsupported_staging_write",at,value);return;}
 unsigned n=(at-0xf446)/2;
 if(n!=order[a->writes]||pc!=nextPC[n]||(cpu->dar[13]&0xffffff)!=0xff8000||(cpu->dar[0]&65535)!=value){a->bad=1;forestViewEvent("unproved_original_staging_write",at,value);return;}
 a->current[n]=value;a->writes++;forestViewEvent("original_irq_staging_write",at,value);
}
static unsigned forestBGPrepareView(const unsigned*view,unsigned generation){
 ForestViewAcquisition*a=forestViewAcquisitions+renderPath;
 if(sourcePC()!=FOREST_VIEW_MARKER_NEXT||a->count!=4||a->used||a->bad)forestViewFailure("unmatched phase1/read tuple");
 unsigned stale=0;
 for(unsigned i=0;i<4;i++){
  if(view[i]!=a->value[i]||a->value[i]!=a->initial[i])forestViewFailure("descriptor differs from all four actual reads");
  if(view[i]!=rawWord(0xf446+i*2))stale=1;
 }
 if(a->irq){
  if(a->writes!=4)forestViewFailure("incomplete original IRQ camera update");
  for(unsigned i=0;i<4;i++){
   unsigned port=renderPath?renderWord(CADENCE_IO_MIRROR-0x200000+0x110+i*2):willowMotionWord(ports+0x110+i*2);
   if(a->current[i]!=rawWord(0xf446+i*2)||port!=a->value[i])forestViewFailure("IRQ camera exchange differs from captured tuple");
  }
 }else if(stale)forestViewFailure("stale tuple without proven IRQ transition");
 a->used=1;forestViewEvent(stale?"coherent_stale_phase1":"current_phase1",0,generation);return 1;
}
static void forestViewCheckCode(void){
 for(unsigned path=0;path<2;path++){
  const uint8_t*p=path?forestViewNativeCode:forestViewOriginalCode;
  if(memcmp(rom+FOREST_VIEW_LOAD_BEGIN,p,sizeof forestViewOriginalCode))forestViewFailure("runtime acquisition opcodes differ");
  static const unsigned sites[]={0x199e,0x1a1c,0x19b0,0x1a30};
  for(unsigned n=0;n<4;n++)if(willowMotionLong(programs[path]+sites[n])!=(0x3b407446u+n*2))forestViewFailure("original source camera write opcode changed");
 }
}
