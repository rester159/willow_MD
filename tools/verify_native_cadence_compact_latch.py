#!/usr/bin/env python3
"""Reproduce native source-epoch-selected compact capture and transition audit."""
import argparse
import collections
import csv
import hashlib
import json
import statistics
import struct
import subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/"build/native-object-publication/compact-v1"
TRANS=ROOT/"build/native-object-publication/compact-transition-v1"
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def rows(p): return list(csv.DictReader(p.open()))
def stats(v): return {"samples":len(v),"min":min(v),"mean":statistics.mean(v),"max":max(v)}
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--core",type=Path,default=Path.home()/"Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib")
    parser.add_argument("--no-run",action="store_true")
    a=parser.parse_args();OUT.mkdir(parents=True,exist_ok=True);TRANS.mkdir(parents=True,exist_ok=True)
    base=json.loads((ROOT/"build/native-object-publication/checkpoint-metadata-v1/evidence.json").read_text())
    assert sha(a.core)==base["core_sha256"] and sha(ROOT/"build/reference_maincpu.bin")==base["source_rom_sha256"]
    if not a.no_run:
        with (OUT/"build.log").open("w") as log:
            subprocess.run(["python3","tools/build_native_cadence_compact_latch.py"],cwd=ROOT,check=True,stdout=log)
        for source,dest,name,args,logname in (
            ("tools/test_native_cadence_compact_latch.c",OUT,"test-compact",[str(a.core),"1800","--coin-route"],"cadence.log"),
            ("tools/test_native_object_latch_compact.c",OUT,"test-host",[],"host.log"),
            ("tools/test_native_cadence_compact_latch_cost.c",OUT,"test-cost",[str(a.core)],"closed.log"),
            ("tools/test_native_compact_transition.c",TRANS,"test-transition",[str(a.core),"1800","--coin-route"],"cadence.log"),
        ):
            subprocess.run(["clang","-Wall","-Wextra","-Werror","-O2","-fstrict-aliasing","-fsanitize=address,undefined","-fno-sanitize-recover=all",source,"-o",str(dest/name)],cwd=ROOT,check=True)
            with (dest/logname).open("w") as log:subprocess.run([str(dest/name),*args],cwd=ROOT,check=True,stdout=log,stderr=subprocess.STDOUT)
    manifest=json.loads((OUT/"manifest.json").read_text())
    for name,digest in manifest["products"].items():assert sha(OUT/name)==digest
    for name,digest in manifest["source_files"].items():assert sha(ROOT/name)==digest
    captures=rows(OUT/"publication-latches.csv");native=rows(OUT/"metadata-acquisitions.csv")
    assert len(captures)==len(native)==1797
    for c,m in zip(captures,native,strict=True):
        assert c["frame"]==m["frame"] and c["capture"]==m["capture"]
        assert c["selected_bank"]==m["bank"] and c["producer_epoch"]==m["source_epoch"]
        assert c["valid_complete_epoch"]==m["native_valid"]
        if m["native_valid"]=="1":
            assert m["native_epoch"]==m["source_epoch"] and m["native_count"]==m["source_count"]==c["count"]
        assert c["tail_nonempty"]==c["busy_selected"]=="0"
    valid=[c for c,m in zip(captures,native) if m["native_valid"]=="1"]
    active=[c for c in captures if c["manager"]==c["scene"]=="2"]
    assert len(valid)==1605 and len(active)==464 and all(c["valid_complete_epoch"]=="1" for c in active)
    bank_bytes=(OUT/"captured-original-banks.bin").read_bytes();assert len(bank_bytes)==1797*2048
    tail_records=0
    for c in valid:
        words=struct.unpack_from(">1024H",bank_bytes,(int(c["capture"])-1)*2048)
        for at in range(int(c["count"])*4,1024,4):
            assert words[at]==words[at+3]==0
            for left,width in ((64,384),(96,320)):
                for x in (0,496):assert x+16<=left or x>=left+width
            tail_records+=1
    irqs=rows(OUT/"irqs.csv");active_irqs=[r for r in irqs if r["manager"]==r["scene"]=="2"]
    assert len(active_irqs)==464 and all(int(r["gameplay_wait_mask"],16)==8 for r in active_irqs)
    assert "1800 paired physicalGenesis frames" in (OUT/"cadence.log").read_text()
    assert "1028 cases" in (OUT/"host.log").read_text() and "2056 closed calls" in (OUT/"closed.log").read_text()
    costs=rows(OUT/"capture-cost.csv");assert len(costs)==1028
    lookup={(r["records"],r["bank"],r["proved"]):r for r in costs}
    assert set(lookup)=={(str(n),str(b),str(p)) for n in range(257) for b in range(2) for p in range(2)}
    active_costs=[int(lookup[(c["count"],c["selected_bank"],"1")]["compact_master"])/53693.175 for c in active]
    active_saved=[int(lookup[(c["count"],c["selected_bank"],"1")]["saved_master"])/53693.175 for c in active]
    windows=rows(OUT/"headroom-windows.csv")
    completed=[w for w in windows if w["end_manager"]==w["end_scene"]=="2" and int(w["main_waits"])]
    headroom=[(int(w["next_irq_master"])-int(w["main_commit_master"]))/53693.175 for w in completed]
    tight=completed[headroom.index(min(headroom))]
    assert tight["irq"]=="1333" and tight["start_frame"]=="1335"
    previous=rows(ROOT/"build/native-object-publication/checkpoint-metadata-v1/metadata-acquisitions.csv")
    changed=[{"frame":int(n["frame"]),"old_valid":int(o["native_valid"]),"new_valid":int(n["native_valid"])}
             for o,n in zip(previous,native) if o["native_valid"]!=n["native_valid"]]
    assert changed==[{"frame":144,"old_valid":1,"new_valid":0},{"frame":430,"old_valid":1,"new_valid":0},{"frame":510,"old_valid":0,"new_valid":1}]
    # The second probe only adds host observations, so all target evidence must
    # be byte-identical, not merely produce visually similar captures.
    for name in ("frames.csv","irqs.csv","irq-ring.bin","captured-original-banks.bin","metadata-acquisitions.csv","publication-latches.csv"):
        assert (OUT/name).read_bytes()==(TRANS/name).read_bytes(),name
    writes=rows(TRANS/"transition-writes.csv");index=rows(TRANS/"transition-contexts.csv")
    context=(TRANS/"transition-contexts.bin").read_bytes();stride=6144+512+65536
    assert len(index)==25 and len(context)==25*stride
    by_frame={int(r["frame"]):context[i*stride:(i+1)*stride] for i,r in enumerate(index)}
    assert len({by_frame[f][:6144] for f in range(1332,1339)})==1
    for f in (1334,1335,1336):
        left=by_frame[f][6144:6656];right=by_frame[f+1][6144:6656]
        assert all(left[n]==right[n] for n in range(512) if n not in (0,1,0x100,0x101))
    lo=int(tight["main_commit_master"]);hi=int(tight["next_irq_master"])
    during=[r for r in writes if lo<=int(r["master_cycles"])<hi]
    assert len(during)==30 and {r["pc"] for r in during}=={"5bf4","5bf8"}
    first=active[0];assert first["capture"]=="1334" and first["producer_epoch"]=="886" and first["count"]=="3"
    raw=struct.unpack_from(">12H",bank_bytes,(1334-1)*2048)
    selected_records=[{"x":raw[i],"y":raw[i+1],"code":raw[i+2],"attribute":raw[i+3],"columns":((raw[i+3]>>8)&15)+1,"rows":(raw[i+3]>>12)+1} for i in range(0,12,4)]
    cleared=[r for r in writes if r["frame"]=="1335" and r["pc"] in {"6600","6602","6604","6606","6608","660a","660c","660e"}]
    assert len(cleared)==8192
    result={
        "schema_version":1,"scope":"Opt-in native bounded raw-record capture from actual source epoch metadata, with full fallback. No sprite renderer/VDP integration, production ownership, all-level or CPS-speed admission.",
        "source_rom_sha256":base["source_rom_sha256"],"core_sha256":base["core_sha256"],
        "paired_physical_frames":1800,"native_irqs":1797,"complete_producer_epochs":1350,
        "proved_compact_captures":1605,"unproved_full_fallback_captures":192,"fallback_changes_vs_metadata_v1":changed,
        "active_captures":464,"active_completed_source_updates":464,"active_missing_updates":0,
        "prefix_host_cases":1028,"prefix_host_invalid_no_mutation_guards":7,"native_closed_calls":2056,
        "native_closed_scope":"Every count0..256, both banks, proved/fallback. Unchanged target bus; exact prefix, untouched full destination tail,65508 non-call-stack RAM bytes, source-bank/nonmetadata SRAM, C-ABI preserved registers and actual capture descriptor checked.",
        "native_compact_helper_bytes_including_offset_table":len((OUT/"compact.bin").read_bytes()),
        "active_closed_capture_ms_by_actual_count_bank":stats(active_costs),
        "active_closed_saved_ms_vs_full_metadata_path":stats(active_saved),
        "full_fallback_closed_ms":stats([int(r["compact_master"])/53693.175 for r in costs if r["proved"]=="0"]),
        "cost_scope":"Exact JSR metadataAcquireAndLatch through callee return, before existing TST/IRQ observations. Closed installed-core costs indexed by actual route count/bank; not claimed as instruction-phase-exact integrated measurements.",
        "main_commit_to_next_irq_marker_upper_bound_ms":stats(headroom),
        "after_first_transition_main_commit_upper_bound_ms":stats(headroom[1:]),
        "headroom_caution":"Both ranges retain/source-label transition separately. Upper bound contains remaining source work and interrupt-entry latency; not guaranteed free render time.",
        "tight_first_active_transition":{"window_irq":1333,"next_irq":1334,"start_frame":1335,"first_active_frame":1336,"upper_bound_ms":min(headroom),
            "selected_bank":0,"selected_epoch":886,"selected_records":selected_records,
            "producer_completed_frame":1334,"selected_record_changes_since_prior_same_bank_capture":0,
            "palettes":"All6144 original CPS palette bytes unchanged at captures1332..1338; no assumed remap or residency epoch.",
            "ports":"All512 ports unchanged between captures1334..1337 except rawP1 input and original object selector.",
            "post_main_wait_text_words_before_next_irq":30,"source_task":"100 at5BAA, interrupted at5BE6",
            "source_scroll1_clear_words":8192,
            "source_scroll1_clear_first_to_last_write_ms":(int(cleared[-1]["master_cycles"])-int(cleared[0]["master_cycles"]))/53693.175,
            "conclusion":"Object descriptors and palettes are reusable candidates, but text surface clearing/HUD/text updates are active. No complete prebuilt-frame claim."},
        "tail_proof":{"source_records_checked":tail_records,"crop_flip_cases":tail_records*4,
                      "crops":[{"left":64,"width":384},{"left":96,"width":320}],"flips":[0,1],
                      "semantics":"X0/attribute0 means16x16 horizontal extent[0,16) or[496,512), outside these crops regardless of Y/code/pen. No arbitrary viewport claim."},
        "native_capture_contract":{"count":"20F530","valid_compact":"20F532","epoch":"20F534","fallback_count":256,
             "consumer":"Must use the captured count and generation. Omitted slots retain exact previous bytes and must not be scanned as a new256-record frame.",
             "ownership":"Existing32 scattered64-byte WRAM chunks retained; source-hole access guards and all tail/hole canaries remain active."},
        "files":{n:sha(OUT/n) for n in ("manifest.json","metadata-acquisitions.csv","publication-latches.csv","producer-epochs.csv","object-write-sites.csv","captured-original-banks.bin","frames.csv","irqs.csv","irq-ring.bin","pointer-sites.csv","headroom-events.csv","headroom-windows.csv","cadence.log","host.log","closed.log","capture-cost.csv","test-compact","test-host","test-cost")},
        "transition_files":{n:sha(TRANS/n) for n in ("transition-writes.csv","transition-contexts.bin","transition-contexts.csv","cadence.log","test-transition")},
        "sources":{n:sha(ROOT/n) for n in ("tools/test_native_cadence_compact_latch.c","tools/test_native_object_latch_compact.c","tools/test_native_cadence_compact_latch_cost.c","tools/test_native_compact_transition.c","tools/verify_native_cadence_compact_latch.py")}}
    (OUT/"evidence.json").write_text(json.dumps(result,indent=2)+"\n")
    print(json.dumps({k:result[k] for k in ("proved_compact_captures","unproved_full_fallback_captures","active_completed_source_updates","active_closed_capture_ms_by_actual_count_bank","main_commit_to_next_irq_marker_upper_bound_ms","tight_first_active_transition")},indent=2))
if __name__=="__main__":main()
