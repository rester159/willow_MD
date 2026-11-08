#include "native_frame_profile.h"
// Integration test against the installed Genesis Plus GX libretro core.
// clang -O2 tools/rom_smoke.c -o build/rom_smoke
#include "vendor/libretro.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include "../native/intro_probe/src/scroll2_data.h"
#define u16 uint16_t
#include "../native/intro_probe/src/forest.h"
#include "../native/intro_probe/src/layers.h"
#undef u16
#include "../native/intro_probe/src/scroll3_data.h"
#include "../native/intro_probe/src/background_pairs.h"
#include "../src/willow_arcade_scroll3_map_store.h"
#include "../src/willow_native_column_stream.h"
#include "../src/willow_native_background_map.h"
#include "../src/willow_native_boot_globals.h"
typedef uint32_t u32;typedef uint16_t u16;typedef int16_t s16;
#include "../native/intro_probe/src/player_sprites.h"
static int frame,fmt=0,two_player=0,palette_probe=0,pose_probe=0,walk_probe=0;
static unsigned char *probe_ram,*probe_vram;
static unsigned char backgroundIntro[INTRO_TILE_COUNT*32],backgroundExtra[(NATIVE_BACKGROUND_ID_LIMIT-NATIVE_BACKGROUND_EXTENSION_BASE)*32];
static unsigned tick_at;
static unsigned scenePixelCheck,sceneNonuniform,scenePixelFrames;
static unsigned hudIndexChecks;
static bool env(unsigned cmd,void *data){
 if(cmd==RETRO_ENVIRONMENT_SET_PIXEL_FORMAT){fmt=*(int*)data;return true;}
 if(cmd==RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY || cmd==RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY){*(const char**)data="/tmp";return true;}
 if(cmd==RETRO_ENVIRONMENT_GET_VARIABLE){struct retro_variable*v=data; if(!strcmp(v->key,"genesis_plus_gx_addr_error")){v->value="enabled";return true;} if(!strcmp(v->key,"genesis_plus_gx_system_hw")){v->value="auto";return true;} return false;}
 if(cmd==RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE){*(bool*)data=false;return true;}
 if(cmd==RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION){*(unsigned*)data=0;return true;}
 if(cmd==RETRO_ENVIRONMENT_SET_VARIABLES || cmd==RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME || cmd==RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS || cmd==RETRO_ENVIRONMENT_SET_CONTROLLER_INFO) return true;
 return false;
}
static void video(const void*data,unsigned w,unsigned h,size_t pitch){
 if(!data || !probe_ram)return;
 if(scenePixelCheck){
  unsigned bytes=fmt==1?4:2;sceneNonuniform=0;scenePixelFrames++;
  for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++)
   sceneNonuniform+=memcmp(data,(const unsigned char*)data+y*pitch+x*bytes,bytes)!=0;
  return;
 }
 unsigned tick=probe_ram[tick_at]|(probe_ram[tick_at+1]<<8);
 if(!(walk_probe && tick>930 && (tick%10==0 || tick==936 || tick==976)) && !(pose_probe && tick>930 && tick<=930+NATIVE_PLAYER_FRAMES*4 && (tick-930)%4==3) && !(palette_probe && tick>930 && tick<=950) && !(tick==0 && frame==1599) && tick!=1 && tick!=70 && tick!=130 && tick!=190 && tick!=250 && tick!=325 && tick!=400 && !(tick>=260 && tick<=400 && tick%10==0) && !(tick>=326 && tick<=356) && tick!=464 && tick!=468 && tick!=472 && tick!=476 && tick!=480 && tick!=630 && tick!=800 && tick!=810 && tick!=830 && tick!=850 && tick!=890 && tick!=898 && tick!=899 && tick!=900 && tick!=901 && tick!=915 && tick!=916 && tick!=920 && tick!=930)return;
 char name[128];snprintf(name,sizeof(name),walk_probe && tick>930?"build/player-walk-native-%u.ppm":pose_probe && tick>930?"build/player-pose-probe-%u.ppm":palette_probe && tick>930?(palette_probe==2?"build/player-damage-probe-%u.ppm":"build/player-palette-probe-%u.ppm"):"build/intro-probe-%u.ppm",tick);FILE*f=fopen(name,"wb");fprintf(f,"P6\n%d %d\n255\n",w,h);
 for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++){unsigned c;unsigned char rgb[3];if(fmt==1){c=*(uint32_t*)((char*)data+y*pitch+x*4);rgb[0]=c>>16;rgb[1]=c>>8;rgb[2]=c;}else{c=*(uint16_t*)((char*)data+y*pitch+x*2);rgb[0]=((c>>(fmt==2?11:10))&31)*255/31;rgb[1]=((c>>5)&(fmt==2?63:31))*255/(fmt==2?63:31);rgb[2]=(c&31)*255/31;}fwrite(rgb,1,3,f);}fclose(f);
}
static void audio(int16_t l,int16_t r){}
static size_t batch(const int16_t*d,size_t n){return n;}
static void poll(void){}
static int16_t input(unsigned p,unsigned d,unsigned i,unsigned id){
 if(!walk_probe || !probe_ram || p || d!=RETRO_DEVICE_JOYPAD || id!=RETRO_DEVICE_ID_JOYPAD_RIGHT)return 0;
 unsigned tick=probe_ram[tick_at]|(probe_ram[tick_at+1]<<8);
 return tick>=935 && tick<975;
}
#define FN(ret,n,args) ret(*n)args=dlsym(core,#n)
#include "native_symbol_lookup.h"
static unsigned symbol(const char*path,const char*name){return fullSymbol(path,name)&65535;}
static unsigned nativeBootGlobalOffset(unsigned address){
 WillowNativeBootGlobals globals;
 unsigned char*p=willowNativeBootGlobalsResolve(&globals,address,1);
 if(!p){fprintf(stderr,"Unmapped boot fixture address %04x\n",address);exit(2);}
 return (unsigned)(p-globals.bytes);
}
/* Core RAM is byte-swapped; packed native fields can begin at odd addresses. */
static int word(const unsigned char*ram,unsigned address){return (int16_t)((ram[address^1]<<8)|ram[(address+1)^1]);}
#define SMOKE_FAIL() do {fprintf(stderr,"Smoke failure at line %d frame %d tick %d\n",__LINE__,frame,probe_ram?word(probe_ram,tick_at):-1);return 1;} while(0)
static unsigned long longword(const unsigned char*ram,unsigned a){return ((unsigned long)(unsigned short)word(ram,a)<<16)|(unsigned short)word(ram,a+2);}
static unsigned definition(const char*path,const char*name){
 FILE*f=fopen(path,"r");char line[256],key[128];unsigned value;if(!f)exit(2);
 while(fgets(line,sizeof line,f))if(sscanf(line,"#define %127s %u",key,&value)==2 && !strcmp(key,name)){fclose(f);return value;}
 fclose(f);exit(2);
}
#include "native_target_memory.h"
#include "native_palette_task_smoke.h"
#include "native_video_publish_smoke.h"
#include "native_plane_hide_smoke.h"
#include "native_sat_publication_smoke.h"
#include "native_player_sound_smoke.h"
#include "native_player_rng_smoke.h"
#include "native_player_cancel_smoke.h"
#include "native_frame_clock_smoke.h"
#include "native_replay_phase_smoke.h"
#include "native_source_input_smoke.h"
#include "native_controller_history_smoke.h"
#include "native_boot_continuation_smoke.h"
#include "native_boot_fade_prefix_smoke.h"
#include "native_scene_plane_commit_smoke.h"
#include "native_scene_text_commit_smoke.h"
#include "native_scene_pools_smoke.h"
#include "native_scene_spawn_smoke.h"
#include "native_opening_actors_smoke.h"
#include "native_backdrop_setup_smoke.h"
#include "native_scene_map_fill_smoke.h"
#include "native_sprite_bank_smoke.h"
#include "native_object_cartridge_smoke.h"
#include "native_opening_message_smoke.h"
#include "native_opening_fades_smoke.h"
#include "native_opening_clear_smoke.h"
#include "native_opening_pan_smoke.h"
#include "native_phase_four_text_smoke.h"
#include "native_demo_state_smoke.h"
#include "native_demo_access_smoke.h"
#include "native_pan_concurrent_smoke.h"
#include "native_demo_init_smoke.h"
#include "native_demo_start_smoke.h"
#include "native_demo_update_smoke.h"
int main(int argc,char **argv){
 if(argc!=4 && (argc!=5 || (strcmp(argv[4],"--scene-countdown-probe") && strcmp(argv[4],"--scene-spawn-probe") && strcmp(argv[4],"--scene-dispatch-probe") && strcmp(argv[4],"--scene-initialize-probe") && strcmp(argv[4],"--scene-pools-probe") && strcmp(argv[4],"--scene-attract-fade-probe") && strcmp(argv[4],"--scene-attract-init-probe") && strcmp(argv[4],"--scene-handoff-probe") && strcmp(argv[4],"--scene-service-handoff-probe") && strcmp(argv[4],"--scene-warning-probe") && strcmp(argv[4],"--scene-text-probe") && strcmp(argv[4],"--scene-combined-probe") && strcmp(argv[4],"--scene-boot-probe") && strcmp(argv[4],"--object-cartridge-probe") && strcmp(argv[4],"--sprite-bank-probe") && strcmp(argv[4],"--scene-map-fill-probe") && strcmp(argv[4],"--boot-fade-prefix-probe") && strcmp(argv[4],"--boot-continuation-probe") && strcmp(argv[4],"--controller-history-probe") && strcmp(argv[4],"--source-input-probe") && strcmp(argv[4],"--replay-phase-probe") && strcmp(argv[4],"--frame-clock-probe") && strcmp(argv[4],"--player-cancel-probe") && strcmp(argv[4],"--player-rng-probe") && strcmp(argv[4],"--player-sound-probe") && strcmp(argv[4],"--sat-publication-probe") && strcmp(argv[4],"--source-backdrop-hide-probe") && strcmp(argv[4],"--layer-palette-hide-probe") && strcmp(argv[4],"--layer-window-clear-probe") && strcmp(argv[4],"--layer-clear-probe") && strcmp(argv[4],"--video-publish-probe") && strcmp(argv[4],"--palette-probe") && strcmp(argv[4],"--damage-probe") && strcmp(argv[4],"--pose-probe") && strcmp(argv[4],"--walk-probe") && strcmp(argv[4],"--class18-probe") && strcmp(argv[4],"--class10-probe") && strcmp(argv[4],"--class14-probe") && strcmp(argv[4],"--boot-globals-probe") && strcmp(argv[4],"--task-init-probe") && strcmp(argv[4],"--startup-latched-probe") && strcmp(argv[4],"--task-lifetime-probe") && strcmp(argv[4],"--palette-owner-probe") && strcmp(argv[4],"--sat-cache-probe") && strcmp(argv[4],"--runtime-memory-probe") && strcmp(argv[4],"--all-pool-allocate-probe") && strcmp(argv[4],"--class12-release-probe") && strcmp(argv[4],"--scene-clear-probe") && strcmp(argv[4],"--scene-clear-walk-probe"))))return 2;
 walk_probe=argc==5 && (!strcmp(argv[4],"--task-lifetime-probe") || !strcmp(argv[4],"--walk-probe") || !strcmp(argv[4],"--scene-clear-walk-probe"));
 /* The fast DMA path omits the 128 KiB split check. Aligned 32/128-byte
  * blocks cannot cross that boundary, regardless of their ROM index. */
 const char*alignedTiles[]={"intro_tiles","intro_forest_tiles","intro_overlay_tiles","native_background_extra"};
 for(unsigned i=0;i<4;i++)if(symbol(argv[3],alignedTiles[i])&31){fprintf(stderr,"Unaligned fast-DMA resource %s\n",alignedTiles[i]);return 2;}
 if(symbol(argv[3],"native_player_sprite_tiles")&127){fputs("Unaligned player DMA resource\n",stderr);return 2;}
 pose_probe=argc==5 && !strcmp(argv[4],"--pose-probe");
 int class18_probe=argc==5 && !strcmp(argv[4],"--class18-probe");
 int class10_probe=argc==5 && !strcmp(argv[4],"--class10-probe");
 int class14_probe=argc==5 && !strcmp(argv[4],"--class14-probe");
 palette_probe=argc==5 && !strcmp(argv[4],"--damage-probe")?2:argc==5 && !strcmp(argv[4],"--palette-probe")?1:0;
 unsigned endTick=walk_probe?1010:pose_probe?930+NATIVE_PLAYER_FRAMES*4:palette_probe?950:930;
 int lifetime_probe=argc==5 && !strcmp(argv[4],"--task-lifetime-probe");
 unsigned taskStateNative=symbol(argv[3],"sourceTaskState"),taskRecordNative=symbol(argv[3],"taskRecords"),taskCommandNative=symbol(argv[3],"taskCommands"),bootGlobalsNative=symbol(argv[3],"sourceBootGlobals"),highScoresNative=symbol(argv[3],"sourceHighScores"),soundCommandsNative=symbol(argv[3],"soundCommands"),lifetimeChecks=0;
 unsigned nativeColors[32],controllerHistoryChecks=0;
 unsigned paletteOwnerAt=symbol(argv[3],"debugPaletteOwner"),paletteSlotsAt=symbol(argv[3],"debugPaletteSlots");
 unsigned char originalColors[192];FILE*cf=fopen("build/reference_maincpu.bin","rb");if(!cf || fseek(cf,0xe5000,SEEK_SET) || fread(originalColors,1,192,cf)!=192)return 2;fclose(cf);
 tick_at=symbol(argv[3],"debugIntroTick");
 unsigned inputStateAt=bootGlobalsNative+nativeBootGlobalOffset(0x806a);
 FILE*walkTrace=walk_probe?fopen("build/arcade-trace/player-walk/frames.bin","rb"):NULL;if(walk_probe && !walkTrace)return 2;
 unsigned normalUpdatesAt=symbol(argv[3],"debugNormalPlayerUpdates");
 unsigned dispatchAt=symbol(argv[3],"debugPlayerDispatchUpdates"),chargeAt=symbol(argv[3],"hudCharge");
 unsigned hudAt=symbol(argv[3],"debugHud"),clockAt=symbol(argv[3],"debugHudClock"),surfaceAt=symbol(argv[3],"debugHudSurface"),hudActiveAt=symbol(argv[3],"debugHudActive");
 unsigned cameraAt=symbol(argv[3],"debugScroll2Record"),mapAt=symbol(argv[3],"debugScroll2Map");
 FILE *cameraTrace=fopen("build/arcade-trace/camera-native/frames.bin","rb");if(!cameraTrace)return 2;
 unsigned char expectedCamera[17670];
 unsigned distantAt=symbol(argv[3],"debugScroll3Record"),distantCellsAt=symbol(argv[3],"debugScroll3Cells");
 FILE*distantTrace=fopen("build/arcade-trace/scroll3-walk/frames.bin","rb"),*distantMaps=fopen("build/arcade-trace/scroll3-walk/maps.bin","rb");
 if(!distantTrace || !distantMaps)return 2;
 unsigned char expectedDistant[1286],expectedDistantMap[16384];uint16_t packedDistant[WILLOW_SCROLL3_MAP_WORDS];

 unsigned objectsAt=symbol(argv[3],"debugStageObjects"),createdAt=symbol(argv[3],"debugStageObjectsCreated");
 unsigned char expectedObjects[3072];FILE *objectTrace=fopen("build/arcade-trace/actor-native/handoff-ram-1588.bin","rb");
 if(!objectTrace || fseek(objectTrace,0x8502,SEEK_SET) || fread(expectedObjects,1,sizeof(expectedObjects),objectTrace)!=sizeof(expectedObjects))return 2;
 fclose(objectTrace);
 unsigned poolsAt=symbol(argv[3],"debugActorPools"),spawnsAt=symbol(argv[3],"debugStageSpawnsCreated");
 unsigned char expectedEntryRam[65536];objectTrace=fopen("build/arcade-trace/actor-native/handoff-ram-1588.bin","rb");
 if(!objectTrace || fread(expectedEntryRam,1,sizeof(expectedEntryRam),objectTrace)!=sizeof(expectedEntryRam))return 2;
 fclose(objectTrace);
 unsigned paletteAt=symbol(argv[3],"debugPaletteControllers"),paletteCountAt=symbol(argv[3],"debugPaletteControllersCreated");
 unsigned char expectedPalettes[1024];objectTrace=fopen("build/arcade-trace/actor-native/handoff-ram-1588.bin","rb");
 if(!objectTrace || fseek(objectTrace,0xf042,SEEK_SET) || fread(expectedPalettes,1,sizeof(expectedPalettes),objectTrace)!=sizeof(expectedPalettes))return 2;
 fclose(objectTrace);
 unsigned forestAt=symbol(argv[3],"debugIntroForestRows"),planeBAt=symbol(argv[3],"planeBAttrs");
 unsigned planeAAt=symbol(argv[3],"planeAAttrs"),ownedAt=symbol(argv[3],"rowOwned"),pendingAt=symbol(argv[3],"pendingRow"),refsAt=symbol(argv[3],"refsBySlot"),idsAt=symbol(argv[3],"ids"),bucketsAt=symbol(argv[3],"tileIndex"),lookupActiveAt=symbol(argv[3],"lookupActive"),lookupBuiltAt=symbol(argv[3],"lookupBuilt"),overlayCountAt=symbol(argv[3],"overlayLoaded"),overlayFirstAt=symbol(argv[3],"overlayFirstSlot");
 unsigned restorePairsAt=fullSymbol(argv[3],"nativeIntroRestorePairs"),restoreLoadedAt=symbol(argv[3],"restorePairsLoaded");
 unsigned pendingColumnsAt=symbol(argv[3],"pendingColumns"),freeTilesAt=symbol(argv[3],"freeTiles");
 unsigned overlayBase=definition("native/intro_probe/src/layers.h","INTRO_TILE_COUNT")+definition("native/intro_probe/src/forest.h","INTRO_FOREST_TILE_COUNT");
 unsigned st=symbol(argv[3],"debugIntroState"),ti=symbol(argv[3],"debugIntroTimer"),busy=symbol(argv[3],"debugIntroTextBusy");
 unsigned writesAt=symbol(argv[3],"debugIntroTextWrites"),expectedWrites[899]={0};
 FILE *writeTrace=fopen("build/native-intro-sequence-writes.csv","r");if(!writeTrace)return 2;
 char writeLine[128];unsigned writeFrame;fgets(writeLine,sizeof(writeLine),writeTrace);
 while(fgets(writeLine,sizeof(writeLine),writeTrace))if(sscanf(writeLine,"%u,",&writeFrame)==1 && writeFrame>=691 && writeFrame<=1588)expectedWrites[writeFrame-690]++;
 fclose(writeTrace);
 for(unsigned i=1;i<=898;i++)expectedWrites[i]+=expectedWrites[i-1];
 unsigned expectedState[899]={0},expectedTimer[899]={0},expectedBusy[899]={0};
 FILE *trace=fopen("build/arcade-trace/actor-native/controller.csv","r");if(!trace)return 2;
 char traceLine[256];fgets(traceLine,sizeof(traceLine),trace);
 unsigned tf,ts,tt,ty,tc,t2,t3,tb;
 while(fgets(traceLine,sizeof(traceLine),trace))if(sscanf(traceLine,"%u,%u,%u,%u,%u,%u,%u,%u",&tf,&t2,&ts,&tt,&ty,&tc,&t3,&tb)==8 && tf>=691 && tf<=1588){expectedState[tf-690]=ts;expectedTimer[tf-690]=tt;expectedBusy[tf-690]=tb;}
 fclose(trace);
 unsigned actorAt=symbol(argv[3],"debugIntroActors"),expectedActors[898][20]={{0}};
 FILE *actorTrace=fopen("build/arcade-trace/actor-native/actors.csv","r");if(!actorTrace)return 2;
 char actorLine[256];fgets(actorLine,sizeof(actorLine),actorTrace);
 unsigned af,ac,am,as,v[8];unsigned long ap;
 while(fgets(actorLine,sizeof(actorLine),actorTrace)){
  if(sscanf(actorLine,"%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%lu",&af,&ac,&am,&as,&v[0],&v[1],&v[2],&v[3],&v[4],&v[5],&v[6],&v[7],&ap)==13 && af>=760 && af<=1587){
   for(unsigned i=0;i<8;i++)expectedActors[af-690][as*10+i]=v[i];
   expectedActors[af-690][as*10+8]=ap>>16;expectedActors[af-690][as*10+9]=ap&65535;
  }
 }
 fclose(actorTrace);
 unsigned playerAt=symbol(argv[3],"debugIntroPlayer"),rawPlayerAt=symbol(argv[3],"debugPlayerRecord"),expectedPlayer[931][11]={{0}};
 unsigned char expectedRawPlayer[931][96]={{0}};
 for(unsigned pass=0;pass<2;pass++){
 FILE *playerTrace=fopen(pass?"build/arcade-trace/actor-native/handoff-player.csv":"build/arcade-trace/actor-native/player.csv","r");if(!playerTrace)return 2;
 char playerLine[512],hex[193];fgets(playerLine,sizeof(playerLine),playerTrace);
 unsigned pf,pc,pm,pg;
 while(fgets(playerLine,sizeof(playerLine),playerTrace))if(sscanf(playerLine,"%u,%u,%u,%u,%192s",&pf,&pc,&pm,&pg,hex)==5 && pf>=691 && pf<=1620){
  unsigned b[96];for(unsigned i=0;i<96;i++){sscanf(hex+i*2,"%2x",&b[i]);expectedRawPlayer[pf-690][i]=b[i];}
  unsigned *e=expectedPlayer[pf-690];e[0]=b[0];e[1]=b[32];e[2]=b[2];e[3]=b[3];e[4]=b[6]*256+b[7];e[5]=b[10]*256+b[11];e[6]=b[34]*256+b[35];e[7]=b[28]*256+b[29];e[8]=b[20]*256+b[21];e[9]=b[22]*256+b[23];e[10]=b[15];
 }
 fclose(playerTrace);
 }
 unsigned cy=symbol(argv[3],"debugIntroCameraY"),bg=symbol(argv[3],"debugIntroBackgroundY"),ov=symbol(argv[3],"debugIntroOverflow");
 void *core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 FILE*f=fopen(argv[2],"rb");if(!f)return 2;fseek(f,0,SEEK_END);size_t n=ftell(f);rewind(f);void*data=malloc(n);fread(data,1,n,f);fclose(f);struct retro_game_info game={argv[2],data,n,NULL};if(!retro_load_game(&game))return 2;
 size_t objectCartridgeCapacity=0;
 if(argc==5&&(!strcmp(argv[4],"--object-cartridge-probe")||!strcmp(argv[4],"--scene-boot-probe")||!strcmp(argv[4],"--scene-combined-probe")||!strcmp(argv[4],"--scene-text-probe")||!strcmp(argv[4],"--scene-warning-probe")||!strcmp(argv[4],"--scene-handoff-probe")||!strcmp(argv[4],"--scene-service-handoff-probe")||!strcmp(argv[4],"--scene-attract-init-probe")||!strcmp(argv[4],"--scene-attract-fade-probe")||!strcmp(argv[4],"--scene-pools-probe")||!strcmp(argv[4],"--scene-initialize-probe")||(!strcmp(argv[4],"--scene-dispatch-probe")||(!strcmp(argv[4],"--scene-spawn-probe")||!strcmp(argv[4],"--scene-countdown-probe"))))){
  size_t(*memorySize)(unsigned)=dlsym(core,"retro_get_memory_size");
  if(!memorySize||n>0x200000||memcmp((unsigned char*)data+0x100,"SEGA MEGA DRIVE ",16))return 2;
  /* Once execution starts this core reports modified save length, not capacity. */
  objectCartridgeCapacity=memorySize(RETRO_MEMORY_SAVE_RAM);
  if(objectCartridgeCapacity!=65536)return 2;
 }

 unsigned (*cpu_reg)(int)=dlsym(core,"m68k_get_reg");
 FILE*bf=fopen("native/intro_probe/res/layers.bin","rb");if(!bf || fread(backgroundIntro,1,sizeof backgroundIntro,bf)!=sizeof backgroundIntro)return 2;fclose(bf);
 bf=fopen("native/intro_probe/res/background_pairs.bin","rb");if(!bf || fread(backgroundExtra,1,sizeof backgroundExtra,bf)!=sizeof backgroundExtra)return 2;fclose(bf);
 probe_vram=dlsym(core,"vram");if(!probe_vram)return 2;
 void*(*targetMemory)(unsigned)=dlsym(core,"retro_get_memory_data");
 uint8_t*probe_sram=targetMemory?targetMemory(RETRO_MEMORY_SAVE_RAM):NULL;
 probe_ram=dlsym(core,"work_ram");int previous=0,stalls=0,checks=0;unsigned maximumResident=0,cacheReadyTick=0;
 unsigned snapshotReady=getenv("WILLOW_HEADLESS_SNAPSHOT")?symbol(argv[3],"debugHeadlessSnapshotReady"):0;
 unsigned snapshotWaitFrames=0;
 uint16_t precedingIds[1296];memset(precedingIds,255,sizeof precedingIds);
 for(frame=0;frame<(snapshotReady?3200:1600);frame++) {
  retro_run();int t=word(probe_ram,tick_at);
  if(snapshotReady){
   if(!word(probe_ram,snapshotReady)){snapshotWaitFrames++;continue;}
   if(t!=previous+1){fprintf(stderr,"Headless snapshot skipped update %d -> %d\n",previous,t);return 1;}
   /* The core is stopped between retro_run calls. Acknowledge now; no
    * cartridge instruction executes until after this complete inspection. */
   probe_ram[snapshotReady^1]=probe_ram[(snapshotReady+1)^1]=0;
  }
  if(t>0){
   /* Older specialized probes still assume actor payloads are in work RAM.
    * Reject those modes on the opt-in SRAM experiment instead of masking
    * target pointers and accidentally validating unrelated RAM. */
   if(((longword(probe_ram,poolsAt)>>16)&255)==0x20 && argc==5 &&
      strcmp(argv[4],"--scene-spawn-probe") && strcmp(argv[4],"--scene-countdown-probe")){
    fprintf(stderr,"This probe mode has not been adapted for SRAM actor storage\n");return 2;
   }
   if(lifetime_probe){
    const unsigned bases[]={taskStateNative,taskRecordNative,taskCommandNative,bootGlobalsNative,highScoresNative,soundCommandsNative},sizes[]={88,512,256,sizeof(WillowNativeBootGlobals),85,128};
    for(unsigned region=0;region<6;region++)for(unsigned n=0;n<sizes[region];n++){
     /* Live camera publication is source state, not a graphics canary.
      * Its scroll words are compared against original traces below. */
     if(region==3 && ((n>=nativeBootGlobalOffset(0x8032)&&n<=nativeBootGlobalOffset(0x8039)) ||
        (n>=nativeBootGlobalOffset(0x804a)&&n<=nativeBootGlobalOffset(0x804b))))continue;
     /* The live source mux owns these fields; do not inject a demo
      * selector with an arbitrary pointer into the ordinary intro. */
     if(region==3){
      unsigned live=0;
      for(unsigned k=0;k<13;k++)if(n==nativeBootGlobalOffset(inputFixtureAddresses[k]))live=1;
      if(live || n==nativeBootGlobalOffset(0x8061) || n==nativeBootGlobalOffset(0x8063))continue;
     }
     unsigned at=bases[region]+n;unsigned char value=(n*73+region*31+0x59)&255;
     if(t==1)probe_ram[at^1]=value;
     else if(probe_ram[at^1]!=value){fprintf(stderr,"Task storage overwritten by graphics tick %d region %u byte %u\n",t,region,n);SMOKE_FAIL();}
     if(t!=1)lifetimeChecks++;
    }
   }
   /* Independent target-layout decoding; there is no duplicate player page. */
   for(unsigned group=0;group<192;group++){
    unsigned source=(unsigned short)word(probe_ram,paletteOwnerAt+group*2);
    unsigned packed=probe_ram[(paletteOwnerAt+384+(group>>1))^1];
    unsigned offset=(packed>>((group&1)*4))&15;
    if(group>=2){if(source!=65535 || offset)SMOKE_FAIL();continue;}
    for(unsigned color=0;color<16;color++){
     unsigned raw;
     if(source<=0xfff0){
      unsigned address=0xe0000+source*2+color*2;
      if(address<0xe5000 || address>=0xe50c0)SMOKE_FAIL();
      unsigned at=address-0xe5000;raw=originalColors[at]*256+originalColors[at+1];
     }else{
      unsigned slot=source-0xfff1;
      if(slot>=2 || (unsigned short)word(probe_ram,paletteSlotsAt+slot*34)!=group)SMOKE_FAIL();
      raw=(unsigned short)word(probe_ram,paletteSlotsAt+slot*34+2+color*2);
     }
     unsigned actual=(raw+(offset<<12))&65535;
     nativeColors[group*16+color]=actual;
    }
   }

   if(!cacheReadyTick && (unsigned short)word(probe_ram,lookupBuiltAt)==1296)cacheReadyTick=t;
   if(t<=930 || walk_probe){
    FILE*trace=walk_probe && t>=897?walkTrace:cameraTrace;
    long offset=walk_probe && t>=897?(long)(t-897)*40198:(long)(t+40)*sizeof expectedCamera;
    if(fseek(trace,offset,SEEK_SET) || fread(expectedCamera,1,sizeof expectedCamera,trace)!=sizeof expectedCamera)return 2;
    if(t>=898 && (probe_ram[inputStateAt^1]!=expectedCamera[4+0x6a] || probe_ram[(inputStateAt+1)^1]!=expectedCamera[4+0x6b])){
     unsigned ph=profileSymbol(argv[3],"debugFrameLines");fprintf(stderr,"Frame phases %d %d %d %d %d\n",profileWord(probe_ram,ph,0),profileWord(probe_ram,ph,2),profileWord(probe_ram,ph,4),profileWord(probe_ram,ph,6),profileWord(probe_ram,ph,8));
     unsigned profile=profileSymbol(argv[3],"debugColumnLines");
     fprintf(stderr,"Column scanlines %d %d %d %d PC %x\n",profileWord(probe_ram,profile,0),profileWord(probe_ram,profile,2),profileWord(probe_ram,profile,4),profileWord(probe_ram,profile,6),cpu_reg(16));
     fprintf(stderr,"Native input mismatch tick %d held %u/%u previous %u/%u\n",t,probe_ram[inputStateAt^1],expectedCamera[4+0x6a],probe_ram[(inputStateAt+1)^1],expectedCamera[4+0x6b]);SMOKE_FAIL();
    }
    if(t>=898)for(unsigned n=0;n<4;n++){
     unsigned at=bootGlobalsNative+nativeBootGlobalOffset(0x8060+n);
     if(probe_ram[at^1]!=expectedCamera[4+0x60+n]){
      fprintf(stderr,"Controller history mismatch tick %d byte %u got %u want %u\n",t,n,probe_ram[at^1],expectedCamera[4+0x60+n]);SMOKE_FAIL();
     }
     controllerHistoryChecks++;
    }
    for(unsigned i=0;i<128;i++)if(probe_ram[(cameraAt+i)^1]!=expectedCamera[1030+i]){
          unsigned ph=profileSymbol(argv[3],"debugFrameLines"),pr=profileSymbol(argv[3],"debugColumnLines");fprintf(stderr,"Late frame phases %d %d %d %d %d; columns %d %d %d %d\n",profileWord(probe_ram,ph,0),profileWord(probe_ram,ph,2),profileWord(probe_ram,ph,4),profileWord(probe_ram,ph,6),profileWord(probe_ram,ph,8),profileWord(probe_ram,pr,0),profileWord(probe_ram,pr,2),profileWord(probe_ram,pr,4),profileWord(probe_ram,pr,6));fprintf(stderr,"Native camera mismatch tick %d byte %u native %02x source %02x\n",t,i,probe_ram[(cameraAt+i)^1],expectedCamera[1030+i]);SMOKE_FAIL();
    }
    if(t>=5)for(unsigned a=0x8032;a<=0x8034;a+=2){
     unsigned at=bootGlobalsNative+nativeBootGlobalOffset(a);
     unsigned got=((unsigned)probe_ram[at^1]<<8)|probe_ram[(at+1)^1];
     unsigned want=willowMotionWord(expectedCamera+4+a-0x8000);
     if(got!=want){fprintf(stderr,"Source camera publication mismatch tick %d address %04x got %04x want %04x\n",t,a,got,want);SMOKE_FAIL();}
    }
    if((unsigned short)word(probe_ram,normalUpdatesAt)!=(unsigned)(t>915?t-915:0))SMOKE_FAIL();
    if(t>915)for(unsigned i=0;i<128;i++)if(probe_ram[(rawPlayerAt+i)^1]!=expectedCamera[1158+i]){
     fprintf(stderr,"Native normal-player mismatch tick %d byte %u native %02x source %02x\n",t,i,probe_ram[(rawPlayerAt+i)^1],expectedCamera[1158+i]);SMOKE_FAIL();
    }
    if(t>=5){
     unsigned cellsAt=longword(probe_ram,mapAt)&65535;
     for(unsigned i=0;i<4096;i++){
      unsigned id=(unsigned short)word(probe_ram,cellsAt+i*2);
      unsigned char *e=expectedCamera+1286+i*4;
      uint32_t pair=((uint32_t)e[0]<<24)|((uint32_t)e[1]<<16)|(e[2]<<8)|e[3];
      if(id>=WILLOW_SCROLL2_PAIR_COUNT || nativeScroll2Pairs[id]!=pair){fprintf(stderr,"Native map mismatch tick %d cell %u id %u\n",t,i,id);SMOKE_FAIL();}
     }
    }
   }

   if(t<=930 || walk_probe){
    long index=t+40;
    if(fseek(distantTrace,index*1286,SEEK_SET) || fread(expectedDistant,1,1286,distantTrace)!=1286)return 2;
    for(unsigned i=0;i<128;i++)if(probe_ram[(distantAt+i)^1]!=expectedDistant[4+0x482+i]){
     fprintf(stderr,"Native Scroll3 mismatch tick %d byte %u native %02x source %02x\n",t,i,probe_ram[(distantAt+i)^1],expectedDistant[4+0x482+i]);SMOKE_FAIL();
    }
    if(t>=2)for(unsigned a=0x8036;a<=0x8038;a+=2){
     unsigned at=bootGlobalsNative+nativeBootGlobalOffset(a);
     unsigned got=((unsigned)probe_ram[at^1]<<8)|probe_ram[(at+1)^1];
     unsigned want=willowMotionWord(expectedDistant+4+a-0x8000);
     if(got!=want){fprintf(stderr,"Distant publication mismatch tick %d address %04x\n",t,a);SMOKE_FAIL();}
    }
    if(t>=5){
     if(fseek(distantMaps,index*16384,SEEK_SET) || fread(expectedDistantMap,1,16384,distantMaps)!=16384)return 2;
     unsigned cells=longword(probe_ram,distantCellsAt)&65535;
     for(unsigned i=0;i<WILLOW_SCROLL3_MAP_WORDS;i++)packedDistant[i]=word(probe_ram,cells+i*2);
     for(unsigned cell=0;cell<4096;cell++){
      unsigned id=willowScroll3MapId(packedDistant,cell);
      if(id>=WILLOW_SCROLL3_PAIR_COUNT || nativeScroll3Pairs[id]!=willowMotionLong(expectedDistantMap+cell*4)){
       fprintf(stderr,"Native Scroll3 map mismatch tick %d cell %u id %u\n",t,cell,id);SMOKE_FAIL();
      }
     }
    }
   }
   unsigned restoredPairs=(unsigned short)word(probe_ram,restoreLoadedAt);
   unsigned restoreAddress=restorePairsAt;
   if(restoredPairs>360 || (t>=898 && restoredPairs!=360))SMOKE_FAIL();
   if(t<914)for(unsigned n=0;n<restoredPairs;n++){
    unsigned far=n>=280,index=far?n-280:n;
    uint16_t x=32+(index%(far?10:20))*(far?32:16),y=(far?-256:-240)+(index/(far?10:20))*(far?32:16);
    unsigned at=far?((y&0x700)<<3)|((y&224)>>3)|(x&0x7e0):((x&0x3f0)<<2)|((y&0x300)<<4)|((y&0xf0)>>2);
    uint32_t pair=willowMotionLong(far?expectedDistantMap+at:expectedCamera+1286+at);
    const uint32_t*d=far?nativeScroll3Pairs:nativeScroll2Pairs;
    unsigned low=0,high=far?WILLOW_SCROLL3_PAIR_COUNT:WILLOW_SCROLL2_PAIR_COUNT;
    while(low<high){unsigned mid=(low+high)/2;if(d[mid]<pair)low=mid+1;else high=mid;}
    unsigned count=far?WILLOW_SCROLL3_PAIR_COUNT:WILLOW_SCROLL2_PAIR_COUNT;
    if(low==count || d[low]!=pair)SMOKE_FAIL();
    unsigned expected=(far?nativeBackground3Offsets:nativeBackground2Offsets)[low];
    if(willowMotionWord((const uint8_t*)data+restoreAddress+n*2)!=expected){fprintf(stderr,"Live restoration offset mismatch tick %d pair %u\n",t,n);SMOKE_FAIL();}
   }
   int forestRows=t<=326?0:t<354?t-326:28;
   if(word(probe_ram,forestAt)!=forestRows){fprintf(stderr,"forest plane transition mismatch tick %d\n",t);SMOKE_FAIL();}
   if(t==355)for(unsigned y=0;y<28;y++)for(unsigned x=0;x<40;x++)
    if(((unsigned short)word(probe_ram,planeBAt+(y*WILLOW_COLUMN_WINDOW+x)*2)&0x4000)!=0){fprintf(stderr,"text plane still owns background tiles\n");SMOKE_FAIL();}
   /* Reconstruct ownership from complete/partially prepared map rows and
    * separately retained overlay patterns, independent of ref counters. */
   unsigned expectedRefs[1312]={0};
   /* Independent inverse from resident IDs; do not trust native hash lookups
    * when reconstructing plane and pin ownership. */
   static uint16_t inverse[65536];memset(inverse,255,sizeof inverse);
   unsigned bucketSeen[1296]={0};
   for(unsigned slot=0;slot<1296;slot++){
    unsigned id=(unsigned short)word(probe_ram,idsAt+slot*2);
    if(id!=65535){if(inverse[id]!=65535)SMOKE_FAIL();inverse[id]=slot;}
   }
   if(word(probe_ram,lookupActiveAt)){
   for(unsigned b=0;b<512;b++){
    unsigned slot=(unsigned short)word(probe_ram,bucketsAt+b*2);
    while(slot!=65535){
     if(slot>=1296 || bucketSeen[slot]++)SMOKE_FAIL();
     unsigned id=(unsigned short)word(probe_ram,idsAt+slot*2);
     if(id==65535 || (id&511)!=b)SMOKE_FAIL();
     slot=(unsigned short)word(probe_ram,bucketsAt+1024+slot*2);
    }
   }
   for(unsigned slot=0;slot<(unsigned short)word(probe_ram,lookupBuiltAt);slot++)if(bucketSeen[slot]!=((unsigned short)word(probe_ram,idsAt+slot*2)!=65535))SMOKE_FAIL();
   if(t>=898 && (unsigned short)word(probe_ram,lookupBuiltAt)!=1296)SMOKE_FAIL();
   }else{
    for(unsigned id=0;id<overlayBase+184+200;id++)if((unsigned short)word(probe_ram,bucketsAt+id*2)!=inverse[id]){fprintf(stderr,"Direct cache mismatch tick %d id %u actual %u expected %u\n",t,id,(unsigned short)word(probe_ram,bucketsAt+id*2),inverse[id]);SMOKE_FAIL();}
   }
   unsigned refsPrepared=(unsigned short)word(probe_ram,symbol(argv[3],"debugRestoreRefsPrepared"));
   unsigned restoredRows=(unsigned short)word(probe_ram,symbol(argv[3],"debugSplitForestRows"));
   if(refsPrepared>intro_restore_change_start[28] || (t>=898 && refsPrepared!=intro_restore_change_start[28]))SMOKE_FAIL();
   for(unsigned n=intro_restore_change_start[restoredRows];n<refsPrepared;n++)if(intro_restore_changes[n].count>0){
    unsigned slot=inverse[intro_restore_changes[n].tile];if(slot>=1296)SMOKE_FAIL();
    expectedRefs[16+slot]+=intro_restore_changes[n].count;
   }
   unsigned initialCells=(unsigned short)word(probe_ram,symbol(argv[3],"debugColumnInitCells"));
   if(initialCells>56 || (t>=898 && initialCells!=56))SMOKE_FAIL();
   unsigned streamAt=longword(probe_ram,symbol(argv[3],"columnStreams"))&65535;
   unsigned streamActive=(unsigned short)word(probe_ram,symbol(argv[3],"debugColumnStreaming"));
   /* The compact CPU mirror omits these hardware columns. Before column
    * streaming starts they must remain blank, including overlay uploads. */
   if(!streamActive)for(unsigned row=0;row<28;row++)for(unsigned col=WILLOW_COLUMN_WINDOW;col<64;col++)
    if(word(probe_vram,0xe000+(row*64+col)*2))SMOKE_FAIL();
   if(streamActive && (unsigned short)word(probe_ram,symbol(argv[3],"debugColumnReady"))!=3)SMOKE_FAIL();
   for(unsigned n=0;n<(streamActive?2*43*28:initialCells);n++){
    unsigned layer=streamActive?n/(43*28):n/28,row=n%28;
    int col=40;
    if(streamActive){
     unsigned record=streamAt+layer*sizeof(WillowColumnStream),slot=(n/28)%43;
     unsigned rows=probe_ram[(record+86+slot)^1];
     col=word(probe_ram,record+slot*2);
     if(row>=rows)continue;
     if(col==WILLOW_COLUMN_EMPTY || rows>28)SMOKE_FAIL();
    }
    unsigned cpuCol=willowColumnSlot(col);
    unsigned attr=(unsigned short)word(probe_ram,(layer?planeBAt:planeAAt)+(row*WILLOW_COLUMN_WINDOW+cpuCol)*2);
    unsigned tile=attr&2047;if(tile<16 || tile>=1312)SMOKE_FAIL();
    expectedRefs[tile]++;
    uint16_t x=32+col*8,y=row*8-240;
    unsigned cell=layer?willowBackground3Cell(x,y):willowBackground2Cell(x,y);
    uint32_t pair=willowMotionLong(layer?expectedDistantMap+cell*4:expectedCamera+1286+cell*4);
    const uint32_t*d=layer?nativeScroll3Pairs:nativeScroll2Pairs;
    unsigned low=0,high=layer?WILLOW_SCROLL3_PAIR_COUNT:WILLOW_SCROLL2_PAIR_COUNT,total=high;
    while(low<high){unsigned mid=(low+high)/2;if(d[mid]<pair)low=mid+1;else high=mid;}
    if(low==total || d[low]!=pair)SMOKE_FAIL();
    unsigned offset=(layer?nativeBackground3Offsets:nativeBackground2Offsets)[low];if(offset==65535)SMOKE_FAIL();
    unsigned size=layer?32:16;
    unsigned entry=(layer?nativeBackground3Entries:nativeBackground2Entries)[offset+(y%size/8)*(size/8)+x%size/8];
    if(inverse[entry&32767]!=tile-16 || attr!=(((2+(entry>>15))<<13)|tile))SMOKE_FAIL();
    if(t>=898){
     if((unsigned short)word(probe_vram,(layer?0xe000:0xc000)+(row*64+willowColumnHardware(col))*2)!=attr)SMOKE_FAIL();
     unsigned id=entry&32767;const unsigned char*bytes;
     if(id<INTRO_TILE_COUNT)bytes=backgroundIntro+id*32;
     else{if(id<NATIVE_BACKGROUND_EXTENSION_BASE || id>=NATIVE_BACKGROUND_ID_LIMIT)SMOKE_FAIL();bytes=backgroundExtra+(id-NATIVE_BACKGROUND_EXTENSION_BASE)*32;}
     int visibleX=-word(probe_vram,0xf000+layer*2);
     int visibleFirst=willowColumnFloor(visibleX),visibleLast=willowColumnFloor(visibleX+319);
     /* New offscreen patterns are queued for the next VBlank. Every visible
      * cell and every unchanged prior pattern must already be in VRAM. */
     if(!streamActive || precedingIds[tile-16]==id || (col>=visibleFirst && col<=visibleLast))
     for(unsigned b=0;b<32;b++)if(probe_vram[(tile*32+b)^1]!=bytes[b]){fprintf(stderr,"Incoming column pixel mismatch tick %d layer %u row %u\n",t,layer,row);SMOKE_FAIL();}
    }
   }
   for(unsigned layer=0;layer<2;layer++)for(unsigned row=0;row<32;row++){
    int pending=word(probe_ram,pendingAt+layer*2);
    unsigned count=((probe_ram[(ownedAt+layer*4+(row>>3))^1]>>(row&7))&1)?40:pending>=0 && (pending&31)==(int)row?(unsigned short)word(probe_ram,pendingColumnsAt+layer*2):0;
    for(unsigned x=0;x<count;x++){
     unsigned at=(layer?planeBAt:planeAAt)+(row*WILLOW_COLUMN_WINDOW+x)*2;
     unsigned tile=(unsigned short)word(probe_ram,at)&2047;
     if(tile<16 || tile>=1312){fprintf(stderr,"Invalid owned tile tick %d tile %u\n",t,tile);SMOKE_FAIL();}
     expectedRefs[tile]++;
    }
   }
   for(unsigned n=0;n<(unsigned short)word(probe_ram,overlayCountAt);n++){
    unsigned slot=inverse[overlayBase+n];
    if(slot!=(unsigned short)word(probe_ram,overlayFirstAt)+n){fprintf(stderr,"Overlay run mismatch tick %d pattern %u\n",t,n);SMOKE_FAIL();}
    if(slot>=1296)SMOKE_FAIL();expectedRefs[16+slot]++;
   }
   unsigned restorePins=(unsigned short)word(probe_ram,symbol(argv[3],"debugForestRestorePins"));
   if(restorePins>INTRO_RESTORE_PATTERNS)SMOKE_FAIL();
   for(unsigned n=0;n<restorePins;n++){
    unsigned slot=inverse[intro_restore_patterns[n]];
    if(slot>=1296)SMOKE_FAIL();expectedRefs[16+slot]++;
   }
   if(word(probe_ram,hudActiveAt)){
    unsigned split=(unsigned short)word(probe_ram,symbol(argv[3],"debugSplitForestRows"));
    unsigned expected=t<=899?0:t<913?(t-899)*2:28;
    if(split!=expected){fprintf(stderr,"Forest restoration tick %d rows %u/%u\n",t,split,expected);SMOKE_FAIL();}
    for(unsigned layer=0;layer<2;layer++)for(unsigned row=0;row<(streamActive?0:split);row++)for(unsigned col=0;col<40;col++){
     unsigned entry=intro_maps[(layer?INTRO_BG_OFFSET+32*40:64*40)+row*40+col];
     unsigned slot=inverse[entry&32767];
     unsigned expectedAttr=((2+(entry>>15))<<13)|(16+slot);
     unsigned actualAttr=(unsigned short)word(probe_ram,(layer?planeBAt:planeAAt)+(row*WILLOW_COLUMN_WINDOW+col)*2);
     if(slot>=1296 || actualAttr!=expectedAttr){fprintf(stderr,"Restored forest mismatch tick %d layer %u row %u col %u\n",t,layer,row,col);SMOKE_FAIL();}
    }
    for(unsigned row=split;row<32;row++)for(unsigned col=0;col<WILLOW_COLUMN_WINDOW;col++)if(!(col==40 && initialCells>28+row) && word(probe_ram,planeBAt+(row*WILLOW_COLUMN_WINDOW+col)*2)){
     fprintf(stderr,"Unexpected background cell before restoration at tick %d row %u\n",t,row);SMOKE_FAIL();
    }
    unsigned count=(unsigned short)word(probe_ram,symbol(argv[3],"debugHudSpriteCount"));
    unsigned sprites=symbol(argv[3],"debugHudSprites");if(count>40)SMOKE_FAIL();
    unsigned cellIndex=(unsigned short)word(probe_ram,symbol(argv[3],"hudCellSprite")+2);
    if(cellIndex){
     const unsigned rows[]={0,1,24,26,27};
     if(cellIndex>65536-150)SMOKE_FAIL();
     for(unsigned cell=0;cell<200;cell++){
      unsigned row=rows[cell/40],col=cell%40,want=63;
      for(unsigned n=0;n<count;n++){
       unsigned at=sprites+n*4,layout=(unsigned short)word(probe_ram,at),x=layout&63,y=(layout>>6)&31;
       if(col>=x&&col<x+((layout>>11)&3)+1&&row>=y&&row<y+((layout>>13)&3)+1)want=n;
      }
      unsigned got=((probe_ram[(cellIndex+cell/2)^1]>>((cell%2)*4))&15)|
                   (((probe_ram[(cellIndex+100+cell/4)^1]>>((cell%4)*2))&3)<<4);
      if(got!=want){fprintf(stderr,"Packed HUD index tick %d cell %u got %u want %u\n",t,cell,got,want);SMOKE_FAIL();}
      hudIndexChecks++;
     }
    }
    for(unsigned n=0;n<count;n++){
     unsigned at=sprites+n*4,tile=(unsigned short)word(probe_ram,at+2),layout=(unsigned short)word(probe_ram,at);
     unsigned size=(((layout>>11)&3)+1)*(((layout>>13)&3)+1);
     if(tile>=16 && tile<1312)for(unsigned i=0;i<size;i++){
      if(tile+i>=1312)SMOKE_FAIL();expectedRefs[tile+i]++;
     }
    }
   }
   if(t<=930 || walk_probe){
    if((unsigned short)word(probe_ram,chargeAt)!=(expectedCamera[4+0x376]*256+expectedCamera[4+0x377]) ||
       probe_ram[(chargeAt+2)^1]!=expectedCamera[4+0x3ca] ||
       probe_ram[(chargeAt+4)^1]!=expectedCamera[4+0x3ea] ||
       probe_ram[(chargeAt+6)^1]!=expectedCamera[4+0x367]){
     fprintf(stderr,"Native dispatcher charge state mismatch tick %d\n",t);SMOKE_FAIL();
    }
    if(probe_ram[hudAt^1]!=expectedCamera[4+0x3c6] || probe_ram[(hudAt+1)^1]!=expectedCamera[4+0x3c7] || probe_ram[(clockAt+7)^1]!=expectedCamera[4+0x3c8]){
     fprintf(stderr,"HUD clock observation PC %x clock-frame %u tick %d\n",cpu_reg(16),probe_ram[(clockAt+6)^1],t);
     fprintf(stderr,"Native HUD clock mismatch tick %d: %u,%u,%u wanted %u,%u,%u\n",t,probe_ram[hudAt^1],probe_ram[(hudAt+1)^1],probe_ram[(clockAt+7)^1],expectedCamera[4+0x3c6],expectedCamera[4+0x3c7],expectedCamera[4+0x3c8]);SMOKE_FAIL();
    }
   }
   if(t==898 || t==899 || t==900 || t==910){
    char path[128];snprintf(path,sizeof path,"build/arcade-trace/actor-native/text-%u.bin",690+t);
    unsigned char source[16384];FILE*hf=fopen(path,"rb");if(!hf || fread(source,1,sizeof source,hf)!=sizeof source)return 2;fclose(hf);
    const unsigned rows[6]={2,3,26,28,29,30};
    for(unsigned row=0;row<6;row++)for(unsigned col=0;col<48;col++)for(unsigned b=0;b<4;b++){
     if(probe_ram[(surfaceAt+row*192+col*4+b)^1]!=source[col*128+rows[row]*4+b]){
      fprintf(stderr,"Native HUD source cell mismatch tick %d row %u col %u byte %u\n",t,row,col,b);SMOKE_FAIL();
     }
    }
   }
   for(unsigned tile=0;tile<16;tile++)if(expectedRefs[tile])SMOKE_FAIL();
   for(unsigned tile=16;tile<1312;tile++)if(expectedRefs[tile]!=(unsigned short)word(probe_ram,refsAt+(tile-16)*2)){
    fprintf(stderr,"Cache reference mismatch tick %d tile %u expected %u actual %u\n",t,tile,expectedRefs[tile],(unsigned short)word(probe_ram,refsAt+(tile-16)*2));SMOKE_FAIL();
   }
   if(t<=930)for(unsigned i=0;i<96;i++)if(probe_ram[(rawPlayerAt+i)^1]!=expectedRawPlayer[t][i]){fprintf(stderr,"Raw native player mismatch tick %d byte %u\n",t,i);SMOKE_FAIL();}
   if(t>=897 && t<=899)for(unsigned i=96;i<128;i++)if(probe_ram[(rawPlayerAt+i)^1]!=expectedEntryRam[0x9846+i]){fprintf(stderr,"Raw player tail mismatch tick %d byte %u\n",t,i);SMOKE_FAIL();}
   if(t<=930)for(unsigned i=0;i<11;i++)if((unsigned short)word(probe_ram,playerAt+i*2)!=expectedPlayer[t][i]){fprintf(stderr,"native player mismatch tick %d field %u\n",t,i);SMOKE_FAIL();}
   if(t>=70 && t<=897){
    unsigned records=longword(probe_ram,poolsAt+2*12);
    for(unsigned actor=0;actor<2;actor++){
     unsigned at=records+(59-actor)*64;
     const unsigned fields[]={2,3,6,10,48,28,20,22};
     const unsigned expectedFields[]={2,3,4,5,6,7,8,9};
     if(*nativeTargetByte(probe_ram,probe_sram,at)!=expectedActors[t][actor*10] || *nativeTargetByte(probe_ram,probe_sram,at+32)!=expectedActors[t][actor*10+1])SMOKE_FAIL();
     for(unsigned n=0;n<8;n++){
      unsigned value=n<2?*nativeTargetByte(probe_ram,probe_sram,at+fields[n]):nativeTargetWord(probe_ram,probe_sram,at+fields[n]);
      if(value!=expectedActors[t][actor*10+expectedFields[n]]){fprintf(stderr,"Allocated intro actor mismatch tick %d actor %u field %u native %u expected %u PC %x\n",t,actor,fields[n],value,expectedActors[t][actor*10+expectedFields[n]],cpu_reg(16));SMOKE_FAIL();}
     }
    }
   }
   if(t>=70 && t<=897)for(unsigned i=0;i<20;i++)if((unsigned short)word(probe_ram,actorAt+i*2)!=expectedActors[t][i]){
    fprintf(stderr,"native actor mismatch tick %d field %u value %u expected %u\n",t,i,(unsigned short)word(probe_ram,actorAt+i*2),expectedActors[t][i]);SMOKE_FAIL();
   }
   if(t<=898 && (word(probe_ram,writesAt)!=expectedWrites[t] || word(probe_ram,st)!=expectedState[t] || word(probe_ram,ti)!=expectedTimer[t] || word(probe_ram,busy)!=expectedBusy[t])){fprintf(stderr,"native task-state/text mismatch at tick %d state %d/%u timer %d/%u busy %d/%u writes %d/%u\n",t,word(probe_ram,st),expectedState[t],word(probe_ram,ti),expectedTimer[t],word(probe_ram,busy),expectedBusy[t],word(probe_ram,writesAt),expectedWrites[t]);fprintf(stderr,"CPU PC %x overflow %d\n",cpu_reg(16),word(probe_ram,ov));SMOKE_FAIL();}
   int expected=t<=70?512:t<326?512-2*(t-70):0;
   if(word(probe_ram,cy)!=expected || word(probe_ram,bg)!=(expected>>1) || word(probe_ram,ov)){
    fprintf(stderr,"camera/cache failure at frame %d tick %d Y %d expected %d overflow %d\n",frame,t,word(probe_ram,cy),expected,word(probe_ram,ov));SMOKE_FAIL();
   }
   unsigned residentCount=0;
   for(unsigned i=0;i<1296;i++){
    if(expectedRefs[16+i])residentCount++;
    unsigned bits=(unsigned short)word(probe_ram,freeTilesAt+(i>>4)*2);
    if(!!(bits&(1u<<(i&15)))!=(expectedRefs[16+i]==0)){fprintf(stderr,"Free tile ownership mismatch tick %d slot %u\n",t,i);SMOKE_FAIL();}
   }
   if(residentCount>maximumResident)maximumResident=residentCount;
   for(unsigned i=0;i<81;i++){
    unsigned bits=(unsigned short)word(probe_ram,freeTilesAt+162+(i>>4)*2);
    if(!!(bits&(1u<<(i&15)))!=!!word(probe_ram,freeTilesAt+i*2)){fprintf(stderr,"Free tile summary mismatch tick %d word %u\n",t,i);SMOKE_FAIL();}
   }
   if((unsigned short)word(probe_ram,freeTilesAt+174)>=1296 ||
      ((unsigned short)word(probe_ram,freeTilesAt+172)&0xfffe))SMOKE_FAIL();
   if(t==previous){stalls++;fprintf(stderr,"stall frame %d tick %d\n",frame,t); }
   if(t!=previous)checks++;
   previous=t;
   if(t==899){
    unsigned char*vr=dlsym(core,"vram");FILE*vf=fopen("build/native-hud-vram.bin","wb");if(!vr || !vf)return 2;fwrite(vr,1,65536,vf);fclose(vf);
   }
   if(t==1)printf("Native pre-restoration-allocation free: %u bytes\n",(unsigned short)word(probe_ram,symbol(argv[3],"debugIntroHeapFree")));
   if(t>=1){
    unsigned queuePointer=symbol(argv[3],"debugClass12Visibility");
    unsigned queueAt=longword(probe_ram,queuePointer)&65535;
    unsigned orderAt=symbol(argv[3],"debugIntroDrawOrder");
    unsigned orderCount=probe_ram[symbol(argv[3],"debugIntroDrawCount")^1],seen=0,previousRank=0;
    if(!orderCount || orderCount>3)SMOKE_FAIL();
    for(unsigned n=0;n<orderCount;n++){
     unsigned actor=probe_ram[(orderAt+n)^1];
     if(actor>2 || (seen&(1u<<actor)))SMOKE_FAIL();
     unsigned rank=actor==1?0:actor==2?1:2;
     if(n && rank<=previousRank)SMOKE_FAIL();
     previousRank=rank;seen|=1u<<actor;
    }
    if(!(seen&4))SMOKE_FAIL();

    if(!queueAt || (unsigned short)word(probe_ram,queueAt+360)!=0x9646 || (unsigned short)word(probe_ram,queueAt+362)!=0x96be ||
       (unsigned short)word(probe_ram,queueAt+364)!=0xf97c || probe_ram[(queueAt+366)^1] ||
       probe_ram[(queueAt+367)^1] || probe_ram[(queueAt+368)^1]){
     fprintf(stderr,"Native class-12 visibility storage initialization mismatch: at %04x cursors %04x %04x %04x counts %u %u %u\n",queueAt,(unsigned short)word(probe_ram,queueAt+360),(unsigned short)word(probe_ram,queueAt+362),(unsigned short)word(probe_ram,queueAt+364),probe_ram[(queueAt+366)^1],probe_ram[(queueAt+367)^1],probe_ram[(queueAt+368)^1]);SMOKE_FAIL();
    }

   }
   if(t==898){
    if(probe_ram[(chargeAt+3)^1]!=expectedEntryRam[0x0a3c]){fputs("Native charge flash mismatch at handoff\n",stderr);SMOKE_FAIL();}
    unsigned globalsAt=symbol(argv[3],"debugPlayerGateGlobals");
    const unsigned sourceGlobals[6]={0x8362,0x8364,0x8365,0x8389,0x8376,0x8377};
    for(unsigned i=0;i<6;i++)if(probe_ram[(globalsAt+i)^1]!=expectedEntryRam[sourceGlobals[i]]){fprintf(stderr,"Native player gate global mismatch byte %u\n",i);SMOKE_FAIL();}
    if(word(probe_ram,createdAt)!=5){fputs("Wrong stage object count at handoff\n",stderr);SMOKE_FAIL();}
    for(unsigned i=0;i<sizeof(expectedObjects);i++)if(probe_ram[(objectsAt+i)^1]!=expectedObjects[i]){fprintf(stderr,"Native stage object byte mismatch at %u\n",i);SMOKE_FAIL();}
    if(word(probe_ram,paletteCountAt)!=6)SMOKE_FAIL();
    for(unsigned i=0;i<sizeof(expectedPalettes);i++)if(probe_ram[(paletteAt+i)^1]!=expectedPalettes[i]){fprintf(stderr,"Native palette controller byte mismatch at %u\n",i);SMOKE_FAIL();}
    if(word(probe_ram,spawnsAt)!=57){fputs("Native spawn count mismatch\n",stderr);SMOKE_FAIL();}
    const unsigned bases[8]={0x9dc6,0xb6c6,0xccc6,0xeac6,0x98c6,0xc2c6,0xc7c6,0xdbc6},ends[8]={0x918e,0x91ee,0x92b6,0x9356,0x912a,0x9216,0x923e,0x932e};
    const unsigned counts[8]={0x9801,0x9804,0x980d,0x9813,0x97fe,0x9807,0x980a,0x9810},cursors[8]={0x981c,0x9822,0x9834,0x9840,0x9816,0x9828,0x982e,0x983a};
    const unsigned capacities[8]={50,48,60,20,20,20,20,60},strides[8]={128,64,64,64,64,64,64,64};
    for(unsigned i=0;i<8;i++){
     unsigned at=poolsAt+i*12,records=longword(probe_ram,at),indices=longword(probe_ram,at+4)&65535;
     if(probe_ram[(at+8)^1]!=capacities[i] || probe_ram[(at+9)^1]!=strides[i] || probe_ram[(at+11)^1]!=expectedEntryRam[counts[i]])SMOKE_FAIL();
     unsigned sourceCursor=expectedEntryRam[cursors[i]]*256+expectedEntryRam[cursors[i]+1];
     if(ends[i]-capacities[i]*2+probe_ram[(at+10)^1]*2!=sourceCursor)SMOKE_FAIL();
     for(unsigned j=0;j<capacities[i];j++){
      unsigned index=probe_ram[(indices+j)^1],sourceIndex=ends[i]-capacities[i]*2+j*2;
      if(bases[i]+index*strides[i]!=expectedEntryRam[sourceIndex]*256+expectedEntryRam[sourceIndex+1])SMOKE_FAIL();
      for(unsigned k=0;k<strides[i];k++)if(*nativeTargetByte(probe_ram,probe_sram,records+j*strides[i]+k)!=expectedEntryRam[bases[i]+j*strides[i]+k]){fprintf(stderr,"Native actor pool mismatch pool %u record %u byte %u\n",i,j,k);SMOKE_FAIL();}
     }
    }
   }
   if(t>0){
    unsigned offset=128;
    if(palette_probe && t>=931 && t<=942 && !(((t-931)/4)&1))offset=palette_probe==2?0:64;
    for(unsigned n=0;n<32;n++){
     unsigned at=(n<16?offset:128)+n*2;
     unsigned expected=(originalColors[at]*256+originalColors[at+1])|0xf000;
     if(nativeColors[n]!=expected){fprintf(stderr,"Player source palette mismatch tick %d color %u\n",t,n);SMOKE_FAIL();}
    }
   }
   if(palette_probe && t==930){
    /* Isolated integration probe: source invulnerability timer, first blue
     * transition due next update. This is not a captured gameplay event. */
    unsigned timerOffset=palette_probe==2?54:80;
    probe_ram[(rawPlayerAt+timerOffset)^1]=0;probe_ram[(rawPlayerAt+timerOffset+1)^1]=16;
    probe_ram[(rawPlayerAt+78)^1]=1;probe_ram[(rawPlayerAt+79)^1]=1;
   }
   if(t>930 && palette_probe){
    unsigned timer=t<946?946-t:0;
    if((unsigned short)word(probe_ram,rawPlayerAt+(palette_probe==2?54:80))!=timer){fprintf(stderr,"Player palette timer mismatch tick %d\n",t);SMOKE_FAIL();}
   }
   if(pose_probe && t>=930 && t<endTick && !((t-930)%4)){
    unsigned address=native_player_frames[(t-930)/4].address;
    for(unsigned b=0;b<4;b++)probe_ram[(rawPlayerAt+20+b)^1]=address>>(24-b*8);
    probe_ram[(rawPlayerAt+28)^1]=255;probe_ram[(rawPlayerAt+29)^1]=255;
   }
   for(unsigned slot=0;slot<1296;slot++)precedingIds[slot]=(unsigned short)word(probe_ram,idsAt+slot*2);
   if(walk_probe && t>=934 && t<945){unsigned pr=profileSymbol(argv[3],"debugColumnLines");fprintf(stderr,"Completed %d scanlines %d %d %d %d\n",t,profileWord(probe_ram,pr,0),profileWord(probe_ram,pr,2),profileWord(probe_ram,pr,4),profileWord(probe_ram,pr,6));}
   if(t>=endTick)break;
  }
 }
 if(checks!=endTick){fprintf(stderr,"Incomplete bootstrap: verified %d of %d logical updates\n",checks,endTick);return 1;}
 if(snapshotReady)printf("Headless bootstrap snapshots: every logical update inspected after publication; %u wait frames; intentionally paused, no bootstrap FPS claim\n",snapshotWaitFrames);
 if(!checks)fprintf(stderr,"Distant allocation free %u largest %u\n",(unsigned short)word(probe_ram,symbol(argv[3],"debugScroll3Free")),(unsigned short)word(probe_ram,symbol(argv[3],"debugScroll3Largest")));
 if(!checks)fprintf(stderr,"No updates: CPU PC %x SP %x overflow %u\n",cpu_reg(16),cpu_reg(15),(unsigned short)word(probe_ram,ov));
 if(!checks){
  unsigned at=longword(probe_ram,symbol(argv[3],"heap"))&65535;
  for(unsigned n=0;n<32768 && at<65534;n++){
   unsigned block=(unsigned short)word(probe_ram,at);if(!block)break;
   if(!(block&1))fprintf(stderr,"Free heap block %04x size %u\n",at,block);
   unsigned size=block&~1u;if(!size)break;at+=size;
  }
 }

 unsigned dmaPeak=(unsigned short)word(probe_ram,symbol(argv[3],"debugDMAQueuePeak"));
 unsigned dmaCapacity=(unsigned short)word(probe_ram,symbol(argv[3],"queueSize"));
 if(dmaPeak>=dmaCapacity)SMOKE_FAIL();
 printf("Native DMA high water: %u of %u operations at tick %u; %u transfer bytes\n",dmaPeak,dmaCapacity,(unsigned short)word(probe_ram,symbol(argv[3],"debugDMAQueuePeakTick")),(unsigned short)word(probe_ram,symbol(argv[3],"debugDMABytesPeak")));
 unsigned copyPeak=(unsigned short)word(probe_ram,symbol(argv[3],"debugDMACopyPeak"));
 unsigned copyCapacity=2*(unsigned short)word(probe_ram,symbol(argv[3],"dataBufferSize"));
 if(copyPeak>=copyCapacity)SMOKE_FAIL();
 printf("Native DMA copied-buffer high water: %u of %u bytes\n",copyPeak,copyCapacity);
 printf("Native sequence: %d updates verified (898 intro, 17 player-gate and 15 normal-player updates), %d missed updates, %d cache overflows\n",checks,stalls,word(probe_ram,ov));
 if(checks==930)printf("Native unified player dispatcher: %u calls; charge value/active/mode/alternate level match all 930 source states\n",(unsigned short)word(probe_ram,dispatchAt));
 if(checks==930)puts("Native HUD: 930 clock/divider states and four full retained-cell checkpoints match source");
 if(checks==930)puts("Native camera/map: 930 full 128-byte records and 926 full 16384-byte maps match source; initialization uses code/ROM only");
 if(checks==930)puts("Native normal player: all 128 bytes match 15 updates after the ready countdown; unsupported service callbacks remain unused");
 if(checks==930)puts("Native player record: 89280 captured bytes across 930 updates, 32-byte tail at three handoff checkpoints, and six gate-global bytes match source");
 if(checks==930){puts("Native stage entry: all 3072 object-pool bytes match the source handoff");
 puts("Native palette entry: all 1024 controller-pool bytes match the source handoff");}
 if(checks>=930)puts("Allocated intro actors: 1656 record states match source trace; renderer views derive from those records");
 if(checks==930)puts("Native spawn entry: 57 spawned records, 22144 pool bytes and all eight allocator stacks match source");
 printf("Native free-slot index: 1296 ownership bits and 81 summary bits checked on every update; peak %u resident patterns\n",maximumResident);
 printf("Native Scroll3: full controller and staged registers checked for %u updates; all 4096 packed map cells checked from update 5\n",walk_probe?1010:930);
 printf("Native incoming column: both 28-cell columns ready before handoff, live-map IDs/palettes and VRAM bytes verified\n");
 printf("Native bounded cache: ready at update %u, before handoff; staged references and pin slots independently checked\n",cacheReadyTick);
 {
  unsigned storage=symbol(argv[3],"tileIndex"),records=storage+3616,indices=records+1024,pool=indices+8;
  if((unsigned short)word(probe_ram,symbol(argv[3],"lookupBuilt"))!=1296 ||
     (longword(probe_ram,pool)&65535)!=records || (longword(probe_ram,pool+4)&65535)!=indices ||
     probe_ram[(pool+8)^1]!=8 || probe_ram[(pool+9)^1]!=128 ||
     probe_ram[(pool+10)^1]!=0 || probe_ram[(pool+11)^1]!=8)SMOKE_FAIL();
  for(unsigned n=0;n<8;n++){
   if(probe_ram[(indices+n)^1]!=7-n)SMOKE_FAIL();
   for(unsigned j=0;j<128;j++)if(probe_ram[(records+n*128+j)^1]!=(j==30?18:0))SMOKE_FAIL();
  }
  puts("Native class-18 pool: eight original-layout records and free-stack order verified in retired intro lookup storage");
 }
 printf("Native heap free: %u bytes\n",(unsigned short)word(probe_ram,symbol(argv[3],"debugIntroHeapFree")));
 {
  unsigned at=longword(probe_ram,symbol(argv[3],"heap"))&65535,largest=0,run=0,total=0;
  for(unsigned n=0;n<32768 && at<65534;n++){
   unsigned block=(unsigned short)word(probe_ram,at),size=block&~1u;
   if(!size)break;
   if(block&1)run=0;else{run+=size;total+=size;if(run>largest)largest=run;}
   at+=size;
  }
  printf("Native heap blocks: %u bytes total, %u bytes largest coalesced run (allocator headers included)\n",total,largest);
 }

 if(palette_probe)puts("Native palette probe: 20 additional palette-flash/expiry updates, exact 32-color source words and timer checked");
 if(walk_probe)puts("Native walking input: 80 additional full player/camera/map states match original; horizontal rendering is not part of this state check");
 if(pose_probe)printf("Native pose probe: %u ROM-selected poses, four updates per pose\n",NATIVE_PLAYER_FRAMES);
 if(argc==5 && !strcmp(argv[4],"--all-pool-allocate-probe")){
  void (*set_reg)(int,unsigned)=dlsym(core,"m68k_set_reg");if(!set_reg || !cpu_reg)return 2;
  unsigned types[]={4,6,12,16,2,8,10,14,18},capacities[]={50,48,60,20,20,20,20,60,8};
  unsigned calls=0;
  probe_ram[0xfe00]=0xfe;probe_ram[0xfe01]=0x60;
  for(unsigned kind=0;kind<9;kind++){
   unsigned pool=kind<8?poolsAt+kind*12:symbol(argv[3],"tileIndex")+4648;
   unsigned records=longword(probe_ram,pool)&65535,indices=longword(probe_ram,pool+4)&65535;
   unsigned count=capacities[kind],stride=types[kind]==4||types[kind]==18?128:64;
   if(records+count*stride>0xf600 || indices+count>0xf600 || probe_ram[(pool+8)^1]!=count || probe_ram[(pool+9)^1]!=stride)SMOKE_FAIL();
   probe_ram[(pool+10)^1]=0;probe_ram[(pool+11)^1]=count;
   for(unsigned n=0;n<count;n++)probe_ram[(indices+n)^1]=count-1-n;
   unsigned char expected[0xf600];memcpy(expected,probe_ram,sizeof expected);
   for(unsigned call=0;call<=count;call++){
    if(call<count){expected[(pool+10)^1]=call+1;expected[(pool+11)^1]=count-1-call;}
    const unsigned char stack[]={0,255,254,0,0,0,0,types[kind]};
    for(unsigned j=0;j<sizeof stack;j++)probe_ram[(0xff00+j)^1]=stack[j];
    set_reg(17,0x2700);set_reg(15,0xffff00);set_reg(16,fullSymbol(argv[3],"normalAllocate"));
    retro_run();
    if(cpu_reg(16)!=0xfffe00 || (call<count?(cpu_reg(0)&65535)!=records+(count-1-call)*stride:cpu_reg(0)!=0) || memcmp(expected,probe_ram,sizeof expected)){
     fprintf(stderr,"Integrated pool allocation mismatch class %u call %u\n",types[kind],call);SMOKE_FAIL();
    }
    calls++;
   }
  }
  printf("Native source allocation: %u compiled allocations/exhaustion calls across all nine classes; full non-stack RAM changes only at the selected count/cursor\n",calls);
  unsigned bases[]={0x9dc6,0xb6c6,0xccc6,0xeac6,0x98c6,0xc2c6,0xc7c6,0xdbc6,0xf490};
  unsigned ends[]={0x918e,0x91ee,0x92b6,0x9356,0x912a,0x9216,0x923e,0x932e,0xf466};
  unsigned cursors[]={0x981c,0x9822,0x9834,0x9840,0x9816,0x9828,0x982e,0x983a,0xf486};
  unsigned counts[]={0x9801,0x9804,0x980d,0x9813,0x97fe,0x9807,0x980a,0x9810,0xf48c};
  unsigned accesses=0;
  for(unsigned kind=0;kind<9;kind++){
   unsigned pool=kind<8?poolsAt+kind*12:symbol(argv[3],"tileIndex")+4648;
   unsigned records=longword(probe_ram,pool)&65535,count=capacities[kind];
   unsigned stride=probe_ram[(pool+9)^1];
   unsigned char expected[0xf600];memcpy(expected,probe_ram,sizeof expected);
   for(unsigned n=0;n<=count;n++)for(unsigned op=0;op<(n==count?2u:4u);op++){
    unsigned address,value,result=0;const char *function;
    if(n==count){
     address=op?counts[kind]:cursors[kind];result=op?0:ends[kind];value=0;
     function=op?"introQueueByte":"introQueueWord";
    }else if(op==0){
     address=ends[kind]-count*2+n*2;result=bases[kind]+(count-1-n)*stride;value=0;function="introQueueWord";
    }else{
     unsigned at=records+n*stride+(op==3?0:stride-2);
     address=bases[kind]+n*stride+(op==3?0:stride-2);value=(0xa591+n*37)&65535;
     if(op==1){result=(unsigned short)word(probe_ram,at);function="introQueueWord";}
     else if(op==2){expected[at^1]=value>>8;expected[(at+1)^1]=value;function="introQueuePutWord";}
     else {expected[at^1]=value;function="introQueuePutByte";}
    }
    const unsigned char stack[]={0,255,254,0,0,0,address>>8,address,0,0,value>>8,value};
    for(unsigned j=0;j<sizeof stack;j++)probe_ram[(0xff00+j)^1]=stack[j];
    set_reg(17,0x2700);set_reg(15,0xffff00);set_reg(16,fullSymbol(argv[3],function));
    retro_run();
    if(cpu_reg(16)!=0xfffe00 || (op<2 && (cpu_reg(0)&(n==count&&op?255:65535))!=result) || memcmp(expected,probe_ram,sizeof expected)){
     fprintf(stderr,"Integrated source bus mismatch class %u address %04x operation %u\n",types[kind],address,op);SMOKE_FAIL();
    }
    accesses++;
   }
  }
  printf("Native source bus: %u target record/stack/count/cursor accesses across all nine pools; unrelated non-stack RAM unchanged\n",accesses);
  const char *fixedNames[]={"debugScroll2Record","debugScroll3Record","debugStageObjects","debugPlayerRecord","debugPaletteControllers","sourceTaskState","sourceTaskState","sourceTaskState","taskRecords","taskCommands","sourceTaskState"};
  unsigned fixedBases[]={0x8402,0x8482,0x8502,0x9846,0xf042,0x0200,0x0a10,0x801a,0x0000,0x8072,0x801e},fixedSizes[]={128,128,3072,128,1024,16,32,4,512,256,4},fixedCalls=0;
  const unsigned fixedOffsets[]={0,0,0,0,0,0,16,48,0,0,84};
  unsigned char fixedExpected[0xf600];memcpy(fixedExpected,probe_ram,sizeof fixedExpected);
  for(unsigned region=0;region<sizeof fixedBases/sizeof fixedBases[0];region++){
   unsigned native=symbol(argv[3],fixedNames[region])+fixedOffsets[region];
   if(native+fixedSizes[region]>0xf600)SMOKE_FAIL();
   for(unsigned offset=0;offset<fixedSizes[region];offset+=2)for(unsigned op=0;op<5;op++){
    unsigned address=fixedBases[region]+offset,at=native+offset;
    unsigned value=(offset*43+region*71+0x9b56)&65535,result=0;
    const char *function;
    if(op==0 || op==2){function="introQueueWord";result=(unsigned short)word(probe_ram,at);}
    else if(op==1){function="introQueuePutWord";fixedExpected[at^1]=value>>8;fixedExpected[(at+1)^1]=value;}
    else if(op==3){function="introQueuePutByte";address++;fixedExpected[(at+1)^1]=value^0xa5;value^=0xa5;}
    else {function="introQueueByte";address++;result=probe_ram[(at+1)^1];}
    const unsigned char stack[]={0,255,254,0,0,0,address>>8,address,0,0,value>>8,value};
    for(unsigned j=0;j<sizeof stack;j++)probe_ram[(0xff00+j)^1]=stack[j];
    set_reg(17,0x2700);set_reg(15,0xffff00);set_reg(16,fullSymbol(argv[3],function));
    retro_run();
    if(cpu_reg(16)!=0xfffe00 || ((op==0||op==2||op==4) && (cpu_reg(0)&(op==4?255:65535))!=result) || memcmp(fixedExpected,probe_ram,sizeof fixedExpected)){
     fprintf(stderr,"Fixed record bus mismatch region %u address %04x operation %u\n",region,address,op);SMOKE_FAIL();
    }
    fixedCalls++;
   }
  }
  printf("Native fixed records: %u target word/byte accesses cover 5304 player/camera/stage-object/palette-controller/task/control/command bytes; unrelated RAM unchanged\n",fixedCalls);
  /* Task/control storage must work before/during lookup conversion without
   * changing the direct index or unrelated source records. */
  unsigned builtAt=symbol(argv[3],"lookupBuilt"),overflowAt=symbol(argv[3],"debugIntroOverflow");
  unsigned savedBuilt=word(probe_ram,builtAt),savedOverflow=word(probe_ram,overflowAt),earlyAccesses=0;
  const unsigned taskAddresses[]={0x0200,0x020e,0x0a10,0x0a2e,0x801a,0x801c,0x0000,0x01fe,0x8072,0x8170,0x801e,0x8020};
  unsigned control=symbol(argv[3],"sourceTaskState"),records=symbol(argv[3],"taskRecords"),commands=symbol(argv[3],"taskCommands");
  const unsigned taskNative[]={control,control+14,control+16,control+46,control+48,control+50,records,records+510,commands,commands+254,control+84,control+86};
  const char *taskFunctions[]={"introQueueWord","introQueueByte","introQueuePutWord","introQueuePutByte"};
  for(unsigned phase=0;phase<2;phase++)for(unsigned n=0;n<sizeof taskAddresses/sizeof taskAddresses[0];n++)for(unsigned op=0;op<4;op++){
   unsigned built=phase?1295:0,address=taskAddresses[n],value=0x91ac;
   probe_ram[builtAt]=built;probe_ram[builtAt+1]=built>>8;
   probe_ram[overflowAt]=savedOverflow;probe_ram[overflowAt+1]=savedOverflow>>8;
   memcpy(fixedExpected,probe_ram,sizeof fixedExpected);
   unsigned native=taskNative[n],result=op?(unsigned)probe_ram[native^1]:(unsigned short)word(probe_ram,native);
   if(op==2){fixedExpected[native^1]=value>>8;fixedExpected[(native+1)^1]=value;}
   else if(op==3)fixedExpected[native^1]=value;
   const unsigned char stack[]={0,255,254,0,0,0,address>>8,address,0,0,value>>8,value};
   for(unsigned j=0;j<sizeof stack;j++)probe_ram[(0xff00+j)^1]=stack[j];
   set_reg(17,0x2700);set_reg(15,0xffff00);set_reg(16,fullSymbol(argv[3],taskFunctions[op]));
   retro_run();
   if(cpu_reg(16)!=0xfffe00 || (op<2 && (cpu_reg(0)&(op?255:65535))!=result) || memcmp(fixedExpected,probe_ram,sizeof fixedExpected)){
    fprintf(stderr,"Early task-control access mismatch: phase %u address %04x operation %u\n",phase,address,op);SMOKE_FAIL();
   }
   earlyAccesses++;
  }
  probe_ram[builtAt]=savedBuilt;probe_ram[builtAt+1]=savedBuilt>>8;
  probe_ram[overflowAt]=savedOverflow;probe_ram[overflowAt+1]=savedOverflow>>8;
  printf("Native task-store lifetime: %u early word/byte accesses preserve the live tile index and unrelated storage\n",earlyAccesses);



 }
 if(argc==5 && !strcmp(argv[4],"--class12-release-probe")){
  void (*set_reg)(int,unsigned)=dlsym(core,"m68k_set_reg");if(!set_reg || !cpu_reg)return 2;
  unsigned pool=poolsAt+2*12,records=longword(probe_ram,pool)&65535,indices=longword(probe_ram,pool+4)&65535;
  if(records+3840>0xf600 || indices+60>0xf600)SMOKE_FAIL();
  for(unsigned i=0;i<3840;i++)probe_ram[(records+i)^1]=(i*73+29)&255;
  probe_ram[(pool+10)^1]=60;probe_ram[(pool+11)^1]=0;
  unsigned char expected[0xf600];memcpy(expected,probe_ram,sizeof expected);
  probe_ram[0xfe00]=0xfe;probe_ram[0xfe01]=0x60;
  for(unsigned call=0;call<60;call++){
   unsigned index=(call*17)%60,record=records+index*64;
   for(unsigned j=0;j<6;j++)expected[(record+j)^1]=0;
   for(unsigned j=14;j<20;j++)expected[(record+j)^1]=0;
   unsigned clear[]={31,32,33,36,46,47};
   for(unsigned j=0;j<sizeof clear/sizeof clear[0];j++)expected[(record+clear[j])^1]=0;
   expected[(pool+10)^1]=59-call;expected[(pool+11)^1]=call+1;
   expected[(indices+59-call)^1]=index;
   unsigned pointer=0xff0000|record;
   const unsigned char stack[]={0,255,254,0,pointer>>24,pointer>>16,pointer>>8,pointer};
   for(unsigned j=0;j<sizeof stack;j++)probe_ram[(0xff00+j)^1]=stack[j];
   set_reg(17,0x2700);set_reg(15,0xffff00);set_reg(16,fullSymbol(argv[3],"introRelease"));
   retro_run();
   if(cpu_reg(16)!=0xfffe00 || memcmp(expected,probe_ram,sizeof expected)){
    fprintf(stderr,"Integrated class-12 release mismatch at record %u\n",index);SMOKE_FAIL();
   }
  }
  for(unsigned call=0;call<=60;call++){
   unsigned result=0;
   if(call<60){
    result=records+expected[(indices+call)^1]*64;
    expected[(pool+10)^1]=call+1;expected[(pool+11)^1]=59-call;
   }
   const unsigned char stack[]={0,255,254,0,0,0,0,12};
   for(unsigned j=0;j<sizeof stack;j++)probe_ram[(0xff00+j)^1]=stack[j];
   set_reg(17,0x2700);set_reg(15,0xffff00);set_reg(16,fullSymbol(argv[3],"normalAllocate"));
   retro_run();
   if(cpu_reg(16)!=0xfffe00 || (call<60?(cpu_reg(0)&65535)!=result:cpu_reg(0)!=0) || memcmp(expected,probe_ram,sizeof expected))SMOKE_FAIL();
  }
  puts("Native class-12 source lifecycle: 60 original-address releases, 60 re-allocations and exhaustion preserve non-cleared fields, free-stack order, visibility queues, other pools and renderer RAM");
 }
 if(class18_probe||class10_probe||class14_probe){
  void (*set_reg)(int,unsigned)=dlsym(core,"m68k_set_reg");if(!set_reg || !cpu_reg)return 2;
  unsigned storage=symbol(argv[3],"tileIndex"),records=storage+3616,pool=records+1032;
  unsigned count=8,stride=128,type=18;
  if(class10_probe){pool=poolsAt+6*12;records=longword(probe_ram,pool)&65535;count=20;stride=64;type=10;}
  if(class14_probe){pool=poolsAt+7*12;records=longword(probe_ram,pool)&65535;count=60;stride=64;type=14;}
  unsigned char recordsBefore[3840];memcpy(recordsBefore,probe_ram+records,count*stride);
  unsigned char lookupBefore[3616];memcpy(lookupBefore,probe_ram+storage,sizeof lookupBefore);
  /* Isolated calls after all gameplay checks; return loop at FFFE00 and stack at FFFF00
   * stay within SGDK's reserved top 0xA00 stack bytes; actor heap ends at F600. SGDK's compiled u8 argument occupies SP+7. */
  if(records+count*stride>0xf600)SMOKE_FAIL();
  probe_ram[0xfe00]=0xfe;probe_ram[0xfe01]=0x60;
  for(unsigned call=0;call<=count;call++){
   const unsigned char stack[]={0x00,0xff,0xfe,0x00,0,0,0,type};
   for(unsigned j=0;j<8;j++)probe_ram[(0xff00+j)^1]=stack[j];
   set_reg(17,0x2700);set_reg(15,0xffff00);set_reg(16,symbol(argv[3],"normalAllocate"));
   retro_run();
   unsigned result=cpu_reg(0);
   if(cpu_reg(16)!=0xfffe00 || (call<count?(result&65535)!=records+(count-1-call)*stride:result!=0))SMOKE_FAIL();
   if(probe_ram[(pool+10)^1]!=(call<count?call+1:count) ||
      probe_ram[(pool+11)^1]!=(call<count?count-1-call:0) ||
      memcmp(lookupBefore,probe_ram+storage,sizeof lookupBefore) ||
      memcmp(recordsBefore,probe_ram+records,count*stride) || word(probe_ram,ov))SMOKE_FAIL();
  }
  printf("Native class-%u allocator: %u descending record allocations and exhaustion preserve the live tile lookup\n",type,count);
 }

 if(argc==5 && (!strcmp(argv[4],"--scene-clear-probe") || !strcmp(argv[4],"--scene-clear-walk-probe"))){
  unsigned expected[1312],stream=longword(probe_ram,symbol(argv[3],"columnStreams"))&65535;
  unsigned planes[]={planeAAt,symbol(argv[3],"planeBAttrs")};
  for(unsigned i=0;i<1312;i++)expected[i]=i<16?0:(unsigned short)word(probe_ram,refsAt+(i-16)*2);
  for(unsigned layer=0;layer<2;layer++){
   for(unsigned slot=0;slot<43;slot++){
    int col=word(probe_ram,stream+layer*sizeof(WillowColumnStream)+slot*2);
    unsigned rows=probe_ram[(stream+layer*sizeof(WillowColumnStream)+86+slot)^1];
    if(col==-32768){if(rows)SMOKE_FAIL();continue;}
    for(unsigned row=0;row<rows;row++){
     unsigned cell=planes[layer]+(row*WILLOW_COLUMN_WINDOW+slot)*2;
     unsigned tile=word(probe_ram,cell)&2047;if(tile>=1312||!expected[tile])SMOKE_FAIL();expected[tile]--;
    }
   }
   for(unsigned row=0;row<32;row++)if(probe_ram[(ownedAt+layer*4+(row>>3))^1]&(1u<<(row&7)))
    for(unsigned x=0;x<40;x++){
     unsigned tile=word(probe_ram,planes[layer]+(row*WILLOW_COLUMN_WINDOW+x)*2)&2047;
     if(tile>=1312||!expected[tile])SMOKE_FAIL();expected[tile]--;
    }
  }
  unsigned request=symbol(argv[3],"debugSceneClearRequest");
  unsigned savedCursor=(unsigned short)word(probe_ram,freeTilesAt+174);
  probe_ram[request^1]=0;probe_ram[(request+1)^1]=1;scenePixelCheck=1;
  unsigned clearFrames=0;
  unsigned phaseAt=symbol(argv[3],"debugSceneClearPhase"),phaseFrames[8]={0};
  while(word(probe_ram,request)!=2 && clearFrames++<120){
   retro_run();unsigned phase=(unsigned short)word(probe_ram,phaseAt);
   if(phase<8)phaseFrames[phase]++;
  }
  for(unsigned n=0;n<3;n++)retro_run();
  if(word(probe_ram,request)!=2||word(probe_ram,ov)||scenePixelFrames<2||sceneNonuniform){fprintf(stderr,"Scene clear request %d overflow %d frames %u nonuniform %u\n",word(probe_ram,request),word(probe_ram,ov),clearFrames,sceneNonuniform);SMOKE_FAIL();}
  unsigned char *vdpRegisters=dlsym(core,"reg");
  if(!vdpRegisters || !(vdpRegisters[1]&0x40) ||
     (unsigned short)word(probe_ram,freeTilesAt+174)!=savedCursor)SMOKE_FAIL();
  for(unsigned i=0;i<16;i++)if(expected[i])SMOKE_FAIL();
  for(unsigned i=16;i<1312;i++)if((unsigned short)word(probe_ram,refsAt+(i-16)*2)!=expected[i])SMOKE_FAIL();
  for(unsigned slot=0;slot<1296;slot++){
   unsigned bits=(unsigned short)word(probe_ram,freeTilesAt+(slot>>4)*2);
   if(!!(bits&(1u<<(slot&15)))!=!expected[slot+16])SMOKE_FAIL();
  }
  for(unsigned group=0;group<81;group++){
   unsigned summary=(unsigned short)word(probe_ram,freeTilesAt+162+(group>>4)*2);
   if(!!(summary&(1u<<(group&15)))!=!!word(probe_ram,freeTilesAt+group*2))SMOKE_FAIL();
  }
  for(unsigned layer=0;layer<2;layer++){
   for(unsigned i=0;i<32*WILLOW_COLUMN_WINDOW;i++)if(word(probe_ram,planes[layer]+i*2))SMOKE_FAIL();
   for(unsigned i=0;i<4096;i++)if(probe_vram[((layer?0xe000:0xc000)+i)^1])SMOKE_FAIL();
   for(unsigned slot=0;slot<43;slot++)
    if(word(probe_ram,stream+layer*sizeof(WillowColumnStream)+slot*2)!=-32768||
       probe_ram[(stream+layer*sizeof(WillowColumnStream)+86+slot)^1])SMOKE_FAIL();
  }
  unsigned near=longword(probe_ram,mapAt)&65535,far=longword(probe_ram,symbol(argv[3],"debugScroll3Cells"))&65535;
  for(unsigned cell=0;cell<4096;cell++){
   if((unsigned short)word(probe_ram,near+cell*2)!=WILLOW_SCROLL2_ZERO_PAIR)SMOKE_FAIL();
   unsigned bit=cell*11,index=bit>>4,shift=bit&15;
   unsigned a=(unsigned short)word(probe_ram,far+index*2);
   unsigned id=shift<=5?(a>>(5-shift))&2047:((a<<(shift-5))|((unsigned short)word(probe_ram,far+(index+1)*2)>>(21-shift)))&2047;
   if(id!=WILLOW_SCROLL3_CLEAR_PAIR)SMOKE_FAIL();
  }
  if((word(probe_vram,0xf402)&127)!=0||word(probe_vram,0xf400)!=96)SMOKE_FAIL();
  printf("Native scene publication: blank rendered output, both planes, hidden SAT, retained maps and exact reference retirement verified after %u source-checked updates\n",endTick);
  printf("Terminal clear completed across %u observed emulated frame; isolated boundary, not full-game timing\n",clearFrames);
  if(clearFrames>1){fprintf(stderr,"Scene clear exceeded the single-frame regression limit\n");SMOKE_FAIL();}
  printf("Frame-boundary phase samples: maps %u references %u ownership tags %u CPU planes %u plane A DMA %u plane B DMA %u SAT/done %u\n",phaseFrames[1],phaseFrames[2],phaseFrames[3],phaseFrames[4],phaseFrames[5],phaseFrames[6],phaseFrames[7]);
  unsigned times=symbol(argv[3],"debugSceneClearTicks");
  printf("Clear phase subticks (SDK timer, approximate):");
  for(unsigned i=1;i<8;i++)printf(" %lu",longword(probe_ram,times+i*4)-longword(probe_ram,times+(i-1)*4));
  printf("; total %lu, excluding request/VBlank wait\n",longword(probe_ram,times+28)-longword(probe_ram,times));
 }
 if(argc==5 && (!strcmp(argv[4],"--layer-clear-probe") || !strcmp(argv[4],"--layer-window-clear-probe") || !strcmp(argv[4],"--layer-palette-hide-probe") || !strcmp(argv[4],"--source-backdrop-hide-probe"))){
  if(nativePlaneHideCheck(core,probe_ram,probe_vram,argv[3],retro_run,cpu_reg,!strcmp(argv[4],"--source-backdrop-hide-probe")?3:!strcmp(argv[4],"--layer-palette-hide-probe")?2:!strcmp(argv[4],"--layer-window-clear-probe")))SMOKE_FAIL();
 }
 if(argc==5 && !strcmp(argv[4],"--object-cartridge-probe")){
  if(nativeObjectCartridgeCheck(core,probe_ram,argv[3],retro_run,cpu_reg,objectCartridgeCapacity))SMOKE_FAIL();
 }
 if(argc==5 && !strcmp(argv[4],"--sprite-bank-probe")){
  if(nativeSpriteBankCheck(core,probe_ram,argv[3],retro_run,cpu_reg))SMOKE_FAIL();
 }
 if(argc==5 && !strcmp(argv[4],"--scene-map-fill-probe")){
  if(nativeSceneMapFillCheck(core,probe_ram,argv[3],retro_run,cpu_reg))SMOKE_FAIL();
 }
 if(argc==5 && (!strcmp(argv[4],"--boot-fade-prefix-probe")||!strcmp(argv[4],"--scene-boot-probe")||!strcmp(argv[4],"--scene-combined-probe")||!strcmp(argv[4],"--scene-text-probe")||!strcmp(argv[4],"--scene-warning-probe")||!strcmp(argv[4],"--scene-handoff-probe")||!strcmp(argv[4],"--scene-service-handoff-probe")||!strcmp(argv[4],"--scene-attract-init-probe")||!strcmp(argv[4],"--scene-attract-fade-probe")||!strcmp(argv[4],"--scene-pools-probe")||!strcmp(argv[4],"--scene-initialize-probe")||(!strcmp(argv[4],"--scene-dispatch-probe")||(!strcmp(argv[4],"--scene-spawn-probe")||!strcmp(argv[4],"--scene-countdown-probe"))))){
  if(!strcmp(argv[4],"--scene-text-probe")){
   void(*setReg)(int,unsigned)=dlsym(core,"m68k_set_reg");
   if(!setReg||nativeSATCall(probe_ram,argv[3],setReg,cpu_reg,retro_run,"DMA_flushQueue",0,NULL)==~0u)SMOKE_FAIL();
  }
  if(nativeBootFadePrefixCheck(core,probe_ram,argv[3],retro_run,cpu_reg,strcmp(argv[4],"--boot-fade-prefix-probe")?objectCartridgeCapacity:0,(!strcmp(argv[4],"--scene-attract-fade-probe")||!strcmp(argv[4],"--scene-initialize-probe")||(!strcmp(argv[4],"--scene-dispatch-probe")||(!strcmp(argv[4],"--scene-spawn-probe")||!strcmp(argv[4],"--scene-countdown-probe"))))?5:!strcmp(argv[4],"--scene-attract-init-probe")?4:!strcmp(argv[4],"--scene-service-handoff-probe")?3:!strcmp(argv[4],"--scene-handoff-probe")?2:!strcmp(argv[4],"--scene-warning-probe")))SMOKE_FAIL();
  if(strcmp(argv[4],"--boot-fade-prefix-probe")&&strcmp(argv[4],"--scene-warning-probe")&&strcmp(argv[4],"--scene-handoff-probe")&&strcmp(argv[4],"--scene-service-handoff-probe")&&strcmp(argv[4],"--scene-attract-init-probe")&&strcmp(argv[4],"--scene-attract-fade-probe")&&strcmp(argv[4],"--scene-pools-probe")&&strcmp(argv[4],"--scene-initialize-probe")&&strcmp(argv[4],"--scene-dispatch-probe")&&strcmp(argv[4],"--scene-spawn-probe")&&strcmp(argv[4],"--scene-countdown-probe")&&nativeScenePlaneCommitCheck(core,probe_ram,argv[3],retro_run,cpu_reg,!strcmp(argv[4],"--scene-combined-probe")||!strcmp(argv[4],"--scene-text-probe")))SMOKE_FAIL();
  if((!strcmp(argv[4],"--scene-pools-probe")||!strcmp(argv[4],"--scene-initialize-probe")||(!strcmp(argv[4],"--scene-dispatch-probe")||(!strcmp(argv[4],"--scene-spawn-probe")||!strcmp(argv[4],"--scene-countdown-probe"))))&&nativeScenePoolsCheck(core,probe_ram,argv[3],retro_run,cpu_reg,(!strcmp(argv[4],"--scene-dispatch-probe")||(!strcmp(argv[4],"--scene-spawn-probe")||!strcmp(argv[4],"--scene-countdown-probe")))?2:!strcmp(argv[4],"--scene-initialize-probe")))SMOKE_FAIL();
  if((!strcmp(argv[4],"--scene-spawn-probe")||!strcmp(argv[4],"--scene-countdown-probe"))&&nativeSceneSpawnCheck(core,probe_ram,argv[3],retro_run,cpu_reg))SMOKE_FAIL();
  if(getenv("WILLOW_AUDIT_DEMO_STATE")&&!strcmp(argv[4],"--scene-countdown-probe")){
   if(nativeDemoStateCheck(core,probe_ram,argv[3],retro_run,cpu_reg))SMOKE_FAIL();
  }
  if(getenv("WILLOW_AUDIT_DEMO_INIT")&&!strcmp(argv[4],"--scene-countdown-probe")){
   if(nativeDemoInitCheck(core,probe_ram,argv[3],retro_run,cpu_reg))SMOKE_FAIL();
  }
  /* Independent continuous-title comparison, separate from blockwise services. */
  if(getenv("WILLOW_TITLE_CONCURRENT")&&!strcmp(argv[4],"--scene-countdown-probe")){
   if(nativePanConcurrentCheck(core,probe_ram,argv[3],retro_run,cpu_reg))SMOKE_FAIL();
   puts("Continuous title-context comparison completed; blockwise service suite is a separate run");return 0;
  }
  if((!strcmp(argv[4],"--scene-spawn-probe")||!strcmp(argv[4],"--scene-countdown-probe"))&&nativeOpeningActorsCheck(core,probe_ram,argv[3],retro_run,cpu_reg,!strcmp(argv[4],"--scene-countdown-probe")))SMOKE_FAIL();
  if((!strcmp(argv[4],"--scene-spawn-probe")||!strcmp(argv[4],"--scene-countdown-probe"))&&nativeBackdropSetupCheck(core,probe_ram,argv[3],retro_run,cpu_reg,!strcmp(argv[4],"--scene-countdown-probe")))SMOKE_FAIL();
  if(!strcmp(argv[4],"--scene-countdown-probe")&&nativeOpeningActorsCheck(core,probe_ram,argv[3],retro_run,cpu_reg,3))SMOKE_FAIL();
  if(!strcmp(argv[4],"--scene-countdown-probe")&&nativeOpeningMessageCheck(core,probe_ram,argv[3],retro_run,cpu_reg,0xc0))SMOKE_FAIL();
  if(!strcmp(argv[4],"--scene-countdown-probe")&&nativeOpeningActorsCheck(core,probe_ram,argv[3],retro_run,cpu_reg,4))SMOKE_FAIL();
  if(!strcmp(argv[4],"--scene-countdown-probe")&&nativeOpeningWaitTerminalCheck(core,probe_ram,argv[3],retro_run,cpu_reg,0))SMOKE_FAIL();
  if(!strcmp(argv[4],"--scene-countdown-probe")&&nativeOpeningActorsCheck(core,probe_ram,argv[3],retro_run,cpu_reg,3))SMOKE_FAIL();
  if(!strcmp(argv[4],"--scene-countdown-probe")&&nativeOpeningMessageCheck(core,probe_ram,argv[3],retro_run,cpu_reg,0xc0))SMOKE_FAIL();
  if(!strcmp(argv[4],"--scene-countdown-probe")&&nativeOpeningActorsCheck(core,probe_ram,argv[3],retro_run,cpu_reg,5))SMOKE_FAIL();
  if(!strcmp(argv[4],"--scene-countdown-probe")&&nativeOpeningWaitTerminalCheck(core,probe_ram,argv[3],retro_run,cpu_reg,1))SMOKE_FAIL();
  if(!strcmp(argv[4],"--scene-countdown-probe")&&nativeOpeningFadesCheck(core,probe_ram,argv[3],retro_run,cpu_reg))SMOKE_FAIL();
  if(!strcmp(argv[4],"--scene-countdown-probe")&&nativeOpeningClearCheck(core,probe_ram,argv[3],retro_run,cpu_reg))SMOKE_FAIL();
  if(!strcmp(argv[4],"--scene-countdown-probe")&&nativePanConcurrentCheck(core,probe_ram,argv[3],retro_run,cpu_reg))SMOKE_FAIL();
  if(!strcmp(argv[4],"--scene-countdown-probe")&&nativeOpeningPanCheck(core,probe_ram,argv[3],retro_run,cpu_reg))SMOKE_FAIL();
  if(!strcmp(argv[4],"--scene-countdown-probe")&&nativePhaseFourTextCheck(core,probe_ram,argv[3],retro_run,cpu_reg))SMOKE_FAIL();
  if(!strcmp(argv[4],"--scene-countdown-probe")&&nativeIntroPanelCheck(core,probe_ram,argv[3],retro_run,cpu_reg))SMOKE_FAIL();
  if(!strcmp(argv[4],"--scene-countdown-probe")&&nativeIntroPanelHoldCheck(core,probe_ram,argv[3],retro_run,cpu_reg))SMOKE_FAIL();
  if(!strcmp(argv[4],"--scene-text-probe")&&nativeSceneTextCommitCheck(core,probe_ram,argv[3],retro_run,cpu_reg))SMOKE_FAIL();
 }
 if(argc==5 && !strcmp(argv[4],"--boot-continuation-probe")){
  if(nativeBootContinuationCheck(core,probe_ram,argv[3],retro_run,cpu_reg))SMOKE_FAIL();
 }
 if(argc==5 && !strcmp(argv[4],"--controller-history-probe")){
  if(nativeControllerHistoryCheck(core,probe_ram,argv[3],retro_run,cpu_reg))SMOKE_FAIL();
 }
 if(argc==5 && !strcmp(argv[4],"--source-input-probe")){
  if(nativeSourceInputCheck(core,probe_ram,argv[3],retro_run,cpu_reg))SMOKE_FAIL();
 }
 if(argc==5 && !strcmp(argv[4],"--replay-phase-probe")){
  if(nativeReplayPhaseCheck(core,probe_ram,argv[3],retro_run,cpu_reg))SMOKE_FAIL();
 }
 if(argc==5 && !strcmp(argv[4],"--frame-clock-probe")){
  if(nativeFrameClockCheck(core,probe_ram,argv[3],retro_run,cpu_reg))SMOKE_FAIL();
 }
 if(argc==5 && !strcmp(argv[4],"--player-cancel-probe")){
  if(nativePlayerCancelCheck(core,probe_ram,argv[3],retro_run,cpu_reg))SMOKE_FAIL();
 }
 if(argc==5 && !strcmp(argv[4],"--player-rng-probe")){
  if(nativePlayerRandomCheck(core,probe_ram,argv[3],retro_run,cpu_reg))SMOKE_FAIL();
 }
 if(argc==5 && !strcmp(argv[4],"--player-sound-probe")){
  if(nativePlayerSoundCheck(core,probe_ram,argv[3],retro_run,cpu_reg))SMOKE_FAIL();
 }
 if(argc==5 && !strcmp(argv[4],"--sat-publication-probe")){
  if(nativeSATPublicationCheck(core,probe_ram,probe_vram,argv[3],retro_run,cpu_reg))SMOKE_FAIL();
 }
 if(argc==5 && !strcmp(argv[4],"--video-publish-probe")){
  void(*setReg)(int,unsigned)=dlsym(core,"m68k_set_reg");if(!setReg||!cpu_reg)return 2;
  if(nativeVideoPublishCheck(probe_ram,argv[3],setReg,cpu_reg,retro_run,bootGlobalsNative,taskCommandNative))SMOKE_FAIL();
 }
 if(argc==5 && !strcmp(argv[4],"--boot-globals-probe")){
  void(*set_reg)(int,unsigned)=dlsym(core,"m68k_set_reg");if(!set_reg||!cpu_reg)return 2;
  unsigned char expected[0xf600];unsigned accesses=0;
  unsigned builtAt=symbol(argv[3],"lookupBuilt");
  for(unsigned phase=0;phase<2;phase++){
   unsigned built=phase?1295:0;probe_ram[builtAt]=built;probe_ram[builtAt+1]=built>>8;
   for(unsigned r=0;r<sizeof(willowNativeBootGlobalRanges)/sizeof(willowNativeBootGlobalRanges[0]);r++){
    const WillowNativeBootGlobalRange*range=willowNativeBootGlobalRanges+r;
    for(unsigned n=0;n<range->count;n++)for(unsigned op=0;op<4;op++){
     unsigned address=range->source+n,at=bootGlobalsNative+range->offset+n;
     if(op>=2&&((address&1)||n+1>=range->count))continue;
     unsigned value=(address*73+phase*31)&65535;
     memcpy(expected,probe_ram,sizeof expected);
     const char*fn;unsigned result=0;
     if(op==0){fn="introQueueByte";result=probe_ram[at^1];}
     else if(op==1){fn="introQueuePutByte";expected[at^1]=value;}
     else if(op==2){fn="introQueueWord";result=(probe_ram[at^1]<<8)|probe_ram[(at+1)^1];}
     else{fn="introQueuePutWord";expected[at^1]=value>>8;expected[(at+1)^1]=value;}
     probe_ram[0xfe00^1]=0x60;probe_ram[0xfe01^1]=0xfe;
     const unsigned char stack[]={0,255,254,0,0,0,address>>8,address,0,0,value>>8,value};
     for(unsigned j=0;j<sizeof stack;j++)probe_ram[(0xff00+j)^1]=stack[j];
     set_reg(17,0x2700);set_reg(15,0xffff00);set_reg(16,fullSymbol(argv[3],fn));retro_run();
     if(cpu_reg(16)!=0xfffe00 || ((op==0||op==2)&&(cpu_reg(0)&(op?65535:255))!=result)||memcmp(expected,probe_ram,sizeof expected)){
      fprintf(stderr,"Native boot globals mismatch address %04x op %u phase %u\n",address,op,phase);SMOKE_FAIL();
     }
     accesses++;
    }
   }
  }
  printf("Native boot globals: %u compiled byte/word accesses cover all 179 retained bytes before lookup conversion; unrelated RAM preserved\n",accesses);
 }
 if(argc==5 && !strcmp(argv[4],"--startup-latched-probe")){
  void(*set_reg)(int,unsigned)=dlsym(core,"m68k_set_reg");if(!set_reg||!cpu_reg)return 2;
  {
   unsigned char expected[0xf600],sourceWords[6144],originalWords[6144];
   const unsigned bases[]={0xe5080,0xe6ea0,0xe7d20,0xeab00,0xe6080,0xe64a0};
   FILE*program=fopen("build/reference_maincpu.bin","rb");if(!program)return 2;
   for(unsigned bank=0;bank<6;bank++)if(fseek(program,bases[bank],SEEK_SET)||fread(sourceWords+bank*1024,1,1024,program)!=1024)return 2;
   fclose(program);
   FILE*oracle=fopen("build/boot-palette-original-words.bin","rb");if(!oracle||fread(originalWords,1,sizeof originalWords,oracle)!=sizeof originalWords)return 2;fclose(oracle);
   memcpy(expected,probe_ram,sizeof expected);
   memset(expected+paletteOwnerAt+384,0,96);
   for(unsigned group=0;group<192;group++){
    unsigned bank=group/32,source=bases[bank]+(group%32)*32,tag=(source-0xe0000)/2;
    expected[(paletteOwnerAt+group*2)^1]=tag>>8;expected[(paletteOwnerAt+group*2+1)^1]=tag;
    unsigned offset=bank<4?0:15-(sourceWords[group*32]>>4);
    unsigned at=paletteOwnerAt+384+group/2;
    expected[at^1]|=offset<<((group&1)?4:0);
    for(unsigned n=0;n<16;n++){
     unsigned index=group*32+n*2,value=((sourceWords[index]<<8)|sourceWords[index+1]);
     value=(value+(offset<<12))&65535;
     if(value!=((originalWords[index]<<8)|originalWords[index+1]))return 1;
    }
   }
   for(unsigned slot=0;slot<2;slot++){expected[(paletteSlotsAt+slot*34)^1]=255;expected[(paletteSlotsAt+slot*34+1)^1]=255;}
   probe_ram[0xff80^1]=0x60;probe_ram[0xff81^1]=0xfe;
   const unsigned char ret[]={0,255,255,128};for(unsigned n=0;n<4;n++)probe_ram[(0xff00+n)^1]=ret[n];
   set_reg(17,0x2700);set_reg(15,0xffff00);set_reg(16,fullSymbol(argv[3],"sourceBootPaletteInitialize"));
   for(unsigned slice=0;slice<8&&cpu_reg(16)!=0xffff80;slice++)retro_run();
   if(cpu_reg(16)!=0xffff80||(cpu_reg(0)&65535)!=1||memcmp(expected,probe_ram,sizeof expected)){
    fprintf(stderr,"Native boot palette initialization mismatch PC %x result %x\n",cpu_reg(16),cpu_reg(0)&65535);SMOKE_FAIL();
   }
   puts("Native boot palette owner: all 3072 words match original 076C/08FE output; both mutable slots free and unrelated RAM preserved");
   if(nativePaletteTaskCheck(probe_ram,argv[3],set_reg,cpu_reg,retro_run))SMOKE_FAIL();
  }
  FILE*f=fopen("build/startup-latched-native-fixtures.bin","rb");if(!f)return 2;
  unsigned char before[65536],after[65536],expected[0xf600];
  const unsigned sources[]={0,0x0200,0x0a10,0x801a,0x801e,0x8072,0x8172,0xefc6,0xf03e};
  const unsigned targets[]={taskRecordNative,taskStateNative,taskStateNative+16,taskStateNative+48,taskStateNative+84,taskCommandNative,soundCommandsNative,highScoresNative,hudAt+14};
  const unsigned sizes[]={512,16,32,4,4,256,128,80,4};
  unsigned starts=0,preservedStarts=0,bootDispatches=0,consumerChecks=0;
  for(unsigned sample=0;sample<256;sample++){
   if(fread(before,1,sizeof before,f)!=sizeof before||fread(after,1,sizeof after,f)!=sizeof after)return 2;
   for(unsigned region=0;region<9;region++)for(unsigned n=0;n<sizes[region];n++)
    probe_ram[(targets[region]+n)^1]=before[sources[region]+n];
   for(unsigned range=0;range<sizeof willowNativeBootGlobalRanges/sizeof willowNativeBootGlobalRanges[0];range++){
    const WillowNativeBootGlobalRange*r=&willowNativeBootGlobalRanges[range];
    for(unsigned n=0;n<r->count;n++)probe_ram[(bootGlobalsNative+r->offset+n)^1]=before[r->source+n];
   }
   /* Original initialization overwrites all pointer bytes without reading
    * them. Arbitrary pre-fixture pointers need no native representation. */
   for(unsigned n=0;n<5;n++)probe_ram[(highScoresNative+80+n)^1]=sample*31+n;
   memcpy(expected,probe_ram,sizeof expected);
   for(unsigned region=0;region<9;region++)for(unsigned n=0;n<sizes[region];n++)
    expected[(targets[region]+n)^1]=after[sources[region]+n];
   for(unsigned range=0;range<sizeof willowNativeBootGlobalRanges/sizeof willowNativeBootGlobalRanges[0];range++){
    const WillowNativeBootGlobalRange*r=&willowNativeBootGlobalRanges[range];
    for(unsigned n=0;n<r->count;n++)expected[(bootGlobalsNative+r->offset+n)^1]=after[r->source+n];
   }
   for(unsigned n=0;n<5;n++)expected[(highScoresNative+80+n)^1]=255;
   for(unsigned slot=0;slot<=0xa0;slot+=0xa0){if(before[slot])preservedStarts++;else starts++;}
   unsigned d2=(sample*317)&65535;
   probe_ram[0xfe00^1]=0x60;probe_ram[0xfe01^1]=0xfe;
   const unsigned char stack[]={0,255,254,0,0,0,d2>>8,d2};
   for(unsigned n=0;n<sizeof stack;n++)probe_ram[(0xff00+n)^1]=stack[n];
   set_reg(17,0x2700);set_reg(15,0xffff00);set_reg(16,fullSymbol(argv[3],"sourceStartupLatched"));
   for(unsigned slice=0;slice<4&&cpu_reg(16)!=0xfffe00;slice++)retro_run();
   if(cpu_reg(16)!=0xfffe00||(cpu_reg(0)&65535)!=(((sample&63)*2+8)&126)||memcmp(expected,probe_ram,sizeof expected)){
    fprintf(stderr,"Native latched startup mismatch sample %u PC %x D1 %x\n",sample,cpu_reg(16),cpu_reg(0)&65535);
    for(unsigned n=0;n<sizeof expected;n++)if(expected[n]!=probe_ram[n]){fprintf(stderr,"First native byte %04x got %02x want %02x\n",n^1,probe_ram[n],expected[n]);break;}
    SMOKE_FAIL();
   }
   if(!before[0]){
    /* Enter at the separately verified scheduler restart with a source-valid
     * empty producer cursor. Other fixture bytes remain arbitrary/retained. */
    probe_ram[(taskStateNative+14)^1]=0;
    probe_ram[(taskStateNative+48)^1]=0;probe_ram[(taskStateNative+49)^1]=0;
    memcpy(expected,probe_ram,sizeof expected);
#define BOOT_WORD(at,v) do{expected[(at)^1]=(v)>>8;expected[((at)+1)^1]=(v)&255;}while(0)
    BOOT_WORD(taskRecordNative,0x0101);
    BOOT_WORD(taskRecordNative+2,0x7fde);BOOT_WORD(taskRecordNative+4,3);BOOT_WORD(taskRecordNative+8,4);
    BOOT_WORD(taskStateNative+4,255);BOOT_WORD(taskStateNative+6,0);
    BOOT_WORD(taskStateNative+48,4);BOOT_WORD(taskStateNative+52,0x1ea2);
    BOOT_WORD(taskCommandNative,0x0c00);BOOT_WORD(taskCommandNative+2,3);
    /* Descriptor lookup is independent of its packed native offset. */
    unsigned flag=65535;
    for(unsigned n=0;n<sizeof willowNativeBootGlobalRanges/sizeof willowNativeBootGlobalRanges[0];n++){
     const WillowNativeBootGlobalRange*r=&willowNativeBootGlobalRanges[n];
     if(r->source<=0xf891&&0xf891<r->source+r->count)flag=bootGlobalsNative+r->offset+0xf891-r->source;
    }
    if(flag==65535)return 2;expected[flag^1]=1;
    const unsigned char ret[]={0,255,254,0};for(unsigned n=0;n<4;n++)probe_ram[(0xff00+n)^1]=ret[n];
    set_reg(17,0x2700);set_reg(15,0xffff00);set_reg(16,fullSymbol(argv[3],"sourceBootDispatch"));
    for(unsigned slice=0;slice<4&&cpu_reg(16)!=0xfffe00;slice++)retro_run();
    if(cpu_reg(16)!=0xfffe00||(cpu_reg(0)&65535)!=0x424f||memcmp(expected,probe_ram,sizeof expected)){
     fprintf(stderr,"Native first boot dispatch mismatch sample %u PC %x status %x\n",sample,cpu_reg(16),cpu_reg(0)&65535);
     for(unsigned n=0;n<sizeof expected;n++)if(expected[n]!=probe_ram[n]){fprintf(stderr,"First native byte %04x got %02x want %02x\n",n^1,probe_ram[n],expected[n]);break;}
     SMOKE_FAIL();
    }
    bootDispatches++;
    if(!before[0xa0]){
     /* These source-valid inputs disable the consumer's optional wake scan;
      * the source wake routine has separate original-CPU coverage. */
     unsigned controlFlags[]={0xf99c,0x828f};
     for(unsigned n=0;n<2;n++)probe_ram[(bootGlobalsNative+nativeBootGlobalOffset(controlFlags[n]))^1]=0;
     probe_ram[(taskStateNative+50)^1]=0;probe_ram[(taskStateNative+51)^1]=0;
     memcpy(expected,probe_ram,sizeof expected);
#define RUN_TASK(slot,status) do{ \
 const unsigned char args[]={0,255,255,128,0,0,(slot)>>8,(slot)&255,0,0,0,0}; \
 probe_ram[0xff80^1]=0x60;probe_ram[0xff81^1]=0xfe; \
 for(unsigned n=0;n<sizeof args;n++)probe_ram[(0xff00+n)^1]=args[n]; \
 set_reg(17,0x2700);set_reg(15,0xffff00);set_reg(16,fullSymbol(argv[3],"sourceTaskDispatch")); \
 for(unsigned slice=0;slice<4&&cpu_reg(16)!=0xffff80;slice++)retro_run(); \
 if(cpu_reg(16)!=0xffff80||(cpu_reg(0)&65535)!=(status)||memcmp(expected,probe_ram,sizeof expected)){ \
  fprintf(stderr,"Native command dispatch mismatch sample %u slot %04x status %x PC %x SP %x\n",sample,(slot),cpu_reg(0)&65535,cpu_reg(16),cpu_reg(15));SMOKE_FAIL();} \
 consumerChecks++; \
}while(0)
     BOOT_WORD(taskRecordNative+0xa0,0x0400);BOOT_WORD(taskStateNative+6,0xa0);
     BOOT_WORD(taskStateNative+62,0x1e18);
     RUN_TASK(0xa0,0x424f);
     unsigned allocated=((unsigned)expected[(taskStateNative+18)^1]<<8)|expected[(taskStateNative+19)^1];
     if(allocated<0x100||allocated>=0x200||(allocated&31))return 2;
     BOOT_WORD(taskCommandNative,65535);BOOT_WORD(taskCommandNative+2,65535);
     BOOT_WORD(taskStateNative+50,4);BOOT_WORD(taskStateNative+10,0x0a14);BOOT_WORD(taskStateNative+12,7);
     BOOT_WORD(taskRecordNative+allocated,0x0c00);BOOT_WORD(taskRecordNative+allocated+4,0);
     BOOT_WORD(taskRecordNative+allocated+6,0x582c);BOOT_WORD(taskRecordNative+allocated+16,0);
     BOOT_WORD(taskRecordNative+allocated+18,3);
     RUN_TASK(0xa0,0x424f);
     /* Selector 2 requires video immediately and must reject atomically.
      * Selector 0's enqueue/wait prefix is covered by the boot/fade chain. */
     probe_ram[(taskRecordNative+allocated+17)^1]=2;
     memcpy(expected,probe_ram,sizeof expected);
     RUN_TASK(allocated,1);
     /* The same rule applies to an unconnected global palette fade. */
     probe_ram[(taskRecordNative+allocated+6)^1]=0x50;
     probe_ram[(taskRecordNative+allocated+7)^1]=0x9c;
     probe_ram[(taskRecordNative+allocated+17)^1]=0;
     memcpy(expected,probe_ram,sizeof expected);
     RUN_TASK(allocated,1);
#undef RUN_TASK
    }
#undef BOOT_WORD
   }
  }
  if(fgetc(f)!=EOF)return 2;fclose(f);
  printf("Native command consumer: %u wake/consume/dependency checks; original target table, free-stack allocation and unavailable video/palette preservation verified\n",consumerChecks);
  printf("Native boot entry: %u first dispatches through sparse native bus; original command, mandatory sleep, register continuation and unrelated RAM verified\n",bootDispatches);
  printf("Native latched startup: 256 original-CPU fixtures, %u inactive task starts, %u active tasks preserved; all option combinations per byte, 64 sound cursors, inherited D2 and unrelated RAM checked\n",starts,preservedStarts);
 }
 if(argc==5 && !strcmp(argv[4],"--task-init-probe")){
  void(*set_reg)(int,unsigned)=dlsym(core,"m68k_set_reg");if(!set_reg||!cpu_reg)return 2;
  unsigned char table[176];FILE*f=fopen("build/reference_maincpu.bin","rb");
  if(!f||fseek(f,0x17dc,SEEK_SET)||fread(table,1,sizeof table,f)!=sizeof table)return 2;fclose(f);
  unsigned trials=0;
  for(unsigned trial=0;trial<4;trial++){
   /* Trial zero checks the real startup result. Later trials execute the
    * compiled initializer over arbitrary preserved bytes, as source does. */
   const unsigned bases[]={taskStateNative,taskRecordNative,taskCommandNative,highScoresNative,soundCommandsNative},sizes[]={88,512,256,85,128};
   if(trial)for(unsigned region=0;region<5;region++)for(unsigned n=0;n<sizes[region];n++)
    probe_ram[(bases[region]+n)^1]=(n*73+region*31+trial*47)&255;
   unsigned char expected[0xf600];memcpy(expected,probe_ram,sizeof expected);
   for(unsigned slot=0;slot<16;slot++)for(unsigned n=0;n<4;n++)expected[(taskRecordNative+slot*32+12+n)^1]=table[slot*4+n];
   for(unsigned slot=0;slot<8;slot++)for(unsigned n=0;n<4;n++)expected[(taskStateNative+16+(7-slot)*4+n)^1]=table[64+slot*4+n];
   /* Reset LEA $8000.W,A5 sign-extends; the original stores full A6. */
   const unsigned values[]={255,255,10,16,0,8};
   for(unsigned n=0;n<6;n++)expected[(taskStateNative+8+n)^1]=values[n];
   for(unsigned n=0;n<4;n++){expected[(taskStateNative+48+n)^1]=0;expected[(taskStateNative+84+n)^1]=0;}
   expected[(taskStateNative+85)^1]=8;
   const unsigned char sound[]={0,0xf0,0,0xff,0,0xf7,0,0xff};
   for(unsigned n=0;n<8;n++)expected[(soundCommandsNative+n)^1]=sound[n];
   for(unsigned n=0;n<256;n++)expected[(taskCommandNative+n)^1]=255;
   for(unsigned n=0;n<80;n++)expected[(highScoresNative+n)^1]=table[96+n];
   for(unsigned n=0;n<5;n++)expected[(highScoresNative+80+n)^1]=255;
   for(unsigned n=0;n<4;n++)expected[(hudAt+14+n)^1]=table[96+n];
   if(trial){
    probe_ram[0xfe00^1]=0x60;probe_ram[0xfe01^1]=0xfe;
    const unsigned ret[]={0,255,254,0};for(unsigned n=0;n<4;n++)probe_ram[(0xff00+n)^1]=ret[n];
    set_reg(17,0x2700);set_reg(15,0xffff00);set_reg(16,fullSymbol(argv[3],"sourceTaskInitialize"));
    /* Cold initialization is not a gameplay frame; finish the masked call. */
    for(unsigned slice=0;slice<4&&cpu_reg(16)!=0xfffe00;slice++)retro_run();
    if(cpu_reg(16)!=0xfffe00){fprintf(stderr,"Initializer PC %x SP %x\n",cpu_reg(16),cpu_reg(15));SMOKE_FAIL();}
   }
   if(memcmp(expected,probe_ram,sizeof expected)){fprintf(stderr,"Native task initialization mismatch trial %u\n",trial);SMOKE_FAIL();}
   trials++;
  }
  unsigned char preserved[0xf600];memcpy(preserved,probe_ram,sizeof preserved);
  for(unsigned address=0xefc6;address<0xf042;address++){
   unsigned wanted;
   if(address<0xf016)wanted=table[96+address-0xefc6];
   else if(address<0xf03e){unsigned offset=address-0xf016,pointer=0xffffefc6u+(offset/4)*8;wanted=(pointer>>((3-offset%4)*8))&255;}
   else wanted=table[96+address-0xf03e];
   probe_ram[0xfe00^1]=0x60;probe_ram[0xfe01^1]=0xfe;
   const unsigned char stack[]={0,255,254,0,0,0,address>>8,address};
   for(unsigned n=0;n<sizeof stack;n++)probe_ram[(0xff00+n)^1]=stack[n];
   set_reg(17,0x2700);set_reg(15,0xffff00);set_reg(16,fullSymbol(argv[3],"introQueueByte"));retro_run();
   if(cpu_reg(16)!=0xfffe00||(cpu_reg(0)&255)!=wanted||memcmp(preserved,probe_ram,sizeof preserved)){
    fprintf(stderr,"Native high-score readback mismatch at %04x\n",address);SMOKE_FAIL();
   }
  }
  unsigned soundAccesses=0;
  for(unsigned n=0;n<64;n++)for(unsigned op=0;op<4;op++){
   unsigned address=0x8172+n*2,at=soundCommandsNative+n*2,value=(0xa591+n*977)&65535;
   const char *fn;unsigned wanted=0;
   if(op==0){fn="introQueuePutWord";preserved[at^1]=value>>8;preserved[(at+1)^1]=value;}
   else if(op==2){fn="introQueuePutByte";address++;value^=0x5a;preserved[(at+1)^1]=value;}
   else{fn="introQueueWord";wanted=(preserved[at^1]<<8)|preserved[(at+1)^1];}
   const unsigned char stack[]={0,255,254,0,0,0,address>>8,address,0,0,value>>8,value};
   for(unsigned j=0;j<sizeof stack;j++)probe_ram[(0xff00+j)^1]=stack[j];
   set_reg(17,0x2700);set_reg(15,0xffff00);set_reg(16,fullSymbol(argv[3],fn));retro_run();
   if(cpu_reg(16)!=0xfffe00||((op&1)&&(cpu_reg(0)&65535)!=wanted)||memcmp(preserved,probe_ram,sizeof preserved)){
    fprintf(stderr,"Native sound ring mismatch word %u op %u\n",n,op);SMOKE_FAIL();
   }
   soundAccesses++;
  }
  printf("Native sound ring: %u compiled byte/word accesses preserve all 64 words and unrelated RAM\n",soundAccesses);
  puts("Native high-score startup: 80 score/name bytes, 40 expanded pointer bytes and shared four-byte high score match original data through compiled source-bus reads");
  printf("Native task initialization: real cold startup and %u retained-state executions match original stack/free-pointer tables, queue sentinels, counts and offsets; unrelated RAM unchanged\n",trials-1);
 }
 if(hudIndexChecks)printf("Native packed HUD index: %u cell comparisons against retained sprite geometry\n",hudIndexChecks);
 if(controllerHistoryChecks)printf("Native live controller history: %u current/previous-byte comparisons against original scene traces\n",controllerHistoryChecks);
 if(lifetime_probe)printf("Native task storage: %u canary bytes checked across intro cache conversion, handoff and walking; %zu inactive task/control/continuation/command/boot-global/high-score/sound bytes preserved; eight live scroll bytes checked against original traces\n",lifetimeChecks,(size_t)(88+512+256+sizeof(WillowNativeBootGlobals)+85+128-10-15));
 if(argc==5 && !strcmp(argv[4],"--palette-owner-probe")){
  void(*set_reg)(int,unsigned)=dlsym(core,"m68k_set_reg");if(!set_reg||!cpu_reg)return 2;
  unsigned dirty=symbol(argv[3],"playerColorsDirty"),overflow=symbol(argv[3],"debugIntroOverflow");
  /* Seed two exact mutable fixtures from the already-verified live colors.
   * The ordinary trace above independently audits ROM-backed ownership. */
  for(unsigned group=0;group<2;group++){
   unsigned at=paletteOwnerAt+group*2,tag=0xfff1+group;
   probe_ram[at^1]=tag>>8;probe_ram[(at+1)^1]=tag;
   unsigned slot=paletteSlotsAt+group*34;
   probe_ram[slot^1]=0;probe_ram[(slot+1)^1]=group;
   for(unsigned n=0;n<16;n++){
    unsigned value=nativeColors[group*16+n];
    probe_ram[(slot+2+n*2)^1]=value>>8;probe_ram[(slot+3+n*2)^1]=value;
   }
  }
  probe_ram[(paletteOwnerAt+384)^1]=0;
  unsigned calls=0;
  for(unsigned index=0;index<34;index++)for(unsigned op=0;op<2;op++){
   unsigned value=(index*0x731+0xabc9)&65535,result=0;
   if(index<32){
    unsigned at=paletteSlotsAt+(index/16)*34+2+(index%16)*2;
    result=(unsigned short)word(probe_ram,at);
   }
   unsigned char expected[0xf600];memcpy(expected,probe_ram,sizeof expected);
#define OWNER_WORD(at,value) do{unsigned v=(value);expected[(at)^1]=v>>8;expected[((at)+1)^1]=v;}while(0)
   if(index>=32){OWNER_WORD(overflow,(unsigned short)word(probe_ram,overflow)+1);}
   else if(op){
    unsigned at=paletteSlotsAt+(index/16)*34+2+(index%16)*2;
    OWNER_WORD(at,value);
    if(index<16)expected[dirty^1]=1;
   }
#undef OWNER_WORD
   probe_ram[0xfe00^1]=0x60;probe_ram[0xfe01^1]=0xfe;
   const unsigned stack[]={0,255,254,0,0,0,0,index,0,0,value>>8,value&255};
   for(unsigned j=0;j<sizeof stack/sizeof stack[0];j++)probe_ram[(0xff00+j)^1]=stack[j];
   set_reg(17,0x2700);set_reg(15,0xffff00);set_reg(16,fullSymbol(argv[3],op?"normalPaletteWrite":"normalPaletteRead"));retro_run();
   if(cpu_reg(16)!=0xfffe00 || (!op&&(cpu_reg(0)&65535)!=result) || memcmp(expected,probe_ram,sizeof expected)){
    fprintf(stderr,"Native palette-owner callback mismatch index %u operation %u\n",index,op);SMOKE_FAIL();
   }
   calls++;
  }
  unsigned copies=0;
  for(unsigned group=0;group<2;group++)for(unsigned kind=0;kind<3;kind++)for(unsigned mixed=0;mixed<2;mixed++){
#define COPY_WORD(r,at,value) do{unsigned v=(value);(r)[(at)^1]=v>>8;(r)[((at)+1)^1]=v;}while(0)
   for(unsigned g=0;g<2;g++){
    COPY_WORD(probe_ram,paletteOwnerAt+g*2,0xfff1+g);
    COPY_WORD(probe_ram,paletteSlotsAt+g*34,g);
    for(unsigned color=0;color<16;color++){
     unsigned value=((mixed?(color+g)&15:7)<<12)|((color*197+g*37)&4095);
     COPY_WORD(probe_ram,paletteSlotsAt+g*34+2+color*2,value);
    }
   }
   probe_ram[(paletteOwnerAt+384)^1]=0;probe_ram[dirty^1]=0;
   unsigned char expected[0xf600];memcpy(expected,probe_ram,sizeof expected);
   unsigned source=0xe5000+kind*64+group*32,slot=paletteSlotsAt+group*34;
   if(!mixed){
    COPY_WORD(expected,paletteOwnerAt+group*2,(source-0xe0000)/2);
    expected[(paletteOwnerAt+384)^1]=7<<(group*4);COPY_WORD(expected,slot,65535);
   }
   for(unsigned color=0;color<16;color++){
    unsigned at=source-0xe5000+color*2;
    unsigned value=(originalColors[at]*256+originalColors[at+1])|((unsigned short)word(probe_ram,slot+2+color*2)&0xf000);
    if(mixed){COPY_WORD(expected,slot+2+color*2,value);}
   }
   if(!group)expected[dirty^1]=1;
   const unsigned stack[]={0,255,254,0,0,0,0,group*16,0,source>>16,(source>>8)&255,source&255};
   for(unsigned j=0;j<sizeof stack/sizeof stack[0];j++)probe_ram[(0xff00+j)^1]=stack[j];
   set_reg(17,0x2700);set_reg(15,0xffff00);set_reg(16,fullSymbol(argv[3],"normalPaletteCopy"));retro_run();
   if(cpu_reg(16)!=0xfffe00 || memcmp(expected,probe_ram,sizeof expected)){
    fprintf(stderr,"Native C240 palette copy mismatch group %u kind %u mixed %u\n",group,kind,mixed);SMOKE_FAIL();
   }
   copies++;
#undef COPY_WORD
  }
  printf("Native palette copy: %u compiled full-page copies preserve every brightness nibble; exact ROM rebinding/slot release or mixed mutable retention and unrelated RAM verified\n",copies);
  printf("Native palette owner: %u compiled callbacks; authoritative health reads, exact owner writes and invalid-index atomicity verified\n",calls);
 }
 if(argc==5 && !strcmp(argv[4],"--sat-cache-probe")){
  void(*set_reg)(int,unsigned)=dlsym(core,"m68k_set_reg");if(!set_reg||!cpu_reg)return 2;
  unsigned sat=symbol(argv[3],"willowSAT"),overflow=symbol(argv[3],"debugIntroOverflow");
  /* kind 0 full, 1 link, 2 clear, 3 invalid upload. Include last valid
   * entry, one-past-end, and all seven link bits without using SDK setters. */
  const unsigned cases[][7]={
   {0,0,65535,32767,15,0xabcd,127},{0,79,32768,0,6,0x1234,0},
   {0,80,1,2,3,4,5},{0,79,1,2,3,4,128},
   {1,79,0,0,0,0,80},{1,80,0,0,0,0,0},{1,79,0,0,0,0,128},
   {2,0,0,0,0,0,0},{3,81,0,0,0,0,0}
  };
  for(unsigned n=0;n<sizeof cases/sizeof cases[0];n++){
   const unsigned*c=cases[n];unsigned char expected[0xf600];memcpy(expected,probe_ram,sizeof expected);
   unsigned args[6]={c[1],c[2],c[3],c[4],c[5],c[6]},address=sat+c[1]*8;
   const char*function=c[0]==0?"willowSATSetFull":c[0]==1?"willowSATSetLink":c[0]==2?"willowSATClear":"willowSATUpload";
   int reject=c[0]==3||((c[0]<2)&&(c[1]>=80||c[6]>127));
#define SAT_WORD(at,value) do{unsigned v=(value);expected[(at)^1]=v>>8;expected[((at)+1)^1]=v;}while(0)
   if(reject){SAT_WORD(overflow,(unsigned short)word(probe_ram,overflow)+1);}
   else if(c[0]==0){
    SAT_WORD(address,c[3]+128);expected[(address+2)^1]=c[4];expected[(address+3)^1]=c[6];
    SAT_WORD(address+4,c[5]);SAT_WORD(address+6,c[2]+128);
   }else if(c[0]==1)expected[(address+3)^1]=c[6];
   else {SAT_WORD(sat,0);expected[(sat+3)^1]=0;}
#undef SAT_WORD
   if(c[0]==1)args[1]=c[6];
   probe_ram[0xfe00^1]=0x60;probe_ram[0xfe01^1]=0xfe;
   const unsigned ret[]={0,255,254,0};for(unsigned j=0;j<4;j++)probe_ram[(0xff00+j)^1]=ret[j];
   for(unsigned j=0;j<6;j++)for(unsigned b=0;b<4;b++)probe_ram[(0xff04+j*4+b)^1]=args[j]>>(24-b*8);
   set_reg(17,0x2700);set_reg(15,0xffff00);set_reg(16,fullSymbol(argv[3],function));retro_run();
   if(cpu_reg(16)!=0xfffe00||memcmp(expected,probe_ram,sizeof expected)){fprintf(stderr,"Native SAT guard/layout mismatch case %u\n",n);SMOKE_FAIL();}
  }
  puts("Native SAT: exact first/last entry layouts, signed coordinate wrapping, seven-bit links, clear and rejected out-of-range writes/uploads preserve unrelated RAM");
 }
 if(argc==5 && !strcmp(argv[4],"--runtime-memory-probe")){
  void(*set_reg)(int,unsigned)=dlsym(core,"m68k_set_reg");if(!set_reg||!cpu_reg)return 2;
  unsigned char before[65536],mutable[65536]={0};memcpy(before,probe_ram,sizeof before);
  unsigned heapAt=longword(probe_ram,symbol(argv[3],"heap"))&65535,totalBefore=0;
  for(unsigned at=heapAt;at<65534;){
   unsigned block=(unsigned short)word(probe_ram,at),size=block&~1u;if(!size)break;
   if(!(block&1)){printf("Available block %04x: %u bytes\n",at,size);totalBefore+=size;for(unsigned n=0;n<size;n++)mutable[at+n]=1;}
   at+=size;
  }
  unsigned freeAt=symbol(argv[3],"free");for(unsigned n=0;n<4;n++)mutable[freeAt+n]=1;
  /* Use the reserved CPU stack area, never live actor/heap memory, for the
   * return sentinel. This is a terminal allocator probe; gameplay is not resumed. */
  probe_ram[0xff80^1]=0x60;probe_ram[0xff81^1]=0xfe;
  /* Task records, primary queue, palette owner and two mutable slots are
   * now persistent. Probe descriptors and the boot continuation. */
  const unsigned sizes[]={28,8};
  enum{BUFFER_COUNT=sizeof sizes/sizeof sizes[0]};unsigned pointers[BUFFER_COUNT];
#define ALLOCATOR_CALL(entry,arg) do { \
   unsigned bytes[]={0,255,255,128,(arg)>>24,((arg)>>16)&255,((arg)>>8)&255,(arg)&255}; \
   for(unsigned j=0;j<8;j++)probe_ram[(0xff00+j)^1]=bytes[j]; \
   set_reg(17,0x2700);set_reg(15,0xffff00);set_reg(16,fullSymbol(argv[3],entry)); \
   retro_run();if(cpu_reg(16)!=0xffff80)SMOKE_FAIL(); \
  }while(0)
  for(unsigned n=0;n<BUFFER_COUNT;n++){
   ALLOCATOR_CALL("MEM_alloc",sizes[n]);pointers[n]=cpu_reg(0);unsigned at=pointers[n]&65535;
   if(!pointers[n]||at+sizes[n]>0xf600)SMOKE_FAIL();
   for(unsigned j=0;j<sizes[n];j++){
    if(!mutable[at+j])SMOKE_FAIL();probe_ram[(at+j)^1]=(n*23+j)&255;
   }
   printf("Runtime buffer %u: %u bytes at %04x\n",n,sizes[n],at);
  }
  for(unsigned n=0;n<BUFFER_COUNT;n++)for(unsigned j=0;j<sizes[n];j++)if(probe_ram[((pointers[n]&65535)+j)^1]!=((n*23+j)&255))SMOKE_FAIL();
  ALLOCATOR_CALL("MEM_getFree",0);unsigned remaining=cpu_reg(0)&65535;
  for(unsigned n=BUFFER_COUNT;n;n--){ALLOCATOR_CALL("MEM_free",pointers[n-1]);}
  ALLOCATOR_CALL("MEM_getFree",0);if((cpu_reg(0)&65535)!=totalBefore)SMOKE_FAIL();
  for(unsigned n=0;n<0xf600;n++)if(!mutable[n]&&probe_ram[n^1]!=before[n^1]){fprintf(stderr,"Allocator probe changed live RAM %04x\n",n);SMOKE_FAIL();}
  printf("Native runtime allocation: %u buffers fit, %u bytes remain; contents, live RAM and %u-byte heap restoration verified\n",BUFFER_COUNT,remaining,totalBefore);
#undef ALLOCATOR_CALL
 }
 return checks==endTick && stalls==0?0:1;
}
