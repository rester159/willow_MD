#!/usr/bin/env python3
"""Strict, read-only live far-cache extension of the existing forest observer.

Only generated observer files change. Dynamic writes require immutable WBF1
bytes, a stable queued plan, exact complete transfers and committed ownership.
"""
from pathlib import Path
import argparse,json,re,struct,sys,types
import test_native_forest_extension as forest
from test_native_background_columns_route import exception_provenance
from native_irq_pointer_boundary import extend as immediate_exception_provenance
from native_far_observer_resources import load_resources,add_shared_resources,emit_header,sha
ROOT=Path(__file__).resolve().parents[1]
DIAGNOSE_SLEEP='--diagnose-sleep' in sys.argv
if DIAGNOSE_SLEEP:sys.argv.remove('--diagnose-sleep')
replace=forest.background.cadence.replace

def spills(platform,target,text,out):
    origin=0x100010;original=(platform/'platform-original.bin').read_bytes();native=(platform/'platform.bin').read_bytes()
    begin=target['symbols']['nativeBackgroundPrepare'];end=min(v for v in target['symbols'].values() if v>begin)
    assert original[begin-origin:begin-origin+2]==b'\x4e\x56'
    frame=struct.unpack_from('>h',original,begin-origin+2)[0];assert -1024<=frame<0
    sites=[]
    for source,dest in [(0x914000,0x208000),(0x918000,0x204000)]:
        prefix=b'\x2d\x7c'+struct.pack('>I',source)
        positions=[at for at in range(begin,end-7,2)if original[at-origin:at-origin+6]==prefix]
        assert len(positions)==1,('Unreviewed immediate spill',positions)
        at=positions[0];data=original[at-origin:at-origin+8];disp=struct.unpack_from('>h',data,6)[0]
        assert frame<=disp<=-4 and not disp&1
        assert native[at-origin:at-origin+8]==b'\x2d\x7c'+struct.pack('>Ih',dest,disp)
        sites.append({'pc':at,'size':8,'source':source,'native':dest,'displacement':disp,'original_opcode':data.hex()})
    insertion=''.join('{%du,8u,1u,16u,%d,0,0u},\n'%(d['pc'],d['displacement'])for d in sites)
    text=replace(text,'static const RenderPointerStoreSite renderPointerStoreSites[]={\n','static const RenderPointerStoreSite renderPointerStoreSites[]={\n'+insertion)
    (out/'far-pointer-sites.json').write_text(json.dumps(sites,indent=2)+'\n');return text

def bind(platform,target,text,out,manifest):
    resource=next(Path(n).parent for n in target['resource_files']if n.endswith('/far-stream.bin'))
    data=load_resources(resource)
    extra=(platform/target['extra_rom']['file']).read_bytes();origin=target['extra_rom']['origin'];at=data['manifest']['origin']-origin
    assert at>=0 and extra[at:at+len(data['blob'])]==data['blob']
    assert target['resource_files'][str(resource/'far-stream.bin')]==sha(resource/'far-stream.bin')
    definitions=(platform/'render-addresses.h').read_text();assert '#define RENDER_BACKGROUND_FAR_STREAM 1' in definitions
    values={n:int(v.rstrip('uUlL'),0)for n,v in re.findall(r'^#define (RENDER_BG_FAR_\w+) (0x[0-9a-fA-F]+[uUlL]*|\d+[uUlL]*)$',definitions,re.M)}
    assert values['RENDER_BG_FAR_CACHE_BYTES']==436 and values['RENDER_BG_FAR_PREPARED_WORD_COUNT']==2048
    assert origin<=values['RENDER_BG_FAR_CODE_BEGIN']<values['RENDER_BG_FAR_CODE_END']<=data['manifest']['origin']
    assert values['RENDER_BG_FAR_RESOURCE_BEGIN']==data['manifest']['origin'] and values['RENDER_BG_FAR_RESOURCE_END']==data['manifest']['origin']+len(data['blob'])
    if target.get('background_shared_stream'):
        add_shared_resources(data,Path(target['background_shared_stream']['directory']))
        sb=data['shared'];at=sb['manifest']['origin']-origin
        assert extra[at:at+len(sb['blob'])]==sb['blob']
    emit_header(data,out/'far-oracle-assets.h')
    own=out/'frozen-observer-tools';header=own/'test_native_background_far_publication.h';header.write_bytes((ROOT/'tools/test_native_background_far_publication.h').read_bytes())
    if target.get('background_shared_stream'):
        # Native surface ABI: use the compiler-derived far word address and
        # its equal-sized preceding surface; size is pinned in build metadata.
        near_words=target['symbols']['bg']+20
        assert values['RENDER_BG_FAR_PREPARED_WORDS']-near_words==4182
        header.write_text('#define FAR_SHARED_NEAR_WORDS '+hex(near_words)+'u\n'+header.read_text())
    bg=own/'test_native_background_publication.h';exception_provenance(bg)
    auxiliary_movem=None
    if 'bgFarClearRow' in target['symbols']:
        entry=target['symbols']['bgFarClearRow']
        assert values['RENDER_BG_FAR_CODE_BEGIN']<=entry and entry+4<=values['RENDER_BG_FAR_CODE_END']
        opcode=extra[entry-origin:entry-origin+4]
        assert len(opcode)==4 and opcode[:2]==bytes.fromhex('48e7')
        auxiliary_movem=(entry,int.from_bytes(opcode,'big'))
    additional_movem=None
    if target.get('background_shared_stream'):
        entry=target['symbols']['bgFarNeeded']
        code=extra[entry-origin:entry-origin+8]
        assert values['RENDER_BG_FAR_CODE_BEGIN']<=entry and entry+8<=values['RENDER_BG_FAR_CODE_END']
        assert code[:4]==bytes.fromhex('4fefffe0') and code[4:6]==bytes.fromhex('48e7')
        additional_movem=(entry+4,int.from_bytes(code[4:],'big'),int.from_bytes(code[:4],'big'))
    lookup_movem=None
    if target.get('background_shared_stream'):
        entry=target['symbols']['willowFarFind.constprop.0']
        code=extra[entry-origin:entry-origin+4]
        assert values['RENDER_BG_FAR_CODE_BEGIN']<=entry and entry+4<=values['RENDER_BG_FAR_CODE_END']
        assert code==bytes.fromhex('48e73830')
        lookup_movem=(entry,int.from_bytes(code,'big'))
    finish_movem=None
    if target.get('background_shared_stream'):
        entry=target['symbols']['bgFarFinish'];code=extra[entry-origin:entry-origin+8]
        if code[:4]==bytes.fromhex('4fefffec') and code[4:6]==bytes.fromhex('48e7'):
            assert values['RENDER_BG_FAR_CODE_BEGIN']<=entry and entry+8<=values['RENDER_BG_FAR_CODE_END']
            finish_movem=(entry+4,int.from_bytes(code[4:],'big'),int.from_bytes(code[:4],'big'))
    immediate_exception_provenance(bg,replace,auxiliary_movem,additional_movem,lookup_movem,finish_movem)
    manifest['pointer_irq_boundaries']={'direct_or_branch_join':'exact declared pointer store, actual A6 EA and all4 prior word-store provenance bytes; branch join additionally requires identical BRA.W to savedPC','auxiliary_movem':auxiliary_movem,'additional_movem':additional_movem,'lookup_movem':lookup_movem,'finish_movem':finish_movem,'hardware_frame_and_actual_irq_proof_unchanged':True}
    b=bg.read_text()
    b=replace(b,' return pc>=RENDER_BG_CODE_BEGIN&&pc<RENDER_BG_CODE_END;',' return (pc>=RENDER_BG_CODE_BEGIN&&pc<RENDER_BG_CODE_END)||farCodePC(pc);')
    b=replace(b,' unsigned phase=encoded&255,status=encoded>>8;', ' unsigned phase=encoded&255,status=encoded>>8;') if False else b
    needle=' BackgroundObserverPath*p=backgroundPaths+renderPath;unsigned phase=encoded&255,status=encoded>>8;'
    b=replace(b,needle,' if(farPhase(encoded))return;\n'+needle)
    b=replace(b,'  backgroundPublishValid();backgroundCheckView(&end,1);','  backgroundPublishValid();backgroundCheckView(&end,1);farValidatePublication(&end);')
    b=replace(b,'static unsigned backgroundVDPBefore(unsigned at,unsigned width,unsigned value){','static unsigned backgroundVDPBefore(unsigned at,unsigned width,unsigned value){\n if(farOpen())return farVDPBefore(at,width,value);')
    b=replace(b,'static void backgroundVDPAfter(unsigned at,unsigned value){','static void backgroundVDPAfter(unsigned at,unsigned value){\n if(farOpen()){farVDPAfter(at,value);return;}')
    b=replace(b,'static void backgroundSnapshot(const BackgroundSpan*s,uint64_t deadline){','static void backgroundSnapshot(const BackgroundSpan*s,uint64_t deadline){\n farSnapshot(backgroundSamples);')
    b=replace(b,'renderVRAM[n^1]!=backgroundExpectedPatterns[n]','renderVRAM[n^1]!=farPatternByte(n)')
    b=replace(b,' p->checkedFrames++;return 0;',' farObserveFrame(path);p->checkedFrames++;return 0;')
    b=replace(b,'if(p->publish.open)backgroundFail("original IRQ returned with open BG publication")','if(p->publish.open||farOpen())backgroundFail("original IRQ returned with open BG/cache publication")')
    bg.write_text(b)
    # Prototypes precede the existing BG header; implementation follows it.
    decl='''static unsigned farCodePC(unsigned),farPhase(unsigned),farOpen(void),farVDPBefore(unsigned,unsigned,unsigned),farPatternByte(unsigned);
static void farVDPAfter(unsigned,unsigned),farSnapshot(unsigned),farObserveFrame(unsigned);
'''
    # BackgroundSpan is declared by the existing header, so use its concrete
    # prototype immediately after that declaration inside the copied header.
    b=bg.read_text();needle='static unsigned backgroundSamples;'
    b=replace(b,needle,needle+'\nstatic void farValidatePublication(const BackgroundSpan*);');bg.write_text(b)
    text=replace(text,'#include "'+str(bg)+'"',decl+'#include "'+str(bg)+'"\n#include "'+str(out/'far-oracle-assets.h')+'"\n#define RENDER_BG_FAR_SOURCE_EPOCH '+hex(target['symbols']['willowNativeBackgroundColumnsState'])+'u\n#include "'+str(header)+'"')
    text=replace(text,' if(!backgroundInit(core))return 2;',' if(!backgroundInit(core))return 2;\n if(!farInit())return 2;')
    text=replace(text,'static bool env(unsigned cmd,void *data){','static unsigned farPixelFormat;\nstatic void farVideo(const void*,unsigned,unsigned,size_t);\nstatic bool env(unsigned cmd,void *data){\n if(cmd==RETRO_ENVIRONMENT_SET_PIXEL_FORMAT)farPixelFormat=*(unsigned*)data;')
    text=replace(text,'static void video(const void*d,unsigned w,unsigned h,size_t p){(void)d;(void)w;(void)h;(void)p;}','static void video(const void*d,unsigned w,unsigned h,size_t p){farVideo(d,w,h,p);}')
    # No FFFF static-word residency stop for a WBF1-cacheable canonical pattern.
    # Unknown source keys and all other genuine source resource refusals remain.
    fh=own/'test_native_forest_extension.h';f=fh.read_text()
    f=replace(f,'if(!publishing&&kind&&status==11&&farMissing)unsupported=1;', 'if(!publishing&&kind&&status==11&&farMissing&&farPattern>=3512)unsupported=1;')
    fh.write_text(f)
    text=replace(text,'Unverified context-store layout PC%x addr%x word%x reg%u value%x index%u\\n",pc,at,v,reg,value,index)',
        'Unverified context-store layout PC%x addr%x word%x reg%u value%x index%u actualPC%x A6%x A7%x\\n",pc,at,v,reg,value,index,cpu->pc,cpu->dar[14],cpu->dar[15])')
    text=replace(text,'fprintf(stderr,"Invalid or duplicated committed gameplay sleep\\n");',
        'fprintf(stderr,"Invalid or duplicated committed gameplay sleep task%x state%x begin%llu commit%llu stamp%llu frame%u usp%x\\n",task,rawWord(task),(unsigned long long)waitBegin,(unsigned long long)waitCommit,(unsigned long long)stamp(),frameNumber,usp);')
    if DIAGNOSE_SLEEP:
        text=replace(text,'static void headroomAfterWrite(unsigned at,unsigned width,unsigned value){',
            'static void headroomAfterWrite(unsigned at,unsigned width,unsigned value){\n if(frameNumber>=1400&&frameNumber<=1403&&at<0x62&&at+width>0x60)fprintf(stderr,"TASKSTATE frame%u pc%x at%x width%u value%x now%x begin%llu commit%llu\\n",frameNumber,sourcePC(),at,width,value,rawWord(0x60),(unsigned long long)waitBegin,(unsigned long long)waitCommit);')
    if '--diagnose-cache' in sys.argv:
        text=replace(text,'static void headroomBeforeWrite(unsigned at,unsigned width,unsigned value){',
            'static void headroomBeforeWrite(unsigned at,unsigned width,unsigned value){\n if((at&65535)<((RENDER_BG_FAR_CACHE_BASE&65535)+RENDER_BG_FAR_CACHE_PREPARED_RESIDENCY_EPOCH_OFFSET+4)&&((at&65535)+width)>((RENDER_BG_FAR_CACHE_BASE&65535)+RENDER_BG_FAR_CACHE_PHASE_OFFSET))fprintf(stderr,"FARWRITE frame%u PC%x at%x width%u value%x old%x\\n",frameNumber,sourcePC(),at,width,value,rawWord(at));')
    text=replace(text,'Invalid source wait context or overwritten native IRQ observations\\n',
       'Invalid source wait context or overwritten native IRQ observations\\n')
    text=replace(text,'fprintf(stderr,"Invalid source wait context or overwritten native IRQ observations\\n");return 1;',
       'fprintf(stderr,"Invalid source wait context or overwritten native IRQ observations: frame%u pc%x wait=%u latchfail=%u observed=%u captured=%u irq=%u prior=%u\\n",frameNumber,sourcePC(),willowMotionLong(sram+0xf024),willowMotionLong(sram+0xf030),willowMotionLong(sram+0xf028),latchCaptures,irq,lastRecordedIRQ);return 1;')
    # A physical frame may end after a complete scatter chunk, before the
    # routine commits its diagnostic acquisition count. Pin this exact loop
    # boundary in both target images, keep partial bytes fully compared, and
    # require completion at the next physical frame boundary.
    original=(platform/'platform-original.bin').read_bytes();native=(platform/'platform.bin').read_bytes()
    matches=[i for i in range(0,len(original)-4,2)if original[i:i+4]==bytes.fromhex('b5c166cc')]
    assert len(matches)==1 and native[matches[0]:matches[0]+4]==original[matches[0]:matches[0]+4]
    boundary=matches[0]+0x100010
    table=struct.pack('>32H',*[0x2660+n*128 for n in range(32)])
    tables=[i for i in range(matches[0],min(matches[0]+1024,len(original)-64),2)if original[i:i+64]==table]
    assert len(tables)==1 and native[tables[0]:tables[0]+64]==table
    table_address=tables[0]+0x100010
    text=replace(text,'static unsigned frameNumber;','static unsigned frameNumber;\nstatic unsigned farLatchCounterCurrent(unsigned,const unsigned*,unsigned);\nstatic uint64_t farLatchBeginMaster;')
    text=replace(text,'  memcpy(expectedScatteredLatch,independentLatch,latchExpectedWords*2);','  /* Expected partial shadow advances only with individually verified reference stores. */')
    text=replace(text,'  latchSeenWords[word]=1;latchWords++;','  willowMotionPutWord(expectedScatteredLatch+latchAt,v);\n  latchSeenWords[word]=1;latchWords++;')
    text=replace(text,'latchCaptures++;captureCycle=(unsigned)cpu->cycles;','latchCaptures++;captureCycle=(unsigned)cpu->cycles;farLatchBeginMaster=stamp();')
    text=replace(text,'unsigned cycles=(unsigned)cpu->cycles-captureCycle;','unsigned cycles=(unsigned)(stamp()-farLatchBeginMaster);')
    text=replace(text,'willowMotionLong(sram+0xf028)!=latchCaptures','!farLatchCounterCurrent(willowMotionLong(sram+0xf028),originalRegs,m68k_get_reg(16))')
    text=replace(text,'#include "'+str(header)+'"','#define FAR_LATCH_CHUNK_TABLE '+hex(table_address)+'u\n#define FAR_LATCH_LOOP_BOUNDARY '+hex(boundary)+'u\n#include "'+str(header)+'"')
    manifest['inflight_latch_boundary']={'pc':boundary,'chunk_table':table_address,'opcode':'b5c166cc','maximum_deferred_physical_frames':1,'all_scattered_bytes_compared':True,'whole_prefix_preassignment_removed':True}
    # A fully acquired tuple may be interrupted at any verified instruction
    # boundary in the following branch-free descriptor stores. Earlier/mixed
    # reads and a second IRQ still fail the unchanged tuple oracle.
    sites=out/'forest-view-acquisition-sites.h';site_text=sites.read_text()
    get=lambda name:int(re.search(r'#define '+name+r' (0x[0-9a-f]+)u',site_text).group(1),0)
    post_begin=get('FOREST_VIEW_ACQUISITION_END');post_end=get('FOREST_VIEW_MARKER_NEXT')
    post=original[post_begin-0x100010:post_end-0x100010]
    assert native[post_begin-0x100010:post_end-0x100010]==post
    forms=[(0x2079,6),(0x5288,2),(0x23c8,6),(0x23c8,6),(0x23f9,10),(0x33c0,6),(0x33ee,8),(0x33ee,8),(0x33ee,8),(0x33fc,8),(0x33fc,8)]
    if struct.unpack_from('>H',post)[0]==0x2039:
        # Masked camera copy retains SR in D0 and all four words on the stack.
        # After restoring SR, the compiler uses D0 for generation and emits
        # four direct stack-to-descriptor stores; still no branches or calls.
        assert '#define FOREST_VIEW_MOVEM 1' in site_text or original[post_begin-0x100010-2:post_begin-0x100010]==bytes.fromhex('46c0')
        forms=[(0x2039,6),(0x5280,2),(0x23c0,6),(0x23c0,6),(0x23f9,10)]+[(0x33ee,8)]*4+[(0x33fc,8)]*2
    cursor=0;boundaries=[]
    if '#define FOREST_VIEW_MOVEM 1' in site_text:
        # MOVEM already completed all four reads. Interrupts during the
        # following exact register-to-local stores preserve that same tuple.
        boundaries=[get('FOREST_VIEW_LOAD_BEGIN')+n for n in (8,10,14,18)]
    for opcode,size in forms:
        assert struct.unpack_from('>H',post,cursor)[0]==opcode,('Unknown post-read sequence',hex(post_begin+cursor))
        boundaries.append(post_begin+cursor);cursor+=size
    assert cursor==len(post)
    sites.write_text(site_text+'\nstatic const uint8_t farViewPostReadCode[]={'+','.join(map(str,post))+'};\nstatic unsigned farViewPostReadPC(unsigned pc){\n static const unsigned boundaries[]={'+','.join(hex(v)+'u' for v in boundaries)+'};\n for(unsigned n=0;n<sizeof boundaries/sizeof*boundaries;n++)if(pc==boundaries[n])return 1;return 0;\n}\n')
    view_header=own/'test_native_forest_view_acquisition.h';v=view_header.read_text()
    v=replace(v,'saved!=FOREST_VIEW_ACQUISITION_END','!farViewPostReadPC(saved)')
    v=replace(v,'  static const unsigned sites[]={0x199e,0x1a1c,0x19b0,0x1a30};','  if(memcmp(rom+FOREST_VIEW_ACQUISITION_END,farViewPostReadCode,sizeof farViewPostReadCode))forestViewFailure("runtime post-read descriptor code differs");\n  static const unsigned sites[]={0x199e,0x1a1c,0x19b0,0x1a30};')
    view_header.write_text(v)
    manifest['coherent_view_irq_boundaries']={'begin':post_begin,'end':post_end,'instruction_starts':boundaries,'bytes':post.hex(),'all_four_original_reads_required':True,'one_original_irq_only':True,'all_source_staging_writes_and_published_ports_checked':True}
    text=spills(platform,target,text,out)
    manifest['inputs'].update(data['pins'])
    for path in [header,bg,fh,out/'far-oracle-assets.h',out/'far-pointer-sites.json',Path(__file__),ROOT/'tools/native_far_observer_resources.py',ROOT/'tools/native_irq_pointer_boundary.py',ROOT/'tools/test_native_background_columns_route.py']:
        frozen=own/path.name if path.parent==ROOT/'tools' else path
        if frozen!=path:frozen.write_bytes(path.read_bytes())
        manifest['inputs'][str(frozen.resolve())]=sha(frozen)
    manifest['far_cache_observation']={'resource':str(resource),'shared_resource':data.get('shared',{}).get('directory'),'state_bytes':436,'owner_bytes':330,'exact_slots':[list(range(759,876)),list(range(1481,1529))],'metadata':values,'no_host_target_writes':True,'phase9':'stable queued plan before VDP','phase10':'complete tile count and committed metadata','phase8':'deferred; IDLE+missing is request refusal, QUEUED is upload wait; neither a publication','phase11':'exact post-source-RAM-clear owner initialization, once only','static_regions_remain_exact':True}
    return text

def main():
    build_only='--build-only' in sys.argv
    if build_only:sys.argv.remove('--build-only')
    original=ROOT/'tools/test_native_forest_extension.py';text=original.read_text()
    before="    module=types.ModuleType('forest_background_builder');module.__file__=str(builder_path)"
    after='''    builder_text=background.cadence.replace(builder_text,
        "  data=original.read_bytes();assert hashlib.sha256(data).hexdigest()==digest,('Target source changed before snapshot',name)",
        "  frozen=platform/'frozen-sources'/original.relative_to(ROOT)\\n  data=frozen.read_bytes();assert hashlib.sha256(data).hexdigest()==digest,('Target frozen source changed before snapshot',name)")
'''+before
    text=replace(text,before,after)
    before="    generated.write_text(text);subprocess.run(manifest['command'],cwd=ROOT,check=True)"
    text=replace(text,before,'    text=far_bind(platform,target,text,out,manifest)\n'+before)
    if build_only:
        text=replace(text,'    assert background.sha(a.core)==background.CORE_SHA',"    (out/'observer-manifest.json').write_text(json.dumps(manifest,indent=2)+'\\n')\n    print('BUILD ONLY',binary);return\n    assert background.sha(a.core)==background.CORE_SHA")
    if '--diagnose-cache' in sys.argv:
        diagnostic=True;sys.argv.remove('--diagnose-cache')
    else:diagnostic=False
    def selected_bind(*args):
        if diagnostic:sys.argv.append('--diagnose-cache')
        try:return bind(*args)
        finally:
            if diagnostic:sys.argv.remove('--diagnose-cache')
    module=types.ModuleType('far_forest_observer');module.__file__=str(original);module.far_bind=selected_bind
    exec(compile(text,str(original),'exec'),module.__dict__);module.main()
if __name__=='__main__':main()
