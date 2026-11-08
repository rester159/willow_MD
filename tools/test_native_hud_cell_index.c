#include "../src/willow_native_hud_cell_index.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
int main(void){
 struct {unsigned char before[16];WillowHudCellIndex index;unsigned char after[16];} storage;
 unsigned char reference[200];unsigned checks=0;
 memset(&storage,255,sizeof storage);memset(reference,255,sizeof reference);
 assert(sizeof(WillowHudCellIndex)==150);
 for(unsigned pass=0;pass<41;pass++)for(unsigned cell=0;cell<200;cell++){
  /* Permute positions, retain earlier writes, and include empty transitions. */
  unsigned at=(cell*73+pass*17)%200;
  unsigned char value=pass==40?255:(at+pass)%40;
  willowHudCellIndexPut(&storage.index,at,value);reference[at]=value;
  for(unsigned n=0;n<200;n++)assert(willowHudCellIndexGet(&storage.index,n)==reference[n]);
  for(unsigned n=0;n<16;n++)assert(storage.before[n]==255&&storage.after[n]==255);
  checks++;
 }
 printf("Packed HUD index: %u retained writes, %u independent cell comparisons; all sprite slots, empty marker and adjacent cells preserved in 150 bytes\n",checks,checks*200);
 return 0;
}
