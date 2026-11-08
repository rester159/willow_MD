#include "../src/willow_native_hud_surface.h"
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
int main(void){
 if(sizeof(WillowHudSurface)!=1188)return 1;
 FILE*f=fopen("native/intro_probe/res/hud_projected.bin","rb");if(!f)return 2;
 patterns=malloc(NATIVE_HUD_PROJECTIONS*32);if(!patterns)return 2;
 if(fread(patterns,32,NATIVE_HUD_PROJECTIONS,f)!=NATIVE_HUD_PROJECTIONS)return 2;fclose(f);
 WillowHudSurface s;willowHudSurfaceInit(&s);
 const unsigned rows[]={2,3,26,28,29,30};unsigned dirtyChecks=0;
 for(unsigned row=0;row<6;row++)for(unsigned col=0;col<48;col++)for(unsigned w=0;w<2;w++){
  memset(s.dirty,0,sizeof s.dirty);
  uint32_t at=0x910000UL+col*128+rows[row]*4+w*2;
  if(!willowHudSurfaceWrite(&s,at,s.cells[row][col][w]^1))return 1;
  for(unsigned r=0;r<6;r++)for(unsigned t=0;t<40;t++){
   unsigned expected=0;
   for(unsigned x=0;x<8;x++)if(r==row && ((6*(t*8+x)+3)/5)/8==col)expected=1;
   if(((s.dirty[r][t>>4]>>(t&15))&1)!=expected)return 1;
   dirtyChecks++;
  }
  memset(s.dirty,0,sizeof s.dirty);willowHudSurfaceWrite(&s,at,s.cells[row][col][w]);
  for(unsigned r=0;r<6;r++)for(unsigned t=0;t<3;t++)if(s.dirty[r][t])return 1;
 }
 WillowHudSurface before=s;
 const uint32_t invalid[]={0x90fffe,0x911800,0x910001,0x910010,0x914000};
 for(unsigned n=0;n<sizeof invalid/sizeof invalid[0];n++)if(willowHudSurfaceWrite(&s,invalid[n],1) || memcmp(&s,&before,sizeof s))return 1;
 const unsigned frames[]={1588,1590,1600};uint8_t source[16384],tile[32];
 for(unsigned frame=0;frame<3;frame++)for(unsigned flips=0;flips<2;flips++){
  char path[128];snprintf(path,sizeof path,"build/arcade-trace/actor-native/text-%u.bin",frames[frame]);
  f=fopen(path,"rb");if(!f || fread(source,1,sizeof source,f)!=sizeof source)return 2;fclose(f);
  willowHudSurfaceInit(&s);
  for(unsigned row=0;row<6;row++)for(unsigned col=0;col<48;col++)for(unsigned w=0;w<2;w++){
   unsigned at=col*128+rows[row]*4+w*2;uint16_t value=willowMotionWord(source+at);
   if(w && flips)value^=((col&1)?32:0)|((col&2)?64:0);
   if(!willowHudSurfaceWrite(&s,0x910000UL+at,value) || s.cells[row][col][w]!=value)return 1;
  }
  snprintf(path,sizeof path,"build/hud-surface-%u-%u.bin",frames[frame],flips);f=fopen(path,"wb");if(!f)return 2;
  for(unsigned row=0;row<6;row++)for(unsigned col=0;col<40;col++){
   if(!willowHudSurfaceCompose(&s,row,col,tile,projection))return 1;
   if(fwrite(tile,1,32,f)!=32)return 2;
  }
  fclose(f);
 }
 printf("HUD surface: %u dirty-tile dependency checks; retained source words, unchanged-write guard, bounds, and six composed frames pass; %zu RAM bytes\n",dirtyChecks,sizeof s);
 free(patterns);return 0;
}
