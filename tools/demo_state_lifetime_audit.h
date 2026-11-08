/* Independent source-address inventory; do not use the native resolver to
 * prove which source bytes survive these original-instruction boundaries. */
static const unsigned demoLifetimeSpans[][2]={
 {0x8289,5},{0x82c2,129},{0x8346,125},{0x83c4,7},{0x83cc,54},{0xf9a2,1}
};
static void demoLifetimeCapture(uint8_t saved[321],const uint8_t source[65536])
{
 unsigned index=0;
 for(unsigned span=0;span<6;span++)for(unsigned n=0;n<demoLifetimeSpans[span][1];n++)
  saved[index++]=source[demoLifetimeSpans[span][0]+n];
}
static int demoLifetimePreserved(const uint8_t saved[321],const uint8_t ram[65536])
{
 unsigned index=0;
 for(unsigned span=0;span<6;span++)for(unsigned n=0;n<demoLifetimeSpans[span][1];n++){
  unsigned at=demoLifetimeSpans[span][0]+n;
  if(ram[at^1]!=saved[index++]){
   fprintf(stderr,"Original CPU changed supplemental demo state at %04x\n",at);return 0;
  }
 }
 return index==321;
}
