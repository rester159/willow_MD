#include "../src/willow_native_object_banks.h"
void objectBanksProbe(void)
{
 volatile uint16_t*p=(volatile uint16_t*)0xff1000UL;
#ifdef WILLOW_OBJECT_PROBE_CARTRIDGE
 WillowNativeObjectBanks*s=(WillowNativeObjectBanks*)0x202002UL;
#else
 WillowNativeObjectBanks*s=(WillowNativeObjectBanks*)0xff2002UL;
#endif
 WillowNativeObjectPort*port=(WillowNativeObjectPort*)0xff1010UL;
 uint32_t address=((uint32_t)p[2]<<16)|p[3];uint8_t ok=0;
 if(p[0]==0)ok=willowObjectWrite(s,address,p[4]);
 else if(p[0]==1)ok=willowObjectFill(s,address,p[4],p[5],p[6]);
 else if(p[0]==2)ok=willowObjectLatch(s,p[7]);
 else if(p[0]==3)ok=willowObjectPortWrite(port,(uint16_t)address,p[4]);
 else if(p[0]==4)ok=willowObjectPortLatch(s,port);
 p[1]=ok?0x544b:1;
}
