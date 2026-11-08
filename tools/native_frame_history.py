"""Make SGDK's optional frame-load average an explicit profiling-build feature."""
from pathlib import Path
import hashlib
import sys
sdk=Path(sys.argv[1]);path=sdk/'src/sys.c';source=path.read_text()
def replace(old,new):
 global source
 if source.count(old)!=1:raise SystemExit('Unexpected SDK source: '+old[:70])
 source=source.replace(old,new)
replace('static u16 frameLoads[LOAD_MEAN_FRAME_NUM];\nstatic u16 frameLoadIndex;\nstatic u16 cpuFrameLoad;',
'''#ifdef WILLOW_SDK_FRAME_PROFILE
static u16 frameLoads[LOAD_MEAN_FRAME_NUM];
static u16 frameLoadIndex;
static u16 cpuFrameLoad;
#endif''')
replace('    memsetU16(frameLoads, 0, LOAD_MEAN_FRAME_NUM);\n    frameLoadIndex = 0;\n    cpuFrameLoad = 0;',
'''#ifdef WILLOW_SDK_FRAME_PROFILE
    memsetU16(frameLoads, 0, LOAD_MEAN_FRAME_NUM);
    frameLoadIndex = 0;
    cpuFrameLoad = 0;
#endif''')
replace('            u16 load = cpuFrameLoad / LOAD_MEAN_FRAME_NUM;',
'''#ifdef WILLOW_SDK_FRAME_PROFILE
            u16 load = cpuFrameLoad / LOAD_MEAN_FRAME_NUM;
#else
            /* SYS_showFrameLoad rejects mean mode in ordinary builds. */
            u16 load = 0;
#endif''')
replace('void SYS_showFrameLoad(bool mean)\n{',
'''void SYS_showFrameLoad(bool mean)
{
#ifndef WILLOW_SDK_FRAME_PROFILE
    if (mean) { debugIntroOverflow++; return; }
#endif''')
replace('    u16 v;\n\n    // frame miss ?',
'''#ifdef WILLOW_SDK_FRAME_PROFILE
    u16 v;
#endif

    // frame miss ?''')
replace('        v = ((deltaFrame - 1) << 8) + frameLoad;',
'''#ifdef WILLOW_SDK_FRAME_PROFILE
        v = ((deltaFrame - 1) << 8) + frameLoad;
#endif''')
replace('        v = frameLoad;',
'''#ifdef WILLOW_SDK_FRAME_PROFILE
        v = frameLoad;
#endif''')
replace('    cpuFrameLoad -= frameLoads[frameLoadIndex];',
'''#ifdef WILLOW_SDK_FRAME_PROFILE
    cpuFrameLoad -= frameLoads[frameLoadIndex];''')
replace('    frameLoadIndex = (frameLoadIndex + 1) & (LOAD_MEAN_FRAME_NUM - 1);',
'''    frameLoadIndex = (frameLoadIndex + 1) & (LOAD_MEAN_FRAME_NUM - 1);
#endif''')
replace('   return (cpuFrameLoad * ((u16) 100)) / (u16) (LOAD_MEAN_FRAME_NUM * 256);',
'''#ifdef WILLOW_SDK_FRAME_PROFILE
   return (cpuFrameLoad * ((u16) 100)) / (u16) (LOAD_MEAN_FRAME_NUM * 256);
#else
   debugIntroOverflow++;
   return 65535; /* Unavailable, never a fabricated zero-load measurement. */
#endif''')
header=('/* Generated from SGDK src/sys.c. Only optional frame-load averaging changes.\n'
 f' * Input SHA256: {hashlib.sha256(path.read_bytes()).hexdigest()}\n'
 ' * Regenerate: tools/native_frame_history.py <SGDK root>. */\n')
out=Path(__file__).resolve().parents[1]/'native/intro_probe/src/willow_sys_generated.inc'
content=header+source
if not out.exists() or out.read_text()!=content:out.write_text(content)
print('Generated optional SDK frame-load history; missed-frame detection retained')
