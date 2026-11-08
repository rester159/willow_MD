#include "../src/willow_native_object_latch_compact.h"
#include <stdio.h>
#include <string.h>
static _Alignas(4) uint8_t work[65544],before[65544],source[2064];
static int offset(unsigned at){
 for(unsigned chunk=0;chunk<32;chunk++)if(at>=willowNativeObjectLatchOffsets[chunk]&&at<willowNativeObjectLatchOffsets[chunk]+64)return (int)(chunk*64+at-willowNativeObjectLatchOffsets[chunk]);
 return -1;
}
int main(void){
 unsigned cases=0;
 for(unsigned alignment=0;alignment<4;alignment++)for(unsigned records=0;records<=256;records++){
  unsigned destinationOffset=(alignment&1)*4,sourceOffset=(alignment>>1)*4;
  for(unsigned n=0;n<sizeof work;n++)before[n]=work[n]=(uint8_t)(n*31u+records);
  for(unsigned n=0;n<sizeof source;n++)source[n]=(uint8_t)(n*73u+records*11u);
  if(!willowNativeObjectLatchCapturePrefix(work+destinationOffset,source+sourceOffset,records))return 1;
  for(unsigned n=0;n<sizeof work;n++){
   int at=n>=destinationOffset&&n<destinationOffset+65536?offset(n-destinationOffset):-1;
   unsigned expected=at>=0&&(unsigned)at<records*8?source[sourceOffset+at]:before[n];
   if(work[n]!=expected){fprintf(stderr,"Prefix canary mismatch count%u align%u at%x\n",records,alignment,n);return 1;}
  }
  cases++;
 }
 memcpy(before,work,sizeof work);
 if(willowNativeObjectLatchCapturePrefix(work,source,257)||
    willowNativeObjectLatchCapturePrefix(work+1,source,1)||
    willowNativeObjectLatchCapturePrefix(work+2,source,1)||
    willowNativeObjectLatchCapturePrefix(work,source+1,1)||
    willowNativeObjectLatchCapturePrefix(work,source+2,1)||
    willowNativeObjectLatchCapturePrefix(NULL,source,0)||
    willowNativeObjectLatchCapturePrefix(work,NULL,0)||memcmp(before,work,sizeof work))return 1;
 printf("Compact prefix host: %u cases, every count0..256 with four aligned source/destination combinations, full65544-byte canaries and7 invalid/no-mutation guards.\n",cases);
 return 0;
}
