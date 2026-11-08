#include "scene_memory_probe_layout.h"
void sceneMemoryProbe(void)
{
 volatile uint16_t*p=(volatile uint16_t*)0xff1000UL;
 uint16_t args[8];for(uint16_t n=0;n<8;n++)args[n]=p[n];
 p[1]=sceneMemoryProbeRun((SceneMemoryProbeStorage*)0xff2002UL,args)?0x544b:1;
}
