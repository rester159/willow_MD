#!/usr/bin/env python3
"""Write/compile an isolated integration proposal; never modify root sources."""
from pathlib import Path
import argparse,difflib,hashlib,json,re,shutil,subprocess
from test_native_visible_edge_opacity_root import extract
ROOT=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def proposed(source):
 marker='static uint16_t bgNearVisibleRowFull(uint16_t y);\nstatic uint16_t bgFarRowBlank(uint16_t row);'
 assert source.count(marker)==1
 helper=(ROOT/'tools/prototype_native_far_row_proof.h').read_text()
 declaration='''#ifdef RENDER_BACKGROUND_FAR_STREAM
typedef struct {uint16_t row[2],full[2];} BackgroundNearRowProof;
#define BG_ROW_PROOF_ARGUMENT ,BackgroundNearRowProof*proof
#define BG_ROW_PROOF_PASS(p) ,p
#define BG_ROW_FULL(y) rowProofFull(proof,y)
#define BG_SHARED_FAR_CODE BG_FAR_CODE
#else
#define BG_ROW_PROOF_ARGUMENT
#define BG_ROW_PROOF_PASS(p)
#define BG_ROW_FULL(y) bgNearVisibleRowFull(y)
#define BG_SHARED_FAR_CODE
#endif
static uint16_t bgNearVisibleRowFull(uint16_t y);
static uint16_t bgFarRowBlank(uint16_t row BG_ROW_PROOF_ARGUMENT);
#ifdef RENDER_BACKGROUND_FAR_STREAM
/* Fresh stack-local memo for one serialized far-row call. Prepared near view,
 * rings, keys and counts cannot change while this caller owns bg.busy. Source
 * map/staged-view/epoch changes still pass through the existing final guards. */
'''
 helper=helper[helper.index('static void rowProofInit'):]
 source=source.replace(marker,declaration+helper+'\n#endif')
 body=extract(source,'bgFarNeeded');new=body.replace('static uint16_t bgFarNeeded','static BG_SHARED_FAR_CODE uint16_t bgFarNeeded',1).replace('uint16_t y)','uint16_t y BG_ROW_PROOF_ARGUMENT)',1).replace('bgNearVisibleRowFull(lastY)','BG_ROW_FULL(lastY)');source=source.replace(body,new)
 body=extract(source,'bgResolveRef');new=body.replace('static uint16_t bgResolveRef','static BG_SHARED_FAR_CODE uint16_t bgResolveRef',1).replace('uint16_t ref)','uint16_t ref BG_ROW_PROOF_ARGUMENT)',1).replace('bgFarNeeded(ref,x,y)','bgFarNeeded(ref,x,y BG_ROW_PROOF_PASS(proof))');source=source.replace(body,new)
 source=source.replace('bgResolveRef(layer,x,y,bgRef(layer,x,y))','bgResolveRef(layer,x,y,bgRef(layer,x,y) BG_ROW_PROOF_PASS(0))')
 body=extract(source,'bgFarRowBlank');new=body.replace('uint16_t row)','uint16_t row BG_ROW_PROOF_ARGUMENT)',1).replace('bgNearVisibleRowFull(ny)','BG_ROW_FULL(ny)').replace('bgNearVisibleRowFull(end)','BG_ROW_FULL(end)');source=source.replace(body,new)
 body=extract(source,'bgResolveRow');new=body.replace('{\n uint16_t x=', '{\n#ifdef RENDER_BACKGROUND_FAR_STREAM\n BackgroundNearRowProof proof;rowProofInit(&proof);\n#endif\n uint16_t x=',1).replace('bgFarRowBlank(row)','bgFarRowBlank(row BG_ROW_PROOF_PASS(&proof))')
 loop='for(uint16_t n=0;n<43;n++)if(!bgResolveCell(layer,(x+n)&(layer?255:127),y))return 0;'
 assert loop in new
 new=new.replace(loop,'''#ifdef RENDER_BACKGROUND_FAR_STREAM
 for(uint16_t n=0;n<43;n++){
  uint16_t worldX=(x+n)&(layer?255:127);
  if(layer){if(!bgResolveRef(1,worldX,y,bgRef(1,worldX,y),&proof))return 0;}
  else if(!bgResolveCell(0,worldX,y))return 0;
 }
#else
 '''+loop+'\n#endif');source=source.replace(body,new)
 body=extract(source,'bgFarBlank');source=source.replace(body,body.replace('bgFarRowBlank(row)','bgFarRowBlank(row BG_ROW_PROOF_PASS(0))'))
 source=source.replace('bgResolveRef(layer,x,y,ref))','bgResolveRef(layer,x,y,ref BG_ROW_PROOF_PASS(0)))')
 return source
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--target',type=Path,default=ROOT/'build/native-render-cadence/far-stream-v7');p.add_argument('--output',type=Path,default=ROOT/'build/native-far-row-proof/integration-v1');a=p.parse_args();target=a.target.resolve();out=a.output.resolve();assert not out.exists();out.mkdir(parents=True)
 manifest=json.loads((target/'manifest.json').read_text());inputs={str(target/'manifest.json'):sha(target/'manifest.json')}
 for name,digest in manifest['source_files'].items():
  path=ROOT/name;assert sha(path)==digest,name;inputs[str(path)]=digest
 for name,digest in manifest['resource_files'].items():assert sha(Path(name))==digest,name
 for variant in ('control','proposed'):
  d=out/variant;shutil.copytree(ROOT/'src',d/'src');(d/'tools').mkdir()
  for file in (ROOT/'tools').glob('native_render_cadence.*'):shutil.copyfile(file,d/'tools'/file.name)
  for name in ('native_render_background.inc','native_background_columns_hooks.S','native_background_columns_abi.c'):shutil.copyfile(ROOT/'tools'/name,d/'tools'/name)
 original=(out/'control/tools/native_render_background.inc').read_text();candidate=proposed(original);(out/'proposed/tools/native_render_background.inc').write_text(candidate)
 (out/'integration.patch').write_text(''.join(difflib.unified_diff(original.splitlines(True),candidate.splitlines(True),fromfile='a/tools/native_render_background.inc',tofile='b/tools/native_render_background.inc')))
 background_bytes=int(re.search(r'#define RENDER_BACKGROUND_BYTES (\d+)u',(target/'render-addresses.h').read_text())[1]);commands=[];results={}
 for variant in ('control','proposed'):
  d=out/variant
  for original_bus in (True,False):
   suffix='-original'if original_bus else '';elf=d/f'render{suffix}.elf'
   flags=['-DRENDER_RETAINED=1','-DRENDER_CONTENT_STREAM=1','-DRENDER_BACKGROUND_RESIDENT=1',f'-DRENDER_BACKGROUND_BYTES={background_bytes}','-DRENDER_BACKGROUND_LIVE=1',f'-DWILLOW_BG_SCROLL2_BASE={0x918000 if original_bus else 0x204000}',f'-DWILLOW_BG_SCROLL3_BASE={0x914000 if original_bus else 0x208000}','-DRENDER_BACKGROUND_LOOKUP=1','-DRENDER_BACKGROUND_NEAR_STRIP=1','-DWILLOW_NATIVE_NEAR_STRIP_PREPARED=1','-DRENDER_BACKGROUND_COLUMNS=1','-DRENDER_BACKGROUND_FAR_STREAM=1','-DWILLOW_NATIVE_FAR_AUX_CODE=1']
   if original_bus:flags+=['-DRENDER_ORIGINAL_BUS=1']
   common=['m68k-elf-gcc','-m68000','-O2','-ffreestanding','-fno-builtin','-fstack-usage','-nostdlib','-Wall','-Wextra','-Werror','-I'+str(d/'src'),'-I'+str(ROOT/'build/native-route-objects/prepared-v2'),'-I'+str(target)]
   cmd=common+flags+['-Wl,--defsym=__render_bss_limit=0xff7000','-Wl,-T,'+str(d/'tools/native_render_cadence.ld'),str(d/'tools/native_render_cadence.c'),str(d/'tools/native_render_cadence.S'),str(target/'render_far_layout.c'),str(d/'tools/native_background_columns_hooks.S'),str(d/'tools/native_background_columns_abi.c'),'-o',str(elf)];commands.append(cmd);subprocess.run(cmd,check=True,cwd=ROOT)
   sym=subprocess.check_output(['m68k-elf-nm','-n',str(elf)],text=True);(d/f'render{suffix}.sym').write_text(sym);symbols={r.split()[2]:int(r.split()[0],16)for r in sym.splitlines()if len(r.split())==3}
   binary=d/f'render{suffix}.bin';aux=d/f'render{suffix}-aux.bin'
   subprocess.run(['m68k-elf-objcopy','-O','binary','-R','.willow_far_stream',str(elf),str(binary)],check=True);subprocess.run(['m68k-elf-objcopy','-O','binary','-j','.willow_far_stream',str(elf),str(aux)],check=True)
   if variant=='control':assert binary.read_bytes()==(target/f'render{suffix}.bin').read_bytes();assert aux.read_bytes()==(target/f'render{suffix}-far-stream.bin').read_bytes()
   if not original_bus:assert aux.read_bytes()==(d/'render-original-aux.bin').read_bytes();assert sym==(d/'render-original.sym').read_text()
   results[f'{variant}{suffix}']={'main_code_bytes':symbols['__render_code_end']-symbols['__render_code_begin'],'main_binary_bytes':binary.stat().st_size,'auxiliary_bytes':aux.stat().st_size,'bss_bytes':symbols['__render_bss_end']-symbols['__render_bss_begin'],'main_free_before_hooks':0x10e000-(0x102000+binary.stat().st_size),'aux_free_before_resources':0x150000-symbols['__render_far_code_end']}
   if not original_bus:
    nofar=[v for v in flags if v not in ('-DRENDER_BACKGROUND_FAR_STREAM=1','-DWILLOW_NATIVE_FAR_AUX_CODE=1')]
    cmd=common+nofar+['-c',str(d/'tools/native_render_cadence.c'),'-o',str(d/'nonfar.o')];commands.append(cmd);subprocess.run(cmd,check=True,cwd=ROOT)
 assert (out/'control/nonfar.o').read_bytes()==(out/'proposed/nonfar.o').read_bytes()
 result={'scope':__doc__,'status':'pass','runtime_admitted':False,'control_reproduces_frozen_target':True,'nonfar_object_byte_identical':True,'both_buses_auxiliary_byte_identical':True,'sizes':results,'inputs':inputs,'commands':commands,'outputs':{str(q.relative_to(out)):sha(q)for q in out.rglob('*')if q.is_file()}};(out/'evidence.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(results,indent=2))
if __name__=='__main__':main()
