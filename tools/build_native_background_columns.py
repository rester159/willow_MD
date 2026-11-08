#!/usr/bin/env python3
"""Build checked native source-column hooks; caller chooses state/code owners."""
from pathlib import Path
import argparse,hashlib,json,struct,subprocess
ROOT=Path(__file__).resolve().parents[1]
SOURCE_SHA='0b66efe85aec1e94fcaf722de9401cc7e4ec09647daf5f9f0232116861240cc1'
LEAVES=[(0x437aa,'willowBackgroundColumnsScroll2Column','30990658000030d9'),(0x4386c,'willowBackgroundColumnsScroll2Row','3099065800003091'),(0x43df6,'willowBackgroundColumnsScroll3Column','309906580a0030d9'),(0x43ea0,'willowBackgroundColumnsScroll3Row','309906580a003091')]
LEAF_PATCH_OPCODE=0x4ef9
LOOP_TAILS=['51c8fff64e75','41e8003e43e9003e51c8ffee4e75','51c8fff64e75','41e8001e43e9001e51c8ffee4e75']
BULKS=[(0x54c8,'willowBackgroundColumnsBulkCopy','3099d35830d93099d35830d93099d35830d93099d35830d9','51c8ffe6'),(0x58e2,'willowBackgroundColumnsPairClear','30c130c2','51c8fffa4e75'),(0x65fe,'willowBackgroundColumnsBulkClear','30c130c230c130c230c130c230c130c2','51c8ffee4e75')]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def build(out,state,code,scroll2,scroll3):
 out=out.resolve();out.mkdir(parents=True,exist_ok=True)
 source=ROOT/'build/reference_maincpu.bin';original=source.read_bytes();assert len(original)==0x100000 and sha(source)==SOURCE_SHA
 if state&1 or code&1 or code<0x100100 or code>=0x1f0000:raise ValueError('Even state/code required; isolated code must be above original ROM')
 if any(base&0x3fff for base in [scroll2,scroll3])or scroll2==scroll3:raise ValueError('Distinct aligned16KiB source map bases required')
 # Owner addresses are parameters, never a production allocation claim.
 cmd=['m68k-elf-gcc','-m68000','-x','assembler-with-cpp',f'-DWILLOW_BG_SCROLL2_BASE={scroll2}',f'-DWILLOW_BG_SCROLL3_BASE={scroll3}','-c',str(ROOT/'tools/native_background_columns_hooks.S'),'-o',str(out/'hooks.o')]
 subprocess.run(cmd,check=True)
 subprocess.run(['m68k-elf-gcc','-m68000','-std=c99','-O2','-ffreestanding','-Wall','-Wextra','-Werror','-I'+str(ROOT/'src'),'-c',str(ROOT/'tools/native_background_columns_abi.c'),'-o',str(out/'abi.o')],check=True)
 subprocess.run(['m68k-elf-ld',f'-Ttext={code:#x}',f'--defsym=willowNativeBackgroundColumnsState={state:#x}','-e','willowBackgroundColumnsController2',str(out/'hooks.o'),'-o',str(out/'hooks.elf')],check=True)
 subprocess.run(['m68k-elf-objcopy','-O','binary',str(out/'hooks.elf'),str(out/'hooks.bin')],check=True)
 text=subprocess.check_output(['m68k-elf-nm','-n',str(out/'hooks.elf')],text=True);(out/'hooks.sym').write_text(text)
 symbols={line.split()[2]:int(line.split()[0],16)for line in text.splitlines()if len(line.split())==3}
 binary=(out/'hooks.bin').read_bytes();program=bytearray(original);patches=[]
 for index,(at,name,expected) in enumerate(LEAVES):
  expected=bytes.fromhex(expected);assert original[at:at+8]==expected
  full_span=expected+bytes.fromhex(LOOP_TAILS[index]);assert original[at:at+len(full_span)]==full_span
  span=symbols[name+'SourceStores']-code;assert binary[span:span+8]==expected,(name,binary[span:span+8].hex())
  replacement=struct.pack('>H',LEAF_PATCH_OPCODE)+struct.pack('>I',symbols[name])+b'\x4e\x71'
  program[at:at+8]=replacement;patches.append(dict(address=at,expected=expected.hex(),replacement=replacement.hex(),target=name,full_source_span=full_span.hex()))
 for at,dest,name in [(0x3b52a,0x42602,'willowBackgroundColumnsController2'),(0x3b530,0x42f48,'willowBackgroundColumnsController3')]:
  expected=b'\x4e\xb9'+struct.pack('>I',dest);assert original[at:at+6]==expected
  replacement=b'\x4e\xb9'+struct.pack('>I',symbols[name]);program[at:at+6]=replacement
  patches.append(dict(address=at,expected=expected.hex(),replacement=replacement.hex(),target=name))
 for at,name,stores,tail in BULKS:
  body=bytes.fromhex(stores);expected=body+bytes.fromhex(tail);assert original[at:at+len(expected)]==expected
  span=symbols[name+'SourceStores']-code;assert binary[span:span+len(body)]==body
  replacement=b'\x4e\xf9'+struct.pack('>I',symbols[name]);program[at:at+6]=replacement
  patches.append(dict(address=at,expected=original[at:at+6].hex(),replacement=replacement.hex(),target=name,full_source_span=expected.hex()))
 (out/'program.bin').write_bytes(program)
 with (out/'hooks.disasm').open('w')as f:subprocess.run(['m68k-elf-objdump','-d',str(out/'hooks.elf')],stdout=f,check=True)
 manifest=dict(scope=__doc__,source_sha256=SOURCE_SHA,code_address=code,state_address=state,state_bytes=48,scroll2=scroll2,scroll3=scroll3,symbols=symbols,patches=patches,
  inputs={str(p):sha(p)for p in [source,ROOT/'tools/native_background_columns_hooks.S',ROOT/'tools/native_background_columns_abi.c',ROOT/'src/willow_native_background_columns.h',Path(__file__)]},
  output_hashes={p.name:sha(p)for p in out.iterdir()if p.is_file()and p.name!='manifest.json'})
 (out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n');return manifest
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);p.add_argument('--state',type=lambda x:int(x,0),required=True);p.add_argument('--code',type=lambda x:int(x,0),required=True);p.add_argument('--scroll2',type=lambda x:int(x,0),required=True);p.add_argument('--scroll3',type=lambda x:int(x,0),required=True);a=p.parse_args()
 print(json.dumps(build(a.output,a.state,a.code,a.scroll2,a.scroll3),indent=2))
if __name__=='__main__':main()
