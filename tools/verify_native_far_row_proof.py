#!/usr/bin/env python3
"""Old/current visibility and source-mask proof; never edits live sources."""
from pathlib import Path
import argparse
import hashlib
import json
import shutil
import subprocess
from test_native_visible_edge_opacity_root import extract

ROOT=Path(__file__).resolve().parents[1]


def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()


def predicates(source,candidate=None):
    names=['bgRef','bgFirstX','bgFirstY','bgRows','bgNearOpacityRef','bgNearVisibleRowFull','bgFarRowBlank','bgFarBlank','bgFarNeeded']
    funcs='\n'.join(extract(source,n) for n in names)
    funcs=funcs.replace('return bgRef(0,x,y);','opacityCalls++;return bgRef(0,x,y);')
    funcs=funcs.replace('bgNearVisibleRowFull(uint16_t y)\n{','bgNearVisibleRowFull(uint16_t y)\n{\n fullCalls++;',1)
    proof=(ROOT/'tools/prototype_native_far_row_proof.h').read_text()
    needed=extract(candidate if candidate is not None else source,'bgFarNeeded')
    needed=needed.replace('BG_SHARED_FAR_CODE ','').replace(' BG_ROW_PROOF_ARGUMENT','').replace('BG_ROW_FULL(lastY)','bgNearVisibleRowFull(lastY)')
    needed=needed.replace('bgFarNeeded(uint16_t ref,uint16_t x,uint16_t y)',
        'proposedFarNeeded(uint16_t ref,uint16_t x,uint16_t y,BackgroundNearRowProof*proof)',1)
    needed=needed.replace('bgNearVisibleRowFull(lastY)','rowProofFull(proof,lastY)')
    blank=extract(source,'bgFarRowBlank').replace('bgFarRowBlank(uint16_t row)',
        'proposedFarRowBlank(uint16_t row,BackgroundNearRowProof*proof)',1)
    blank=blank.replace('bgNearVisibleRowFull(ny)','rowProofFull(proof,ny)').replace('bgNearVisibleRowFull(end)','rowProofFull(proof,end)')
    return funcs+'\n'+proof+'\n'+needed+'\n'+blank+'\n'


def host_functions(source,candidate=None):
    text=predicates(source,candidate)+'\n'+extract(source,'bgSet')+'\n'
    text+='\n'.join(extract(source,n) for n in ['bgResolveRef','bgResolveCell'])+'\n'
    row=extract(source,'bgResolveRow')
    text+=row.replace('bgResolveRow(', 'baselineResolveRow(',1)+'\n'
    ref=extract(source,'bgResolveRef').replace('bgResolveRef(uint16_t layer,uint16_t x,uint16_t y,uint16_t ref)',
       'proposedResolveRef(uint16_t layer,uint16_t x,uint16_t y,uint16_t ref,BackgroundNearRowProof*proof)',1)
    ref=ref.replace('bgFarNeeded(ref,x,y)','proposedFarNeeded(ref,x,y,proof)')
    text+=ref+'\n'
    row=row.replace('{\n uint16_t x=', '{\n BackgroundNearRowProof proof;rowProofInit(&proof);\n uint16_t x=',1)
    row=row.replace('bgFarRowBlank(row)','proposedFarRowBlank(row,&proof)')
    row=row.replace('bgResolveCell(layer,(x+n)&(layer?255:127),y)',
      'proposedResolveRef(layer,(x+n)&(layer?255:127),y,bgRef(layer,(x+n)&(layer?255:127),y),layer?&proof:0)')
    text+=row+'\n'
    return text


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output',type=Path,default=ROOT/'build/native-far-row-proof/host-v1')
    p.add_argument('--source',type=Path,default=ROOT/'build/native-forest-extension/target-far-stream-v7-frozen/frozen-sources/tools/native_render_background.inc')
    p.add_argument('--candidate',type=Path,default=ROOT/'tools/native_render_background.inc')
    a=p.parse_args();out=a.output.resolve();assert not out.exists();out.mkdir(parents=True)
    source=a.source.resolve();test=ROOT/'tools/test_native_visible_edge_opacity_root.c'
    fixtures=[ROOT/f'build/native-visible-opacity/edges-v1/source-masks-view{n}.bin' for n in (120,130,132)]
    inputs=[source,test,Path(__file__).resolve(),ROOT/'tools/prototype_native_far_row_proof.h',
        ROOT/'tools/test_native_visible_edge_opacity_root.py',ROOT/'src/willow_native_background_key_cache.h',
        ROOT/'src/willow_native_plane_surface.h',ROOT/'src/willow_native_presentation_plan.h',*fixtures]
    if a.candidate:inputs.append(a.candidate.resolve())
    before={str(p):sha(p) for p in inputs}
    for path in inputs:
        dest=out/'inputs'/path.relative_to(ROOT);dest.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(path,dest)
    # Stable names let the native follow-up reproduce these exact inputs even
    # when the baseline is a previously frozen target rather than live code.
    (out/'baseline-source.inc').write_bytes(source.read_bytes())
    if a.candidate:(out/'candidate-source.inc').write_bytes(a.candidate.read_bytes())
    s=source.read_text();funcs=host_functions(s,a.candidate.read_text() if a.candidate else None);(out/'actual-functions.h').write_text(funcs)
    c=test.read_text().replace('static unsigned long checks,cases,overlapCases,rowClearCases;',
        'static unsigned long checks,cases,overlapCases,rowClearCases;\nstatic unsigned fullCalls,opacityCalls,proofRows,maximumFullCalls;')
    begin=c.index('static void verifyCells(void){');end=c.index('static void sourceFixture',begin)
    c=c[:begin]+'''static void verifyCells(void){
 for(unsigned row=0;row<bgRows(1);row++){
  BackgroundNearRowProof proof;rowProofInit(&proof);unsigned cachedCalls=0;
  unsigned expectedBlank=bgFarRowBlank(row);fullCalls=0;
  CHECK(proposedFarRowBlank(row,&proof)==expectedBlank);cachedCalls+=fullCalls;
  for(unsigned col=0;col<43;col++){
   unsigned x=(bgFirstX(1)+col)&255,y=(bgFirstY(1)+row)&255,ref=independentRef(1,x,y);
   unsigned old=bgFarNeeded(ref,x,y);CHECK(old==oracle(ref,x,y));fullCalls=0;
   CHECK(proposedFarNeeded(ref,x,y,&proof)==old);cachedCalls+=fullCalls;
   CHECK(proposedFarNeeded(ref,x,y,0)==old);overlapCases++;
  }
  CHECK(cachedCalls<=2);if(cachedCalls>maximumFullCalls)maximumFullCalls=cachedCalls;proofRows++;
 }
}
''' +c[end:]
    old='unsigned y=(bgFirstY(1)+row)&255,first=bgFirstX(1);CHECK(bgResolveRow(1,row));'
    new='''unsigned y=(bgFirstY(1)+row)&255,first=bgFirstX(1);
  __typeof__(bg) before=bg;CHECK(baselineResolveRow(1,row));__typeof__(bg) expectedState=bg;bg=before;
  CHECK(bgResolveRow(1,row));CHECK(!memcmp(&bg,&expectedState,sizeof bg));'''
    assert old in c;c=c.replace(old,new)
    # Fresh context must not reuse a prior view/row-count/key result. A same
    # parity but different full world-row key must also recompute.
    marker=' printf("{\\"cases\\":'
    pos=c.index(marker)
    c=c[:pos]+''' setup(127,127,0,0,0,0,0,0);BackgroundNearRowProof proof;rowProofInit(&proof);
 fullCalls=0;CHECK(rowProofFull(&proof,127));CHECK(fullCalls==1);CHECK(rowProofFull(&proof,127));CHECK(fullCalls==1);
 CHECK(rowProofFull(&proof,0));CHECK(fullCalls==2);CHECK(rowProofFull(&proof,2));CHECK(fullCalls==3);
 /* The old cache is deliberately discarded after a dependency changes. */
 bg.rowCounts[0][31]=43;rowProofInit(&proof);CHECK(!rowProofFull(&proof,127));
 setup(127,127,0,0,0,0,0,0);rowProofInit(&proof);CHECK(rowProofFull(&proof,127));
 bg.view[0]++;bg.rowCounts[0][31]=43;rowProofInit(&proof);CHECK(!rowProofFull(&proof,127));
''' +c[pos:]
    c=c.replace('\\"sanitizers\\":true}', '\\"sanitizers\\":true,\\"row_local_proof_rows\\":%u,\\"maximum_full_proofs_per_row\\":%u}')
    c=c.replace('rowClearCases,checks);return 0;', 'rowClearCases,checks,proofRows,maximumFullCalls);return 0;')
    (out/'host.c').write_text(c)
    cmd=['clang','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-sanitize-recover=all',
        '-I'+str(out/'inputs/src'),'-I'+str(out),str(out/'host.c'),'-o',str(out/'test')]
    subprocess.run(cmd,check=True)
    with (out/'result.json').open('w') as stream:subprocess.run([str(out/'test'),*[str(p)for p in fixtures]],stdout=stream,check=True)
    result=json.loads((out/'result.json').read_text())
    report={'scope':__doc__,'status':'pass','runtime_admitted':False,'native_test_pending':True,
        'result':result,'compile':cmd,'inputs':before,'live_inputs_still_match':{str(p):sha(p)==before[str(p)]for p in inputs},
        'proposal':'8-byte stack-local exact world-row/result cache; row&1 selects slot but full row identity must match. Share it only within one serialized far-row resolution including the row-blank precheck. Direct/cold calls use NULL.',
        'limitations':['Host sanitizer proof; actual68000 and complete live cache/IRQ integration remain separate.',
            'The context is invalid across changes to prepared near view/rowCounts/rings/keys or immutable resource bindings.']}
    report['outputs']={str(p.relative_to(out)):sha(p)for p in out.rglob('*')if p.is_file()}
    (out/'evidence.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(result,indent=2))


if __name__=='__main__':main()
