#!/usr/bin/env python3
"""Independently check cold native background residency in the paired cadence gate.

All declared background patterns, retained object patterns, reserved blank tiles
and CRAM are read from actual core memory every ownership-armed frame. Existing
source, SAT, timing, provenance and VDP ownership checks remain unchanged. This
does not admit live background maps, priority composition, HUD or full-game FPS.
"""
from pathlib import Path
import argparse,hashlib,json,re,shutil,struct,subprocess
import test_native_render_cadence as cadence

ROOT=Path(__file__).resolve().parents[1]
sha=cadence.sha

def checked(path,digest,pins):
 path=path.resolve();data=path.read_bytes()
 assert hashlib.sha256(data).hexdigest()==digest,('Input hash mismatch',str(path))
 pins[str(path)]=digest
 return data

def prepare_oracle(platform,assets,out):
 pins={};target_path=platform/'manifest.json';asset_path=assets/'manifest.json'
 target=json.loads(target_path.read_text());prepared=json.loads(asset_path.read_text())
 pins.update({str(target_path):sha(target_path),str(asset_path):sha(asset_path)})
 assert target['resource_files'][str(asset_path)]==sha(asset_path),'Target was not compiled against selected asset manifest'
 for name,digest in target['products'].items():checked(platform/name,digest,pins)
 for name,digest in target['resource_files'].items():checked(Path(name),digest,pins)
 for name,digest in prepared['hashes'].items():checked(Path(name) if Path(name).is_absolute() else ROOT/name,digest,pins)
 for name,digest in prepared['outputs'].items():checked(assets/name,digest,pins)
 # Retain matching native source evidence without forbidding later unrelated
 # work on the builder. Target products and actual observer inputs stay pinned.
 frozen={}
 for name,digest in target['source_files'].items():
  source=Path(name) if Path(name).is_absolute() else ROOT/name
  data=source.read_bytes();assert hashlib.sha256(data).hexdigest()==digest,('Target source changed before pinning',name)
  copy=out/'frozen-target-sources'/source.relative_to(ROOT);copy.parent.mkdir(parents=True,exist_ok=True);copy.write_bytes(data)
  pins[str(copy)]=digest;frozen[name]={'copy':str(copy),'sha256':digest}
 extra=cadence.extra_rom_resource(platform,target,True)
 assert extra['origin']==0x110000
 assert extra['data']==(assets/'resident-upload.bin').read_bytes()
 header=(platform/'render-addresses.h').read_text()
 assert re.search(r'^#define RENDER_BACKGROUND_RESIDENT 1$',header,re.M)
 assert re.search(r'^#define RENDER_BACKGROUND_BYTES '+str(extra['bytes'])+r'u$',header,re.M)
 assert prepared['reserved_tiles']=={'low':[0,15],'window':[876,980],'objects':[981,1480],'tables_begin':1536}
 assert prepared['uploaded_patterns']==906 and prepared['upload_bytes']==28992
 physical={}
 for tile,pattern in struct.iter_unpack('>H32s',(assets/'physical-tiles.bin').read_bytes()):
  assert tile not in physical and (16<=tile<=875 or 1481<=tile<=1535)
  physical[tile]=pattern
 assert len(physical)==prepared['uploaded_patterns']
 assert list(physical)==sorted(physical)
 blob=extra['data'];magic,version,count,total,directory=struct.unpack_from('>IHHII',blob)
 assert (magic,version,total,directory)==(0x57425231,1,len(blob),16)
 assert count==len(prepared['upload_runs'])
 blob_tiles={};runs=[];previous=0
 for n,declared in enumerate(prepared['upload_runs']):
  address,words,offset=struct.unpack_from('>HHI',blob,directory+n*8)
  assert address%32==0 and words>0 and words%16==0 and offset%4==0
  assert offset>=directory+8*count and offset+2*words<=len(blob) and address>=previous
  assert address==declared['first_tile']*32 and words==declared['count']*16
  assert blob[offset:offset+words*2]==(assets/'upload-data.bin').read_bytes()[declared['data_byte_offset']:declared['data_byte_offset']+words*2]
  for at in range(0,words*2,32):
   tile=(address+at)//32;assert tile not in blob_tiles;blob_tiles[tile]=blob[offset+at:offset+at+32]
  previous=address+words*2;runs.append({'vram_address':address,'words':words,'source_offset':offset})
 assert blob_tiles==physical,'Compiled WBR1 does not reproduce independent physical tile records'
 pool=bytearray(1536*32)
 for tile,pattern in physical.items():pool[tile*32:tile*32+32]=pattern
 object_paths=[Path(name).parent for name in target['resource_files'] if name.endswith('/route_objects.h')]
 assert len(object_paths)==1
 objects=object_paths[0];object_manifest=json.loads((objects/'manifest.json').read_text())
 pins[str(objects/'manifest.json')]=sha(objects/'manifest.json')
 patterns=checked(objects/'object-patterns.bin',object_manifest['outputs']['object-patterns.bin'],pins)
 assert object_manifest['first_tile']==981 and len(patterns)==500*32
 pool[981*32:1481*32]=patterns
 cram=(assets/'cram.bin').read_bytes();assert len(cram)==128 and cram==(objects/'cram.bin').read_bytes()
 assert (assets/'source-palettes.bin').read_bytes()==(objects/'source-palettes.bin').read_bytes()
 oracle=out/'background-residency-assets.h'
 text='/* Independent host-only oracle; never copied into the emulated machine. */\n'
 for symbol,data in [('backgroundResidencyPatterns',pool),('backgroundResidencyCRAM',cram)]:
  text+='static const uint8_t '+symbol+'[]={\n'+'\n'.join(','.join(map(str,data[n:n+32]))+',' for n in range(0,len(data),32))+'\n};\n'
 oracle.write_text(text);pins[str(oracle)]=sha(oracle)
 return pins,oracle,bytes(pool),cram,{'background_patterns':len(physical),'background_bytes':len(physical)*32,'object_patterns':500,'blank_patterns':1536-len(physical)-500,'runs':runs,'frozen_target_sources':frozen,'object_assets':str(objects)}

def main():
 parser=argparse.ArgumentParser(description=__doc__)
 parser.add_argument('--platform',type=Path,default=ROOT/'build/native-render-cadence/background-resident-v1')
 parser.add_argument('--assets',type=Path,default=ROOT/'build/native-background-residency/prepared-v1')
 parser.add_argument('--output',type=Path,default=ROOT/'build/native-background-residency/observer-v1')
 parser.add_argument('--frames',type=int,default=1800)
 parser.add_argument('--core',type=Path,default=Path.home()/'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib')
 args=parser.parse_args();assert 1<=args.frames<=36000
 platform,assets,out=args.platform.resolve(),args.assets.resolve(),args.output.resolve()
 assert not out.exists(),'Use a fresh evidence directory'
 out.mkdir(parents=True)
 pins,oracle,pool,cram,contract=prepare_oracle(platform,assets,out)
 binary,manifest=cadence.build(platform,out,extra_rom=True)
 generated=out/'test_native_render_cadence.c';header=ROOT/'tools/test_native_background_residency.h'
 text=generated.read_text()
 text=cadence.replace(text,'int main(int argc,char**argv){','#include "'+str(oracle)+'"\n#include "'+str(header)+'"\nint main(int argc,char**argv){')
 text=cadence.replace(text,'   if(renderObserveFrame(path))return 1;','   if(renderObserveFrame(path))return 1;\n   if(backgroundResidencyObserve(path))return 1;')
 text=cadence.replace(text,' renderFinish();',' backgroundResidencyFinish();\n renderFinish();')
 generated.write_text(text)
 subprocess.run(manifest['command'],cwd=ROOT,check=True)
 pins.update({str(path):sha(path) for path in [Path(__file__).resolve(),header]})
 manifest['inputs'].update(pins)
 manifest.update(generated_sha256=sha(generated),binary_sha256=sha(binary),extension_scope=__doc__)
 with(out/'observer.log').open('w')as log:
  result=subprocess.run([str(binary),str(args.core),str(args.frames),'--coin-route',str(out)],cwd=ROOT,stdout=log,stderr=subprocess.STDOUT)
 manifest['run']={'exit_code':result.returncode,'frames':args.frames,'core_sha256':sha(args.core),'log_sha256':sha(out/'observer.log')}
 (out/'observer-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
 assert all(sha(Path(name))==digest for name,digest in manifest['inputs'].items()),'Observer input changed during run'
 if result.returncode:raise SystemExit(result.returncode)
 summary=json.loads((out/'background-residency-summary.json').read_text())
 assert all(path['ownership_armed_frames_checked']>0 for path in summary['paths'])
 assert summary['paths'][0]['ownership_armed_frames_checked']==summary['paths'][1]['ownership_armed_frames_checked']
 actual=(out/'render-final-vram.bin').read_bytes();assert len(actual)==65536 and actual[:len(pool)]==pool
 assert (out/'render-final-cram.bin').read_bytes()==cram
 evidence={'scope':__doc__,'contract':contract,'frames':args.frames,'residency':summary,'final_pattern_pool_bytes_exact':len(pool),'final_background_bytes_exact':contract['background_bytes'],'final_cram_bytes_exact':len(cram),'source_and_renderer_summary':json.loads((out/'render-summary.json').read_text()),'inputs':manifest['inputs'],'outputs':{name:sha(out/name) for name in ['observer-manifest.json','observer.log','background-residency-summary.json','render-summary.json','render-final-vram.bin','render-final-cram.bin']}}
 (out/'residency-evidence.json').write_text(json.dumps(evidence,indent=2)+'\n')
 print(json.dumps({'frames':args.frames,'residency':summary,'final_background_bytes_exact':contract['background_bytes'],'evidence':str(out/'residency-evidence.json')},indent=2))

if __name__=='__main__':main()
