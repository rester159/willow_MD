/* Host-only symbol memoization. Keep the original text lookup on cache misses;
 * reject changed symbol files during a run instead of calling stale addresses.
 * This changes test overhead only, never emulated CPU accounting or game code. */
#include <sys/stat.h>
static unsigned fullSymbol(const char *path,const char *name)
{
 static char cachedPath[4096];
 static struct stat identity;
 static struct {char name[256];unsigned address;} entries[256];
 static unsigned count;
 struct stat current;
 if(stat(path,&current)){perror(path);exit(2);}
 if(strcmp(path,cachedPath)){
  if(strlen(path)>=sizeof cachedPath){fprintf(stderr,"Symbol path too long\n");exit(2);}
  strcpy(cachedPath,path);identity=current;count=0;
 }else{
#ifdef __APPLE__
  int timeChanged=current.st_mtimespec.tv_sec!=identity.st_mtimespec.tv_sec||current.st_mtimespec.tv_nsec!=identity.st_mtimespec.tv_nsec;
#else
  int timeChanged=current.st_mtim.tv_sec!=identity.st_mtim.tv_sec||current.st_mtim.tv_nsec!=identity.st_mtim.tv_nsec;
#endif
  if(current.st_dev!=identity.st_dev||current.st_ino!=identity.st_ino||current.st_size!=identity.st_size||timeChanged){
   fprintf(stderr,"Symbol file changed during native test: %s\n",path);exit(2);
  }
 }
 for(unsigned n=0;n<count;n++)if(!strcmp(entries[n].name,name))return entries[n].address;
 FILE*f=fopen(path,"r");char line[512],sym[256],type;unsigned address;
 if(!f){perror(path);exit(2);}
 while(fgets(line,sizeof line,f))if(sscanf(line,"%x %c %255s",&address,&type,sym)==3&&!strcmp(sym,name)){
  fclose(f);
  if(count==sizeof entries/sizeof entries[0])count=0;
  strcpy(entries[count].name,sym);entries[count++].address=address;return address;
 }
 fclose(f);fprintf(stderr,"missing symbol: %s\n",name);exit(2);
}
