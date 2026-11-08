# Willow native engine: audio path (music + sound effects)

Status: proposal, 2026-11-01. Scope: the ground-up native game
(NATIVE_ENGINE_PLAN.md). Tags: VERIFIED = checked in this repo/toolchain
today; SOURCE = read from arcade program/data or earlier tooling, not
re-run today; UNKNOWN = not established, must be measured.

## 1. Rules this path must satisfy

- The shipping game does not run, emulate, translate or patch the arcade Z80
  sound program (wl_09.12b). The 68000 only posts events; audio costs ~0 on
  the 68000 (plan section 1 and the subsystem budget table).
- Instruments may be adapted to Genesis; music, cue timing, tracks and
  gameplay sounds are preserved (AGENTS.md "Native audio adaptation").
- Cartridge <= 5,242,880 bytes in total. Current M1 ROM: 524,288 bytes
  (VERIFIED, game/willow-native-m1-20261101a.bin).
- Arcade code may be executed **offline only**, as a research instrument
  that produces data (as B1 is for traces). Its outputs are register logs
  and samples, never code in the cartridge.

Consequence for earlier work (all evidence only, none of it ships):
- `native/engine/src/willow_native_audio_sequencer.h`, `audio_runtime.h`,
  `game_audio.h`, `campaign_audio.h`: a C translation of the arcade
  sequencer (040C/0B97) running on the 68000. SOURCE: ~0.93 ms mean per
  service, 13.9 ms peak, ~24% of real time in B1 (PORTING.md 2026-10-24).
  Violates both "no translation" and "0 on the 68000".
- `build/native-world/z80-audio-v1/` and B1 `.../perf/arcade/b1/z80/`
  (`patchsrc.py`, `driver.s`): the arcade Z80 program patched to run on the
  Genesis Z80 with an OPM->OPN/PSG mapper and a 4-voice OKI mixer. Patches
  the arcade program: excluded. Useful parts: the mapping design (voices 0-4
  FM, 5-7 PSG, OPM noise -> PSG noise), the pitch tables (PITCHF/PITCHP)
  and the OKI ADPCM decoder in `mksound.py`.
- `audio_genesis.h` targets SGDK PCM4, which uses the whole Z80
  (SOURCE: 223 cycles/sample at 16 kHz, PORTING.md) and leaves no room for
  music. Excluded.

## 2. Arcade sound command set (SOURCE unless noted)

Sources: `build/native-audio-admission/README.md` (corrected routes, checked
against unmodified Z80 instructions), `build/willow_audio_commands.json`
(OKI table; its older bank-1 "music track" claims are superseded by the
admission README).

| Command | Kind | Count | Notes |
|---|---|---|---|
| 00-1F | music (fixed table 0E70, 8-track loader 01FA) | 32 | canonical music cues |
| 20-44 | FM/PSG sound effect (bank-1 table 8004, single-stream loader 0230) | 37 | header bit 7 set |
| 45-64 | fixed table again (aliases routed to music) | - | |
| 65-7F | bank-1 table again (aliases routed to effects) | - | |
| 80 | no-op | 1 | |
| 81-EF | OKI sample: id = low 7 bits mod 42, stop then deferred start | 111 | 40 valid samples (ids 1-35, 37-41) |
| F0-FF | system (F4 = sequencer/YM reset, fade controls, ...) | 16 | F4 leaves OKI voices running |

Stage-1 (arcade stage 0) events used by the first deliverable:

| Event | Command | Kind | Evidence |
|---|---|---|---|
| Stage start music | 0x13 (word 0x0013, then 0x02FA parameter latch) | music, 8 tracks (6 used) | SOURCE, admission README; stage controller 0xA114 |
| Jump landing | 0x2B | FM effect | VERIFIED call site 0xC86C (player.md, catalog) |
| Shot spawn | 0x20 (decimal 32) | FM effect | SOURCE: call site 0xECA6 near shot code 0xED0C; actor8_one.h `sound(...,32)` |
| Charge start (hold 16) | 0x31 | FM effect | SOURCE: call site 0xC672 |
| Charge full (every 16 at 127) | 0x44 | FM effect | SOURCE: call site 0xC6BA |
| Charge release/stop | 0xF4 | system: sequencer/YM reset | UNKNOWN: a full reset would also stop music; must check with an SNDLOG trace before mapping (likely meant to stop the 0x44 effect only) |
| Hurt | 0x82 | OKI sample 2, voice mask 2, 0.20 s | SOURCE (catalog; length VERIFIED from wl_18/19 table) |
| Death | 0x83 | OKI sample 3, voice mask 2, 0.46 s | same |

Also in stage 0: 0x34 (cue before class-10 actor allocation), 0x2A (slide),
0x33 (type-2 actor). Not part of the first deliverable.

## 3. What exists for producing register logs

- Per-track YM2151 write logs: **do not exist as files** (VERIFIED). The
  admission tests (`build/native-audio-admission/sequencer/test_music.c`,
  `test_effects.c`, `test_mixed.c`) run the unmodified arcade Z80 program
  headlessly in the Genesis Plus GX Z80 core with the CPS sound map, and
  capture every port access (F000/F001 = YM2151 address/data, F002+ OKI)
  per 249.7 Hz service tick, but only keep golden snapshots at ticks
  1/2/32/256/1024/2048/4096 (VERIFIED, test_music.c lines 38-70).
- Producing them is a small variant of test_music.c (headless, no GUI):
  log every (tick, register, value) write for N ticks instead of comparing.
  Timing is exact by construction: one service = 14,336 YM clocks
  = 1/249.70 s (SOURCE, audio_runtime.h, z80-audio-v1/DESIGN.md).
- The reference libretro core (`.../perf/gpgx-cps/genesis_plus_gx_libretro.dylib`)
  does **not** emulate a YM2151 or an MSM6295 (VERIFIED: `core/sound/` has
  only ym2612, ym3438, ym2413, psg, opll). It runs the CPS 68000 program;
  sound only exists in it as the B1 patched Z80 driver. So:
  - `SNDLOG=1` in `lockstep.c` prints the sound latch per frame: the arcade
    **command timeline** of real gameplay (cue timing reference). Useful.
  - `AUDIODUMP` dumps the B1 Genesis output (already FM/PSG-adapted): an
    A/B listening reference for the conversion, not arcade audio.
  - `SNDTEST` pokes commands into the B1 mailbox: same caveat.
- No YM2151 emulator in this project and no VGM files locally, but a ymfm
  copy (BSD-3, includes `ymfm_opm.cpp`) exists in the sibling project
  `~/genesis ports/rolling thunder/vendor/ymfm/` (VERIFIED by search). It can
  render arcade-exact OPM audio offline without any download.
- No OKI WAVs on disk (`build/oki_wav/` is missing, VERIFIED); `mksound.py`
  decodes them from `willow.zip` (wl_18.11c + wl_19.12c) in a few lines.

## 4. SGDK XGM2 in build/sgdk-native (VERIFIED unless noted)

- Driver: `src/snd/xgm2/drv_xgm2.s80` (8,192 bytes), 68000 side `xgm2.c`
  (6,242 bytes text). API in `inc/snd/xgm2.h`: `XGM2_play`, `XGM2_playTrack`
  (multi-track file), `XGM2_playPCMEx(sample, len, channel, priority 0-15,
  halfRate, loop)`, fades, tempo, `XGM2_getCPULoad`.
- PCM: 3 channels, 13.3 kHz or 6.65 kHz, 8-bit signed, 256-byte aligned
  and padded. SFX are PCM only; there is no FM-SFX / channel-lock API.
- Music is processed on the Z80's own VBlank interrupt; the 68000 does
  nothing per frame except during fades (`XGM2_doVBlankFadeProcess`).
  DMA contention protection is via `Z80_useBusProtection`; the M1 game
  already uses `DMA_queueDma` / SGDK tilemap DMA, which honour it. Any
  hand-written VDP DMA must set the flag too.
- Tools: `bin/xgm2tool.jar` converts YM2612+SN76489 VGM to XGM2/XGC2, and a
  list of VGMs to one multi-track XGC (shared PCM). Its usage text states
  it "won't work correctly on VGM file which require sub frame accurate
  timing": XGM2 music is quantized to 60 Hz frames. Options: -s silent,
  -n/-p NTSC/PAL, sample auto-ignore / rate-fix toggles.
- rescomp 3.97: `XGM2 name "a.vgm" "b.vgm" ... [options]` and
  `WAV name "x.wav" XGM2 [6650|13300] [far]`.
- Java: `/usr/bin/java` has no runtime; `/opt/homebrew/opt/openjdk@21/bin/java`
  works and `game/build.sh` already passes it as `JAVA=`. Run xgm2tool
  with `-Djava.awt.headless=true` (no args opens a GUI).
- With PCM in use, YM2612 channel 6 is the DAC: music has 5 FM + 3 tone
  PSG + noise. This matches the existing 0-4 FM / 5-7 PSG mapping.

## 5. Options considered

A. **Offline OPM log -> converted VGM -> XGM2 (recommended).**
   Arcade Z80 runs only offline to log YM2151 writes; a host converter maps
   them to YM2612/PSG and writes VGM; xgm2tool packs it. FM effects and OKI
   voices become PCM samples played by XGM2.
B. Reuse the earlier native FM/PSG player. It is the translated sequencer
   (68000, ~24% of time) or the patched arcade Z80 program. Rejected by the
   rules, not by quality.
C. Own Z80 stream player at 250 Hz (register stream + PCM mixer, built on
   the z80-audio-v1 timing/mixing design with the translated sequencer
   removed). Keeps sub-frame timing and allows real FM effects, but is new
   driver work (cycle proofs, DMA safety). Fallback if A's 60 Hz
   quantization or PCM effects are judged unacceptable by ear.

## 6. Recommended path (A) in detail

Data flow (all offline, under `game/tools/audio/`, outputs in `game/res/`):

1. **opmlog** (host C, from test_music.c/test_effects.c + the existing
   genplus-z80 core): for each cue 00-1F and 20-44, issue the command
   through the original IRQ path (0038, as test_mixed.c does), run service
   ticks, record (tick, reg, value) for YM2151 and OKI events. Stop an
   effect when its stream ends; for music, detect the loop by the first
   repeat of the 2 KiB sound-RAM state at a tick boundary (UNKNOWN whether
   every song repeats exactly; fallback: fixed length with the loop point
   taken from the track-0 jump control).
2. **opm2opn** (host, Python or C): one per-song channel plan (default
   voices 0-4 -> FM 1-5, 5-7 -> PSG 1-3, noise -> PSG noise; override per
   song where a busier part needs FM). Conversion per register: KC/KF ->
   F-number/block for the 7.670453 MHz OPN (reuse B1 PITCHF/PITCHP
   tables); TL/AR/D1R/D2R/D1L/RR/KS/MUL/DT1 copy; DT2 folded into MUL
   where possible, else dropped; algorithm/feedback copy (same 8
   algorithms); OPM per-channel PMS/AMS -> OPN equivalents, OPM LFO
   rate/waveform -> nearest of the 8 OPN LFO rates (triangle only);
   PSG voices take envelope from TL of carriers (as audio_output.h does).
   Output VGM 1.60, YM2612 + SN76489 clocks, waits in 44.1 kHz samples
   rounded from 249.70 Hz ticks, VGM loop offset at the detected loop.
3. **xgm2tool** (or rescomp `XGM2` with all song VGMs): one multi-track
   XGC; `XGM2_playTrack(n)`.
4. **Effects** (20-44): render each effect's converted OPN stream to WAV
   with the local Genesis Plus GX `ym2612.c`/`psg.c` built into a host
   renderer, resample to 13,300 Hz; or render the original OPM log with the
   local ymfm OPM (rolling thunder/vendor/ymfm) for arcade-exact timbre.
   Prefer ymfm if it builds: effects are PCM anyway, so no channel mapping
   compromise is needed for them.
   Looping effects (one SOURCE cue stays active, likely 0x44's family)
   use the XGM2 loop flag.
5. **OKI voices**: decode ADPCM (mksound.py decoder) at 7,575.76 Hz and
   resample to 6,650 Hz (halfRate; the source Nyquist is only 3.79 kHz).
   WAV resources, `XGM2` driver.
6. **68000 side** (`game/src/sound.c`, generated `sound_table.h`): a
   256-entry table built from the admission routes: command -> {music
   track | PCM id + channel + priority + halfRate/loop | system action}.
   `soundPost(cmd)` is a table lookup and one XGM2 call (Z80 bus request,
   a few hundred cycles, only on events). System F0-FF map to
   XGM2_stop/fade/pause after their source behaviour is checked; F4 per
   section 2.
7. **Channel policy**: music PCM-free, so all 3 PCM channels are SFX.
   Priorities from the arcade (OKI voice mask and effect priority byte in
   the 0230 header); `SOUND_PCM_CH_AUTO` with priority, new >= old wins.
   Arcade OKI has 4 voices; we have 3 shared with FM effects. Measure
   collisions in busy traces (UNKNOWN).

Known fidelity differences (need user acceptance; instruments adaptation is
already authorized, these two go a little beyond it):
- Note events quantized from 249.7 Hz ticks to 60 Hz frames: up to ~8 ms
  onset shift, average tempo exact; fast pitch slides lose steps.
  Measure from the logs: fraction of key-on events that move, max shift.
- Arcade FM effects steal music voices; as PCM they play on top and the
  music voice keeps sounding. Plan section 1 says "PCM only for voices";
  rendering effects to PCM changes that line and needs agreement.
- The 0x02FA parameter latch semantics (F00A) for music must be checked
  to see whether it alters playback; UNKNOWN.

## 7. Sizes and cost

| Item | Size | Tag |
|---|---|---|
| XGM2 Z80 driver + 68000 lib | ~14.4 KiB | VERIFIED (object sizes) |
| OKI 40 samples, 60.05 s @ 6,650 Hz | 399,351 B (+<10 KiB alignment) | VERIFIED arithmetic from the ROM table |
| same @ 13,300 Hz | 798,702 B | VERIFIED arithmetic |
| 37 FM effects as PCM @ 13.3 kHz | ~150-400 KiB (0.3-1 s each) | UNKNOWN until rendered |
| 32 songs as XGC | ~250 KiB-1 MiB | UNKNOWN until converted |
| 68000 table + code | < 2 KiB | estimate |
| **Audio total** | ~0.8-1.8 MiB | partly UNKNOWN |

Against 5 MiB: 0.5 MiB game now + ~1.6 MiB packed art (SOURCE,
audio-integration-status) + audio <= 1.8 MiB leaves >= 1 MiB. Keep all audio
data below the 4 MiB line (not `far`) so the Z80 never reads a bank-switched
region (UNKNOWN how the mapper will be laid out).

Runtime: Z80 fully owned by XGM2 (use `XGM2_getCPULoad` to measure the
worst scene). 68000: 0 per frame, a short bus request per posted event
(VERIFIED by reading xgm2.c; cycles UNKNOWN). Z80 RAM: XGM2 owns all 8 KiB.

## 8. First deliverable (M1 audio slice)

Goal: stage 1 music 0x13 plus 0x2B, 0x20, 0x31, 0x44, 0x82, 0x83 playing
from player events in the M1 ROM, measured, nothing else changed.

1. Build `opmlog`; dump 0x13 (until loop) and 0x2B/0x20/0x31/0x44 (until
   end or 2 s). Check: first 4,096 ticks of the 0x13 log equal the existing
   music golden events.
2. Write `opm2opn` with the default plan; produce `s1.vgm` and effect VGMs;
   render effects to WAV; decode OKI samples 2 and 3.
3. `game/res/audio.res`: `XGM2 music_s1 "s1.vgm"`, `WAV sfx_2b ... XGM2`,
   etc.; `sound.c` with the table for these 7 commands; call
   `XGM2_play(music_s1)` at stage start and `soundPost()` from player.c at
   the spec's event points (player.md 3.x).
4. Verify headlessly: ROM size; event->call trace vs the arcade SNDLOG
   timeline for a scripted run (same frames +- 1); `XGM2_getCPULoad` and
   missed-frame counter; 68000 frame time unchanged; rendered audio
   (headless core audio dump) compared by ear and spectrum with the
   B1 AUDIODUMP and the arcade longplay. Report updates/s with the change.
5. User listens; decide on 60 Hz quantization and PCM effects (or fall
   back to option C).

## 9. Blockers and open questions

- 0xF4 on charge release: real effect unknown (would reset music if taken
  literally). Check with SNDLOG + admission routes before mapping.
- XGM2 60 Hz quantization and PCM-rendered FM effects are design
  deviations needing user acceptance (section 6).
- Loop-point detection per song not proven; some songs may need manual
  loop marks.
- Only 3 PCM channels vs 4 OKI voices + FM effects: collision rate in busy
  scenes unmeasured.
- Arcade-exact effect timbre: use the local ymfm copy (rolling thunder
  vendor dir); building it host-side is not yet tried (UNKNOWN).
- Music audible quality of the 5 FM + 3 PSG mapping was judged only in B1;
  per-song channel plans are manual work for 32 songs.
- Do not run `build/native-audio-admission/prepared/generate.py` (it
  overwrites a source file); the new tools copy what they need.
