"""Exact declared pointer-store boundary or direct join, then hardware IRQ."""


def extend(header, replace, auxiliary_movem=None, additional_movem=None, lookup_movem=None, finish_movem=None):
    source = header.read_text()
    extra = '''
 /* A pointer-store boundary can also be the target of the alternate bank's
  * direct branch. Require declared opcodes/relocation, actual A6 EA, and all
  * four already-observed source bytes; a join additionally pins its BRA.W.
  * The caller separately
  * proves current supervisor A7, level6, ordered PC/SR and real IRQ entry. */
 if(pc==ownerPC+8&&ownerPC>=0x100000&&ownerPC+8<=0x110000&&renderAllowedInterruptPC(ownerPC)){
  for(unsigned n=0;n<RENDER_POINTER_STORE_COUNT;n++){
   const RenderPointerStoreSite*s=renderPointerStoreSites+n;
   if(s->pc!=ownerPC||s->size!=8||s->kind!=1||s->reg!=16||s->publishing==1)continue;
   unsigned offset=ownerPC-0x100000;
   const uint8_t*a=platforms[0]+offset,*b=platforms[1]+offset;
   if(willowMotionWord(a)!=0x2d7c||willowMotionWord(b)!=0x2d7c||
      (int16_t)willowMotionWord(a+6)!=s->displacement||
      willowMotionWord(a+6)!=willowMotionWord(b+6))return 0;
   unsigned original=willowMotionLong(a+2),native=willowMotionLong(b+2);
   if(!samePointer(original,native))return 0;
   unsigned ea=(cpu->dar[14]+s->displacement)&0xffffff;
   if(ea<0xff7000||ea+4>0xff7600||(ea&1))return 0;
   unsigned base=ea&65535,prior=owner[base];
   if(prior!=ownerPC){
    const RenderPointerStoreSite*t=0;
    for(unsigned j=0;j<RENDER_POINTER_STORE_COUNT;j++)if(renderPointerStoreSites[j].pc==prior)t=renderPointerStoreSites+j;
    if(!t||t->size!=8||t->kind!=1||t->reg!=16||t->publishing==1||
       t->displacement!=s->displacement||prior<0x100000||prior+12>0x110000||!renderAllowedInterruptPC(prior))return 0;
    a=platforms[0]+(prior-0x100000);b=platforms[1]+(prior-0x100000);
    if(willowMotionWord(a)!=0x2d7c||willowMotionWord(b)!=0x2d7c||
       (int16_t)willowMotionWord(a+6)!=t->displacement||willowMotionWord(a+6)!=willowMotionWord(b+6)||
       willowMotionWord(a+8)!=0x6000||memcmp(a+8,b+8,4)||
       prior+10+(int16_t)willowMotionWord(a+10)!=pc)return 0;
    original=willowMotionLong(a+2);native=willowMotionLong(b+2);
    if(!samePointer(original,native))return 0;
   }
   if(rawLong(base)!=original)return 0;
   for(unsigned k=0;k<4;k++)if(owner[base+k]!=prior||
      savedValue[base+k]!=original||savedByteIndex[base+k]!=k||savedRegister[base+k]!=16)return 0;
   return 1;
  }
 }
'''
    extra += '''
 /* Completed declared MOVE.L d16(A6),d16(A6). Check both exact opcodes,
  * actual bounded EAs and the already-observed four-byte copy provenance.
  * The hardware PC/SR stores are still checked by renderExceptionStore. */
 if(pc==ownerPC+6&&ownerPC>=0x100000&&ownerPC+6<=0x110000&&renderAllowedInterruptPC(ownerPC)){
  for(unsigned n=0;n<RENDER_POINTER_STORE_COUNT;n++){
   const RenderPointerStoreSite*s=renderPointerStoreSites+n;
   if(s->pc!=ownerPC||s->size!=6||s->kind!=2||s->reg!=17||s->publishing!=0)continue;
   const uint8_t*a=platforms[0]+ownerPC-0x100000,*b=platforms[1]+ownerPC-0x100000;
   if(willowMotionWord(a)!=0x2d6e||memcmp(a,b,6)||
      (int16_t)willowMotionWord(a+2)!=s->sourceDisplacement||
      (int16_t)willowMotionWord(a+4)!=s->displacement)return 0;
   unsigned src=(cpu->dar[14]+s->sourceDisplacement)&0xffffff;
   unsigned dst=(cpu->dar[14]+s->displacement)&0xffffff;
   if(src<0xff7000||src+4>0xff7600||dst<0xff7000||dst+4>0xff7600||(src&1)||(dst&1))return 0;
   src&=65535;dst&=65535;
   unsigned value=rawLong(src),prior=owner[src];
   if(!prior||rawLong(dst)!=value)return 0;
   for(unsigned k=0;k<4;k++)if(owner[src+k]!=prior||savedValue[src+k]!=value||savedByteIndex[src+k]!=k||
      owner[dst+k]!=ownerPC||savedValue[dst+k]!=value||savedByteIndex[dst+k]!=k||savedRegister[dst+k]!=17)return 0;
   return 1;
  }
 }
'''
    if auxiliary_movem:
        address, opcode = auxiliary_movem
        extra += f'''
#define FAR_IRQ_AUX_MOVEM_PC 0x{address:x}u
#define FAR_IRQ_AUX_MOVEM_WORD 0x{opcode:x}u
 /* This exact auxiliary function entry is common immutable ROM in both
  * paths. Keep the same completed MOVEM/hardware-frame proof as main ROM. */
 if(ownerPC==0x{address:x}u&&pc==0x{address + 4:x}u&&
    willowMotionLong(rom+ownerPC)==0x{opcode:x}u&&
    willowMotionLong(renderExtraROM+(ownerPC-RENDER_EXTRA_ROM_ORIGIN))==0x{opcode:x}u)return 1;
'''
    if additional_movem:
        address, opcode, prefix = additional_movem
        extra += f'''
#define FAR_IRQ_RESERVED_MOVEM_PC 0x{address:x}u
#define FAR_IRQ_RESERVED_MOVEM_WORD 0x{opcode:x}u
#define FAR_IRQ_RESERVED_MOVEM_PREFIX 0x{prefix:x}u
 /* Exact LEA stack reservation followed by completed MOVEM in immutable
  * shared auxiliary ROM. The hardware-frame/provenance checks still apply. */
 if(ownerPC==0x{address:x}u&&pc==0x{address+4:x}u&&
    willowMotionLong(rom+ownerPC-4)==0x{prefix:x}u&&
    willowMotionLong(renderExtraROM+(ownerPC-4-RENDER_EXTRA_ROM_ORIGIN))==0x{prefix:x}u&&
    willowMotionLong(rom+ownerPC)==0x{opcode:x}u&&
    willowMotionLong(renderExtraROM+(ownerPC-RENDER_EXTRA_ROM_ORIGIN))==0x{opcode:x}u)return 1;
'''
    if lookup_movem:
        address, opcode = lookup_movem
        extra += f'''
#define FAR_IRQ_LOOKUP_MOVEM_PC 0x{address:x}u
#define FAR_IRQ_LOOKUP_MOVEM_WORD 0x{opcode:x}u
 if(ownerPC==0x{address:x}u&&pc==0x{address+4:x}u&&
    willowMotionLong(rom+ownerPC)==0x{opcode:x}u&&
    willowMotionLong(renderExtraROM+(ownerPC-RENDER_EXTRA_ROM_ORIGIN))==0x{opcode:x}u)return 1;
'''
    if finish_movem:
        address, opcode, prefix = finish_movem
        extra += f'''
#define FAR_IRQ_FINISH_MOVEM_PC 0x{address:x}u
#define FAR_IRQ_FINISH_MOVEM_WORD 0x{opcode:x}u
#define FAR_IRQ_FINISH_MOVEM_PREFIX 0x{prefix:x}u
 if(ownerPC==0x{address:x}u&&pc==0x{address+4:x}u&&
    willowMotionLong(rom+ownerPC-4)==0x{prefix:x}u&&
    willowMotionLong(renderExtraROM+(ownerPC-4-RENDER_EXTRA_ROM_ORIGIN))==0x{prefix:x}u&&
    willowMotionLong(rom+ownerPC)==0x{opcode:x}u&&
    willowMotionLong(renderExtraROM+(ownerPC-RENDER_EXTRA_ROM_ORIGIN))==0x{opcode:x}u)return 1;
'''
    needle = 'static unsigned backgroundInterruptedMovem(unsigned ownerPC,unsigned pc){'
    header.write_text(replace(source, needle, needle + extra))
