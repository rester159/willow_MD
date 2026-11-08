#include "../src/willow_native_hud_surface.h"
#include "../src/willow_arcade_player_clock.h"
#include "../native/intro_probe/src/player_data.h"
#include "../native/intro_probe/src/hud.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint8_t *patterns;
static const uint8_t*projection(uint16_t code,uint16_t attr,uint8_t col,uint8_t phase,uint8_t side){
 if(code==0x71a6 && !(attr&31))return patterns;
 if(code<NATIVE_HUD_CODE_MIN || code>NATIVE_HUD_CODE_MAX)return 0;
 uint16_t row=native_hud_directory[code-NATIVE_HUD_CODE_MIN];if(row==65535)return 0;
 uint16_t record=native_hud_palette_rows[row][attr&31];if(!record)return 0;
 unsigned low=record-1;
 unsigned id=native_hud_codes[low][2+(col&1)];
 return patterns+native_hud_projection_ids[id][((attr&32)?10:0)+phase*2+side]*32;
}
static WillowHudSurface surface;
static unsigned commands;
static uint16_t sourceWord(unsigned long at){uint16_t v;if(!nativePlayerDataRead(at,&v))abort();return v;}
static void writeText(uint32_t at,uint16_t value){if(!willowHudSurfaceWrite(&surface,at,value))abort();}
static void command(uint16_t c,uint16_t arg){if(c!=12 || arg)abort();commands++;}
int main(void){
 FILE*f=fopen("native/intro_probe/res/hud_projected.bin","rb");if(!f)return 2;
 patterns=malloc(NATIVE_HUD_PROJECTIONS*32);if(!patterns || fread(patterns,32,NATIVE_HUD_PROJECTIONS,f)!=NATIVE_HUD_PROJECTIONS)return 2;fclose(f);
 FILE*cells=fopen("build/native-hud-initial-cells.bin","wb"),*tiles=fopen("build/native-hud-initial-tiles.bin","wb");if(!cells || !tiles)return 2;
 for(unsigned dip=0;dip<16;dip++){
  WillowPlayerHud d={.command=command};WillowPlayerHealth h={.textWrite=writeText};
  WillowPlayerCharge q={.textWrite=writeText};WillowPlayerClock t={0};WillowPlayerCollision c={sourceWord,0,0,0,0,0};
  willowPlayerHudColdStage0(&d,&h,&q,&t,&c,dip);willowHudSurfaceInit(&surface);
  willowPlayerHudInit(&d,&h,&q,&c,0);willowPlayerHudLabels(&h,&c);
  for(unsigned phase=0;phase<2;phase++){
   if(phase)willowPlayerClockStep(&t,&d,&h);
   for(unsigned row=0;row<6;row++)for(unsigned col=0;col<48;col++)for(unsigned w=0;w<2;w++){
    uint8_t bytes[2];willowMotionPutWord(bytes,surface.cells[row][col][w]);if(fwrite(bytes,1,2,cells)!=2)return 2;
   }
   for(unsigned row=0;row<6;row++)for(unsigned col=0;col<40;col++){
    uint8_t tile[32];if(!willowHudSurfaceCompose(&surface,row,col,tile,projection) || fwrite(tile,1,32,tiles)!=32)return 2;
   }
  }
 }
 fclose(cells);fclose(tiles);free(patterns);
 if(commands!=16)return 1;
 puts("Initial HUD: 16 ROM/DIP-derived states and first clock updates exported without captured state inputs");return 0;
}
