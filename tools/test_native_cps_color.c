#include "../src/willow_native_cps_color.h"
#include <stdio.h>
int main(void)
{
 for(uint32_t word=0;word<65536;word++){
  uint16_t color=willowNativeCPSColor(word);
  if(putchar(color>>8)==EOF||putchar(color&255)==EOF)return 1;
 }
 return ferror(stdout)?1:0;
}
