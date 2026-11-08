#!/usr/bin/env python3
"""Verify scattered raw-latch consumption against the existing source-pixel oracle."""
from pathlib import Path
import ctypes as c
import hashlib
import json
import random
import struct
import subprocess
import verify_native_object_sat_bridge as geometry

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "build/native-object-latch-sat"
OUT.mkdir(exist_ok=True)
subprocess.run(["cc", "-std=c99", "-Wall", "-Wextra", "-Werror", "-O2", "-fstrict-aliasing", "-shared", "-fPIC", str(ROOT / "tools/test_native_object_latch_sat.c"), "-o", str(OUT / "test.dylib")], check=True)
lib = c.CDLL(str(OUT / "test.dylib"))
Reader = c.CFUNCTYPE(c.c_uint8, c.c_void_p, c.c_uint16, c.POINTER(c.c_uint16))
tail = [c.POINTER(geometry.View), c.POINTER(geometry.Budget), c.POINTER(geometry.Piece), c.c_uint16, geometry.Asset, c.c_void_p, c.POINTER(geometry.Result), c.POINTER(c.c_uint16)]
lib.willowObjectSatTestBuildReader.argtypes = [Reader, c.c_void_p, c.c_uint16] + tail
lib.willowObjectSatTestBuildReaderDeferred.argtypes = [Reader, c.c_void_p, c.c_uint16] + tail
lib.willowObjectSatTestBuildScattered.argtypes = [c.POINTER(c.c_uint16)] + tail
original_build = geometry.lib.willowObjectSatTestBuild
checks = {"reader": 0, "scattered": 0, "deferred": 0}
read_counts = []


def through_reader(bank, words, *args, deferred=False):
    assert words % 4 == 0 and words <= 1024
    visited = []
    @Reader
    def read(_, record, out):
        assert record == len(visited) and record < words // 4
        visited.append(record)
        for n in range(4):
            out[n] = bank[record*4+n]
        return 1
    function=lib.willowObjectSatTestBuildReaderDeferred if deferred else lib.willowObjectSatTestBuildReader
    function(read, None, words // 4, *args)
    read_counts.append(len(visited))


def through_scattered(bank, words, *args):
    assert words == 1024
    lib.willowObjectSatTestBuildScattered(bank, *args)

def through_deferred(bank,words,*args):
    through_reader(bank,words,*args,deferred=True)


for mode, build in [("reader", through_reader), ("scattered", through_scattered), ("deferred",through_deferred)]:
    geometry.lib.willowObjectSatTestBuild = build
    for path in sorted((ROOT / "build/arcade-trace").glob("intro-vram-*.bin")):
        raw = path.read_bytes()
        for bank in [0, 0x8000]:
            words = struct.unpack(">1024H", raw[bank:bank+0x800])
            for flip in [0, 1]:
                result, _, _ = geometry.run(words, flip)
                assert result.status == 0
                checks[mode] += 1
geometry.lib.willowObjectSatTestBuild = original_build

# Failed/missing reads reject whole staging and stop at that original record.
view = geometry.View(96,16,320,224,0)
budget = geometry.Budget()
pieces = (geometry.Piece*80)()
sat = (c.c_uint16*320)()
result = geometry.Result()
@geometry.Asset
def transparent(*_):
    return 2
visited = []
@Reader
def fails_second(_, record, out):
    visited.append(record)
    if record == 1:
        return 0
    for n,v in enumerate([140,90,0x33,0]):
        out[n] = v
    return 1
lib.willowObjectSatTestBuildReader(fails_second,None,256,c.byref(view),c.byref(budget),pieces,80,transparent,None,c.byref(result),sat)
assert visited == [0,1] and result.status == 1 and result.records == 1 and result.record == 1
visited.clear()
lib.willowObjectSatTestBuildReader(fails_second,None,257,c.byref(view),c.byref(budget),pieces,80,transparent,None,c.byref(result),sat)
assert not visited and result.status == 1
lib.willowObjectSatTestBuildReader(Reader(),None,256,c.byref(view),c.byref(budget),pieces,80,transparent,None,c.byref(result),sat)
assert result.status == 1

# Interval admission independently compares against the per-sprite reservation
# path, including clipped spans, simultaneous endpoints, seeded text budgets,
# both scanline limits and total-entry exhaustion. Partial failure diagnostics
# intentionally differ; acceptance, every successful word and budget must not.
rng=random.Random(0x57494c4c)
accepted=rejected=0
@geometry.Asset
def opaque(_,code,source,x,y,attribute):
    attribute[0]=4
    return 1
for case in range(512):
    count=rng.randrange(1,82)
    base=geometry.Budget()
    base.count=rng.randrange(0,16)
    for line in range(224):
        base.sprites[line]=base.count
        base.pixels[line]=base.count*rng.choice([8,16])
    bank=[]
    for record in range(count):
        y=rng.choice([1,8,15,16,17,30,220,230,239,240,500]) if case&1 else rng.randrange(512)
        bank.extend([140,y,0,0])
    bank.extend([0,0,0,0xff00])
    @Reader
    def read_case(_,record,out):
        for n in range(4):out[n]=bank[record*4+n]
        return 1
    outputs=[]
    for function in [lib.willowObjectSatTestBuildReader,lib.willowObjectSatTestBuildReaderDeferred]:
        b=geometry.Budget.from_buffer_copy(bytes(base));p=(geometry.Piece*80)();s=(c.c_uint16*320)();r=geometry.Result()
        function(read_case,None,len(bank)//4,c.byref(view),c.byref(b),p,80,opaque,None,c.byref(r),s)
        outputs.append((r,b,p,s))
    left,right=outputs
    assert bool(left[0].status)==bool(right[0].status),(case,left[0].status,right[0].status)
    if not left[0].status:
        accepted+=1
        assert bytes(left[0])==bytes(right[0])
        assert bytes(left[1])==bytes(right[1])
        assert bytes(left[2])==bytes(right[2]) and bytes(left[3])==bytes(right[3])
    else:rejected+=1

# A zero-filled source record is not an implicit blank/end marker. It must
# still produce code0 when its true transformed extent intersects the crop.
@Reader
def zero_record(_,record,out):
    for n in range(4):out[n]=0xff00 if record==1 and n==3 else 0
    return 1
visibility_cases=[(0,0,0,1),(16,0,0,0),(0,16,0,0),
                  (192,32,1,1),(176,32,1,0),(192,16,1,0)]
for left,top,flip,expected in visibility_cases:
    crop=geometry.View(left,top,320,224,flip)
    for function in [lib.willowObjectSatTestBuildReader,lib.willowObjectSatTestBuildReaderDeferred]:
        b=geometry.Budget();p=(geometry.Piece*80)();s=(c.c_uint16*320)();r=geometry.Result()
        function(zero_record,None,2,c.byref(crop),c.byref(b),p,80,opaque,None,c.byref(r),s)
        assert r.status==0 and r.records==1 and r.pieces==expected

# Native compile verifies no hidden2KiB staging allocation or writable globals.
native = OUT / "native.c"
native.write_text('''#include "willow_native_object_latch_sat.h"
WillowObjectSatResult build(const volatile uint8_t*ram,const WillowObjectSatView*v,
 WillowObjectSatBudget*b,WillowObjectSatPiece*p,WillowObjectSatAsset asset,void*c)
{return willowNativeObjectLatchSatBuild(ram,v,b,p,80,asset,c);}
''')
subprocess.run(["m68k-elf-gcc", "-m68000", "-std=c99", "-Os", "-ffreestanding", "-Wall", "-Wextra", "-Werror", "-fstack-usage", "-I", str(ROOT / "src"), "-c", str(native), "-o", str(OUT / "native.o")], check=True)
sizes = subprocess.run(["m68k-elf-size", str(OUT / "native.o")], check=True, capture_output=True, text=True).stdout
data = sizes.strip().splitlines()[-1].split()
assert int(data[1]) == 0 and int(data[2]) == 0
stack = (OUT / "native.su").read_text()
assert all(int(line.split("\t")[1]) < 256 for line in stack.splitlines())
sources = [ROOT / path for path in ["src/willow_native_object_sat.h", "src/willow_native_object_latch.h", "src/willow_native_object_latch_sat.h", "tools/test_native_object_latch_sat.c", "tools/verify_native_object_latch_sat.py", "tools/verify_native_object_sat_bridge.py"]]
report = {"scope": "scattered-latch adapter geometry and original-pen pixel proof; no production ownership/IRQ/full-frame/FPS admission", "captured_cases_per_path": checks, "additional_original_pixels_verified": sum(checks.values())*320*224, "reader_records_range": [min(read_counts),max(read_counts)], "reader_failure_and_size_guards": 3, "deferred_admission_cases":{"accepted":accepted,"rejected":rejected}, "native_size": sizes, "native_stack_usage": stack, "source_sha256": {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in sources}}
report['zero_record_crop_flip_visibility_cases']=len(visibility_cases)*2
(OUT / "evidence.json").write_text(json.dumps(report, indent=2) + "\n")
print(json.dumps(report, indent=2))
