#include "../src/willow_native_controller.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
 for(unsigned buttons=0;buttons<65536;buttons++){
  unsigned expected=((buttons&8)?1:0)|((buttons&4)?2:0)|
   ((buttons&2)?4:0)|((buttons&1)?8:0)|((buttons&64)?16:0)|((buttons&16)?32:0);
  assert(willowNativeControllerInput(buttons)==expected);
 }
 puts("Native controller: all 65536 button combinations retain the original host mapping, including ignored buttons");
}
