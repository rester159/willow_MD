/* Bare-metal 68000 composition probe: no video setup, SDK calls or UI. */
#include "../src/willow_native_hud_surface.h"
#include "../native/intro_probe/src/hud.h"
extern const uint8_t hudProjectionData[];
__asm__(".section .rodata\n.balign 2\n.global hudProjectionData\nhudProjectionData:\n.incbin \"native/intro_probe/res/hud_projected.bin\"\n.text\n");
static inline __attribute__((always_inline)) const uint8_t*projection(uint16_t code,uint16_t attr,uint8_t col,uint8_t phase,uint8_t side)
{
 if(code==0x71a6 && !(attr&31))return hudProjectionData;
 if(code<NATIVE_HUD_CODE_MIN || code>NATIVE_HUD_CODE_MAX)return 0;
 uint16_t row=native_hud_directory[code-NATIVE_HUD_CODE_MIN];if(row==65535)return 0;
 uint16_t record=native_hud_palette_rows[row][attr&31];if(!record)return 0;
 unsigned low=record-1;
 unsigned id=native_hud_codes[low][2+(col&1)];
 return hudProjectionData+native_hud_projection_ids[id][((attr&32)?10:0)+phase*2+side]*32;
}
void hudProbe(void)
{
 const WillowHudSurface*s=(const WillowHudSurface*)0xff2000UL;
 uint32_t*out=(uint32_t*)0xff3000UL;
 volatile uint16_t*result=(volatile uint16_t*)0xff1000UL;
 unsigned first=result[1]?result[2]:0,rows=result[1]?first+1:6,tiles=result[1]?4:40;
 *result=0;
 for(unsigned row=first;row<rows;row++)for(unsigned tile=0;tile<tiles;tile++,out+=8)
  if(!willowHudSurfaceCompose(s,row,tile,(uint8_t*)out,projection)){*result=2;return;}
 *result=0x4844;
}
