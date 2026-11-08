#!/usr/bin/env python3
"""Execute old/current visibility predicates on a real68000 core."""
from pathlib import Path
import argparse,hashlib,json,re,shutil,struct,subprocess
from verify_native_far_row_proof import predicates
ROOT=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,default=ROOT/'build/native-far-row-proof/native-v1');p.add_argument('--host-proof',type=Path,default=ROOT/'build/native-far-row-proof/host-v1');a=p.parse_args();out=a.output.resolve();assert not out.exists();out.mkdir(parents=True)
 hp=a.host_proof.resolve();report=json.loads((hp/'evidence.json').read_text());assert report['status']=='pass'
 source=hp/'baseline-source.inc' if (hp/'baseline-source.inc').exists() else hp/'inputs/tools/native_render_background.inc';candidate=hp/'candidate-source.inc';paths=[source,Path(__file__).resolve(),ROOT/'tools/verify_native_far_row_proof.py',ROOT/'tools/prototype_native_far_row_proof.h',ROOT/'tools/test_native_far_row_proof_native.c',ROOT/'tools/test_native_render_cache_closed.c',ROOT/'tools/vendor/libretro.h',ROOT/'src/willow_native_background_key_cache.h',hp/'evidence.json']
 if candidate.exists():paths.append(candidate)
 core=Path.home()/'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib';fixtures=[ROOT/f'build/native-visible-opacity/edges-v1/source-masks-view{n}.bin'for n in(120,130,132)];paths+=fixtures
 assert sha(ROOT/'tools/prototype_native_far_row_proof.h')==report['inputs'][str(ROOT/'tools/prototype_native_far_row_proof.h')]
 inputs={str(p):sha(p)for p in paths+[core]}
 for q in paths:
  dest=out/'inputs'/q.relative_to(ROOT);dest.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(q,dest)
 funcs=predicates(source.read_text(),candidate.read_text() if candidate.exists() else None).replace('opacityCalls++;','').replace('fullCalls++;','');(out/'predicates.inc').write_text(funcs)
 fixture='''#include <stdint.h>
#include <stddef.h>
#include "willow_native_background_key_cache.h"
#define BG_INVALID 65535U
#define WILLOW_BG_CATALOG_LAYER2_PAIR_COUNT 554
#define WILLOW_BG_CATALOG_LAYER3_PAIR_COUNT 248
#define WILLOW_BG_CATALOG_FULL_OPAQUE 1
#define WILLOW_BG_CATALOG_EMPTY 2
#define WILLOW_BG_CATALOG_LAYER2_FLAGS_ADDRESS 0x200008
#define WILLOW_BG_CATALOG_LAYER3_FLAGS_ADDRESS 0x200530
#define WILLOW_BG_CATALOG_LAYER2_OPACITY_ADDRESS 0x200b30
#define WILLOW_BG_CATALOG_LAYER3_OPACITY_ADDRESS 0x203470
typedef struct{uint8_t x,y,hx,hy,w,h,scale,valid;}BackgroundRing;
typedef struct{uint16_t view[4];BackgroundRing rings[2];WillowNativeBackgroundKeyCache keys;uint8_t rowCounts[2][32];}Background;
#define bg (*(Background*)0xff4000)
#define IO ((volatile uint16_t*)0x20f800)
#include "predicates.inc"
const uint32_t rowABI[]={sizeof(Background),sizeof(BackgroundNearRowProof)};
void nativeRowInitialize(void){
 for(uint16_t n=0;n<4;n++)bg.view[n]=((const uint16_t*)0x200000)[n];
 for(uint16_t l=0;l<2;l++){
  BackgroundRing*r=bg.rings+l;r->scale=l?2:1;r->x=bgFirstX(l)>>r->scale;r->y=bgFirstY(l)>>r->scale;
  r->hx=r->hy=0;r->w=l?12:22;r->h=l?8:15;r->valid=1;
 }
 for(uint16_t n=0;n<330;n++)willowNativeBackgroundNearKeySet(&bg.keys,n,n);
 for(uint16_t n=0;n<96;n++)bg.keys.far[n]=n;
 for(uint16_t l=0;l<2;l++){
  for(uint16_t n=0;n<32;n++)bg.rowCounts[l][n]=0;
  const uint8_t*flags=(const uint8_t*)(l?WILLOW_BG_CATALOG_LAYER3_FLAGS_ADDRESS:WILLOW_BG_CATALOG_LAYER2_FLAGS_ADDRESS);
  for(uint16_t row=0;row<bgRows(l);row++){
   uint16_t y=(bgFirstY(l)+row)&(l?255:127),count=0;
   for(uint16_t col=0;col<43;col++){uint16_t ref=bgRef(l,(bgFirstX(l)+col)&(l?255:127),y);count+=flags[ref]!=(l?2:1);}
   bg.rowCounts[l][y&31]=count;
  }
 }
}
void nativeOldRow(void){
 uint16_t row=IO[0],x=bgFirstX(1),y=(bgFirstY(1)+row)&255;uint32_t lo=0,hi=0;
 if(!bgFarRowBlank(row))for(uint16_t col=0;col<43;col++){
  uint16_t worldX=(x+col)&255,ref=bgRef(1,worldX,y);
  if(bgFarNeeded(ref,worldX,y)){if(col<32)lo|=1UL<<col;else hi|=1UL<<(col-32);}
 }
 *(volatile uint32_t*)(IO+2)=lo;*(volatile uint32_t*)(IO+4)=hi;
}
void nativeProposedRow(void){
 uint16_t row=IO[0],x=bgFirstX(1),y=(bgFirstY(1)+row)&255;uint32_t lo=0,hi=0;
 BackgroundNearRowProof proof;rowProofInit(&proof);
 if(!proposedFarRowBlank(row,&proof))for(uint16_t col=0;col<43;col++){
  uint16_t worldX=(x+col)&255,ref=bgRef(1,worldX,y);
  if(proposedFarNeeded(ref,worldX,y,&proof)){if(col<32)lo|=1UL<<col;else hi|=1UL<<(col-32);}
 }
 *(volatile uint32_t*)(IO+2)=lo;*(volatile uint32_t*)(IO+4)=hi;
}
''';(out/'fixture.c').write_text(fixture);cmds=[]
 def run(cmd,log=None):
  cmds.append(cmd)
  if log:
   with (out/log).open('w')as f:subprocess.run(cmd,cwd=ROOT,stdout=f,stderr=subprocess.STDOUT,check=True)
  else:subprocess.run(cmd,cwd=ROOT,check=True)
 run(['m68k-elf-gcc','-m68000','-std=c99','-O2','-Wall','-Wextra','-Werror','-Wno-unused-function','-ffreestanding','-fno-builtin','-fstack-usage','-nostdlib','-I'+str(out/'inputs/src'),'-I'+str(out),'-Wl,-Ttext=0x102000,-e,nativeRowInitialize',str(out/'fixture.c'),'-lgcc','-o',str(out/'fixture.elf')])
 run(['m68k-elf-objcopy','-O','binary',str(out/'fixture.elf'),str(out/'fixture.bin')]);sym=subprocess.check_output(['m68k-elf-nm','-n',str(out/'fixture.elf')],text=True);(out/'fixture.sym').write_text(sym)
 def addr(n):return int(re.search(r'^([0-9a-f]+) [A-Za-z] '+n+'$',sym,re.M)[1],16)
 abi=struct.unpack_from('>2I',(out/'fixture.bin').read_bytes(),addr('rowABI')-0x102000);assert abi[1]==8
 (out/'row-fixture.h').write_text(f'#define ROW_STATE_BYTES {abi[0]}\n#define ROW_INIT {addr("nativeRowInitialize"):#x}\n#define ROW_OLD {addr("nativeOldRow"):#x}\n#define ROW_NEW {addr("nativeProposedRow"):#x}\n')
 run(['m68k-elf-objdump','-d',str(out/'fixture.elf')],'fixture.disassembly')
 run(['clang','-std=c99','-O2','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(out),str(out/'inputs/tools/test_native_far_row_proof_native.c'),'-o',str(out/'test')])
 run([str(out/'test'),str(core),str(out/'fixture.bin'),str(out/'timings.csv'),*[str(q)for q in fixtures]],'result.json')
 result=json.loads((out/'result.json').read_text());e={'scope':__doc__,'status':'pass','runtime_admitted':False,'actual_68000':True,'context_bytes':abi[1],'result':result,'inputs':inputs,'commands':cmds,'limitations':['Closed predicate rows with source-derived mask fixtures and synthetic alignment cases; no live source/IRQ/VDP/FPS claim.','Diagnostic counter increments were removed from both native variants before compiling and timing.','The stack-local proof remains valid only during one serialized far-row call with unchanged prepared near dependencies.'],'outputs':{str(q.relative_to(out)):sha(q)for q in out.rglob('*')if q.is_file()}};(out/'evidence.json').write_text(json.dumps(e,indent=2)+'\n');print(json.dumps(result,indent=2))
if __name__=='__main__':main()
