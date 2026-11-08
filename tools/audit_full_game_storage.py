#!/usr/bin/env python3
"""Full-ROM content storage experiment, NOT a finished native game fit proof.

Enumerates every source chip against MAME; includes every physical graphics bit
and every sample byte rather than relying on incomplete reachable-pose lists.
Native code and extracted tables are explicit reservations, not invented files.
"""
from pathlib import Path
import argparse, collections, hashlib, json, re, struct, sys, zipfile, zlib
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from cps1_gfx import assemble_region, decode_tile, genesis_tiles
from cps1_program import assemble_maincpu

def sha(b): return hashlib.sha256(b).hexdigest()

def compress(b):
    history = collections.defaultdict(collections.deque)
    out = bytearray(); at = 0
    while at < len(b):
        flag_at = len(out); out.append(0); flags = 0
        for bit in range(8):
            if at == len(b): break
            length = 0; distance = 0
            if at + 3 <= len(b):
                candidates = history[b[at:at+3]]
                while candidates and at-candidates[0] > 4096: candidates.popleft()
                for pos in reversed(candidates):
                    n = 3
                    while n < 18 and at+n < len(b) and b[pos+n] == b[at+n]: n += 1
                    if n > length: length, distance = n, at-pos
                    if length == 18: break
            advance = length if length >= 3 else 1
            if length >= 3: out.extend(struct.pack('>H', ((distance-1)<<4)|(length-3)))
            else: flags |= 1<<bit; out.append(b[at])
            for pos in range(at, at+advance):
                if pos+3 <= len(b):
                    q = history[b[pos:pos+3]]; q.append(pos)
                    if len(q) > 48: q.popleft()
            at += advance
        out[flag_at] = flags
    return bytes(out)

def decode(b, count):
    out = bytearray(); at = 0
    while len(out) < count:
        flags = b[at]; at += 1
        for bit in range(8):
            if len(out) == count: break
            if flags & (1<<bit): out.append(b[at]); at += 1
            else:
                token = int.from_bytes(b[at:at+2], 'big'); at += 2
                distance, length = (token>>4)+1, (token&15)+3
                assert distance <= len(out) and len(out)+length <= count
                for _ in range(length): out.append(out[-distance])
    assert at == len(b)
    return bytes(out)

def build(out, mame, codec, block_bytes):
    out.mkdir(parents=True, exist_ok=False)
    arcade = ROOT/'willow.zip'
    source = mame.read_text()
    match = re.search(r'ROM_START\( willowu \)(.*?)ROM_END', source, re.S)
    assert match, 'missing exact USA ROM-set definition'
    chips = re.findall(r'ROM_LOAD\w*\(\s*"([^"]+)".*?SHA1\(([0-9a-f]+)\)', match[1])
    ledger = []; inputs = {str(arcade):sha(arcade.read_bytes()),str(mame):sha(mame.read_bytes())}
    with zipfile.ZipFile(arcade) as z:
        assert set(z.namelist()) == {n for n,h in chips}, 'unclassified or missing chips'
        for name, digest in chips:
            b = z.read(name); assert hashlib.sha1(b).hexdigest() == digest, name
            kind = ('board_logic' if len(b)==279 else 'sound_program_and_data' if name=='wl_09.12b'
                    else 'samples' if name in ('wl_18.11c','wl_19.12c')
                    else 'main_program_and_data' if name in ('wlu_30.11f','35.11h','wlu_31.12f','wlu_36.12h','wlm-32.8h')
                    else 'graphics')
            ledger.append(dict(name=name,bytes=len(b),sha1=digest,sha256=sha(b),category=kind))
        gfx = assemble_region(z)
        adpcm = z.read('wl_18.11c')+z.read('wl_19.12c')
        sound = z.read('wl_09.12b')
    # Fresh decode of the ENTIRE physical region: includes all stages, actors,
    # unused art, UI, endings and the unpopulated last MiB. No capture filtering.
    print('Decoding every physical graphics tile...', flush=True)
    raw = b''.join(genesis_tiles(decode_tile(gfx,code,16)) for code in range(32768))
    assert raw == (ROOT/'build/willow_genesis_tiles.bin').read_bytes()
    # Inverse bit permutation independently verifies every physical graphics byte.
    restored = bytearray(len(gfx))
    for code in range(32768):
        for y in range(16):
            for x in range(16):
                off=code*128+((y//8)*2+x//8)*32+(y%8)*4+(x%8)//2
                pen=(raw[off]>>(0 if x&1 else 4))&15
                base=code*128+y*8+(x//8)*4
                for plane in range(4): restored[base+plane] |= ((pen>>plane)&1)<<(7-x%8)
    assert restored == gfx
    remap=bytes((((b>>4)+1)&15)<<4|((b+1)&15) for b in range(256))
    streams = {'all_physical_gfx_native16': raw.translate(remap)}
    for name,size,first,count,layout in [('scroll1_left',8,0x7000,4096,0),('scroll1_right',8,0x7000,4096,1),('scroll3',32,0xa00,1024,0)]:
        b=b''.join(genesis_tiles(decode_tile(gfx,c,size,layout)) for c in range(first,first+count))
        old=ROOT/'build'/('willow_'+name+('_tiles' if name=='scroll3' else '')+'.bin')
        assert b==old.read_bytes()
        streams[name]=b.translate(remap)
    # Scroll2 exactly aliases the full 16x16 stream; no extra payload required.
    assert raw[0x200000:0x300000] == (ROOT/'build/willow_scroll2_tiles.bin').read_bytes()
    streams['all_original_adpcm']=adpcm
    # Keep the already prepared, directly streamable audio in addition to every
    # original ADPCM byte. Thus no storage claim depends on a new audio decoder.
    pcm_path=ROOT/'build/oki_wav/pcm4_oki_16k_signed8.bin'; pcm=pcm_path.read_bytes()
    index_path=pcm_path.parent/'index.json'; index=json.loads(index_path.read_text())
    assert len(pcm)==0x100000
    valid={r['id'] for r in index['samples'] if 'file' in r}
    assert valid=={r['sample_id'] for r in index['pcm4_pcm_samples']}
    for r in index['pcm4_pcm_samples']+index['pcm4_pcm_attenuated_variants']:
        assert r['offset']%256==0 and r['offset']+r['padded_size']<=len(pcm)
    inputs[str(pcm_path)]=sha(pcm);inputs[str(index_path)]=sha(index_path.read_bytes())
    records=[]; data=bytearray(); logical=bytearray(); summaries=[]
    for stream,b in streams.items():
        start=len(data); first_record=len(records)
        for at in range(0,len(b),block_bytes):
            block=b[at:at+block_bytes]
            if codec=='deflate':
                compressor=zlib.compressobj(9,zlib.DEFLATED,-12)
                packed=compressor.compress(block)+compressor.flush()
            else: packed=compress(block)
            is_raw=len(packed)>=len(block)
            if is_raw: packed=block
            assert (packed if is_raw else zlib.decompress(packed,-12) if codec=='deflate' else decode(packed,len(block)))==block
            # Align to words so the hardware pack can also support DMA staging.
            if len(data)&1:data.append(0)
            records.append((len(data),len(packed),len(block),int(is_raw),len(logical)))
            data.extend(packed); logical.extend(block)
        summaries.append(dict(name=stream,logical_bytes=len(b),stored_payload_with_alignment=len(data)-start,
            block_first=first_record,block_count=len(records)-first_record,sha256=sha(b)))
        print(stream,len(b),'->',len(data)-start,flush=True)
    header_bytes=16+16*len(records)
    pack=bytearray(struct.pack('>4sIII',b'WST2' if codec=='deflate' else b'WST1',len(records),len(logical),header_bytes))
    for offset,stored,decoded,raw_flag,logical_at in records:
        pack.extend(struct.pack('>IHHII',header_bytes+offset,stored,decoded,raw_flag,logical_at))
    pack.extend(data)
    (out/'content.pack').write_bytes(pack);(out/'expected-decoded.bin').write_bytes(logical)
    (out/'pcm.bin').write_bytes(pcm)
    main=assemble_maincpu(arcade)
    # Full original CPU region sizes are deliberately conservative upper bounds
    # for unexpanded extracted data. They are RESERVED, never packed as CPU ROMs.
    table_reserve=len(main)+len(sound)
    native_code_reserve=0x100000  # a capacity allocation, NOT completed code
    used=native_code_reserve+table_reserve+len(pcm)+len(pack)
    padded=(used+0x1ffff)&~0x1ffff
    result=dict(scope=__doc__,source_set='willowu',codec=codec,zlib_version=zlib.ZLIB_RUNTIME_VERSION,source_chips=ledger,
        physical_graphics_bytes_checked=len(gfx),native_pixel_values_checked=len(gfx)*2,
        streams=summaries,pack_bytes=len(pack),block_directory_bytes=header_bytes,blocks=len(records),
        maximum_decoder_output_bytes=block_bytes,pcm_bytes=len(pcm),pcm_sample_count=len(valid),
        extracted_main_data_reservation=len(main),music_and_sound_data_reservation=len(sound),
        native_code_and_header_reservation=native_code_reserve,
        budget_used_before_final_padding=used,budget_padded_bytes=padded,
        budget_remaining_after_padding=0x500000-padded,limit_bytes=0x500000,
        content_storage_fits=used<=0x500000,completed_native_game_fits_proven=False,
        source_CPU_ROMs_embedded=False,gameplay_integration=False,performance_test=False,
        caveats=['Native code, final extracted tables and native renderer metadata are not final linked artifacts.',
            'One MiB for native code is a reservation, not a measured final code size.',
            'Source-sized data reservation covers every original data byte but does not bound expanded native metadata.',
            'This replaces expanded per-pose/per-shift artwork with compressed source-pen native tiles.',
            'Live cache RAM, palette fitting, preparation, decompression scheduling and full-game frame rate are unproven.'],
        inputs=inputs,outputs={p.name:sha(p.read_bytes()) for p in out.iterdir() if p.is_file()})
    result['inputs'][str(Path(__file__).resolve())]=sha(Path(__file__).read_bytes())
    (out/'manifest.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({k:result[k] for k in ['pack_bytes','pcm_bytes','budget_padded_bytes','budget_remaining_after_padding','content_storage_fits','completed_native_game_fits_proven']}))

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);p.add_argument('--mame-source',type=Path,required=True);p.add_argument('--codec',choices=['lzss','deflate'],default='deflate');p.add_argument('--block-bytes',type=int,choices=[512,1024,2048,4096],default=1024)
    a=p.parse_args();build(a.output.resolve(),a.mame_source.resolve(),a.codec,a.block_bytes)
