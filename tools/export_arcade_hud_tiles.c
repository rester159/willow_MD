/* Enumerate glyph/palette keys from translated source writes, not guessed UI. */
#include "../src/willow_arcade_player_hud.h"
#include "../native/intro_probe/src/player_data.h"
#include <stdio.h>
#include <stdlib.h>
static uint16_t text[8192];
static uint32_t keys[65536];
static uint16_t word(unsigned long at){uint16_t v;if(!nativePlayerDataRead(at,&v))abort();return v;}
static void writeText(uint32_t at,uint16_t v){
 if(at<0x910000 || at>=0x914000 || (at&1))abort();
 unsigned n=(at-0x910000)/2;text[n]=v;
 if(n&1)keys[text[n-1]]|=1UL<<(v&31);
}
static void command(uint16_t a,uint16_t b){(void)a;(void)b;}
int main(void){
 WillowPlayerHealth h={.hp=8,.maxHp=8,.armor=8,.textWrite=writeText};
 for(unsigned n=0;n<12;n++)h.items[n]=3;
 WillowPlayerCollision c={word,0,0,0,0,0};
 WillowPlayerCharge q={.textWrite=writeText};
 WillowPlayerHud d={.time={0x12,0x34},.gold={0,0x56,0x78,0x90},
  .score={0xab,0xcd,0xef,0x01},.otherScore={0},.highScore={0},.lives=0,.command=command};
 willowPlayerHudLabels(&h,&c);
 for(unsigned attr=0;attr<4;attr++)if(attr!=1)for(unsigned value=0;value<256;value++){
  uint8_t byte=value;willowHudNumber(&h,0x910000,&byte,1,0,attr);
 }
 h.hp=0;willowPlayerHealthHud(&h);h.hp=8;
 for(unsigned character=0;character<2;character++)for(unsigned level=0;level<7;level++)for(unsigned alternate=0;alternate<5;alternate++){
  q.level=level;q.alternateLevel=alternate;
  willowPlayerHudInit(&d,&h,&q,&c,character?4:0);
  uint8_t r[128]={0};r[32]=character?4:0;r[49]=1;
  uint16_t cap=character?0x8fff:word(0x4c08UL+level*2);
  for(unsigned value=0;value<cap;value+=64){q.value=value;willowPlayerChargeStep(&q,&c,r);}
 }
 puts("code,palette");
 for(unsigned code=0;code<65536;code++)for(unsigned palette=0;palette<32;palette++)if(keys[code]&(1UL<<palette))printf("%u,%u\n",code,palette);
 return 0;
}
