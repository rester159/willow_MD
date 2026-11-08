#include "../src/willow_arcade_dialogue.h"
#include "../build/arcade_intro_text.h"
#include <stdio.h>
#include <stdlib.h>
static unsigned char program[0x100000];static unsigned frame;static FILE*writes;
static unsigned short word(unsigned long a){if(a+1>=sizeof(program))abort();return program[a]*256+program[a+1];}
static void emit(unsigned long address,unsigned short code,unsigned short attr){fprintf(writes,"%u,%lu,%u,%u\n",frame,address,code,attr);}
int main(void){
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f)return 2;
 if(fread(program,1,sizeof(program),f)!=sizeof(program))return 2;fclose(f);
 f=fopen("build/arcade-trace/actor-native/controller.csv","r");if(!f)return 2;
 writes=fopen("build/native-dialogue-writes.csv","w");if(!writes)return 2;fputs("frame,address,code,attribute\n",writes);
 WillowArcadeDialogue d;willowArcadeDialogueInit(&d,&intro_message_1049);
 char line[256];fgets(line,sizeof(line),f);unsigned major,state,timer,target,camera,gate,busy,checks=0;
 while(fgets(line,sizeof(line),f)){
  if(sscanf(line,"%u,%u,%u,%u,%u,%u,%u,%u",&frame,&major,&state,&timer,&target,&camera,&gate,&busy)!=8)return 2;
  if(frame<1153)continue;if(state!=20)break;
  willowArcadeDialogueStep(&d,word,emit);
  if(d.text.busy!=busy){fprintf(stderr,"dialogue completion mismatch frame %u native %u source %u\n",frame,d.text.busy,busy);return 1;}
  checks++;
 }
 fclose(f);fclose(writes);printf("Dialogue task completion: %u consecutive frames match\n",checks);return checks>200?0:1;
}
