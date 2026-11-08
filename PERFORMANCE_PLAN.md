# Willow 45 FPS plan (from measured state, 2026-10-24)

Goal: full port, no content cuts, >=45 source ticks/s in busy gameplay, cartridge <= 5,242,880 bytes.

## Where we are (candidate-v48, headless Home boot, stage 1)

- 35 emulated frames per source tick (~1.7 ticks/s). 45 FPS needs <= 1.33.
- Cartridge 5,237,346 bytes (5,534 spare). Work RAM: 20 bytes free between BSS and stack.
- Gameplay-only profile (frame-pointer sampling, window after PLAY):

| Share | Work | Frames per tick |
| --- | --- | --- |
| ~47% | Screen publication (renderer, scenery staging, transfer kernel) | ~16 |
| ~26% | Music/sound driver on the 68000 (240 Hz ticks, real time) | ~9 |
| ~12% | Game logic | ~4 |
| ~6% | Scenery streaming (camera columns) | ~2 |
| ~5% | VBlank wait | ~2 |
| ~4% | Sprite emission | ~1.4 |

Audio costs ~0.26 of every frame regardless of game speed, so at 45 FPS the
non-audio budget per tick is ~0.98 frames (~125k cycles). Today it is ~26 frames.

## What it would take

1. **Audio off the 68000 (frees ~26%).** The Z80 already runs SGDK PCM4 at
   near-full load, so this needs a new combined Z80 driver: 4-voice PCM mixing
   plus the translated music sequencer (src/willow_native_audio_prepared.h,
   ~700 lines of C) and the YM2151->YM2612/PSG adaptation (audio_output.h).
   sjasm is in build/sgdk-native/bin. Verification: the existing golden
   (check_audio_port_fast.c: 833,370 ticks, output 229c9b5d, state 578c143a)
   plus PCM cue timing. Risk: Z80 cycle budget for mixing + sequencing.
2. **Renderer publication to ~0.4 frame (from ~16).** Replace the Stage/
   Commit pipeline in native mode with a native publication: incremental
   scenery tile/plane queues (no per-publication scans of 1,360 slots or
   4,096 plane cells), a hash index for the scenery tile cache (needs RAM
   freed by retiring the old renderer state), precomposed or delta block
   uploads, one DMA list per VBlank. Requires native coverage of the shop,
   frontend text and ending scenes so the old renderer's ~8 KB of RAM can be
   reclaimed.
3. **Game logic ~4x faster (from ~4 frames per tick).** Profile-guided work on
   the native engine: banked-ROM dictionary/terrain reads (world cells,
   contact descriptors), sprite template expansion, entity service dispatch;
   targeted __attribute__((optimize("O2"))) on hot functions within the
   ~5.5 KB ROM headroom; table-driven replacements for 32-bit multiply/divide.

All three are required; none alone reaches 45 FPS. Each must keep source
behaviour exact (existing traces, audio golden, render equivalence checks).
