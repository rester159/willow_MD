/* Closed native C binding: immutable original ROM, original WRAM owner, and
 * CPS writes relocated into diagnostic WRAM spans exactly like source oracle.
 * No source/game state is supplied from captures. Not a shipping ROM target. */
#include "../src/willow_arcade_gameplay_services.h"
#include "../src/willow_arcade_shop_resume.h"
static uint16_t readWord(uint16_t at){return *(volatile uint16_t*)(0xff0000UL+at);}
static uint8_t readByte(uint16_t at){return *(volatile uint8_t*)(0xff0000UL+at);}
static void writeWord(uint16_t at,uint16_t value){*(volatile uint16_t*)(0xff0000UL+at)=value;}
static void writeByte(uint16_t at,uint8_t value){*(volatile uint8_t*)(0xff0000UL+at)=value;}
static uint16_t romWord(uint32_t at){return *(const uint16_t*)at;}
static uint16_t collisionWord(unsigned long at){return *(const uint16_t*)at;}
static void videoWord(uint32_t at,uint16_t value){
 if(at>=0x900000&&at<0x902020)writeWord(0xa000+at-0x900000,value);
 else if(at>=0x917aa8&&at<0x917ad0)writeWord(0x7000+at-0x917aa8,value);
 else if(at>=0x910000&&at<0x914000)writeWord(0x4000+at-0x910000,value);
 else writeWord(0x300c,0xbad);
}
void*memcpy(void*to,const void*from,unsigned long bytes){uint8_t*d=to;const uint8_t*s=from;while(bytes--)*d++=*s++;return to;}
void*memset(void*to,int value,unsigned long bytes){uint8_t*d=to;while(bytes--)*d++=(uint8_t)value;return to;}
void shopNativeProbe(void){
 const WillowVisibilityBus bus={readWord,readByte,writeWord,writeByte};
 WillowPlayerCollision collision={0};collision.word=collisionWord;
 uint16_t d4=readWord(0x3000);uint32_t a4=((uint32_t)readWord(0x3004)<<16)|readWord(0x3006);
 WillowArcadeGameplayServices service={.bus=&bus,.busWord=romWord,.collision=&collision,
  .d4=&d4,.a4=&a4,.videoWrite=videoWord,.textWrite=videoWord};
 WillowArcadeShopTask task={0};
 uint16_t result=willowArcadeShopResume(&task,&bus,romWord,willowArcadeGameplayServiceResume,&service);
 writeWord(0x3000,d4);writeWord(0x3004,a4>>16);writeWord(0x3006,a4);
 writeWord(0x3008,result);writeWord(0x300a,task.pc);
}
