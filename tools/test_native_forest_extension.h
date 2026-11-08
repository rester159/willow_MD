/* Read-only first-coverage-failure capture. No target writes or fallback art. */
static unsigned forestActiveTicks,forestCoverageStop,forestCaptureCount;
static unsigned forestSeenReject[2][2][256];
static unsigned forestHasKey(const uint32_t*keys,unsigned count,unsigned key){
 unsigned lo=0,hi=count;while(lo<hi){unsigned mid=(lo+hi)/2;if(keys[mid]<key)lo=mid+1;else hi=mid;}
 return lo<count&&keys[lo]==key;
}
static unsigned forestKeyIndex(const uint32_t*keys,unsigned count,unsigned key){
 unsigned lo=0,hi=count;while(lo<hi){unsigned mid=(lo+hi)/2;if(keys[mid]<key)lo=mid+1;else hi=mid;}
 return lo<count&&keys[lo]==key?lo:~0u;
}
static unsigned forestPixelRef(unsigned layer,unsigned x,unsigned y,unsigned*address,unsigned*code,unsigned*attr){
 unsigned scale=layer==2?4:5,cx=(x>>scale)&63,cy=(y>>scale)&63;
 unsigned cell=layer==2?((cy&15)|((cx&63)<<4)|((cy&48)<<6)):((cy&7)|((cx&63)<<3)|((cy&56)<<6));
 *address=(layer==2?0x18000:0x14000)+cell*4;*code=willowMotionWord(cps+*address);*attr=willowMotionWord(cps+*address+2);
 const uint32_t*keys=layer==2?forestBGKeys2:forestBGKeys3;
 unsigned count=layer==2?sizeof forestBGKeys2/sizeof*forestBGKeys2:sizeof forestBGKeys3/sizeof*forestBGKeys3;
 unsigned id=forestKeyIndex(keys,count,(*code<<16)|*attr);if(id==~0u)return id;
 unsigned mask=layer==2?1:3,shift=layer==2?1:2;
 return (id<<(shift*2))+(((y>>3)&mask)<<shift)+((x>>3)&mask);
}
static void forestReject(unsigned kind,unsigned publishing,unsigned status,unsigned bank,unsigned records,unsigned epoch,unsigned palette){
 if(renderPath||!renderActive()||status>255)return;
 unsigned missingObject=0,objectOrdinal=0,objectCode=0,objectAttribute=0,objectX=0,objectY=0;
 unsigned invalidPrefix=0,paletteAt=~0u,paletteExpected=0,paletteActual=0;
 unsigned missingBG=0,bgLayer=0,bgX=0,bgY=0,bgCode=0,bgAttribute=0,bgAddress=0;
 unsigned certifiedPrefix=bank<2&&records<=256&&(publishing?renderWord(0xf532):renderWord(0xf50eu+bank*8))==1;
 unsigned nearMissing=0,nearCode=0,nearAttribute=0,nearSubtile=0,nearPattern=0,nearAddress=0,nearDx=0,nearDy=0;
 unsigned farMissing=0,farCode=0,farAttribute=0,farSubtile=0,farPattern=0,farAddress=0,farX=0,farY=0;
 if(certifiedPrefix){
  const uint8_t*p=cps+(bank?0x8000:0);
  for(unsigned n=0;n<records;n++){
   unsigned code=willowMotionWord(p+n*8+4),attribute=willowMotionWord(p+n*8+6);
   if((attribute&0xff00)==0xff00){invalidPrefix=1;continue;}
   if(code>0x27ff)continue;
   if(!forestHasKey(forestObjectKeys,sizeof forestObjectKeys/sizeof*forestObjectKeys,(code<<16)|attribute)&&!missingObject){
    missingObject=1;objectOrdinal=n;objectCode=code;objectAttribute=attribute;objectX=willowMotionWord(p+n*8);objectY=willowMotionWord(p+n*8+2);
   }
  }
 }
 for(unsigned n=0;n<sizeof renderOraclePalettes;n+=2)if(willowMotionWord(cps+0x1c000+n)!=willowMotionWord(renderOraclePalettes+n)){
  paletteAt=n;paletteExpected=willowMotionWord(renderOraclePalettes+n);paletteActual=willowMotionWord(cps+0x1c000+n);break;
 }
 /* An IRQ can change staging while preparation is open. Use the actual BG
  * operation descriptor, never a later staged or prior visible camera. */
 unsigned views[4];for(unsigned n=0;n<4;n++)views[n]=kind?renderWord((publishing?0xf5ac:0xf59c)+n*2):(publishing?willowMotionWord(ports+0x110+n*2):rawWord(0xf446+n*2));
 /* Exact native ring demand, including the existing guard cells. A missing
  * pair outside this ring is not treated as visible/required failure. */
 for(unsigned layer=0;layer<2;layer++){
  unsigned scale=layer?2:1,mask=layer?255:127;
  unsigned firstX=((((96+views[layer*2])&65535)>>3)-1)&mask;
  unsigned firstY=(((16+views[layer*2+1])&65535)>>3)&mask;
  unsigned mx=firstX>>scale,my=firstY>>scale,w=layer?12:22,h=layer?8:15;
  for(unsigned row=0;row<h;row++)for(unsigned col=0;col<w;col++){
   unsigned x=(mx+col)&63,y=(my+row)&63;
   unsigned cell=layer?((y&7)|((x&63)<<3)|((y&56)<<6)):((y&15)|((x&63)<<4)|((y&48)<<6));
   unsigned base=layer?0x14000:0x18000,code=willowMotionWord(cps+base+cell*4),attribute=willowMotionWord(cps+base+cell*4+2);
   const uint32_t*keys=layer?forestBGKeys3:forestBGKeys2;unsigned count=layer?sizeof forestBGKeys3/sizeof*forestBGKeys3:sizeof forestBGKeys2/sizeof*forestBGKeys2;
   if(!forestHasKey(keys,count,(code<<16)|attribute)&&!missingBG){missingBG=1;bgLayer=layer+2;bgX=x;bgY=y;bgCode=code;bgAttribute=attribute;bgAddress=0x900000+base+cell*4;}
  }
 }
 /* Near cells are materialized even in the native guard columns. */
 unsigned nx=((((96+views[0])&65535)>>3)-1)&127,ny=(((16+views[1])&65535)>>3)&127;
 for(unsigned dy=0;dy<28+((views[1]&7)!=0);dy++)for(unsigned dx=0;dx<43;dx++){
  unsigned x=(nx+dx)&127,y=(ny+dy)&127,cx=x>>1,cy=y>>1;
  unsigned cell=(cy&15)|((cx&63)<<4)|((cy&48)<<6),address=0x18000+cell*4;
  unsigned code=willowMotionWord(cps+address),attribute=willowMotionWord(cps+address+2);
  unsigned id=forestKeyIndex(forestBGKeys2,sizeof forestBGKeys2/sizeof*forestBGKeys2,(code<<16)|attribute);
  unsigned subtile=((y&1)<<1)|(x&1);
  if(id!=~0u&&forestBGWords2[id*4+subtile]==65535){
   if(!nearMissing){nearCode=code;nearAttribute=attribute;nearSubtile=subtile;nearPattern=forestBGPatterns2[id*4+subtile];nearAddress=0x900000+address;nearDx=dx;nearDy=dy;}
   nearMissing++;
  }
 }
 /* The source-oriented opacity bytes come from the exact compiled catalog.
  * A missing far word is required only when an actual original far pixel is
  * opaque and the corresponding original near pixel is transparent. This
  * host-only inspection runs after refusal and consumes no emulated cycles. */
 if(kind&&!publishing&&status==11)for(unsigned sy=0;sy<224;sy++)for(unsigned sx=0;sx<320;sx++){
  unsigned x=96+views[0]+sx,y=16+views[1]+sy,address,code,attr;
  unsigned ref=forestPixelRef(2,x,y,&address,&code,&attr);
  if(ref==~0u||(forestBGOpacity2[ref*8+(y&7)]&(0x80u>>(x&7))))continue;
  x=96+views[2]+sx;y=16+views[3]+sy;
  ref=forestPixelRef(3,x,y,&address,&code,&attr);
  if(ref!=~0u&&forestBGWords3[ref]==65535&&(forestBGOpacity3[ref*8+(y&7)]&(0x80u>>(x&7)))){
   if(!farMissing){farCode=code;farAttribute=attr;farSubtile=ref&15;farPattern=forestBGPatterns3[ref];farAddress=0x900000+address;farX=sx;farY=sy;}
   farMissing++;
  }
 }
 unsigned unsupported=0;
 if(!publishing&&!kind)unsupported=missingObject||(certifiedPrefix&&records>8)||invalidPrefix||(status==11&&paletteAt!=~0u&&!renderWord(0xf56e));
 if(!publishing&&kind)unsupported=missingBG&&(status==2||status==6||status==7);
 if(!publishing&&kind&&nearMissing&&(status==4||status==8||status==9||status==10))unsupported=1;
 if(!publishing&&kind&&status==11&&farMissing)unsupported=1;
 if(forestCoverageStop)return;
 if(!unsupported&&(forestSeenReject[kind][publishing][status]||forestCaptureCount>=32))return;
 forestSeenReject[kind][publishing][status]=1;
 unsigned id=forestCaptureCount++;char name[128];
 snprintf(name,sizeof name,"rejection-%03u-source.bin",id);FILE*f=outputFile(name,"wb");if(!f)abort();backgroundDumpSource(f,0);fclose(f);
 snprintf(name,sizeof name,"rejection-%03u-objects.bin",id);f=outputFile(name,"wb");if(!f||fwrite(cps,1,2048,f)!=2048||fwrite(cps+0x8000,1,2048,f)!=2048)abort();fclose(f);
 snprintf(name,sizeof name,"rejection-%03u.json",id);f=outputFile(name,"w");if(!f)abort();
 fprintf(f,"{\"schema_version\":1,\"kind\":\"%s\",\"operation\":\"%s\",\"status\":%u,\"frame\":%u,\"active_input_ticks\":%u,\"master_cycles\":%" PRIu64 ",\"source_pc\":%u,\"bank\":%u,\"records\":%u,\"epoch\":%u,\"palette_epoch\":%u,\"unsupported\":%s,\"missing_object_key\":%s,\"object_ordinal\":%u,\"object_code\":%u,\"object_attribute\":%u,\"object_x\":%u,\"object_y\":%u,\"instance_capacity_exceeded\":%s,\"prefix_marker_unsupported\":%s,\"palette_changed\":%s,\"palette_byte_offset\":%u,\"palette_expected\":%u,\"palette_actual\":%u,\"missing_bg_ring_key\":%s,\"bg_layer\":%u,\"bg_map_x\":%u,\"bg_map_y\":%u,\"bg_code\":%u,\"bg_attribute\":%u,\"bg_source_address\":%u,\"views\":[%u,%u,%u,%u],\"snapshot_source_bytes\":123392,\"snapshot_object_bytes\":4096,\"capture_boundary\":\"actual rejected operation on original-bus path, before completing the paired hardware frame; no host target writes\",",
 kind?"background":"object",publishing?"publish":"prepare",status,frameNumber,forestActiveTicks,stamp(),sourcePC(),bank,records,epoch,palette,unsupported?"true":"false",missingObject?"true":"false",objectOrdinal,objectCode,objectAttribute,objectX,objectY,certifiedPrefix&&records>8?"true":"false",invalidPrefix?"true":"false",paletteAt!=~0u?"true":"false",paletteAt,paletteExpected,paletteActual,missingBG?"true":"false",bgLayer,bgX,bgY,bgCode,bgAttribute,bgAddress,views[0],views[1],views[2],views[3]);
 fprintf(f,"\"object_prefix_certified\":%s,\"missing_near_reference_count\":%u,\"near_code\":%u,\"near_attribute\":%u,\"near_subtile\":%u,\"near_pattern\":%u,\"near_source_address\":%u,\"near_guard_dx\":%u,\"near_guard_dy\":%u,",certifiedPrefix?"true":"false",nearMissing,nearCode,nearAttribute,nearSubtile,nearPattern,nearAddress,nearDx,nearDy);
 fprintf(f,"\"missing_exposed_far_pixels\":%u,\"far_code\":%u,\"far_attribute\":%u,\"far_subtile\":%u,\"far_pattern\":%u,\"far_source_address\":%u,\"far_visible_x\":%u,\"far_visible_y\":%u}\n",farMissing,farCode,farAttribute,farSubtile,farPattern,farAddress,farX,farY);fclose(f);
 if(unsupported){forestCoverageStop=id+1;fprintf(stderr,"Confirmed live resource coverage boundary: capture%u kind%u status%u frame%u activeTick%u object%04x/%04x missing%u records%u paletteChange%u BGkey%04x/%04x missing%u\n",id,kind,status,frameNumber,forestActiveTicks,objectCode,objectAttribute,missingObject,records,paletteAt!=~0u,bgCode,bgAttribute,missingBG);}
}
static void forestStopAfterPairedFrame(unsigned ticks){
 FILE*f=outputFile("coverage-stop.json","w");if(!f)abort();
 fprintf(f,"{\"paired_frames_completed\":%u,\"active_input_ticks\":%u,\"first_unsupported_capture\":%u,\"source_register_failures_retained\":%u,\"all_other_per_frame_source_and_vdp_checks_completed\":true,\"coverage_complete\":false,\"reason\":\"Original source reached an unsupported live resource; no fallback art or source state patch applied.\"}\n",frameNumber+1,ticks,forestCoverageStop-1,backgroundRegisterFailures);fclose(f);fflush(NULL);
}
