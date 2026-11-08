/* Headless 68000 validation of even and odd record-memory accesses. */
#include "../src/willow_arcade_player_motion.h"
void recordAccessProbe(void){
 volatile uint16_t*command=(volatile uint16_t*)0xff1000UL;
 uint8_t*p=(uint8_t*)(0xff2000UL+command[1]);
 uint32_t value=((uint32_t)command[2]<<16)|command[3],result=0;
 switch(command[0]){
 case 0:result=willowMotionWord(p);break;
 case 1:result=willowMotionLong(p);break;
 case 2:willowMotionPutWord(p,(uint16_t)value);break;
 case 3:willowMotionPutLong(p,value);break;
 }
 command[4]=result>>16;command[5]=result;
}
