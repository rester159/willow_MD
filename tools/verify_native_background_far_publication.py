#!/usr/bin/env python3
"""Independently replay far-cache transfers and check epoch-resolved source pixels.

No target mutations. Complete low-plane snapshots only; source failures,
missing publications and deadlines remain explicit. Actor priority and HUD
are outside this pixel oracle.
"""
from pathlib import Path
import argparse,csv,hashlib,json,struct,types
import verify_native_background_publication as base
from native_far_observer_resources import load_resources,add_shared_resources
from test_native_render_cadence import replace
ROOT=Path(__file__).resolve().parents[1]
U16=lambda d,a:struct.unpack_from('>H',d,a)[0]
U32=lambda d,a:struct.unpack_from('>I',d,a)[0]

def require(ok,msg):
    if not ok:raise ValueError(msg)

def put16(d,a,v):struct.pack_into('>H',d,a,v)
def put32(d,a,v):struct.pack_into('>I',d,a,v)

class FarOracle:
    def __init__(self,run,resource,pin,manifest):
        self.run=run;self.data=load_resources(resource,pin);d=self.data
        shared=manifest['far_cache_observation'].get('shared_resource')
        if shared:
            add_shared_resources(d,Path(shared))
            for path,digest in d['pins'].items():pin(Path(path),digest)
        self.near_keys=dict(d.get('near_keys',[]))
        self.offsets={k.removeprefix('RENDER_BG_FAR_CACHE_').removesuffix('_OFFSET').lower():v for k,v in manifest['far_cache_observation']['metadata'].items() if k.endswith('_OFFSET')}
        require(self.offsets['owner']==0 and self.offsets['residency_epoch']==402,'Unreviewed compiler cache ABI')
        self.tiles=[t for t,p in d['pool']];self.pool=set(self.tiles);self.keys=dict(d['keys'])
        self.initial=tuple(p for t,p in d['pool']);self.history=[{1:self.initial},{1:self.initial}]
        events=list(csv.DictReader((run/'far-batches.csv').open()));pin(run/'far-batches.csv')
        states=pin(run/'far-batches.bin');transfers=pin(run/'far-transfers.bin')
        require(len(states)==436*len(events),'Partial far event state stream')
        self._replay(events,states,transfers,owners=None)
        self.rows=list(csv.DictReader((run/'far-publications.csv').open()));pin(run/'far-publications.csv')
        self.states=pin(run/'far-publication-states.bin');self.owner_records=pin(run/'far-publication-owners.bin')
        require(len(self.states)==436*len(self.rows) and len(self.owner_records)==330*len(self.rows),'Partial publication cache stream')
        self.current_epoch=None;self.current={};self.images=[];self.streamed_publications=0

    def _replay(self,events,states,transfers,owners=None):
        d=self.data;o=self.offsets;owners=[list(self.initial),list(self.initial)];epochs=[0,0];opened=[None,None];pos=0;initialized=[False,False]
        self.counts={'batches':0,'patterns':0,'queued_waits':0,'request_refusals':0,'late_batches':0}
        self.history=[{1:self.initial},{1:self.initial}];self.commit_times=[{},{}]
        for i,row in enumerate(events):
            r={k:int(v,16)if k=='pc'else int(v)for k,v in row.items()};p=r['path'];s=states[i*436:(i+1)*436]
            require(r['sample']==i and p in (0,1),'Event order/path')
            phase=r['phase'];actualowners=struct.unpack_from('>165H',s)
            if phase==11:
                require(not initialized[p] and not r['status'],'Repeated/invalid initialization')
                expected=bytearray(436)
                for n,pattern in enumerate(self.initial):put16(expected,n*2,pattern)
                expected[o['slots']:o['slots']+16]=bytes([255])*16
                put16(expected,o['key'],65535);put16(expected,o['phase'],1);put32(expected,o['residency_epoch'],1)
                require(bytes(expected)==s and r['residency_epoch']==1,'Initial cache metadata differs')
                initialized[p]=True;epochs[p]=1;self.commit_times[p][1]=r['master_cycles'];continue
            require(initialized[p],'Cache event before post-clear initialization')
            if phase in (8,9):
                require(actualowners==tuple(owners[p]) and U32(s,o['residency_epoch'])==epochs[p]==r['residency_epoch'],'Unobserved cache ownership')
                if phase==8:
                    require(opened[p] is None and not r['status'],'Unpaired deferred preparation')
                    if U16(s,o['phase'])==2:self.counts['queued_waits']+=p==0
                    elif U16(s,o['phase'])==1 and U16(s,o['missing'])==1:self.counts['request_refusals']+=p==0
                    else:raise ValueError('Deferred preparation has no queued/refused request')
                else:
                    require(opened[p] is None and U16(s,o['phase'])==2 and not r['status'],'Invalid batch begin')
                    opened[p]=(s,r)
                continue
            require(phase==10 and opened[p] is not None,'Unpaired completed batch')
            before,start=opened[p];opened[p]=None;expected=bytearray(before)
            key=U16(before,o['key']);request=U16(before,o['requested']);completed=U16(before,o['completed']);cursor=U16(before,o['cursor']);copies=0
            require(key<len(d['refs'])//16 and request and 0<r['status']<=4 and r['status']==r['patterns'],'Invalid copy count/key')
            for n in range(16):
                if not request&(1<<n):continue
                slot=before[o['slots']+n];pattern=d['refs'][key*16+n][0]
                if slot==255:
                    require(not pattern or d['fixed'][pattern]!=65535,'Unassigned dynamic reference')
                    completed|=1<<n;continue
                require(slot<165,'Slot out of pool')
                if owners[p][slot]==pattern:completed|=1<<n;continue
                if copies==r['patterns']:break
                for field in ('displayed_use','prepared_use'):
                    require(not (U32(before,o[field]+(slot//32)*4)>>(slot%32)&1),'Evicts protected slot')
                require(d['fixed'][pattern]==65535,'Streams a fixed pattern')
                require(transfers[pos:pos+32]==d['patterns'][pattern*32:(pattern+1)*32],'Transferred pattern bytes differ')
                pos+=32;copies+=1;owners[p][slot]=pattern;epochs[p]+=1
                put16(expected,slot*2,pattern);completed|=1<<n;cursor=n+1
                self.history[p][epochs[p]]=tuple(owners[p]);self.commit_times[p][epochs[p]]=r['master_cycles']
            require(copies==r['patterns'] and r['words']==16*copies and r['controls']==2*copies,'Incomplete pattern stores')
            put16(expected,o['completed'],completed);put16(expected,o['cursor'],cursor);put16(expected,o['phase'],1 if completed==request else 2)
            put32(expected,o['residency_epoch'],epochs[p]);put32(expected,o['prepared_residency_epoch'],0)
            require(bytes(expected)==s and r['residency_epoch']==epochs[p],'Committed cache differs from independent replay')
            if p==0:
                self.counts['batches']+=1;self.counts['patterns']+=copies
                self.counts['late_batches']+=max(r['master_cycles'],r['retire_upper_master'])>r['deadline_master']
        require(all(initialized) and not any(opened) and pos==len(transfers),'Incomplete/unexplained transfer data')
        require(self.history[0]==self.history[1],'Original/native cache commit history differs')

    def publication(self,sample,row,vr,resident):
        require(sample<len(self.rows),'Missing far publication certificate');r={k:int(v)for k,v in self.rows[sample].items()}
        require(r['sample']==sample and r['frame']==int(row['frame']),'Mismatched publication association')
        epoch=r['residency_epoch'];require(epoch in self.history[0],'Unobserved publication epoch')
        require(int(row['master_cycles'])>=self.commit_times[0][epoch],'Publication precedes complete tile commit')
        require(r['total_patterns']==epoch-1,'Publication transfer count differs from committed epoch')
        owners=self.history[0][epoch];state=self.states[sample*436:(sample+1)*436];o=self.offsets
        require(struct.unpack_from('>165H',state)==owners==struct.unpack_from('>165H',self.owner_records,sample*330),'Publication owner ledger differs')
        require(U32(state,o['residency_epoch'])==epoch,'Publication target epoch differs')
        if not int(row['cold_partial']):
            self.streamed_publications+=epoch>1
            require(U16(state,o['phase'])==1 and not U16(state,o['missing']) and U32(state,o['prepared_residency_epoch'])==epoch,'Unprepared far publication')
            for field,column in [('source_epoch','source_epoch'),('palette_epoch','palette'),('preparation_generation','generation')]:
                require(U32(state,o[field])==(int(row[column]) if column in row else r[field]),'Stale far certificate '+field)
            require([U16(state,o['view']+n*2)for n in range(4)]==[int(row[k])for k in ('s2x','s2y','s3x','s3y')],'Stale far view')
        self.current_epoch=epoch;self.current={pattern:tile for tile,pattern in zip(self.tiles,owners)}
        require(len(self.current)==165,'Duplicate committed pattern owners')
        for tile,pattern in zip(self.tiles,owners):
            raw=self.data['patterns'][pattern*32:(pattern+1)*32]
            require(vr[tile*32:(tile+1)*32]==raw,'Committed physical pool pattern differs')
            resident[tile]=raw

    def word(self,code,attr,sub,layer=3):
        keys=self.keys if layer==3 else self.near_keys
        key=(code<<16)|attr;require(key in keys,'Unprepared source scenery key')
        ref=keys[key]+sub;pattern=self.data['refs'][ref][0];tile=self.data['fixed'][pattern]
        if tile==65535:tile=self.current.get(pattern,65535)
        return 65535 if tile==65535 else tile|self.data['flags'][ref]

    def image(self,sample,expected,actual,cram,output):
        selection=self.run/'far-first-streamed-frame.json'
        if not selection.exists() or sample!=json.loads(selection.read_text())['background_sample']:return
        from PIL import Image
        colors=[]
        for color in struct.unpack('>64H',cram):
            rgb=[((color>>shift)&7)*34 for shift in (1,5,9)]
            colors.append(tuple((v>>(3 if n!=1 else 2))*255//(31 if n!=1 else 63)for n,v in enumerate(rgb)))
        for name,pixels in [('expected-source-low-planes',expected),('actual-vdp-low-planes',actual)]:
            im=Image.new('RGB',(320,224));im.putdata([colors[p]for p in pixels]);im.save(output/(name+'.png'))
        Image.open(self.run/'far-first-streamed-frame.ppm').save(output/'actual-core-frame.png')
        a=Image.open(output/'expected-source-low-planes.png');b=Image.open(output/'actual-core-frame.png')
        sheet=Image.new('RGB',(640,224));sheet.paste(a,(0,0));sheet.paste(b,(320,0));sheet.save(output/'source-background-and-actual-frame.png')
        self.images.append({'sample':sample,'scope':'Left: exact source-derived fitted low planes. Right: actual core complete frame includes sprites. No actor/priority/full-frame color equivalence claimed.'})


def verifier_source():
    text=Path(base.__file__).read_text()
    # Permit only completed paired prefixes/diagnostics, never arbitrary aborts.
    start=text.index('    if not strict_parity:\n');end=text.index("    for name, digest in manifest['inputs'].items():\n",start)
    text=text[:start]+'''    if not strict_parity:
        require(manifest['run']['exit_code'] in (1,3), 'Observer aborted or did not complete')
        register_failures=list(csv.DictReader((run/'register-failures.csv').open()));pin(run/'register-failures.csv')
        paired=list(csv.DictReader((run/'frames.csv').open()));pin(run/'frames.csv')
        if manifest['run']['exit_code']==3:
            stop=json.loads(pin(run/'coverage-stop.json'))
            require(stop['all_other_per_frame_source_and_vdp_checks_completed'] and len(paired)==stop['paired_frames_completed'],'Incomplete paired coverage prefix')
        else:
            require(manifest.get('diagnostic_register_failure_collection') and register_failures,'Unexplained failed run')
            require(json.loads(pin(run/'render-summary.json'))['paired_vdp_frames']==len(paired),'Incomplete diagnostic paired frames')
'''+text[end:]
    text=replace(text,"json.loads(pin(run/'background-summary.json'))['samples']","len(list(csv.DictReader((run/'background-publications.csv').open())))")
    text=replace(text,"json.loads(pin(run/'background-summary.json'))['paths'][0]['missing_active_publications']","None")
    text=replace(text,"    resident[0] = bytes(32)","    resident[0] = bytes(32)\n    dynamic=make_far(run,pin,manifest)")
    text=replace(text,'    def native_cell(name):','    def native_cell(name,epoch):')
    text=replace(text,"        rec = {name: src[at:at+size] for name,(at,size) in regions.items()}","        rec = {name: src[at:at+size] for name,(at,size) in regions.items()}\n        dynamic.publication(sample,row,vr,resident)")
    text=replace(text,'                    ref, colors = source_cell(layer,code,attr,sub)','                    ref, colors = source_cell(layer,code,attr,sub)\n                    if layer==3 or dynamic.near_keys:ref=dynamic.word(code,attr,sub,layer)')
    text=replace(text,'native_cell(name)[','native_cell(name,dynamic.current_epoch)[')
    text=replace(text,'        total_pixels += 320*224','        dynamic.image(sample,expected,actual,cram,output)\n        total_pixels += 320*224')
    text=replace(text,"    (output/'frames.json').write_text", "    report['far_cache']=dict(dynamic.counts,epochs=len(dynamic.history[0]),streamed_epoch_publications=dynamic.streamed_publications,images=dynamic.images)\n    require(len(dynamic.rows)==len(rows),'Extra/missing far publication states')\n    (output/'frames.json').write_text")
    return text

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--run',type=Path,required=True);p.add_argument('--assets',type=Path,required=True)
    p.add_argument('--resource',type=Path,default=ROOT/'build/native-background-far-stream/prepared-v1');p.add_argument('--canonical',type=Path,default=ROOT/'build/native-route-background-audit/checkpoint-v1/canonical-v1');p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();text=verifier_source();module=types.ModuleType('far_pixels');module.__file__=base.__file__
    module.make_far=lambda run,pin,manifest:FarOracle(run,a.resource,pin,manifest)
    exec(compile(text,base.__file__,'exec'),module.__dict__);report=module.verify(a.run,a.assets,a.canonical,a.output,True)
    frozen=a.output/'resolved-pixel-verifier.py';frozen.write_text(text)
    for path in [Path(__file__),frozen,ROOT/'tools/native_far_observer_resources.py']:report['inputs'][str(path.resolve())]=base.sha(path)
    report['scope']=__doc__;report['target_admitted']=False
    (a.output/'report.json').write_text(json.dumps(report,indent=2)+'\n')
    require(report['far_cache']['patterns']>0,'No streamed patterns tested')
    if not report['strict_source_observer_completed']:raise SystemExit(1)
if __name__=='__main__':main()
