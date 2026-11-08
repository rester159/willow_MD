#include "../src/willow_native_tile_mirror_transpose.h"
#include "../src/willow_native_attribute_mirror.h"
#include "../src/willow_native_intro_attribute_mirror.h"
/* Independent target diagnostic; control and data are private test RAM. */
void tileMirrorProbe(void)
{
 volatile uint16_t *command=(volatile uint16_t*)0xffff0100UL;
 uint16_t *storage=(uint16_t*)0xffff2000UL;
 uint16_t converted=command[1],result=0;
 switch(command[0]){
 case 0:result=willowTileMirrorConvert(storage,&converted,command[2]);break;
 case 1:result=willowTileMirrorWrite(storage,converted,command[2],command[3]);break;
 case 15:result=willowIntroAttributeWrite((WillowIntroAttributeMirror*)storage,command[2],command[3]);break;
 case 16:result=0xffff;willowIntroAttributeRead((WillowIntroAttributeMirror*)storage,command[2],&result);break;
 case 17:result=willowIntroAttributeWriteOwner((WillowIntroAttributeMirror*)storage,command[2],command[3]);break;
 case 18:if(command[2]<WILLOW_TILE_MIRROR_CELLS)result=willowIntroAttributeOwner((WillowIntroAttributeMirror*)storage,command[2]);break;
 case 7:result=willowAttributeMirrorWrite((WillowAttributeMirror*)storage,command[2],command[3]);break;
 case 8:if(command[2]<WILLOW_TILE_MIRROR_CELLS)result=willowAttributeMirrorRead((WillowAttributeMirror*)storage,command[2]);break;
 case 9:result=willowAttributeMirrorWriteOwner((WillowAttributeMirror*)storage,command[2],command[3]);break;
 case 10:if(command[2]<WILLOW_TILE_MIRROR_CELLS)result=willowAttributeMirrorOwner((WillowAttributeMirror*)storage,command[2]);break;
 case 3:willowMirrorTransposeInit((uint8_t*)storage,(WillowMirrorTranspose*)0xffff0120UL);result=1;break;
 case 4:result=willowMirrorTransposeStep((uint8_t*)storage,(WillowMirrorTranspose*)0xffff0120UL,command[2]);break;
 case 5:if(command[2]<WILLOW_TILE_MIRROR_CELLS)result=willowMirrorTransposeRead((uint8_t*)storage,(WillowMirrorTranspose*)0xffff0120UL,command[2]);break;
 case 6:result=willowMirrorTransposeWrite((uint8_t*)storage,(WillowMirrorTranspose*)0xffff0120UL,command[2],command[3]);break;
 case 2:if(command[2]<WILLOW_TILE_MIRROR_CELLS)result=willowTileMirrorRead(storage,converted,command[2]);break;
 }
 command[1]=converted;command[4]=result;
}
