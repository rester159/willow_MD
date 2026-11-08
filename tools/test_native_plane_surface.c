#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
static uint16_t memory[32768];static unsigned address,dataWords,commands,startWords,startCommands;
static void control(uint32_t v){address=((v>>16)&0x3fff)|((v&3)<<14);commands++;}
static void data(uint16_t v){memory[(address&65535)/2]=v;address=(address+2)&65535;dataWords++;}
#define WILLOW_NATIVE_PLANE_CONTROL(v) control(v)
#define WILLOW_NATIVE_PLANE_DATA(v) data(v)
static void testWriteBegin(uint16_t v){address=v;}
static void testWriteWord(uint16_t v){data(v);}
static void testReadBegin(uint16_t v){address=v;}
static uint16_t testReadWord(void){uint16_t v=memory[(address&65535)/2];address=(address+2)&65535;return v;}
static void testTimingBegin(unsigned kind,unsigned words){(void)kind;(void)words;startWords=dataWords;startCommands=commands;}
static void testTimingEnd(unsigned rows,unsigned words){if(dataWords-startWords!=words||commands-startCommands!=rows){fprintf(stderr,"Traffic %u/%u expected%u/%u\n",dataWords-startWords,commands-startCommands,words,rows);abort();}}
static void testFail(unsigned line,uint32_t actual,uint32_t expected){fprintf(stderr,"Plane test line%u actual%u expected%u\n",line,actual,expected);}
#include "test_native_plane_surface_cases.h"
int main(void){if(!testRun())return 1;printf("{\"publications\":%u,\"refusals\":%u,\"invalid_edits\":%u,\"plane_words_checked_per_publication\":2048}\n",successes,refusals,invalidEdits);return 0;}
