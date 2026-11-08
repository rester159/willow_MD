#ifndef WILLOW_TEST_NATIVE_BACKGROUND_RESIDENCY_H
#define WILLOW_TEST_NATIVE_BACKGROUND_RESIDENCY_H

/* Host observation only. Never changes emulated memory or cycle accounting. */
static unsigned backgroundResidencyFrames[2],backgroundResidencyFirst[2];
static int backgroundResidencyObserve(unsigned path){
 if(!renderPaths[path].ownershipArmed)return 0;
 if(!backgroundResidencyFrames[path])backgroundResidencyFirst[path]=frameNumber;
 for(unsigned n=0;n<sizeof backgroundResidencyPatterns;n++){
  unsigned actual=renderVRAM[n^1];
  if(actual!=backgroundResidencyPatterns[n]){
   fprintf(stderr,"Background residency mismatch frame%u path%u tile%u byte%u actual%02x expected%02x\n",frameNumber,path,n/32,n%32,actual,backgroundResidencyPatterns[n]);return 1;
  }
 }
 for(unsigned n=0;n<sizeof backgroundResidencyCRAM;n+=2){
  uint16_t word;memcpy(&word,(const uint8_t*)renderCRAM+n,2);
  unsigned bus=((word&0x1c0)<<3)|((word&0x38)<<2)|((word&7)<<1);
  unsigned expected=((unsigned)backgroundResidencyCRAM[n]<<8)|backgroundResidencyCRAM[n+1];
  if(bus!=expected){fprintf(stderr,"Background CRAM mismatch frame%u path%u color%u actual%04x expected%04x\n",frameNumber,path,n/2,bus,expected);return 1;}
 }
 backgroundResidencyFrames[path]++;return 0;
}
static void backgroundResidencyFinish(void){
 FILE*f=outputFile("background-residency-summary.json","w");if(!f)abort();
 fprintf(f,"{\n  \"scope\":\"Cold immutable pattern residency only; no live background display, priority or full-game FPS admission.\",\n  \"pattern_pool_bytes_per_frame\":%zu,\n  \"cram_bytes_per_frame\":%zu,\n  \"paths\":[",sizeof backgroundResidencyPatterns,sizeof backgroundResidencyCRAM);
 for(unsigned path=0;path<2;path++)fprintf(f,"%s{\"path\":%u,\"ownership_armed_frames_checked\":%u,\"first_checked_frame\":%u}",path?",":"",path,backgroundResidencyFrames[path],backgroundResidencyFirst[path]);
 fputs("]\n}\n",f);fclose(f);
}
#endif
