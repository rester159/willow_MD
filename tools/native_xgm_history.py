"""Keep the SDK audio driver, allocating optional CPU-load history on demand."""
from pathlib import Path
import hashlib
import sys

sdk = Path(sys.argv[1])
source_path = sdk / 'src/snd/xgm.c'
source = source_path.read_text()
replacements = [
('static u16 xgmIdleTab[32];\nstatic u16 xgmWaitTab[32];',
 'static u16 *xgmLoadHistory;\n#define xgmIdleTab xgmLoadHistory\n#define xgmWaitTab (xgmLoadHistory + 32)'),
]
old = replacements[0][0]
if source.count(old) != 1:
    raise SystemExit('Unexpected SDK history declarations')
source = source.replace(*replacements[0])
start = source.index('u32 NO_INLINE XGM_getCPULoad()\n{')
end = source.index('\nstatic void resetLoadCalculation()\n{', start)
getter = source[start:end]
needle = '    SYS_disableInts();'
assert getter.count(needle) == 1
getter = getter.replace(needle, needle+'''
    /* Profiling is optional: driver loading never reserves this history. */
    if (!xgmLoadHistory)
    {
        xgmLoadHistory = MEM_alloc(128);
        if (!xgmLoadHistory) { debugIntroOverflow++; SYS_enableInts(); return 0; }
        memsetU16(xgmLoadHistory, 0, 64);
    }

''')
source = source[:start]+getter+source[end:]
needle = '    s1 = xgmIdleTab;\n    s2 = xgmWaitTab;'
assert source.count(needle) == 1
source = source.replace(needle, '''    if (!xgmLoadHistory)
    {
        xgmTabInd = 0;
        xgmIdleMean = 0;
        xgmWaitMean = 0;
        return;
    }

'''+needle)
out = Path(__file__).resolve().parents[1] / 'native/intro_probe/src/willow_xgm_generated.inc'
header = ('/* Generated from SGDK src/snd/xgm.c; only optional profiling storage changes.\n'
          f' * Input SHA256: {hashlib.sha256(source_path.read_bytes()).hexdigest()}\n'
          ' * Regenerate with tools/native_xgm_history.py <SGDK root>. */\n')
source = source.replace('#include \"snd/drv_xgm.h\"', '#include <../src/snd/drv_xgm.h>')
content = header+source
if not out.exists() or out.read_text() != content:
    out.write_text(content)
print(f'Generated {out.name}; audio-driver operations unchanged')
