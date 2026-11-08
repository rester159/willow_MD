#ifndef WILLOW_NATIVE_FRAME_PROFILE_H
#define WILLOW_NATIVE_FRAME_PROFILE_H
#include <stdio.h>
#include <string.h>
/* Optional diagnostic storage is absent from ordinary cartridges. Never
 * reinterpret unrelated RAM as timings when the symbols are unavailable. */
static unsigned profileSymbol(const char*path,const char*name)
{
 FILE*f=fopen(path,"r");if(!f)return 0x10000;
 char line[512],kind,label[256];unsigned at;
 while(fgets(line,sizeof line,f))if(sscanf(line,"%x %c %255s",&at,&kind,label)==3&&!strcmp(label,name)){
  fclose(f);return at&65535;
 }
 fclose(f);return 0x10000;
}
static unsigned profileWord(const unsigned char*ram,unsigned base,unsigned offset)
{
 if(base==0x10000)return 65535; /* Unavailable, not a measured scanline. */
 return ((unsigned)ram[(base+offset)^1]<<8)|ram[(base+offset+1)^1];
}
#endif
