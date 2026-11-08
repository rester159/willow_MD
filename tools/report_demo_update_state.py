"""Report observed low-global ownership; never imply full resolver coverage."""
from pathlib import Path
import hashlib,json,re,argparse
parser=argparse.ArgumentParser()
parser.add_argument("--sequence",action="store_true")
parser.add_argument("--input-prefix",type=Path,help="Prefix of externally retained bus-access evidence")
parser.add_argument("--updates",type=int,default=120)
args=parser.parse_args()
name="demo-sequence" if args.sequence else "demo-update"
root=Path(__file__).resolve().parents[1]
prefix=args.input_prefix if args.input_prefix else root/f'build/{name}'
access=Path(f'{prefix}-bus-access.bin').read_bytes()
if len(access)!=65536 or any(x>7 for x in access):raise SystemExit('Invalid observed access bitset')
owned=set(range(0x210))|set(range(0xa10,0xa30))
for name in ['willow_native_boot_globals.h','willow_native_pool_auxiliary.h']:
    for at,n,_ in re.findall(r'\{(0x[0-9a-f]+),(\d+),(\d+)\}',(root/'src'/name).read_text()):
        owned.update(range(int(at,16),int(at,16)+int(n)))
main=(root/'native/intro_probe/src/main.c').read_text()
for name,call in [('willow_native_demo_start_state.h','willowNativeDemoStartStateResolve(SOURCE_DEMO_START_STATE,at,bytes)'),
                  ('willow_native_demo_update_state.h','willowNativeDemoUpdateStateResolve(SOURCE_DEMO_UPDATE_STATE,at,bytes)')]:
    if call not in main:raise SystemExit(f'Missing cartridge binding: {call}')
    text=(root/'src'/name).read_text()
    for lo,hi in re.findall(r'if\(at>=(0x[0-9a-f]+)&&at<(0x[0-9a-f]+)&&count<=',text):
        span=set(range(int(lo,16),int(hi,16)))
        if span&owned:raise SystemExit('Overlapping low-global owners')
        owned.update(span)
    for at in re.findall(r'if\(at==(0x[0-9a-f]+)&&count==1\)',text):owned.add(int(at,16))
missing=[at for at in range(0x2000) if access[at] and at not in owned]
spans=[]
for at in missing:
    if spans and spans[-1][-1]+1==at:spans[-1].append(at)
    else:spans.append([at])
result={'scope':'Observed low globals below 2000 in '+(f'{args.updates} CPU-checked demo service updates' if args.sequence else 'one CPU-checked complete first-demo update'),
 'limitation':'Bus footprint plus resolver starting addresses only; excludes direct pointer access extents, high-memory binding coverage, graphics bus mapping and later gameplay',
 'source_sha256':hashlib.sha256((root/'build/reference_maincpu.bin').read_bytes()).hexdigest(),
 'bus_sha256':hashlib.sha256(access).hexdigest(),
 'observed_read_bytes':sum(bool(x&1) for x in access),
 'observed_write_bytes':sum(bool(x&2) for x in access),
 'observed_resolver_start_addresses':sum(bool(x&4) for x in access),
 'new_sram_bytes':27,'missing_low_bytes':len(missing),
 'missing_low_ranges':[{'start':f'{s[0]:04x}','end':f'{s[-1]:04x}'} for s in spans]}
Path(f'{prefix}-state.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))
if missing:raise SystemExit(1)
