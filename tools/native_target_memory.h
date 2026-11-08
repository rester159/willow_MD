#ifndef WILLOW_HOST_TARGET_MEMORY_H
#define WILLOW_HOST_TARGET_MEMORY_H
/* Host-only memory view. Preserve full target pointers: work RAM is byte
 * swapped in this core, while the guarded 64 KiB SRAM fixture is not.
 * Low offsets are accepted for existing symbol()-based work-RAM fixtures. */
static uint8_t *nativeTargetByte(uint8_t *ram,uint8_t *sram,uint32_t address)
{
 /* The 68000 exposes a 24-bit bus; SGDK symbols can carry E0 in byte 3. */
 address&=0x00ffffff;
 if(address<=0xffff)return ram+(address^1);
 if(address>=0xff0000 && address<=0xffffff)return ram+((address&65535)^1);
 if(sram && address>=0x200000 && address<=0x20ffff)return sram+(address-0x200000);
 fprintf(stderr,"Invalid native target memory address %08x\n",address);abort();
}
static uint16_t nativeTargetWord(uint8_t *ram,uint8_t *sram,uint32_t address)
{return ((uint16_t)*nativeTargetByte(ram,sram,address)<<8)|*nativeTargetByte(ram,sram,address+1);}
static void nativeTargetPutWord(uint8_t *ram,uint8_t *sram,uint32_t address,uint16_t value)
{*nativeTargetByte(ram,sram,address)=value>>8;*nativeTargetByte(ram,sram,address+1)=value;}
#endif
