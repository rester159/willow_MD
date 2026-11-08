#!/usr/bin/env python3
"""Compile and headlessly compare the48-byte column-writer C lifecycle API."""
from pathlib import Path
import argparse,hashlib,json,re,subprocess
ROOT=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,default=ROOT/'build/native-background-columns/api-v1');a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
 core=Path.home()/'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib'
 inputs=[ROOT/'src/willow_native_background_columns.h',ROOT/'tools/native_background_columns_api.c',ROOT/'tools/test_native_background_columns_api.c',ROOT/'tools/test_native_render_cache_closed.c',ROOT/'tools/vendor/libretro.h',Path(__file__)]
 hashes={str(p):sha(p)for p in inputs};commands=[]
 def run(cmd,log=None):
  commands.append(cmd)
  if log:
   with(out/log).open('w')as f:subprocess.run(cmd,cwd=ROOT,stdout=f,stderr=subprocess.STDOUT,check=True)
  else:subprocess.run(cmd,cwd=ROOT,check=True)
 run(['m68k-elf-gcc','-m68000','-std=c99','-O2','-Wall','-Wextra','-Werror','-ffreestanding','-fno-builtin','-nostdlib','-Wl,-Ttext=0x102000,-e,willowBackgroundColumnsApiProbe','tools/native_background_columns_api.c','-lgcc','-o',str(out/'api.elf')])
 run(['m68k-elf-objcopy','-O','binary',str(out/'api.elf'),str(out/'api.bin')])
 symbols=subprocess.check_output(['m68k-elf-nm','-n',str(out/'api.elf')],text=True);(out/'api.sym').write_text(symbols)
 entry=int(re.search(r'^([0-9a-f]+) T willowBackgroundColumnsApiProbe$',symbols,re.M)[1],16)
 (out/'columns-api-fixture.h').write_text(f'#define API_ENTRY {entry:#x}\n')
 run(['clang','-O2','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(out),'tools/test_native_background_columns_api.c','-o',str(out/'test')])
 run([str(out/'test'),str(core),str(out/'api.bin')],'test.log')
 assert hashes=={str(p):sha(p)for p in inputs},'Inputs changed during API proof'
 assert 'PASS 4160 compiled68000 API cases' in(out/'test.log').read_text()
 result={'status':'pass','scope':__doc__,'core_sha256':sha(core),'inputs':hashes,'cases':4160,'snapshot_properties':128,'commands':commands,'outputs':{str(p.relative_to(out)):sha(p)for p in out.iterdir()if p.is_file()and p.name!='evidence.json'}}
 (out/'evidence.json').write_text(json.dumps(result,indent=2)+'\n');print((out/'test.log').read_text())
if __name__=='__main__':main()
