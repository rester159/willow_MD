/* Retained-memory side of the original-instruction clear oracle. Expected
 * graphics words are independent raw storage, checked at every fill return. */
#include "../src/willow_native_scene_memory.h"
static WillowNativeScroll1 sceneText;
static WillowNativeObjectBanks sceneObjects;
static WillowHudSurface sceneHud;
static uint16_t sceneNear[4096],sceneFar[WILLOW_SCROLL3_MAP_WORDS];
static const uint32_t sceneNearDictionary[]={0x12345678,0,0x98765432};
static const uint32_t sceneFarDictionary[]={0x54321234,0x0a000000,0xabcd5678};
static WillowMapStore sceneNearStore={sceneNear,sceneNearDictionary,3,0,0,0};
static WillowNativeSceneMemory sceneMemory={&sceneText,&sceneNearStore,sceneFar,
 sceneFarDictionary,&sceneHud,&sceneObjects,3,1,1};
static uint16_t sceneExpected[5][8192],sceneDisplay[1024];
static unsigned sceneFills,sceneChecks;
static unsigned sceneNextRegion;
static uint16_t sceneCounts[2];
static const uint32_t sceneBases[]={0x910000,0x918000,0x914000,0x900000,0x908000};
static void sceneCheck(void)
{
 for(unsigned region=0;region<5;region++)for(unsigned n=0;n<(region<3?8192u:1024u);n++){
  uint16_t actual;
  if(!region)actual=sceneText.words[n];
  else if(region<3){
   uint32_t pair=region==1?willowMapStorePair(&sceneNearStore,n/2):
    sceneFarDictionary[willowScroll3MapId(sceneFar,n/2)];
   actual=(n&1)?pair:pair>>16;
  }else actual=sceneObjects.bank[region-3][n];
  if(actual!=sceneExpected[region][n]){
   fprintf(stderr,"Retained scene mismatch region %u word %u after fill %u\n",region,n,sceneFills);exit(1);
  }
 }
 if(memcmp(sceneObjects.displayed,sceneDisplay,sizeof sceneDisplay))exit(1);
 static const unsigned rows[]={2,3,26,28,29,30};
 for(unsigned r=0;r<6;r++)for(unsigned x=0;x<48;x++)for(unsigned w=0;w<2;w++)
  if(sceneHud.cells[r][x][w]!=sceneExpected[0][x*64+rows[r]*2+w])exit(1);
 sceneChecks++;
}
static void sceneSeed(unsigned seed)
{
 sceneNextRegion=255;sceneCounts[0]=rw(0x829c);sceneCounts[1]=rw(0x829e);
 for(unsigned n=0;n<8192;n++)sceneText.words[n]=sceneExpected[0][n]=(seed*977+n*17)^0xa55a;
 for(unsigned cell=0;cell<4096;cell++){
  unsigned near=(seed+cell)%3,far=(seed+cell*2)%3;
  willowMapStorePutCell(&sceneNearStore,cell,near);willowScroll3MapSet(sceneFar,cell,far);
  sceneExpected[1][cell*2]=sceneNearDictionary[near]>>16;
  sceneExpected[1][cell*2+1]=sceneNearDictionary[near];
  sceneExpected[2][cell*2]=sceneFarDictionary[far]>>16;
  sceneExpected[2][cell*2+1]=sceneFarDictionary[far];
 }
 for(unsigned n=0;n<1024;n++){
  sceneObjects.bank[0][n]=sceneExpected[3][n]=(seed*43+n*7)^0x5a5a;
  sceneObjects.bank[1][n]=sceneExpected[4][n]=(seed*73+n*11)^0xa5a5;
  sceneObjects.displayed[n]=sceneDisplay[n]=seed*131+n*23;
 }
 memset(&sceneHud,0,sizeof sceneHud);
 for(unsigned n=0;n<8192;n++)
  willowHudSurfaceWrite(&sceneHud,0x910000UL+n*2,sceneText.words[n]);
 for(unsigned n=0;n<videoSize/2;n++){
  unsigned region=0;while(sceneBases[region]!=videoBase)region++;
  ww(0x2000+n*2,sceneExpected[region][n]);
 }
 sceneCheck();
}
static void sceneFill(uint32_t at,uint16_t first,uint16_t second,uint16_t pairs)
{
 unsigned region=0;while(region<5&&sceneBases[region]!=at)region++;
 if(region==5||pairs!=(region<3?4096:512)||second||
  first!=(region==0?0x7020:region==2?0x0a00:0))exit(1);
 if(sceneNextRegion==255){if(region!=0&&region!=3)exit(1);}
 else if(region!=sceneNextRegion)exit(1);
 sceneNextRegion=region+1;
 if(rw(0x829c)!=(region<3?sceneCounts[0]:0)||
    rw(0x829e)!=(region<3?sceneCounts[1]:0))exit(1);
 if(!willowNativeSceneMemoryFill(&sceneMemory,at,first,second,pairs))exit(1);
 for(unsigned n=0;n<pairs;n++){
  sceneExpected[region][n*2]=first;sceneExpected[region][n*2+1]=second;
  videoWord(at+n*4,first);videoWord(at+n*4+2,second);
 }
 sceneFills++;sceneCheck();
}
