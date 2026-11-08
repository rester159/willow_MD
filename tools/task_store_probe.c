#include "../src/willow_native_task_store.h"
#include "../src/willow_arcade_event_commands.h"
#ifdef SGDK_GCC
#define PROBE_STORE ((WillowNativeTaskStore*)0xff2000UL)
#define PROBE_SAMPLE (*(volatile uint16_t*)0xff1000UL)
#define PROBE_STATUS (*(volatile uint16_t*)0xff1002UL)
static uint16_t probeSource(uint32_t address){return *(volatile uint16_t*)address;}
#else
static WillowNativeTaskStore probeStore;
static uint16_t probeSample,probeStatus;
#define PROBE_STORE (&probeStore)
#define PROBE_SAMPLE probeSample
#define PROBE_STATUS probeStatus
static uint16_t probeSource(uint32_t address);
#endif
static uint16_t probeWord(uint16_t a){uint16_t value=0;if(!willowNativeTaskReadWord(PROBE_STORE,a,&value))PROBE_STATUS=1;return value;}
static uint8_t probeByte(uint16_t a){uint8_t*p=willowNativeTaskResolve(PROBE_STORE,a);if(!p){PROBE_STATUS=2;return 0;}return *p;}
static void probePutWord(uint16_t a,uint16_t v){if(!willowNativeTaskWriteWord(PROBE_STORE,a,v))PROBE_STATUS=3;}
static void probePutByte(uint16_t a,uint8_t v){uint8_t*p=willowNativeTaskResolve(PROBE_STORE,a);if(!p){PROBE_STATUS=4;return;}*p=v;}
void taskStoreProbe(void){
 const WillowVisibilityBus bus={probeWord,probeByte,probePutWord,probePutByte};
 uint16_t sample=PROBE_SAMPLE;PROBE_STATUS=0;
 willowArcadeTaskInitStacks(&bus,probeSource);willowArcadeTaskInitFree(&bus,probeSource);
 probePutWord(0x801a,(sample&63)*4);
 for(uint16_t n=0;n<80;n++)willowArcadeEventCommand(&bus,n*977+sample,n*317);
 for(uint16_t n=0;n<9;n++)if(willowArcadeTaskAllocate(&bus,0x582c,n,n*977)!=(n<8))PROBE_STATUS=5;
 for(uint16_t n=0;n<8;n++){
  probePutWord(0x0204,0xff);probePutWord(0x0206,0x100+n*32);
  willowArcadeTaskFinish(&bus,1);
 }
 if(!PROBE_STATUS)PROBE_STATUS=0x544b;
}
