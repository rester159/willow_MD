#!/usr/bin/env python3
"""Read-only interrupted-pointer-store diagnostic; preserves the strict abort."""
from pathlib import Path
import test_native_background_far_publication as base

original_bind = base.bind


def bind(platform, target, text, out, manifest):
    text = original_bind(platform, target, text, out, manifest)
    header = out / 'sprite-observer-with-background-hooks.h'
    source = header.read_text()
    needle = ' unsigned sp=cpu->dar[15]&0xffffff,pc=sourcePC();'
    extra = '''
 if(frameNumber>=1490&&sp>=0xfff000&&ownerPC>=0x100000){
  fprintf(stderr,"IRQ-STORE frame%u path%u owner%x PC%x A7%x A6%x at%x value%x SR%x stage%u\\n",
   frameNumber,renderPath,ownerPC,pc,cpu->dar[15],cpu->dar[14],at,value,
   renderGetRegister?renderGetRegister(17):0xffff, p->exceptionStage);
  if(ownerPC==0x106716){
   unsigned base=(cpu->dar[14]-84)&65535,off=ownerPC-0x100000;
   fprintf(stderr,"IRQ-PRIOR base%x raw%x original%x native%x ops%x/%x disp%x/%x\\n",base,rawLong(base),
    willowMotionLong(platforms[0]+off+2),willowMotionLong(platforms[1]+off+2),
    willowMotionWord(platforms[0]+off),willowMotionWord(platforms[1]+off),
    willowMotionWord(platforms[0]+off+6),willowMotionWord(platforms[1]+off+6));
   for(unsigned k=0;k<4;k++)fprintf(stderr,"IRQ-BYTE %u owner%x value%x index%u register%u\\n",k,owner[base+k],savedValue[base+k],savedByteIndex[base+k],savedRegister[base+k]);
  }
 }
'''
    source = base.replace(source, needle, needle + extra)
    header.write_text(source)
    manifest['inputs'][str(header.resolve())] = base.sha(header)
    own = out / 'frozen-observer-tools' / Path(__file__).name
    own.write_bytes(Path(__file__).read_bytes())
    manifest['inputs'][str(own.resolve())] = base.sha(own)
    manifest['irq_pointer_diagnostic'] = 'Log only; exact observer refusal remains.'
    return text


if __name__ == '__main__':
    base.bind = bind
    base.main()
