#!/usr/bin/env python3
"""Parse pinned WBF1 resources for read-only dynamic far-cache observation."""
from pathlib import Path
import hashlib,json,struct
ROOT=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def load_resources(resource,pin=None):
    resource=Path(resource).resolve();pins={}
    def read(p,digest=None):
        p=Path(p).resolve();h=sha(p);assert digest is None or h==digest,('Changed far resource',p)
        pins[str(p)]=h
        if pin:pin(p,digest)
        return p.read_bytes()
    m=json.loads(read(resource/'manifest.json'))
    for n,h in m['inputs'].items():read(Path(n)if Path(n).is_absolute()else ROOT/n,h)
    for n,h in m['outputs'].items():read(resource/n,h)
    blob=read(resource/'far-stream.bin');layout=json.loads(read(resource/'layout.json'))
    fields=dict(zip(layout['header_fields'],struct.unpack_from('>IHH6I4H6I',blob)))
    assert fields['magic']==0x57424631 and fields['version']==1 and fields['headerBytes']==64
    assert fields['totalBytes']==len(blob)==m['bytes'] and fields['flags']==1
    assert fields['patternCount']==m['pattern_count']==3512 and fields['poolCount']==165
    assert fields['farPairCount']==248 and fields['farReferenceCount']==3968
    catalog_path=next(Path(n)for n in m['inputs']if n.endswith('/catalog.bin'))
    catalog=read(catalog_path,m['inputs'][str(catalog_path)])
    assert len(catalog)==fields['catalogBytes'] and hashlib.sha256(catalog).digest()==blob[64:96]
    regions={v['name']:v for v in layout['regions']}
    def region(name):
        d=regions[name];return blob[d['offset']:d['offset']+d['bytes']]
    patterns=region('patterns');attrs=region('attributes');pool=list(struct.iter_unpack('>HH',region('pool')));fixed=list(struct.unpack('>3512H',region('fixed_map')))
    refs=list(struct.iter_unpack('>HH',catalog[fields['catalogFarRefsOffset']:fields['catalogFarRefsOffset']+3968*4]))
    keys=list(struct.iter_unpack('>IH',catalog[fields['catalogFarPairsOffset']:fields['catalogFarPairsOffset']+248*6]))
    assert [t for t,p in pool]==list(range(759,876))+list(range(1481,1529))
    assert all(0<p<3512 and fixed[p]==65535 for t,p in pool) and len({p for t,p in pool})==165
    assert fixed[0]==0 and patterns[:32]==bytes(32)
    assert [o for k,o in keys]==list(range(0,3968,16)) and [k for k,o in keys]==sorted({k for k,o in keys})
    flags=[((attrs[i>>1]>>(0 if i&1 else 4))&15)<<11 for i in range(3968)]
    for (pattern,word),flag in zip(refs,flags):assert pattern<3512 and not flag&0x8000 and (word==65535 or word&0xf800==flag)
    return dict(manifest=m,layout=layout,fields=fields,blob=blob,patterns=patterns,attributes=attrs,flags=flags,pool=pool,fixed=fixed,refs=refs,keys=keys,pins=pins)
def add_shared_resources(data,resource):
    from verify_native_background_shared_stream import verify
    resource=Path(resource).resolve();verify(resource)
    m=json.loads((resource/'manifest.json').read_text());blob=(resource/'shared-stream.bin').read_bytes()
    _,_,_,*fields=struct.unpack_from('>IHH18I',blob)
    total,origin,base,basebytes,catbase,catbytes,nf,nn,nr,bind,bindbytes,refs,attrs,keys,patterns,fixed,pool,np=fields
    assert base==data['manifest']['origin'] and basebytes==len(data['blob'])
    assert blob[bind:bind+32]==hashlib.sha256(data['blob']).digest()
    data['refs']=list(struct.iter_unpack('>HH',blob[refs:refs+nr*4]))
    data['attributes']=blob[attrs:attrs+nr//2]
    data['flags']=[((data['attributes'][i>>1]>>(0 if i&1 else 4))&15)<<11 for i in range(nr)]
    data['near_keys']=[]
    for i in range(nf,nf+nn):
        layer,code,attr,mask=struct.unpack_from('>4H',blob,keys+i*8)
        assert layer==2 and mask==15
        data['near_keys'].append(((code<<16)|attr,i*16))
    data['shared']=dict(manifest=m,blob=blob,directory=str(resource))
    for name,digest in m['inputs'].items():data['pins'][name]=digest
    for name,digest in m['outputs'].items():data['pins'][str(resource/name)]=digest
    data['pins'][str(resource/'manifest.json')]=sha(resource/'manifest.json')
    return data
def emit_header(data,path):
    values={'farOraclePatterns':(8,data['patterns']),'farOraclePool':(16,[x for row in data['pool']for x in row]),'farOracleFixed':(16,data['fixed']),'farOracleReferences':(16,[p for p,w in data['refs']]),'farOracleAttributes':(16,data['flags'])}
    text='/* Immutable host-only WBF1 oracle, never target data or captured geometry. */\n'
    text+='#define FAR_ORACLE_REFERENCE_COUNT %du\n#define FAR_ORACLE_KEY_COUNT %du\n'%(len(data['refs']),len(data['refs'])//16)
    for name,(bits,items)in values.items():
        text+='static const uint%d_t %s[]={\n'%(bits,name)
        text+='\n'.join(','.join(str(x)for x in items[n:n+32])+',' for n in range(0,len(items),32))+'\n};\n'
    Path(path).write_text(text)
