#include "../src/willow_arcade_text_box.h"
#include <stdio.h>
#include <stdlib.h>
static unsigned char program[0x100000];static unsigned phase;
static unsigned short word(unsigned long a){if(a+1>=sizeof(program))abort();return program[a]*256+program[a+1];}
static void emit(unsigned long address,unsigned short code,unsigned short attr){printf("%u,%lu,%u,%u\n",phase,address,code,attr);}
int main(void){
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f)return 2;
 if(fread(program,1,sizeof(program),f)!=sizeof(program))return 2;fclose(f);
 puts("phase,address,code,attribute");
 for(phase=0;phase<5;phase++)willowArcadeTextBoxPhase(phase,word,emit);
 return 0;
}
