# Willow Genesis port — handoff

## Update (later 2026-10-23, FPS goal session) — read first

User goal: 45+ FPS within the 5 MiB (5,242,880-byte) cartridge (user confirmed
"5 mbytes"). Details in the PORTING.md section "FPS work session".

- Latest candidate: v40 `candidate-text-bands` (v36 division tables, v37
  black-palette commit for oversized cuts [visual deviation, user to confirm],
  v38 dirty/probe fast paths, v39-v40 measured larger world admission, v40
  text-object row bands). Benchmarks: bench_attract.sh/milestones*.py; Home:
  home_run.sh. Observers need per-candidate offsets (make_offsets.py).
- Previous latest candidate: v35 `candidate-scenery-scan` (5,217,652 bytes, 25 KB spare
  thanks to v34's PCM-gap placement). Contains v31 gap coalescing, v32 Begin
  approach fix, v33 fast audio ports, v34 PCM-gap linking, v35 row-skip scan.
  None promoted to `native/`; v33+ not yet boot-tested end to end.
- MILESTONE: v45 reaches Home stage-1 gameplay with player control from a
  normal boot (unmodified ROM, headless, ~4.2M emulated frames). Gameplay rate
  ~0.0043 source updates/s (need 45). Observers before v18 could not detect
  PLAY (16-bit mode fields read as bytes).
- Earlier: v32 reached the forest dialogue box (~3.45M frames),
  then display.failed 13: >24 changed visible tiles in one publication.
- Measured rate: ~0.1 source updates/s in intro and attract title (need 45).
  Audio is now ~12% of CPU; the remainder is renderer/publication CPU work
  (VRAM readback front capture, retain/stage prep, sprite packing, text) and
  the attract task itself. No gameplay scene has been measured yet.
- WARNING: do not run build/native-audio-admission/prepared/generate.py; it
  overwrites src/willow_native_audio_prepared.h (incident logged, restored).

## Earlier handoff (still accurate unless superseded above)

Prepared 2026-10-23 at the user's request to reach a reasonable checkpoint,
write a handoff, and stop. Resume only when the user asks.

## Honest current status

**There is still no verified playable ROM from normal boot. No gameplay result
establishes 50+ FPS.** The latest full cartridge launches the Home menu and
advances through the map and forest intro, but stops during the forest camera
pan before player control. Do not call it finished or deliver it as playable.

The latest full linked candidate is **v30: 5,242,122 bytes**, leaving **758 bytes**
under the confirmed **5 MiB / 40 Mbit / 5,242,880-byte** limit. All 51 supplied
objects/resources remain; only `video_transfer.o` changed from v29. This proves
the byte size of that linked candidate, not complete campaign acceptance or
that future renderer changes will fit. The performance gate remains closed.

Static work RAM ends at `0xfff000`, with a 4,096-byte stack reservation and no
spare static RAM. Whole-program stack safety remains unproven. The new physical
transfer function and all its callees have a compiler-derived bound of 316
bytes, or 322 including the VBlank exception; this excludes its callers.

Current rendering reconstructs too much artwork and fits palettes on the 68000.
The intro takes enormous numbers of emulated frames. Host test throughput is
not Genesis FPS. A materially more native renderer may make 50+ FPS plausible,
but neither renderer nor simulation costs establish feasibility yet. Animation
refresh reduction alone is not a demonstrated solution to the current costs.

## Where the authoritative work is

Workspace: `/Users/rester159/genesis ports/_capcom/willow` (no Git repository).
Use `AGENTS.md` for the user's full requirements. `PORTING.md` contains extensive
history; its older “live” entries and earlier storage conclusions are historical.

The following paths are relative to this workspace. For commands below:

```sh
R=build/native-world/cartridge-output-binding-v1
```

| Item | Path / state |
| --- | --- |
| Production sources | `native/`; still the earlier v15 integration, not the latest isolated candidates |
| Production build | `build/native-world/cartridge-platform103`, `cartridge-link101` |
| Latest full candidate sources | `$R/candidate-intro-bounded-burst/native` (v30) |
| Latest full build | `$R/platform-v30`, `$R/link-v30/cartridge.bin` |
| Latest complete normal-boot observation | `$R/boot-v30-play-controls-v10` — terminal failure, exit 1, unchanged inputs |
| Next candidate | `$R/candidate-intro-gap-coalescing/native` — proposed v31, **not full-linked or boot-tested** |
| Current status | `$R/integration-status.json` |
| Production-to-v30 changes | `$R/intro-v30-production.diff` |
| Next candidate vs v30 | `$R/intro-gap-coalescing-v31-v30.diff` |
| Object preservation | `$R/intro-v30-resource-preservation.json` |
| Transfer stack evidence | `$R/stack-callgraph-v30/transfer-subtree.json` |

v30 ROM SHA-256:
`6fb49d7d6970195c0b482c930f0736d6c6020a6da842189922b3cd1d08443cf7`

Candidates have their own copied `native/` trees and symlinks to the project's
`build`, `src`, and `tools`. Preserve previous candidates and failed experiments.
Do not edit frozen sources while a build or observer hashes them.

## Latest actual full-ROM failure

v30 passed the earlier Y208, Y192, and Y76 failures. The Y76 fix is confirmed
in normal boot, not merely in a fixture. Evidence:
`$R/boot-v30-play-controls-v10/passed-y76.json`, `passed-y76-ram.bin`, and
`passed-y76.png`. That image is **intro scenery/text**, not gameplay evidence.

It then failed at **emulator frame 1,125,869, camera Y74**, route step 16,
timer 9, `render.display.failed = 17`, foreground failure reason 10.
There was no preceding game/audio failure and no PLAY entry.

The final collected batch contains:

- 720 bytes, 12 prepared dirty tiles, 100 SAT words, all 64 CRAM words,
  two scroll spans, no plane changes.
- Tile destinations: `349, 444, 446, 448, 450, 454, 460, 462, 463, 464, 465, 466`.
- Eight tile spans + CRAM + SAT + two scroll spans = **12 operations**.
- Logical collection completed; preflight refused before physical publication.

Inspect `manifest.json`, `run.log`, `runtime-state.json`, `commit-detail.json`,
`work-ram.bin`, and `registers.json` in that observation directory.

## What v30 fixed and verified

Earlier candidates fixed protected scenery/sprite loans, prepared-SAT handoff,
and exact unchanged-art reuse. Those changes are retained in v30; see its diff
and earlier PORTING entries. They do not constitute a finished retained-motion
renderer or a 50 FPS result.

For the Y76 batch, simply raising the six-operation limit **failed actual
transfer timing**. v30 builds VDP register commands before Begin in a temporary
200-byte stack plan. The physical burst uses prepared command words, retains
epoch/deadline checks, preserves Z80 ownership, and drains the final FIFO.

Its admission profiles are:

- Existing: at most 6 operations and 1,536 bytes.
- Additional: world batches at most 10 operations and 800 bytes; two operations
  are tiny scroll FIFO writes, leaving at most 8 DMA transfers.

A ten-DMA, 800-byte sprite-only batch was measured unsafe on PAL and is rejected.
Do not blindly increase operation/byte limits or remove timing guards.
DMA starts must remain at/before line 244; physical writes stay in the guarded
window. Four bounded Begin attempts avoid repeatedly missing the grant; after
that, the caller regains control for live audio service.

`$R/bounded-burst-regression-v2/manifest.json` passes all 68 cases in NTSC/PAL,
each with cold boot and soft reset: 272 executions. Checks include physical
VRAM/CRAM/SAT/scroll contents, untouched memory, Z80 ROM-bus contention,
pre-owned bus preservation, invalid inputs, the captured 776-byte batch,
800-byte boundaries, and a 1,536-byte/six-operation batch. These checks are
transfer evidence, not audible timing, full-game performance, or physical-console
certification.

## Next candidate: coalesce unchanged gaps

The Y74 batch has enough space in the existing 24-tile prepared buffer to fill
its short gaps with the pixels already in physical VRAM. This preserves those
pixels and lets neighboring uploads become contiguous transfers.

Implemented only in `$R/candidate-intro-gap-coalescing`:

- New `native/cartridge/video_commit_gaps.h`: after logical collection, sort
  destination/payload pairs together; merge the smallest legal gaps that fit
  the buffer and byte limit, reading existing VRAM into those gaps.
- `native/cartridge/video_commit_stage.c`: invoke this after sealing and before
  publication, using the owned Stage buffer and existing live audio service.
- Target cases 68/69: the captured Y74 batch and a rotated destination order.

For this captured batch, the current admission loop fills two one-tile gaps
and stops once the batch is admissible: 14 uploaded tiles in six tile spans,
784 bytes / ten total operations. The added tiles retain their original
physical pixels, without adding a buffer or removing content.

**Targeted tests passed** in `$R/gap-coalescing-target-v2/manifest.json`: both
cases, NTSC/PAL, cold/soft reset, exact physical output and untouched VRAM.
The first attempt (`gap-coalescing-target-v1`) failed compilation on unsigned
comparison warnings; fixed using `24u` in v2. Preserve that failed result.

**Not yet done:** full cartridge link/size check, full normal boot, capacity and
reserved-gap edge regressions, nonzero gap-pixel stress, updated whole-stack
analysis. The newest candidate is experimental. Do not promote it or claim
the full game now works based on the two transfer cases.

## Exact next steps on resumption

1. Review the gap-copy ownership and boundary cases. The original collected
   pointers are frozen, Stage is drained, and no source task may interleave.
   Added gap data must remain byte-exact; preserve prepared tiles when sorting
   or shifting. Never cross reserved VRAM regions or exceed 24 prepared tiles.
2. Build the **full** next candidate and check the final image before running it:

   ```sh
   python3 "$R/candidate-intro-gap-coalescing/native/cartridge/build_platform.py" \
     --inputs build/native-world/cartridge-platform103/link-inputs.json \
     --sdk build/native-world/cartridge-sdk-controller-v1 \
     --output "$R/platform-v31" --link-output "$R/link-v31"
   ```

   These output directories have not been created at handoff. The previous
   margin is only 758 bytes; handle any link failure without removing content.
   The build recompiles all 28 native units and retains all supplied objects.
3. If the strict link passes, run the same full normal-boot/controller observer:

   ```sh
   python3 "$R/run_home_v10.py" v31 v31-play-controls-v10 2000000 home live
   ```

   It uses the headless Genesis Plus GX core. It chooses Home with normal Start
   presses/releases, holds A through the source intro, then attempts idle,
   movement, attack, and jump for 128 actual source gameplay ticks after PLAY.
   It does not seed game RAM, patch the ROM, load a state, or skip the intro.
   Check both campaign and route PLAY state and actual changes in position/
   action. Success is not merely surviving more emulator frames.
4. Keep one observation live. Poll its real process/session; do not restart
   merely because a tool observation times out. Inspect `latest-ram.bin` while
   live and `work-ram.bin` after terminal exit. To decode the final capture:

   ```sh
   python3 "$R/inspect_runtime_state.py" v31-play-controls-v10
   ```

5. Fix the next actual failure if any; verify normal boot and controls before
   promoting the isolated changes into `native/`. Then establish complete
   campaign, audio, save, per-level visual and storage acceptance. Only then
   proceed to measured FPS work, apart from the already-authorized retained-art
   integration. Do not redefine the objective around a passing component test.

## Requirements that remain in force

- **Headless only.** No RetroArch/MAME/preview GUI. MAME reference-capture recipes
  are prohibited on this Mac even with `-video none` or SDL dummy drivers.
- **Leave Ollama running.** The user explicitly declined unloading it. No model
  was stopped by this work. Only our own observer scheduling policy was adjusted.
- Native Genesis implementation of arcade-derived behavior; no runtime arcade
  CPU emulator, invented prototype behavior, deleted actors/stages/cues, or
  intro state injection. Use captured source traces as behavior evidence.
- Full cartridge at most 5 MiB including headers/padding/code/data/audio.
  Bank switching does not provide extra capacity. Full-game fit precedes FPS
  tuning. ROM size does not prove RAM safety or completed content binding.
- Adapt instruments while preserving music, logical tracks, cues and live timing.
- Retained artwork may refresh less often, but source simulation, collision,
  animation clocks, attacks, timers, audio, current positions, facing, visibility,
  camera and ordering must advance. Whole-display skipping is a later fallback.
  Report simulation, geometry presentations, fresh artwork and skipped displays
  separately. The accepted worst-case allowance was 48 FPS; desired gameplay is
  50+ FPS. Neither is verified.
- Each of six levels needs its own source-derived background palette and matching
  tile indices; keep character/text banks separate and preserve source fades/
  cycles. Existing visuals have been rejected. Validate actual in-engine output
  at matching scenes; source longplay is in AGENTS.md/PORTING.md.
- Initial original-font Home/Arcade selector only carries
  `PORTED BY RESTER159, 2026`. A attacks, B jumps. Arcade C adds credits, Start
  starts/continues; Arcade retains attract mode and DIP options. Home has no
  attract mode, options/music selection, Start pause with held-launch protection.
- Versioned interrupted-write-tolerant SRAM high scores/settings, separate Home/
  Arcade rankings. Home one-credit/no-continue clear unlocks persistent NG+:
  double effective enemy energy and shop prices. Extra enemies are conditional
  on storage/performance headroom and are not enabled merely by this request.
- All-level player/enemy/background captures, complete campaign/ending,
  meaningful audible timing, SRAM behavior, hardware compatibility, and busy-
  scene performance remain acceptance requirements, not proven by this handoff.

## Stopped state

All tests/builds started in this work are terminal. The full v30 observer ended
with its recorded fault; the final gap-copy target checks ended successfully.
There is no live game test to wait for. Production sources remain unpromoted.
The active goal is paused at the user's request, not marked complete.


## 2026-10-24 status (native renderer, candidate-v48)

Current candidate: `build/native-world/cartridge-output-binding-v1/candidate-v48`
(link-v48, 5,236,xxx bytes). Gameplay now ~46 frames per source tick (was ~10,200);
details and remaining gap in PORTING.md "Native gameplay renderer integrated".
Measurement tools in `build/native-world/cartridge-output-binding-v1`:
`fp_run.sh <candidate> <from> <to>` (frame-pointer build, inclusive categories),
`fp_callees.py <run> <function>`, `counters_run.sh` + `instrument.py` (phase
timers), `tick_rate.py <run.log>`, `home_profile18.sh` (Home boot + controls).
Never run `build/native-audio-admission/prepared/generate.py` (overwrites src).
