#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
static uint16_t memory[32768];static unsigned address,increment=2,dataWords,controlWords;
static unsigned beginData,beginControl,expectedWords,expectedControls,active,base;
static FILE*traffic,*snapshots;
static void emit(unsigned type,unsigned value){if(active){unsigned char b[4]={type>>8,type,value>>8,value};if(fwrite(b,1,4,traffic)!=4)abort();}}
static void control(uint32_t v){emit(4,v>>16);emit(4,v);address=((v>>16)&0x3fff)|((v&3)<<14);controlWords+=2;}
static void regWrite(uint16_t v){if(v!=0x8f80&&v!=0x8f02)abort();emit(4,v);increment=v&255;controlWords++;}
static void data(uint16_t v){if(active&&((address-base)&65535)>=4096)abort();emit(0,v);memory[(address&65535)/2]=v;address=(address+increment)&65535;dataWords++;}
#define WILLOW_NATIVE_PLANE_CONTROL(v) control(v)
#define WILLOW_NATIVE_PLANE_DATA(v) data(v)
#define WILLOW_NATIVE_BACKGROUND_PLANE_REGISTER(v) regWrite(v)
static void testWriteBegin(uint16_t v){address=v;}
static void testWriteWord(uint16_t v){data(v);}
static void testReadBegin(uint16_t v){address=v;}
static uint16_t testReadWord(void){uint16_t v=memory[address/2];address=(address+increment)&65535;return v;}
static void testTimingBegin(unsigned kind,unsigned words,unsigned controls,unsigned at){(void)kind;if(active||increment!=2)abort();active=1;base=at;expectedWords=words;expectedControls=controls;beginData=dataWords;beginControl=controlWords;}
static void testTimingEnd(void){if(!active||increment!=2||dataWords-beginData!=expectedWords||controlWords-beginControl!=expectedControls){fprintf(stderr,"Traffic words%u/%u controls%u/%u inc%u\n",dataWords-beginData,expectedWords,controlWords-beginControl,expectedControls,increment);abort();}active=0;for(unsigned n=0;n<32768;n++){unsigned char b[2]={memory[n]>>8,memory[n]};if(fwrite(b,1,2,snapshots)!=2)abort();}}
static void testFail(unsigned line,uint32_t actual,uint32_t expected){fprintf(stderr,"Plane publisher line%u actual%u expected%u\n",line,actual,expected);}
#include "test_native_background_plane_publish_cases.h"
int main(int argc,char**argv){if(argc!=3)return 2;traffic=fopen(argv[1],"wb");snapshots=fopen(argv[2],"wb");if(!traffic||!snapshots)return 2;for(unsigned n=0;n<32768;n++)memory[n]=(uint16_t)(n^0xa55a);if(!testRun())return 1;fclose(traffic);fclose(snapshots);printf("{\"publications\":%u,\"refusals\":%u,\"invalid_edits\":%u}\n",successes,refusals,invalidEdits);return 0;}
