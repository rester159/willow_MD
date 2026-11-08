#include "../src/willow_native_hud_sprite.h"
#include <assert.h>
#include <stdio.h>
int main(void){
 unsigned checks=0;assert(sizeof(WillowNativeHudSprite)==4);
 for(unsigned col=0;col<64;col++)for(unsigned row=0;row<32;row++)
 for(unsigned w=1;w<=4;w++)for(unsigned h=1;h<=4;h++){
  unsigned short tile=(col*977+row*317+w*123+h*7919)&65535;
  WillowNativeHudSprite s=willowHudSpriteMake(col,row,w,h,tile);
  assert(willowHudSpriteCol(&s)==col&&willowHudSpriteRow(&s)==row);
  assert(willowHudSpriteWidth(&s)==w&&willowHudSpriteHeight(&s)==h&&s.tile==tile);
  assert(!(s.layout&0x8000));checks++;
 }
 printf("Packed HUD sprite: %u complete hardware geometry combinations preserve position, dimensions and full tile words in four bytes\n",checks);
}
