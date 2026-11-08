# Arcade-source parity work

The target is a native implementation of the supplied Willow USA arcade set's
behavior. The current `out/verified/willow.bin` is a previously tested prototype,
not a faithful port. “Verified” in that old output folder means its prototype
smoke tests passed, not equivalence to the arcade. Do not present it as complete.

Latest behavioral checkpoint (2026-10-05): the guarded native build executes
900 consecutive demo-service updates, including the timer-expiry exit. Every
retained-state/graphics endpoint matches the original-CPU-verified sequence:
player/actors, collision, stage/scroll, sprites, CPS palettes, D4/A4 and all nine
allocation pools. The timer reaches zero, the player becomes inactive, mode 6/6
is entered and a mode-six text task is queued at 78F0. Earlier status notes
incorrectly called this a fade. The latest guarded cartridge run includes all auxiliary task scans and wakes;
interrupt timing remains excluded. The guarded combined scheduler now continues through both queued text tasks
and the 180-tick hold to phase 8/0;
the attract loop is not complete.
The guarded first-stage demo now has an active manager-task route. A separate
host translation test verifies 900 complete scans with auxiliary tasks against
the original CPU; the compiled cartridge now passes the same combined scans. Interrupt timing
remains unverified.

The longer test found missing native attack scratch words at 0FAA/0FAC, fixed
with four bytes of separately owned SRAM. It also exposed unequal pre-title
random-seed inputs in the test fixtures. The final continuous test explicitly
uses the original-CPU fixture's cold-zero RNG input before title dispatch and
checks the saved RNG/clock inputs before the demo. It never substitutes service
outputs. Initial evidence: build/demo-extended-investigation/native-sequence-matched.log.
The faster direct-record variant repeats all 900 endpoints successfully in
build/fixed-reader-investigation/direct-900.log. The native ROM-reader
variant repeats all 900 endpoints in
build/rom-direct-spans-investigation/leaf-cold-900.log.
The extended fixture expands 6201 original ROM words losslessly from 9310 ROM
bytes into 12402 RAM bytes at startup. It leaves 5828 heap bytes and fits below
the SRAM cartridge boundary. The preceding ROM-reader variant ends at _stext
1FF8A0 and takes 7307 total / 13 peak diagnostic slices; these are not an FPS
measurement. Bootstrap publication is deliberately paused
for headless inspection. See PORTING.md, “900-update extended service fixture.”

Latest timing covers the first 120 source-verified updates with the extended
900-update data fixture. Direct fixed-record access reduced isolated mean CPU
cost from 133.658 to 107.456 ms; the native ROM-reader leaf reached 99.189 ms,
fixed-word masks reached 88.380 ms, native owner lookups reached 84.824 ms,
and the native object writer reached 83.822 ms. A native demo byte reader now
reduces this to 81.644 ms mean / 157.951 ms peak, another 2.598% reduction,
with no extra working RAM. Its merged player/actor bounds check preserves
the existing owners and fallback behavior. All 196608 address comparisons
pass with original and two address-specific payload patterns; byte values,
overflow deltas and unrelated RAM/SRAM match the compiled C reader.
All prior fixed-owner, object-writer, ROM-reader and actor-bus audits pass.

The latest WILLOW_PACKED_SCROLL3_SOURCE_PROBE variant removes a duplicate
32768-byte plain map-ID table, preserving every original value through the
existing lossless packed table. All 16384 IDs, 311324 copy/rejection cases,
and a separate 1010-frame original Scroll3 trace pass. The compiled cartridge
passes all 900 endpoints in build/packed-scroll3-source-investigation/sequence-900.log,
including exit, with unchanged 5965 total / 11 peak diagnostic slices.
The latest timing is 81.657 ms mean / 157.951 ms peak, about 0.013 ms more per
update in exchange for 32 KiB ROM space. Text ends at 1F7F20 and initial data
at 1F7FF2, leaving 32782 bytes before the 2 MiB SRAM boundary. The heap remains
5828 bytes. Only the first 120 updates were CPU-profiled. Mean logic cost still exceeds the 60 Hz budget by
4.90 times; concurrent VDP/Z80/IRQ work is excluded. This is not playable-game
FPS. The port remains incomplete and has not met the 60 FPS requirement.
See PORTING.md, “Native demo byte reader” and “Packed-only Scroll3 source IDs.”

New source continuation (2026-10-05): 8E56..8EC2 is translated in
src/willow_arcade_attract_demo_finish.h. It waits for the first text task,
queues the second stage-specific message, waits for its completion, counts
180 ticks, issues gated 08FB/FF and unconditional F4/FF sound commands,
advances to phase 8/0, and cancels the first matching tagged task.
3840 original-CPU comparisons cover both states, all six stages, busy flags,
timer wraparound, sound gates/cursor wrap, cancellation and live D1/D2/A4.
A separate 12-scenario sequence runs both actual text coroutines and the
hold through 3216 original scheduler boundaries and 282 text dispatches.
That original-CPU test uses constructed exit states. The guarded cartridge
now also continues from its actual update-900 endpoint through both real text
tasks and 180 native hold calls to phase 8/0, with full non-stack RAM/SRAM
comparisons at task/service boundaries. All twelve stage-specific message
tasks and eight invalid context/free-stack cases pass separate cartridge tests.
The latest guarded build also admits states 6/6 and 6/8 through the real
slot-20 manager dispatcher. Active state 6/4, phase-eight continuation and
complete attract-loop execution remain pending. WILLOW_DEMO_FINISH_PROBE
now adds 1204 exact source-data bytes, including the original 8CC6/8CC8
dispatch words; the guarded load end is 1F8CF2, leaving 29454 bytes below SRAM. No new FPS or display
fidelity result is claimed. Evidence: build/demo-finish-investigation and
build/native-demo-finish-investigation.

Manager routing (2026-10-05): 3840 additional original-CPU cases verify the
810E dispatch path, including freeze gating, jump-table D1, input/flag handling
and the one-tick sleep boundary. Twelve concurrent scheduler scenarios verify
3216 original scan/wake boundaries and 3498 manager/message dispatches through
phase 8/0. Those source tests use constructed post-demo inputs. The cartridge
passes 240 constructed slot-20 dispatch comparisons against that verified
manager/runtime, including full unrelated non-stack RAM/SRAM. The prior
continuous 900-update test still excludes auxiliary scheduler tasks/interrupts;
its post-demo service chain is not a continuously scheduled full attract loop.
Evidence: build/demo-finish-routing-investigation. No new FPS or visual claim.


Gameplay live-register checkpoint (2026-10-05): ordinary and stage-four sprite
writers now preserve source D1/D2 as well as D4. Original-CPU comparisons cover
4096 ordinary cases, 6720 stage-four cases, 128 visibility dispatches and 128
consecutive renderer frames. The source-derived demo model independently
carries D1/D2 and matches all 900 original-CPU endpoints, while the six existing
RAM/graphics/D4-A4/access traces remain byte-identical. The guarded cartridge
binds these results to its existing execution fields and result slots, adding
no persistent RAM, and passes the same 900 live-register endpoints plus all
existing state/graphics and demo-ending checks. Diagnostic slices are now
5969 total / 11 peak; no new CPU-cycle profile was collected. This closes register coverage for the tested demo path;
it does not establish arbitrary-population gameplay or active scheduler
admission. Evidence: build/demo-live-registers-investigation. No new FPS claim.


Active-demo task checkpoint (2026-10-05): the explicit demo-tick capability now
routes phase 6/state 4 through the outer manager and native task runtime.
The original-CPU comparison passes all 900 updates from 810E through gameplay,
post-update flags/input and the one-tick 1750 sleep boundary, including all
four live registers and state/graphics. It supplies outer state four to the
existing service fixture on both sides before execution. The guarded cartridge
passes all 900 actual slot-20 task dispatches and independent wake/sleep
checks, plus freeze and invalid-stage/timer cases. Its timer-expiry exit and
existing post-demo service/message continuation also pass. Diagnostic slices
are 6324 total / 11 peak, not video frames or a new cycle profile. Admission is limited to the
retained stage-zero, 1..900-timer fixture; auxiliary tasks remain undispatched.
Service failures report diagnostics and need not be atomic after gameplay
writes. This is not complete campaign scheduling or a 60 FPS result.
Evidence: build/demo-task-routing-investigation.

Concurrent demo reference checkpoint (2026-10-05): the lifecycle harness now
exports independent original CPU task contexts/stacks, native continuation
labels, native RAM and retained Scroll1. Regenerating the 3726-cycle opening
matches the existing demo-start RAM and text fixtures exactly. The new
`make test-arcade-demo-auxiliary-tasks` runs 900 scheduler scans and wakes:
900 manager dispatches, 900 command-consumer dispatches, 56 text-blink
dispatches, 24 continuing fade dispatches, one new fade and the first ending
message dispatch. Non-private RAM, all maps/sprites/palettes and saved manager
D1/D2/D4/A4 match original instructions at every scan, with independent wake
comparisons. Auxiliary contexts come from the source lifecycle; the manager
caller is explicitly supplied at 1756 returning to 810E, because that lifecycle
fixture invokes the scene outside a manager task. CPU stack/context storage is
excluded from semantic comparisons; its saved manager register values and
sleep state are checked separately. No source output is replaced by model
output. This is host-level evidence, not a new cartridge or FPS result.
Evidence: build/demo-auxiliary-investigation.

Compiled concurrent demo checkpoint (2026-10-05): all 900 complete cartridge
scans/wakes pass against the independent original-CPU-verified host reference.
The earlier manager-only limitation is superseded for this stage-zero fixture.
The test found and fixed a fade admission/read gap: gameplay palette animation
can bind eight groups outside the opening ROM spans. The fade reader now uses
those exact words from the retained demo data. No additional persistent RAM.
Dispatch counts, active continuation labels, auxiliary task semantic records,
manager D1/D2/D4/A4, gameplay state, palettes, text, maps and sprites match at
all 900 endpoints, including newly dispatched tasks at timer expiry. The
manager boot payload absent from the synthetic reference caller is checked
for preservation. Fixed boot slots absent from that reference remain excluded.
The compiled sequence takes 6666 total / 12 peak diagnostic slices, not FPS.
The prior 3724-cycle opening and full storage/reader audits still pass. This
is a guarded diagnostic path; normal playback, interrupts, display/audio
publication, phase-eight/campaign continuation and 60 FPS remain unfinished.
Evidence: build/native-demo-auxiliary-investigation.

Continuous post-demo checkpoint (2026-10-05): the original CPU, native host
and guarded cartridge now agree for 1141 consecutive combined scans/wakes,
including all 900 gameplay updates and 241 post-demo scans. The first ending
message continues from its actual scan-900 dispatch; it is never reset for
this check. Both messages, sound writes, the 180-tick hold and tagged-task
cancellation reach phase 8/state 0/timer zero. All eight source trace streams
retain identical first-900 prefixes. The compiled sequence matches dispatch
counts, live continuations, task semantics, D1/D2/D4/A4, gameplay state, original
CPS palette words, all maps and sprites at every endpoint. Counts are 1141
manager and 1254 auxiliary dispatches, 6907 total / 12 peak diagnostic slices.
The opening's 3724-cycle comparison also passes. These are behavioral checks,
not video-frame timing or a completed attract loop. Phase-eight handler 8EC4,
subsequent outer states, campaign/input integration, display/audio publication
and sustained 60 FPS still need work. Reproduce with
`make test-arcade-demo-post-sequence`; evidence is in
build/demo-post-sequence-investigation and build/native-demo-post-sequence-investigation.

Phase-eight entry checkpoint (2026-10-05): the guarded native manager now
admits state zero through an explicit interlude-reset capability. Original
8EDE resets all nine pools, clears 8402..8501, advances to state two and
allocates the global fade. The independent host/original-CPU sequence and
final compiled cartridge pass 1142 combined scans/wakes, including that
fade's first dispatch. All eight preceding 1141-record trace prefixes are
unchanged. 513 direct and 513 outer-manager source cases cover pool/task
variants and the freeze gate; old task/runtime regressions pass. No new
persistent RAM. The final compiled run uses fixed archived reference files:
6916 total / 12 peak diagnostic slices, not FPS. State-two fade completion
and full graphics clear pass 64 separate original-CPU cases, but that next
routine is not yet admitted into the cartridge scheduler. Phase-eight scene,
complete attract loop, campaign, display/audio/interrupt timing and 60 FPS
remain incomplete. Evidence: build/interlude-reset-investigation and
build/native-interlude-reset-investigation.

Phase-eight clear checkpoint (2026-10-05): the guarded cartridge now passes
1191 combined scheduler scans/wakes through phase eight/state four. The fade
finishes at scan 1190; manager slot 20 observes completion at scan 1191 and
executes the complete original 65A4 graphics clear. State, task continuation,
saved registers, original palette words and all five graphics regions match
at every scan. The first 1142 source records in all eight streams are unchanged.
A separate clear capability preserves the old reset-only API admission. No
persistent RAM added; 7001 total / 12 peak diagnostic slices are not FPS.
Next is 8EFA palette/map/message initialization and the remaining actor scene;
campaign, display/audio/interrupt timing and sustained 60 FPS remain incomplete.
Evidence: build/interlude-clear-investigation and build/native-interlude-clear-investigation.

## Required evidence

Every translated system needs source routine addresses, state/data ownership,
matching input traces, and comparisons of observable state against the arcade.
Video supplies color and composition references only. Do not infer movement,
physics, timers, collision, AI, or camera logic from video. Do not invent missing
systems or silently use custom trial/companion logic in the faithful entry.

## Camera work completed on 2026-10-03

`src/willow_arcade_intro.h` translates the unskipped intro camera path, including
manager states 8/10/12/14/16. It is not yet linked into the release prototype.

| Source | Meaning | Evidence |
| --- | --- | --- |
| A118–A140 | State 8, target Y=640, initial camera Y=512, timer=7 | Instruction anchors; live state |
| A234–A256 | Seven-tick wait, then timer=63 | Native/arcade trace |
| A25C–A2C2 | Second wait, state 12, timer=29, text/actor dispatch | Camera-side state matches; task side effects pending |
| A2C4–A35E | Message-dependent transitions and camera call | Native text task produces completion; no replayed gate |
| A2F8–A30E | While camera Y is nonzero, subtract 2 from target Y | Native/arcade trace |
| 42B3E–42B96 | Follow target minus 128, clamp to lower bound | Native/arcade trace |
| 42FAC, 42FC4, 43174 | Independent Scroll3 map; Stage 0 half-speed Y | Instructions and scroll-register trace |
| 1A0C–1A30 | Staged hardware Y writes, 768−Scroll2 Y and 1792−Scroll3 Y | Register values match after initialization |

350 consecutive frames (691–1040) match camera state, timer, target, and position.
Both hardware scroll registers match from frame 697 onward, with task/VBlank
publication latency. This does not validate initial layer-fill scheduling,
fades, skipped intro, or the entire task scheduler. Text-task verification is
described below.

`tools/cps1_captured_background.py` renders original tile graphics using captured
CPS video RAM, palette RAM, and scroll registers. Frame 720 matches the arcade
screenshot exactly (384×224 = 86,016 pixels). Frame 900 differs only in 88 pixels
occupied by the independently rendered “TO” text. This is an independent
reference decoder; do not ship captured screenshots as game logic or assets.

## Reproduce

From the project directory, with the supplied `willow.zip`:

```sh
mkdir -p build/arcade-trace
mame willowu -rompath . -noreadconfig -video none -sound none -nothrottle \
  -skip_gameinfo -autoboot_delay 0 -autoboot_script tools/arcade_intro_trace.lua \
  -snapshot_directory build/arcade-trace -cfg_directory build/arcade-trace/cfg \
  -nvram_directory build/arcade-trace/nvram -seconds_to_run 40
python3 tools/verify_arcade_intro.py
# Pillow is required for this reference image decoder:
.tools/sorsha-python/bin/python tools/cps1_captured_background.py 720
```

## Corrections and remaining integration

The earlier label “attract/demo map” for table 2EB24 was wrong. It belongs to
Scroll3, while 2DBF8 belongs to Scroll2. The prototype incorrectly combines the
layers using one table and one camera. Its palette-fit scores therefore measure
conversion against that prototype composition, not full arcade visual fidelity.

Replace that compositor with independently translated source layer maps and
scrolling. Translate text/actor/fade tasks and input skipping before integrating
the opening. Then establish source parity for player motion, animation,
collision/combat, enemy AI, bosses, shops, progression, sound, and the front end.
The existing large research function and its catalogs are evidence to audit,
not proof that the campaign is implemented or correct.

## Native layer renderer checkpoint

The ROM-backed lookup in `tools/cps1_source_layers.py` translates 43658–43768
and 43D4E–43DB2. It uses separate selector tables and complemented source Y.
Eleven intro checkpoints now match the captured layer-RAM decoder exactly:
86,016 pixels each, zero differences. Runtime palette RAM is an external input
for that comparison; this does not claim a translated fade controller.

`native/intro_probe` is an isolated native Genesis renderer target, not a
replacement game entry. It streams original, converted tile graphics on two
VDP planes under the translated camera controller. It does not play captured
frames. The native asset fit reserves PAL0/PAL1, shares PAL2/PAL3 across the
background layers, and crops 32 pixels from each side of the 384-wide arcade
image to fit 320 pixels without resampling. These hardware compromises are
explicit and still need final visual fidelity work.

Headless Genesis Plus GX validation: 402 camera updates, zero missed updates,
zero cache overflows, and seven exact matches against an independent compositor
of the converted Genesis tiles. Moving checkpoints match the source's two-frame
publication delay. A 1400-pattern cache covers at most 1192 resident patterns.
VBlank DMA and staggered background-row prefetch eliminate the initial stalls;
the prefetch changes transfer scheduling, not camera timing or visible pixels.

Run `make test-intro-probe SGDK_ROOT=/tmp/willow-sgdk-resume` after producing the
arcade trace captures above. The saved diagnostic is
`out/diagnostics/intro/intro-renderer.bin`, with its manifest and validation logs.
No RetroArch GUI was launched or activated. The existing prototype release was
not replaced. Actor/fade tasks, skipping and game handoff remain required
before this can become the real opening sequence.


## Native intro text scheduling and clear writes

`src/willow_arcade_text.h` and `src/willow_arcade_intro_text.h` translate the
intro text parser and dispatch shared by the host test and native diagnostic.
The decoder uses source tables at 7980/7984/798C/7A1A/7A1E: style 1302 selects
half-record text, a three-tick glyph delay, and destination slot 2. The earlier
extractor ignored the record format and returned unrelated dialogue; the opening
extractor now uses the source decoder. Other catalog paths still need auditing.

The character parser at 7912–797E handles the slash terminator, ampersand line
breaks, and forty-column wraps. The 1750/1598 sleep/wake behavior produces the
intro's completion signals. All 350 captured frames match without replaying the
arcade's message-busy flag. Message attributes and destination addresses are
extracted from the supplied program, not inferred from the video.

The manager calls 7D80 with row 13 at A2D0, and rows 13 and 16 at A328. That
routine writes code 71A6, attribute zero, to two rows across forty columns.
Including overwritten/cleared cells, 486 final tile code/attribute comparisons
match nine captured Scroll1 RAM snapshots. The previously displayed message is
no longer excluded from the comparison. These checks cover frames 780–1020;
they do not establish the later third-message clear or the complete intro.

The native diagnostic executes the same text/clear routines and exports the
write count for headless checking. It now draws these glyphs as described below. The character sound call at 786A,
actors, fades, skip behavior, and gameplay handoff remain to be implemented. Delay-zero scheduling and other
text formats have not been validated by this intro trace.


## Visible native intro lettering

The diagnostic now draws the supplied ROM's Scroll1 glyphs using both 8×8
layouts, selected by source map-column parity. Source pen 15 becomes transparent
Genesis pen 0; glyph pixels are otherwise unchanged. The source map address
sets position, including the 32-pixel center crop and CPS vertical origin of 16.
The palette is group 3 from captured full-brightness Scroll1 palette RAM, rounded
to the nearest physical RGB333 levels; palette initialization/fades remain
external inputs rather than translated source logic.

`tools/verify_intro_glyphs.py` independently decodes the arcade's captured
Scroll1 codes, attributes, and raw ROM graphics. All 8,257 opaque text pixels
across nine arcade screenshots match exactly. The native compositor check
includes text at seven checkpoints, including the third message (that last
checkpoint is an internal renderer check beyond the 350-frame arcade state
trace). Text DMA appears one scanout later, while the camera retains its extra
source register-publication stage. These are separate timing paths.

The Genesis renderer uses 80 dynamic glyph tiles at indices 1416–1495 and
40 16×8 sprites, twenty per text row, with PAL0. It currently handles source
rows 13–14 and attributes used by these three messages only. This consumes the
sprite-per-scanline limit on those two rows; actor composition and a general
text renderer still need work. Seven headless composite checks match exactly;
402 native updates have zero missed frames and zero cache overflows. This
limited intro result is not a full-game 60 FPS claim.


## Intro actor translation (not yet integrated)

`src/willow_arcade_intro_actor.h` translates the class-12 selector-1 routine at
3257E and selector-3 variant-zero routine at 32690, plus animation helpers
1F40/1F4C. Dispatch table 2F3C4 establishes those handler identities; they are
not the unrelated main-pool player/AI handlers. The opening allocations at
A28A/A2A6 set positions (218,136)/(160,80), active values 2/1, and selectors 1/3.
Selector 3 initializes timer 222 and animation 32AA2, waits for camera Y
zero, then counts down before selecting 32ACA. The subsequent helper at 2162
is reported as an unimplemented event; visibility/queue calls are not translated.
These routines are deliberately not wired into the renderer while those tasks
and source scheduler ordering remain unresolved.

`make test-intro-actors` checks ten source instruction anchors and 34 captured
actor-record comparisons. State is seeded once per interval, then advanced
without replaying subsequent actor values. The comparisons cover frames 845/850
for both actors and 995–1140 for selector 1 only. The latter files correspond to
the retained `arcade_magic_trace.lua` capture window. These are routine checks,
not full unskipped-intro parity: the older frame-855 capture has a one-tick
animation mismatch when advanced from frame 840, and the camera/skip timing of
that mixed historical capture series is unresolved. It is explicitly excluded
from the passing intervals, not treated as a passing trace. Selector 3's post-camera
countdown, the allocation tick, event 2162, and actor pixels need fresh matched
evidence before integration. No MAME or RetroArch GUI was launched.

The object catalog's opening-message decoder also ignored style 1302 and was
still reporting unrelated witch dialogue. It now uses the shared source text
decoder and validates the actual opening message. Other catalog entries remain
research evidence rather than proof of implemented gameplay.


## Fresh unskipped actor trace and native integration

A new isolated capture, `tools/arcade_intro_actor_trace.lua`, records both
class-12 actor slots every frame from 750–1400 without skip input. It ran with
`SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy`, `-video none -sound none`, and
isolated config/NVRAM paths under `build/arcade-trace/actor-native`. No GUI was
opened. The capture finished normally at 1400.

The critical schedule is now established: A2F8 calls 9EF2 only while the camera
is nonzero; 9EF2 calls the class-12 scheduler at 9F04, before camera publication.
Actors are allocated at frame 760 and first updated at 761. Actor updates stop
after 1016 while the camera is zero. The state-20 path at A3E2 resumes them at
1154. This intentional pause must not be replaced by continuous animation.
The older mixed-capture discrepancy at 855 remains a provenance limitation; it
is superseded for integration by the new complete capture, not explained away.

The host translation matches all 1,282 records from 760–1400, including both
actors' creation, animation pointers/countdowns, states, coordinates, and selector 3's
222-tick post-camera wait. Manager and camera values are external inputs in that
host test. No per-frame actor state is replayed or seeded from captured records.
Thirteen instruction anchors cover the handlers, animation walker, and schedule.

The native diagnostic now runs these same routines with its own camera/manager,
loads animation records directly from the supplied ROM, and matches all 666
actor records across source frames 760–1092. The headless test reads actual actor
structures instead of copying them each frame: the initial copying added enough
work to cross a frame boundary during streaming. Removing that debug-only work
restored 402 updates with zero missed frames and all seven pixel checks passing.
This is a narrow intro budget result, not evidence of spare capacity for the
full game. Visible actor sprites/visibility queues are still pending. The helper
at 2162 is now identified as offscreen culling; it has no observable effect in
this stationary intro trace but still requires translation for moving cameras.


## Intro sprite identity, geometry, and source pixels

Correction: the prior attribution of the (160,80), selector-3 actor as Willow
was wrong. Its 32AA2 stream uses sprite block 0B00 and depicts the nearby scripted
character. Selector 1's 325BC stream uses blocks 0760/0766/076B and depicts the
raised tree-house scenery. The blue-clad player is a separate rendered object
(block 0033 in these captures), whose path still needs integration. The object
catalog now removes the incorrect candidate identity.

`tools/cps1_intro_actor_render.py` translates the evidenced blocked-record path
at 3B880 and position-table lookup at 3B99A. It preserves source attribute flips,
512-coordinate wrapping, relative position-table offsets, and visibility gates.
Ten actor/object-RAM checks across five captures pass, including the nonvisible
selector-3 actor at frame 960. These checkpoints do not validate every visibility
boundary, priority class, or unpacked/multipart sprite format.

`tools/verify_intro_actor_pixels.py` decodes those sprite blocks directly from
ROM graphics, using captured original palette RAM. All 11,046 visible opaque
pixels match the arcade screenshots. It uses two-frame-old actor/camera records
for object scanout and excludes pixels covered by the independently decoded
Scroll1 text layer, which is drawn over these actors. This checks source geometry
and layering, not Genesis palette conversion. The source sprite block addressing
wraps the low four tile-code bits horizontally and advances sixteen codes per
vertical tile. No captured pixels are used as port assets.

Native actor sprite rendering remains pending. Its resource allocation must
leave room for the existing background tile cache and text while accommodating
the two additional source sprite palettes; no unverified palette substitution
has been installed. Run `make test-intro-actors` for the state and sprite checks.


## Native scripted-sprite renderer checkpoint

The two verified scripted sprites now render natively, selected by their source
animation pointers. Original 16×16 tile shapes, offsets, flips, transparency,
and two-frame publication are preserved. This includes the windmill scenery and
nearby scripted character, **not** the separate blue-clad player sprite.
`tools/native_intro_sprites.py` fits their 28 original RGB colors into PAL1's
15 opaque entries with the existing chroma-sensitive fitter. Measured ordinary
RGB RMSE is 14.03 levels per channel across the converted frames. Original
palette RAM remains an explicit external input; source palette setup/fades and
final color-quality acceptance are not claimed.

All 74 distinct nonblank 16×16 blocks are resident, eliminating animation-time
pattern DMA. Each block is four column-major Genesis tiles. VRAM allocation:
background cache 16–1335 (1320 patterns), text 1336–1415, actor blocks in
1416–1535, 1664–1791, and 1984–2047. Plane A is C000, plane B E000, scroll table
F000, sprite table F400. These disjoint ranges keep the established background
patterns while fitting 296 sprite tiles. The expanded prefetch window needs at
most 1226 distinct background patterns in the current descent.

Background row preparation is split into twenty columns per frame, one row
ahead of the prior prefetch point. The source clear at 7D80 still emits/counts
all writes, but its known forty-column text pair is cleared once in renderer RAM.
Text sprites are rebuilt as runs of up to four occupied glyph tiles, omitting
spaces, which frees sprite bandwidth for the scenery behind the lettering.
The last text link is restored before appending actors, including on the first
visible actor frame; text changes are applied before the final sprite-list DMA.

The expanded native check samples 21 frames, including visibility onset,
animation changes, and message replacement. All 71,680 pixels per frame match
the independent compositor of converted assets. Actual native actor state still
matches the fresh arcade capture across 333 frames. The integration completes
402 updates with zero missed frames and zero cache overflows. Initial dynamic
uploads and per-tile text clearing failed these timing checks and were replaced;
those failed builds are not the saved checkpoint. This remains a bounded intro
test, not a full-game 60 FPS or arcade-color equivalence claim.


## Native fixed-player intro record and sprite

The blue-clad player is the fixed record FF9846 itself. The older catalog claim
that its initial shared position was (0,0) omitted 2F08's stage-dispatch call.
A0E0 → 2F08 → stage-zero table entry 2F66 → 2FBA loads checkpoint-zero (192,72)
from 3078 through 3012/3016. BF74 selects facing 3 and animation D15A on the
first scheduled update (761); its blocked sprite is code 0033, attribute 2220.
BFE8 holds the intro pose until global gate FF8348 opens. The catalog now corrects
both that initialization and the false attribution of the separate A2A6 actor.

`src/willow_arcade_intro_player.h` translates these selected position/state/pose
fields. The fresh headless trace matches 710 consecutive records, frames
691–1400, from source-defined initialization; it does not seed player state from
the trace. Ten source anchors cover initialization, selection, and gate handlers.
The gate-open branch has instruction evidence only because this trace retains
a closed gate. Health, other record setup, gameplay control, movement, and the
full input handoff are not implemented by this header.

The native diagnostic executes the same player routine and matches selected
player fields over 402 frames. Its initial pose is rendered from original graphics.
PAL0 now jointly fits source lettering and player colors (24 distinct source RGB
values; combined RGB RMSE 11.93), while PAL1 retains the two scripted sprites
(RMSE 14.03). This preserves separate blue/red player colors without taking a
background palette. These are measured hardware approximations, not exact CPS
color reproduction.

Resident sprite blocks increase to 82. Background cache is 1296 patterns,
text tiles 1312–1391, sprite blocks occupy 1392–1535, 1664–1791, and 1984–2047.
The measured background working set remains at most 1226 patterns. All 21 native
composite checkpoints match and 402 updates have no missed frames or cache
overflows. Source validation now covers 15 object-record/visibility checks and
13,590 visible sprite pixels across five captures. It accounts for text occlusion
and source object order: scripted character in front of player, player in front
of windmill. Earlier counts/VRAM layouts above describe prior checkpoints.


## Dialogue task selector 4: translated, display integration pending

The task word at A394 is 0419, not message record 1049. Dispatcher 78F4 reads
the high byte as a handler-table offset: 4 selects 7B46, while message lookup
7994 uses only the low byte, 19. The corrected shared decoder now separates
handler and message index. Handler 6's 16-bit text streams are decoded as words
through their 002F terminator; their drawing task is not yet translated.

`src/willow_arcade_text_box.h` translates 7B6A–7C4E using original destination,
pattern and width tables at 7C50/7C64/7C78/7C7E. Five panel phases occur on local
ticks 0,4,8,12,16. `src/willow_arcade_dialogue.h` enters the existing character
parser on tick 17 and uses source glyph base 7100 rather than 7000. This is
source behavior, not an inferred UI animation.

A fresh dummy-video/dummy-audio capture now runs through frame 1650 and saves
the controller trace and Scroll1 RAM. The opening panel starts at 1153; the
manager leaves its dialogue state at 1497. Native C produces all 928 panel
writes, matching every phase. Dialogue lettering, panel contents and completion
match 128,128 tile/code/attribute comparisons over 344 consecutive frames. The
message is the original warning about Bavmorda's reach beyond the village.
Run `make test-intro-dialogue` to reproduce the host verification. These routines
are not yet connected to the Genesis display; its current visible diagnostic
still stops short of the panel sequence.

The same fresh trace places the two following title states at 1497 and 1522,
and the actual source gameplay handoff at frame 1588 (major state 2, state 2,
intro gate 1). These are validation targets, not native timing constants. The
native manager/title tasks, wider panel renderer and complete handoff still
require translation/integration. Existing 402-update/21-frame Genesis checks
continue to pass after the decoder change.


## Complete unskipped manager and title-task translation

`src/willow_arcade_intro_sequence.h` now composes the source camera manager,
short messages, expanding dialogue panel and word-based title task. It translates
A360–A428's remaining waits and returns a handoff flag at the source boundary.
`src/willow_arcade_title_text.h` translates handler 6's two-byte codes, four-tile
writer at 7F12, column advances, delay and terminator, plus the complete 4096-cell
Scroll1 clear at 6642/65FE. Generated title data retains the original binary
codes; it is not invented text or a screenshot.

From initialization at frame 691 through the handoff at 1588, all 898 consecutive
manager records match: state, timer, target/camera Y, message completion, major
state and gate. No manager state or completion signal is replayed into this
translation. The only runtime ROM input is the original panel/animation data.
Across 448 captured Scroll1 snapshots, 557,984 tile-code/attribute comparisons
pass, including the dialogue, both titles, and the clear between them. The
post-handoff HUD/gameplay writes are excluded from the text comparison because
those helpers are still untranslated; the boundary state itself is checked.
The title parser is trace-validated for these two one-line messages, not every
possible task-6 record. Run `make test-intro-sequence` to reproduce the checks.

The class-12 scheduling predicate now includes waiting states 22 and 24, whose
branches also reach A3E2. The visible Genesis diagnostic still uses its earlier
manager while the panel/title display is integrated. Its 402-update/21-composite
regression checks continue to pass. This change does not execute 4F44, 4035E,
9A92, native player gameplay, audio commands, palette tasks or skipped-intro
branches, and is not a completed playable port.

## ROM-derived Scroll1 asset bank and palette provenance

`tools/native_intro_scroll1.py` now extracts the six intro messages and five
panel phases directly from original task records. It writes 281 tile-code
entries with both 8x8 layouts, deduplicated to 208 patterns (6,656 bytes).
The packed patterns preserve every original opaque pen and transparency;
the source palette bank preserves the original CPS color words. They are
assets for the pending native panel/title renderer, not recorded frame playback.

The Scroll1 palette setup at 9CA–9D6 copies the fixed ROM bank E6EA0.
`tools/cps1_palette.py` now exposes this bank. Existing native text/player
and scripted-actor generators use ROM palettes instead of captured palette
RAM. Regenerating their graphics and palette headers produces byte-identical
output. The native source-word reader now includes the original panel tables
7C50–7D70 and returns FFFF for unsupported addresses rather than indexing
outside its animation array. The full intro sequence test exercises this same
generated reader and compares every requested word to the original program.

A fresh unskipped arcade run used dummy SDL drivers with `-video none -sound
none`; no emulator GUI was launched. Eleven reference images cover all five
panel expansion phases, dialogue lettering, and both following titles.
`make test-intro-scroll1-assets` verifies 78,160 opaque source-layer pixels and
704 palette words against those images and live palette RAM. Transparent
pixels and Genesis palette fitting/compositing are outside this layer-only
check. The panel is gold and cyan with transparent ornament and an opaque
black interior; the title reads “STAGE 1 / CROSSROAD.”

`make test-intro-sequence` still passes 898 manager frames and 557,984 tile/
attribute comparisons across 448 snapshots using the generated native ROM
reader. `make test-intro-probe SGDK_ROOT=/tmp/willow-sgdk-resume` also passes:
402 native updates with no missed updates or overflow, and all 21 converted
composites remain exact. These checks do not establish 60 FPS for the complete
game. The visible Genesis diagnostic still lacks the panel/title compositor,
gameplay handoff helpers, gameplay, audio and other unfinished port work.

## Native stationary-forest plane transition

`tools/native_intro_forest.py` derives the stationary camera-Y=0 forest from
the original map/graphics/palette data. It preserves 1,040 fully opaque native
foreground cells and fills the remaining 80 cells from source layer pixels.
This scene needs 692 distinct resident patterns, with no new patterns and no
visible color change relative to the existing two-plane native display. This
is a property of this stationary intro scene, not a claim that the scrolling
game can use one background plane.

The native renderer now transitions one complete row per VBlank after both
camera publication stages have settled at zero. It releases that row's old
layer references and queues the opaque replacement and a blank BG_B row in
the same VBlank. All 28 visible BG_B rows are free by update 354, leaving the
second Genesis plane available for high-priority Scroll1 text without a
window plane or large single-frame upload. The title/panel writer still needs
to be connected to that plane; this change does not display the dialogue yet.

The headless native test checks all 402 updates, the exact 28-row transition,
and that all 1,120 visible BG_B cache references were released. It reports zero
missed updates and zero cache overflows. Pixel verification now includes every
transition frame (updates 326–356), for 49 full composites and 3,512,320 pixel
comparisons with zero mismatches against the prior converted source composite.
This preserves the existing Genesis palette approximation; it does not claim
RGB identity with the arcade or full-game 60 FPS.

## Native dialogue, titles and unskipped manager display

The Genesis diagnostic now runs `WillowArcadeIntroSequence` through the source
handoff boundary, including the five expanding dialogue-panel phases, the
warning text, “STAGE 1,” and “CROSSROAD.” The stationary forest transition frees
BG_B, whose high-priority text pixels sit over low-priority actor sprites and
the opaque BG_A forest. Transparent frame ornament still shows the windmill
and scenery underneath. The existing 320-pixel center crop also crops the
arcade's wide dialogue frame; no lettering or border geometry is rescaled.

`tools/native_intro_overlay.py` converts original glyph/panel patterns and
the fixed Willow pose into a shared late-intro PAL0: 35 original colors fitted
to 15 opaque Genesis colors, weighted RGB RMSE 14.96. PAL1 actors and the two
background palettes are unchanged. This palette switches only when the source
dialogue task starts. The late player patterns are prepared in the unused
lower caption row after the final single-line caption completes. Overlay
patterns are pinned incrementally during preceding idle updates, so no large
first-frame graphics upload is required.

The native panel writer uses predecoded ROM-derived destination/pattern pairs
instead of repeatedly decoding them during the display update. The original
dialogue task still controls every phase and wait. A dedicated comparison with
the direct C translation validates all 928 logical writes and the cropped
native commands for all five phases. Full Scroll1 clears are coalesced into
the visible map while preserving the source write count. No recorded timing,
frame images, or captured state drives these native routines.

Extended source evidence corrected the scheduling at A3BA–A3DE: the transition
from dialogue to the first title returns without an actor update. A428 also
skips the intro actor scheduler at handoff. In contrast, A41A explicitly runs
the actor scheduler when entering the second title. The new scheduling helper
uses source state, busy flag and timer to preserve this distinction. Fresh
headless traces now cover actors and the fixed player record through frame
1587, before gameplay entry.

`make test-intro-probe test-intro-actors test-intro-dialogue
test-intro-scroll1-assets SGDK_ROOT=/tmp/willow-sgdk-resume` passes. Native
checks cover all 898 manager/timer/message/write-count updates, 1,656 scripted
actor records, and 897 fixed-player records. There are zero missed updates and
zero cache overflows. All 61 converted native composites match, totaling
4,372,480 pixel comparisons, including each expansion phase, later dialogue,
both titles, and the last scanout before handoff. Original Scroll1 reference
checks remain 78,160 opaque pixels across 11 arcade images, with exact original
palette words. The updated build and evidence are in `out/diagnostics/intro`.

This remains a diagnostic rather than the full playable port. It reaches and
reports the source handoff boundary but does not execute gameplay entry helpers
4F44, 4035E or 9A92. Audio, palette fades, the skip path, gameplay systems and
the full campaign are not completed or covered by this 898-update result.

## Stage-entry object pool and player gate evidence

`src/willow_arcade_stage_objects.h` translates 9A92–9AD6 and allocator 2CA4.
It scans the original 48-record, 64-byte pool for an inactive slot and copies
only the fields written by the source routine. It preserves unrelated bytes,
stops when allocation fails, and reads the stage descriptor table from ROM.
The native diagnostic now calls this initializer at its handoff boundary.
Five Stage 0 records are created. A fresh headless capture of RAM immediately
before and after frame 1588 matches all 3,072 native pool bytes. Host tests
also cover full and partially exhausted pools. This establishes allocation
and initialization, not those records' gameplay handlers or collision logic.

The same capture extends the fixed player evidence through the intro gate.
Frame 1588 opens the gate but does not update the player. At 1589, BFE8 changes
facing from 3 to 2, selects animation D14A, and sets the countdown to 16. C00C
counts down on subsequent updates and enters player state 2 at frame 1605.
The existing C translation matches all 19 records from 1587 through 1605 from
source-driven initialization. This gate-open path is now host trace-verified;
it is not yet integrated into the native renderer's post-handoff updates.

This investigation also found and corrected a defect in the late-intro player
asset conversion: it had decoded D15A's blocks without applying the original
horizontal flip. The previous native pixel comparison used the same converted
asset and therefore could not detect that source-conversion error. The new
`tools/verify_intro_overlay_player.py` checks the complete 48×48 pose against
original graphics and the source attribute, including transparent/missing
blocks and the palette mapping. All 2,304 pixels match after the fix. Earlier
claims of converted-composite parity remain valid only at that stated scope;
the old late-pose asset was not faithful to the original orientation.

`make test-stage-entry` checks the ROM instruction anchors, original object
pool, allocation failure behavior, and player gate transition. The native
test additionally checks all 3,072 stage-pool bytes at update 898. The complete
898-update/61-composite regression still passes with no missed updates or
cache overflow. The diagnostic checkpoint includes the corrected pose and
native stage initializer. HUD initialization (4F44), the other entry routine
(4035E), object behavior, gameplay, audio, fades and skip handling remain work
required for the full port.

## Palette-controller setup and stage spawn-table translation

`src/willow_arcade_palette_controllers.h` translates 4109A–410EA and the
16-record allocator at 2C86. BC4 dispatches this pool to handlers whose
destination is palette RAM (C2C loads 91C000); these are palette controllers,
not moving sprites or animated graphics-tile descriptors. Stage 0 creates six
records. The native diagnostic now initializes them at handoff before 9A92.
All 1,024 bytes match the original FFF042 pool at frame 1588. Host tests cover
exhaustion, partial allocation, retained bytes, and the negative count sentinel.
The per-frame palette handlers are not yet executed by the native diagnostic.

Adding this setup exposed a CPU overrun while queuing the handoff's cleared
text plane row by row. The native renderer now queues that complete cleared
map as one contiguous transfer; its 3,584 bytes plus the sprite transfer fit
the VBlank budget. The 898-update regression again has zero missed updates or
cache overflows, and all 61 converted composites match. Both native entry
pools are compared byte for byte at the boundary: 3,072 object-pool bytes plus
1,024 palette-controller bytes.

`src/willow_arcade_stage_spawns.h` translates the table walker at 40374–403F0,
including class/selector-specific eligibility, unsigned coordinate boundaries,
and the original destination fields. Allocation is a callback because each
class consumes a separate source free list. Stage 0's table creates 57 records:
19 class-4, 30 class-6, two class-12, and six class-16 records. A host adapter
uses the original allocator count/cursor operands and the captured *pre-entry*
RAM to model those free lists. All 3,648 resulting record bytes, four counts,
and four cursors match the post-entry capture. Thirteen additional synthetic
cases check the excluded rectangle's exact edges, minimum-X cutoff, selector
flags, and unsigned wrap behavior; allocation failure and an empty table also
pass. ROM instruction anchors verify the branch conditions and copy offsets.

This 57-record table walker is host-verified, not yet integrated with native
gameplay pools. The captured pre-entry RAM is test setup only and is never
used as native runtime state. The Stage 5 caller's two-table branch is not
translated here. Source free-list initialization, native gameplay pool
integration, HUD setup, record handlers, and the remaining port work are
still required. Run `make test-stage-entry` for the entry checks and
`make test-intro-probe SGDK_ROOT=/tmp/willow-sgdk-resume` for the native regression.

## Code-initialized native gameplay actor pools

`src/willow_arcade_actor_pool.h` now translates the source pool initialization,
pop, and release operations for the Stage 0 classes. Class 4 has 50 records
of 128 bytes; class 6 has 48, class 12 has 60, and class 16 has 20 records of
64 bytes each. Initialization clears each record, sets its class tag at +30,
and preserves the source stack order (highest index allocated first). Release
clears only the fields cleared by the corresponding source routine and pushes
the slot back for immediate reuse. Instruction anchors cover the capacities,
strides, tags and reset fields. Host tests verify retained bytes and exhaustion.

The native diagnostic initializes these pools from code at startup. A28A/A2A6's
intro spawn consumes the top two class-12 slots. At handoff the translated
40374 table walker creates all 57 stage-entry records in those pools. Neither
captured free lists nor captured record contents are runtime inputs. The
immutable source descriptors and selector eligibility are decoded at startup;
coordinate eligibility, allocation, record activation and the field writes
still occur at the source handoff. Preparing immutable descriptors earlier
avoids decoding the ROM repeatedly during the already busy handoff frame.

The source test now also initializes its pools from code instead of seeding
pre-entry RAM. Its 57 resulting records match 4,864 captured bytes, including
the full class-4 records' 128-byte extent. Native headless checks compare all
four capacities, strides, stack contents, counts and cursors. They compare
14,464 pool bytes, covering allocated and unused records. The two original
intro class-12 records are excluded from this raw-byte comparison: their
selected state fields still live in the separately verified intro structs,
rather than complete mirrored CPS records. The other 176 pool records match.

The four native pools occupy 14,592 bytes. Their allocations fit after reducing
the SDK transient DMA copy buffer from 8 KiB to its 2 KiB minimum; long-lived
graphics sources use ROM or static RAM directly. This changes copy-buffer RAM,
not the per-VBlank transfer budget. Pools are separate allocations so the
existing SDK heap fragmentation does not require one 14,592-byte contiguous
block. The verified build reports 3,098 bytes of remaining heap. All 898
updates still pass without missed updates or cache overflow, and all 61 image
checkpoints match the converted source composition.

This completes native Stage 0 spawn allocation/initialization for the decoded
57-record table, not the records' behavior. Their handlers, native player
gate/gameplay updates, HUD entry, audio, palette updates, skipped-intro path,
other stage entry branches and full campaign remain unfinished. The updated
diagnostic and full verification log are saved in `out/diagnostics/intro`.

## Native player gate and ready pose

The diagnostic now continues the fixed player's BFE8/C00C transition after
the intro handoff. The player is unchanged at source frame 1588, turns from
facing 3 to 2 and selects D14A at 1589, then counts down from 16 and reaches
player state 2 at 1605. The native comparison covers all 915 selected player
records from initialization through that boundary, including these 17 new
updates. It reports zero missed updates and zero cache overflows. The intro
manager/text comparison remains bounded to its original 898 updates.

The original D14A and D15A records share geometry, tile code, and palette;
their only rendering difference is horizontal flip. Asset generation now
asserts that relationship directly from ROM. The renderer mirrors the already
resident 16×16 chunks, reversing their positions within the 48-pixel actor
width and enabling each chunk's hardware H-flip. No extra sprite patterns or
palette changes are needed for the ready pose.

`tools/verify_native_player_gate_pixels.py` compares the actual native scanout
with original player graphics/attributes converted to the active palette.
Four checkpoints straddle the facing change and countdown, with 2,904 visible
player pixels matching. Pixels occluded by the diagnostic's held scripted
character are excluded explicitly. This is a player-only check, not full
post-intro scene parity: scripted actors are still held after handoff, HUD is
absent, and the normal gameplay handlers do not run. The preceding 61 complete
intro composites still match. Run `make test-intro-probe test-stage-entry
SGDK_ROOT=/tmp/willow-sgdk-resume` to reproduce the checks.

The updated diagnostic reaches player state 2 but does not implement its
movement, combat, collision or other gameplay behavior. Native gameplay-loop
scheduling, HUD setup, actor/palette handlers, audio, skip handling and the
remaining stages still require work before this is the requested playable port.

## Native player movement primitives and instruction oracle

`src/willow_arcade_player_motion.h` translates C52A (horizontal facing),
C55A (crouched facing), 208C (two-axis fixed-point movement), 20B4
(horizontal movement), and C72E (falling after collision). The functions use
original big-endian record offsets, signed table deltas, unsigned longword
position wrapping, and the source's word-sized speed addition before unsigned
clamping. C72E also performs the original falling-state field writes. Collision
itself and the enclosing player-state dispatch are not implemented by these
functions. These helpers are not yet connected to the native intro diagnostic.

`make test-player-motion` executes the unchanged source instructions in the
installed Genesis Plus GX 68000 core with no GUI, video surface, audio output,
or input interaction. A test-only Genesis ROM wrapper provides reset vectors,
a header and an IRQ-masked return loop; the five original routines and their
movement tables retain their original addresses and bytes. This wrapper is an
oracle artifact, not a port or a distributable game. The C implementations are
compared with the original instructions across 5,120 cases and all 96 bytes of
each player record, including bytes the functions should preserve. Tests cover
all 256 input masks with all four valid starting facing values, random
fractional/wrapping positions, every byte-valued facing for 20B4's unusual
EXT.W/ADD.B indexing, contact and non-contact falling, clamping boundaries and
word-overflow speeds. All comparisons pass. The helpers also compile for
68000 with warnings treated as errors.

This establishes the shared movement arithmetic and facing rules. Connecting
them to the player state machine, collision maps, animation, attacks and
native gameplay scheduling remains necessary; no new playable-ROM or full-game
frame-rate claim follows from this unit-level instruction parity.

## Native player floor, ceiling and wall collision

`src/willow_arcade_player_collision.h` now translates the player's tile
collision routines: Scroll2 cell addressing (3E28), foot samples (3E4C),
paired feet/material merging (38F6), head samples (3F62), paired head probes
(39FE), right/left wall samples (4306/43BC), four wall probes (3772), and
facing-side wall probes (3814). Geometry and collision descriptors come from
the original ROM. The implementation preserves ordered probes, corrections
before later probes, integer/fractional coordinate writes, slope direction,
material flags, signed ceiling/right-wall offsets, unsigned left-wall offsets,
and the special second tile lookup for floor descriptors with bit 6 set.
The callback interface reads original Scroll2 high attribute bytes, before
Genesis palette fitting discards unrelated source metadata.

`make test-player-collision` passes 76,800 differential cases. It compares all
96 player-record bytes, final sample coordinates, and the returned contact
value with the original 68000 routines. The test ROM changes exactly one
collision-code immediate: the Scroll2 base at 3E44, from CPS address 918000 to
available Genesis work RAM FF4000. The expected original immediate is asserted
before replacement. This relocation supplies the same tile bytes to both
implementations; branch instructions, descriptor data and collision arithmetic
remain original. As in the movement oracle, the Genesis wrapper/header and
return loop exist only for testing. No GUI, sound output, or app interaction
is used.

The first 64,512 cases cover every original collision descriptor 0..46 and
all local X/Y positions for single samples, plus randomized multi-probe
records/maps. The remaining 12,288 cases use a ROM-generated forest window.
`tools/verify_stage0_collision_map.py` independently checks all 384 visible
high attribute bytes against source Scroll2 RAM captured at frame 1530.
This includes the non-collision high-byte bits as well as the collision type.
The generated window is a test fixture; it is not captured data used as a
runtime asset, and it does not claim parity for the arcade map-streaming
scheduler or unloaded portions of the map. The forest test finds floor and
wall contacts; its sampled locations have no ceiling contacts, while the
synthetic descriptor tests exercise 2,176 ceiling contacts.

The collision code compiles for the target 68000 with `-ffreestanding` and
warnings treated as errors. The preceding 5,120 movement cases still pass.
Logs are `build/player-collision-verified.log` and
`build/player-motion-verified.log`. These routines are native C building blocks;
the player state machine, map-streaming integration and gameplay scheduler
still need to call them. The diagnostic/release ROMs were not replaced by
these isolated tests, and a playable full port remains unfinished.

## Ground-player handler, charging and state transitions

`src/willow_arcade_player_ground.h` translates the complete C3B6 ground-player
handler for its three source dispatch values (0, 2, 4), including both FF83DC
branches. It connects original facing/input handling, animation selection and
advancement, two-axis motion, floor collision and falling. Its transitions
include crouching, jump/down-jump selection and starting an attack. The new
states' handlers are not part of C3B6 and remain to be translated. Unsupported
dispatch values return an explicit failure instead of simulating a default.

C610/C6C0/C6F6 attack charging is also translated. The input context provides
shared-system callbacks for available effect slots, allocation, RNG and sound.
Call order matches the source, including the allocation threshold, the three
effect-record writes, charge cadence table, charge-release value, initial
charge sound and repeated full-charge sound. The callbacks are requirements
for integration, not silently omitted effects. The test supplies source-shaped
pool/RNG/sound services and checks their results against the actual ROM's
29D0, 1D8E, and 786A routines.

`make test-player-ground` passes 32,768 cases against original C3B6 instructions
using the ROM-derived forest collision window. Every case compares all 96
player-record bytes, final collision sample coordinates, charged-shot output,
the allocated effect's 64 bytes, free count/cursor, random seed/scratch byte,
and the entire sound queue. Cases exercise all held/pressed input combinations
across dispatch/facing/charge configurations, including alternate FF83DC
behavior and byte countdown boundaries. The final outputs include 1,116 attack
states, 31,135 jump/fall states, 512 effect allocations and 544 sound-producing
cases. These counts describe test coverage, not a gameplay recording.

The oracle clears D0 before entry, matching the upstream BF2C caller. C3B6's
MOVE.B dispatch preserves D0's upper byte, so arbitrary host-emulator startup
register state is not a valid entry precondition. As with collision tests,
only the Scroll2 RAM base immediate is relocated; handler logic and its
subroutines remain source instructions. The C handler compiles for 68000 with
freestanding compilation and warnings treated as errors. The verification log
is `build/player-ground-verified.log`.

This is a source-verified handler, not yet the full player update: top-level
C13E dispatch, its later wall/head checks, damage/health and other common calls,
new movement/attack states, rendering, shared effect/audio systems and gameplay
scheduling still need integration. No replacement playable ROM is claimed, and
the full native arcade-port goal remains unfinished. Testing stayed headless.

## Down-jump handler C8F2

`src/willow_arcade_player_drop.h` translates C8F2, the player substate-6
handler reached by the down+jump branch when the source material flag has
bit 7 set. All five phase values (0, 2, 4, 6, 8) are implemented: initialization,
two six-update countdown phases, the final six-update delay/pose change, and
eight updates of downward motion before returning to ordinary descending
substate 2/phase 4. Byte-sized timers wrap exactly as in the source. Falling
speed is compared before adding acceleration; no invented post-add clamp is
applied. Horizontal movement, charge-effect cadence, animation pointers and
facing-dependent source pose selection retain the original ordering.

`make test-player-drop` passes 32,795 comparisons with original C8F2 and its
original subroutines in the headless instruction oracle. These include 32,768
single-update boundary/input/state cases and one complete 27-update sequence.
Every update compares all 96 player-record bytes, effect allocation/cursor,
effect record bytes, RNG state and preserved charge/sound globals. The complete
sequence starts at (192,72), holds right, and ends at (210,49.875) in descending
substate 2/phase 4. It exercises this handler in isolation: the enclosing
player dispatcher's collision/common calls are not part of that endpoint.
There are 730 transitions to normal descent across the tests and 512 effect
allocations. The handler compiles for the target 68000 with freestanding
compilation and warnings treated as errors. Verification is recorded in
`build/player-drop-verified.log`.

This adds the full source down-jump handler, not the normal-jump handler C78A,
its midair attack scheduler, or full top-level player scheduling. Those paths
and the native runtime integration still require work. No GUI was launched
and no diagnostic/release ROM was replaced by these unit-test artifacts.

## Normal jump phases, ledge contact and climbing

`src/willow_arcade_player_jump.h` translates normal-jump phase routines
C7BC/C7EC/C820: initialization, ascent with ceiling checks, the apex transition,
descent, floor landing, landing sound and ledge transitions. It preserves the
source's signed fixed-point displacement, word-sized acceleration arithmetic,
pre-add falling-speed comparison, facing/animation updates and existing
contact flag behavior. These are the phase routines called by C78A, not yet
the complete C78A handler: its later CA1C attack scheduler and final horizontal
movement still need to be connected through the native dispatcher.

The required 3A84 ledge probe and 3FA2 ledge-eligible floor sample are translated,
including the other character's non-mutating 403C ceiling precheck. Their
semantics differ from ordinary floor collision: they require material bit 7,
do not apply the flat-tile near-edge rejection, and do not perform the second
floor-tile lookup. C8B6/370C/4442 falling effects are also translated. Tile type
32 requests a class-12 record with the original position/selector writes; the
eight-update cooldown starts even when allocation fails.

The complete CCF0 ledge-climb handler is now native C as well. Its seven phases
use the original CD74 position table, byte countdowns and D33A animation table.
Position table writes modify integer words without clearing the fractional
words. The final phase returns to standing and resamples the floor. Charge
cadence and effect allocation continue after every phase in source order.

`make test-player-jump` passes 65,536 instruction-oracle comparisons: 32,768
normal-jump phase cases and 32,768 complete climb-handler cases. Checks compare
all 96 player bytes, collision sample coordinates, allocated effect bytes,
free counts/cursors, RNG state and the sound queue. Jump tests cover the
ROM-derived forest plus uniform original collision descriptors, including
6,811 landings/sound requests, 570 ledge transitions and eight class-12 effect
allocations. Climb tests include 735 completions and 512 charge-effect
allocations. Both implementations compile for 68000 with freestanding
compilation and warnings treated as errors. Results are recorded in
`build/player-jump-verified.log`.

As with preceding collision oracles, the test-only CPS tile-RAM address is
relocated into Genesis work RAM; handler branches and collision data remain
original. All testing is headless. The new code has not replaced the running
intro diagnostic or been presented as a complete playable ROM. Midair attacks,
top-level player/common-state handling, map streaming and native gameplay
integration remain outstanding alongside the previously listed full-port work.

## Projectile creation and complete attack/airborne handlers

`src/willow_arcade_player_attack.h` translates CB24..CCEE's thirteen projectile
selections. Charge tiers, power-level mapping and object selectors come from
the original CB08/CB4C/CC50 tables. The translation preserves the four-normal-
shot cap, type-9 global gating, pool-exhaustion behavior, normal-shot counter,
standing/crouching spawn offsets, paired opposite-facing shots, the eight
class-12 spread records, and the separate class-2 effect request. Records are
initialized with only the fields written by the source; stale fractional and
other untouched bytes remain unchanged, as verified by the oracle.

CA1C's midair attack scheduler is now translated, including its input gate,
four/five-update timers, ground/crouch/air poses, shot-emission point and
recovery. This completes native C78A composition: jump phase, attack scheduler,
landing-to-attack transition, then horizontal movement. C9DA grounded attacks
are also translated, including their distinct recovery, C46E jump interruption,
vertical table movement, floor collision and falling. These are complete
individual handlers; the top-level C13E common player update remains separate.

`make test-player-attack` passes 32,768 instruction-oracle cases: 8,192 each
for CB24 projectile creation, CA1C scheduling, full C78A, and full C9DA. All
thirteen selections are exercised, with 10,259 successful allocations across
classes 2, 8 and 12. Every case checks all 96 player bytes, all 27 test pool
records (including unused records), three free counts/cursors, charge/selection
and active-shot globals, collision coordinates, RNG state and the sound queue.
Pool records begin with varied bytes so unintended clearing is detectable.

The initial test domain included charge values beyond the original table and
was corrected using source evidence: 4B0E reads seven charge caps from 4C08,
whose high bytes range from 8F to BF. Tests now span charge bytes 0..BF and
power levels 0..6, including broad combinations beyond any one level's own
cap. The native entry rejects lookup indices outside the original 28-byte
CB08 table; it does not invent a clamp or additional projectile selection.
The source oracle executes unchanged handler/allocator/RNG/sound instructions,
with only the previously documented test-only Scroll2 RAM relocation.

The combined implementation compiles for 68000 with freestanding compilation
and warnings treated as errors. The verification log is
`build/player-attack-verified.log`. All tests stayed headless. This advances
player logic and object creation; it does not yet implement spawned projectile
updates, integrate all actor pools into the running native diagnostic, or
complete damage/health/common player calls, the gameplay loop, audio and the
remaining full-port requirements. No finished playable ROM is claimed.

## Sliding, six-way movement dispatch and player bounds

`src/willow_arcade_player_slide.h` translates the complete CDC2 handler for
player substate 10. All five phases are implemented: startup sound/pose,
countdown, fixed-point horizontal sliding with facing-side wall collision,
deceleration/recovery, and return to standing. Recovery can accept the source
jump/down-jump transitions. The common tail processes attack input first;
when an attack starts it returns before vertical movement/floor collision,
exactly as the original branch does. Otherwise it applies the original vertical
table displacement, floor collision and falling. Speed subtraction wraps as a
word, and a wall hit does not perform that subtraction.

`src/willow_arcade_player_movement.h` connects all six normal-mode branches
(standing, jumping, attacking, dropping, climbing, sliding), then calls the
shared wall and head collision routines. This translates C144..C156, the
movement prefix of C13E. It deliberately does not claim to implement C13E's
mode gate or its later health/damage call. The C33E lower-world boundary and
C392 camera-relative horizontal bounds are implemented separately for their
correct eventual position after health processing. They preserve signed word
comparisons, wrapped boundary additions, fractional coordinates, the early
left-clamp return, and the lower-bound rebound/death-state branches.

`make test-player-movement` passes 98,304 comparisons: 32,768 full CDC2 cases,
49,152 movement-prefix cases (8,192 per dispatch branch), and 16,384 independent
lower/screen-bound cases. The movement oracle executes the original dispatcher
and collision instructions, with a test-only RTS at C158 to end precisely
before the health call. This scoped replacement is in addition to the already
documented test-only Scroll2 RAM relocation; no native runtime executes this
oracle ROM. Boundary routines are invoked at their original entries. The tests
compare all player bytes, collision coordinates, allocation effects, charge
and projectile globals, RNG state and sound queues. Sliding exercises 512
charge-effect allocations and 6,990 sound-producing cases; the dispatcher
exercises 1,435 allocations across its pools.

The composed implementation and boundary helpers compile for the target
68000 with freestanding compilation and warnings treated as errors. Results
are saved in `build/player-movement-verified.log`. Testing stayed headless.
The native intro diagnostic has not been relabeled or replaced with a claimed
finished port. Completing health/damage and other common player processing,
then integrating the player, projectile/actor updates, renderer, map streaming
and game scheduler remains necessary for the requested playable native game.

## Common health processing and full normal-player update

`src/willow_arcade_player_health.h` translates C170 and its required routines:
C2B8's alternating tile-hazard probes, 3A66C damage/armor-item loss, invulnerability
and hurt timers, brightness-preserving palette flashes, 4A34 health/armor HUD,
and 4CE4 inventory-icon redraws. Armor items are removed in the original
35C/35A/35B priority. The two flash timers preserve their different reset
behavior, and the palette decision uses BCHG's *old* bit value. Each of the
sixteen affected palette words retains its current brightness nibble before
OR-ing the original ROM color. HUD tile codes, attributes, addresses and icon
tables are original; fitting those updates into the Genesis renderer is still
an integration requirement.

`src/willow_arcade_player_update.h` now translates the complete C13E normal
Willow-player routine. FF83EA mode zero dispatches normal movement; mode two
runs wall/head collision without movement dispatch; other nonzero modes
return immediately. Active paths then execute health processing, the lower
world bound and the horizontal screen bounds in source order. This supersedes
the preceding movement-prefix-only status. BF42's outer player-state dispatch,
4B0E charge-HUD processing, actor rendering and the enclosing game scheduler
are not part of C13E and remain separate work.

`make test-player-update` passes 45,056 comparisons: 8,192 standalone C170
cases and 36,864 full C13E cases. The latter comprises 4,096 cases for each
normal movement branch and each tested nonzero mode (1, 2, 3). Comparisons
include all 96 player bytes, health/inventory globals, collision samples,
all 32 player palette words, the entire 16 KiB text RAM fixture, sound queue,
RNG state, and allocated records/free counts/cursors for three pools. The full
update cases exercise 425 allocations; standalone health cases include 600
hurt-active records, 124 dying-state records and 1,408 sound-producing cases.

The full-update oracle no longer inserts the movement prefix's early RTS.
Original C13E and all translated callees execute to their real return. The
only code-data adjustments in the test ROM are verified immediate addresses:
Scroll2 map RAM, player palette RAM and four HUD base-address loads are moved
into separate Genesis work-RAM fixtures. Native runtime code uses callbacks
for those services. No branch, timer or logic opcode is replaced. Both the
common health and composed player code compile for the target 68000 with
freestanding compilation and warnings treated as errors. Verification is saved
in `build/player-update-verified.log`.

All checks remained headless. This is verified native normal-player logic,
not a completed game or an updated playable ROM. The running diagnostic still
needs the full player record and globals initialized from source, outer-state
and frame scheduling, graphics/palette/HUD service integration, and the
remaining actor/projectile, audio and campaign work before release.

## Native ROM integration: full player intro record

The native intro diagnostic now owns an authoritative 128-byte player record,
replacing the partial intro struct as its state source. The renderer/debug
struct is derived from that record only when a gate update changes it.
`src/willow_arcade_player_record.h` translates 2928's 32-longword clear,
Stage 0/checkpoint-zero setup at 2F08/3012, and BF62/BF74/BFC2/BFE8/C00C's
intro initialization and ready countdown. It includes the original record
field resets, movement pointer, facing cache, acceleration fields, animation
pointer/timer and gate-global writes. It neither seeds the runtime from
captured RAM nor claims other-stage/checkpoint initialization.

The diagnostic's four-health setting is selected through the original 1BF2
DIP table (index 1), followed by the source 2D60/2F00 initialization convention.
The captured DIP byte independently confirms bits 0C equal 04. Six native
gate-global bytes at handoff match HP, armor, their cached values and charge.
The portable player headers now use SGDK's existing fixed-width type aliases
when compiled by SGDK, and standard stdint types in host/freestanding tests.

The expanded headless core check compares 87,840 raw captured player bytes:
96 bytes per update for all 915 updates through source frame 1605. It also
checks the remaining 32 bytes at three handoff checkpoints, using the full
captured work-RAM record's unchanged tail. Thus all 128 bytes are checked at
handoff, while the per-update capture scope remains explicitly 96 bytes.
The earlier selected-field player checks remain in place as renderer-view
checks. Stage pools, spawn counts/stacks, text, camera and pixel checks still
pass. Native execution reports zero missed updates and zero cache overflows;
remaining heap is 2,962 bytes.

The initial integration exposed a real handoff-frame overrun. Avoiding repeated
record-to-renderer conversion during the unchanged gate wait was insufficient
alone; two new cold-start ROM lookup comparisons had also added work to every
hot table read. Moving those cold-only addresses after the existing hot ranges
restored the budget without changing source scheduling or spreading handoff
initialization across frames. The final build passes the full intro suite and
the expanded raw-record check. Its 61 full intro image checkpoints and four
player-ready checkpoints retain the preceding pixel parity.

The refreshed diagnostic, symbols, manifest and verification logs are in
`out/diagnostics/intro`. ROM SHA256:
`14e8d21e97aea32e5a26584243a9e07e43be268f4f6b41ffe4918053285c97be`.
Reproduce with `make test-intro-probe test-stage-entry
SGDK_ROOT=/tmp/willow-sgdk-resume`. No GUI was launched. The diagnostic still
ends its implemented player behavior at ready state 2; normal-player C13E,
outer scheduling, runtime graphics/services and the remaining game systems
must be integrated before it is the requested playable port.

### Native player tables and shared RNG/sound producers

`tools/native_player_data.py` now extracts 2,102 bytes of original ROM tables
into `native/intro_probe/src/player_data.h`: player animation records, movement,
collision descriptors and probe offsets, charging/projectile selection, climb
positions, inventory icons and health palettes. These are original data, not
captured player state. The separate lookup leaves the intro's timing-sensitive
lookup unchanged. Unsupported or odd addresses return failure rather than a
plausible default word. The normal-player differential test now reads these
packed tables, verifies each requested value against the original ROM, and
fails on a missing address. This establishes coverage for the tested normal
player paths, not all future player states or characters.

`src/willow_arcade_services.h` translates RNG entry 1D8E, including its scratch
byte, and the four sound producer entries 7818/7824/783C/7844. The sound ring
preserves 64 words, original byte offsets, wraparound, two-byte gating state,
and command terminators. This is the producer only; playback and consumer
scheduling are not yet integrated. The normal-player oracle now uses these
shared C services instead of its earlier local RNG and sound stand-ins.

Headless verification:
- `make test-arcade-services`: all 65,536 possible RNG seeds and 1,024 sound
  cases match execution of the original instructions, including every even
  ring write offset, four gate combinations and all four producer entries.
- `make test-player-update`: 8,192 health and 36,864 complete normal-player
  cases pass; the latter uses the packed data and shared service translations.
- Packed data and shared service wrappers compile with freestanding 68000
  GCC, `-O2 -Wall -Wextra -Werror`.

Logs: `build/arcade-services-verified.log`, `build/player-data-verified.log`.
The native diagnostic ROM is unchanged; C13E still needs runtime scheduling,
collision-map storage, pools, palette/HUD rendering and complete actor services.
This does not establish whole-game performance or completion. No GUI launched.
The Makefile now forces SDL dummy video/audio drivers for development recipes;
explicit GUI launches remain separate and require the user's request.

### Native class-2/class-8 player pools and renderer RAM recovery

The native diagnostic now initializes the original two player allocation pools:
class 2 (20 records of 64 bytes, source base FF98C6) and class 8 (20 records of
64 bytes, source base FFC2C6). Both use the source descending free-list order,
clear records and class tags. They remain unused during the verified intro,
as in the arcade. The six native pools total 17,152 record-storage bytes;
17,024 bytes are compared at handoff, retaining the documented exclusion of
two class-12 intro records represented by separate selected-field structs.
All 218 free-list entries and all six counts/cursors match the capture.

The first build exposed a real allocation failure. The renderer reserved
64 tile-reference columns per row despite only loading/releasing the visible
40 columns. Its two reference arrays now use 40 columns, recovering 3,072
bytes. The DMA tilemap buffers still use the hardware's 64-column layout.
The headless check's resident-array address calculation was updated to the
new storage layout; all existing release, raw-state and image checks remain.

`make test-intro-probe test-stage-entry SGDK_ROOT=/tmp/willow-sgdk-resume`
passes: 915 native updates, zero missed updates/cache overflows, all raw-player
checks, 61 complete intro image checkpoints and four ready-player checkpoints.
Remaining heap is 3,358 bytes. Log: `build/player-pools-native-verified.log`.
The refreshed diagnostic and manifest are in `out/diagnostics/intro`.
Normal player C13E scheduling, map/HUD/palette services and gameplay rendering
remain to be integrated; this diagnostic is still not the finished game port.

### Source Scroll2 lookup and row/column transfers

`src/willow_arcade_scroll2.h` translates 4368E/436C2..43766 and the
43768/4382A transfer paths, including their chunk-transition helpers through
4395E. The lookup implements all four layout modes, returns the original
four-byte tile-record address, and retains actor fields +70/+74/+80/+112.
The transfer writes 34 cells per column or 42 per row, keeps full code and
attribute words, and preserves the source layout-index and tile-ring wraps.
This is the native data path needed for the mutable collision map; it does
not substitute direct world-map reads for the original loaded tile RAM.

`make test-arcade-scroll2` uses original unmodified instructions in the
headless oracle. It compares 65,536 lookup cases (address plus all 128 actor
bytes), then 4,096 row/column cases across all four modes (all 16,384 ring
bytes plus actor state). Destination ring addresses are supplied in Genesis
work RAM through A0; no source instruction patch is needed. The test also
loads the Stage 0 forest using the translated column transfers and original
stage pointer/mode tables, then compares all 384 visible tile codes and full
attributes against the captured CPS ring at source frame 1530. Captures are
verification only, never runtime initialization inputs.

These routines still need the native scrolling actor scheduler and ring
storage integrated with the renderer/player. They do not implement the full
Scroll2 actor state machine or dynamic map edits. The native diagnostic ROM
is unchanged. Verification log: `build/scroll2-source-verified.log`.

### Scroll2 initial centering and camera publication

The native Scroll2 module now includes original entry 42754 (initial camera
centering) and the complete 4282E..428C0 publication/strip-update tail. The
former retains signed 16-bit wraparound and the asymmetric initial bounds:
X checks its lower bound only; Y checks its upper bound only when its lower
bound did not apply. Neither modifies fractional coordinate bytes.

Publication preserves source X-then-Y transfer order and the bit-4 latch
condition: equality triggers the strip load and toggles the stored bit. It
reads original offsets from 43504, uses the previously verified lookup and
transfer routines, and then applies the seven-entry stage dispatch. Stage 1
retains the published X before its special 1536 coordinate replacement;
stage 2 latches the alternate-path state; stage 3 applies the 4608 lower bound
inside [4608,5120). All four camera bounds are copied to their source globals.
Camera movement and dynamic bounds selection before 4282E remain separate
and unfinished; this is not a claim that the full controller is integrated.

The headless instruction oracle now checks 8,192 initial-centering cases and
7,168 publication cases, alongside all preceding map lookup/transfer and
captured forest checks. Publication compares every actor byte, all tile-ring
bytes, both hardware scroll outputs, four bound globals and stage X. Its only
instruction operand relocation is verified 4365A: CPS RAM 918000 becomes the
oracle's FF4000 buffer. No branch or instruction behavior is replaced.
Target wrappers compile with freestanding 68000 GCC and warnings as errors.
Log: `build/scroll2-source-verified.log`. The diagnostic ROM remains unchanged;
all tests are headless and the full native game remains unfinished.

### Vertical camera motion and stage-event bounds

`src/willow_arcade_scroll2_motion.h` translates the full eight-state vertical
camera dispatch at 42B1C, its upward-motion helper 42BAC, and ROM-driven lower
bound selection at 42E66. It preserves signed versus unsigned comparisons,
word wraparound, fractional acceleration, airborne two-pixel tracking, bobbing
velocity reversals, and the one-/three-pixel transition states. The first
oracle run caught a decimal/hex offset transcription in the ROM-bound enable
byte; the corrected implementation uses actor byte 103 (source +0067).

The same module implements all seven stage-event handlers at 42D66 and the
stage-3 vertical-transition regions at 42D16. Both stage-3 event gates are
checked sequentially, preserving the second event's overwrite when both fire.
It retains stage-1 gate precedence, unsigned region endpoints, and source
updates to shared gate flags and stage X.

`make test-arcade-scroll2-motion` executes unmodified original instructions
headlessly: 32,768 vertical cases check all 128 camera bytes across eight
states; 28,672 stage-event/region cases check every actor byte and the written
shared globals, including region edges and every event-gate combination.
Freestanding 68000 wrappers compile with `-O2 -Wall -Wextra -Werror`.
Log: `build/scroll2-motion-bounds-verified.log`.
Horizontal camera tracking, cold controller setup and native runtime wiring
remain unfinished; the diagnostic ROM is unchanged and no GUI was launched.

### Complete active Scroll2 camera update

`willow_arcade_scroll2_motion.h` now implements all eight horizontal states
at 42924, including shared tracking at 4298E and the advancing left bound
at 42E4E. It retains signed-distance tracking, unsigned upper-bound clamps,
fractional forced scrolling, stage-end player record writes, alternate-path
player motion, and source state-transition delays. The shared minimum-bound
helper sees the prior camera position, as in the original instruction order.

`willowScroll2Active` combines the full active entry 4281E: stage-event bounds,
horizontal movement, stage-3 region transitions, ROM-driven vertical bounds
and motion, hardware publication, exposed tile strips, then stage outputs.
The source target coordinates in actor +106/+108 remain caller inputs;
outer controller selection and cold initialization are not implemented by
this entry. Native runtime scheduling/storage/graphics integration remains
outstanding, and the diagnostic ROM has not been replaced.

`make test-arcade-scroll2-active` passes 14,336 horizontal-only cases and
14,336 complete active-update cases covering seven stages and all eight
horizontal states. It compares all 128 camera bytes, all 128 player bytes,
all 16,384 map-ring bytes, written event/stage globals, both scroll registers
and all four published bounds. Only the verified CPS tile-RAM base operand
is relocated in the headless oracle; the original instruction flow is intact.
A complete active-update wrapper also compiles with freestanding 68000 GCC
using `-O2 -Wall -Wextra -Werror`. Verification log:
`build/scroll2-active-verified.log`. No GUI launched; full port still unfinished.

### Cold Scroll2 controller and chunk initialization

`src/willow_arcade_scroll2_start.h` implements source 548C (one complete
16x16 chunk load), all five cold phases at 42638..4281C, and outer target
selection/state dispatch at 42602. Cold setup reads the original stage map
pointer, layout mode, bound-table pointer and enable byte; it retains source
layer-control masking, initial bounds, four-chunk batches, final recentering,
and bit-4 latch initialization. Stage 1 uses the original first-inactive
palette-controller allocation service, preserving untouched record bytes.
The outer dispatcher chooses player or FF0FB0/FF0FB2 alternate targets as
source FF8348 specifies, and connects cold state 0 to the verified active
state 2 handler. Target/global inputs remain caller-owned source state.

`make test-arcade-scroll2-start` passes 4,480 cases through original entry
42602 across seven stages, five phases, both target selectors and palette
allocation positions/exhaustion. It compares all 128 camera bytes, the full
16 KiB tile ring, all sixteen palette allocation records, scroll/control
registers and palette-scheduler call counts. The unrelated 0BC4 palette
scheduler is replaced in this oracle only by a verified call-counter stub;
its internals are explicitly outside this test. The source first-inactive
allocator itself is executed unchanged. The other verified operand relocation
redirects CPS tile RAM to the oracle buffer. Native code receives explicit
palette allocation and scheduler callbacks rather than silently omitting them.
A complete controller wrapper compiles for freestanding 68000 with warnings
as errors. Log: `build/scroll2-start-verified.log`.

The native diagnostic remains unchanged. Runtime ring storage, controller
scheduling, palette services and renderer/player integration still need work;
this does not establish completed gameplay or whole-game 60 FPS. No GUI used.

### Original palette animation and real cold-start scheduling

`src/willow_arcade_palette_animation.h` translates handler types 0, 1, 6 and
7 (C0A, D1A, 1224, 1252), including the shared unsigned camera-rectangle gate,
initialization/cleanup, byte timer wrap, cycling and sequence sentinels, and
the seven stage-1 color bands. C92 copies all sixteen original palette words
while ORing destination brightness exactly as the arcade does. RGB conversion
is not part of these handlers; no new color fitting or tint is introduced.
The ascending sixteen-slot BC4 scheduler dispatches these implemented types
and explicitly reports unsupported types 2..5 instead of ignoring them.

`make test-arcade-palette-animation` passes 32,768 cases, checking all 64
controller bytes and all 2,048 palette words against original instructions.
Only the two CPS palette-base operands are relocated for the headless oracle;
comparison normalizes those pointers back to their original address space.
Freestanding 68000 compilation also passes with warnings as errors.

The cold-controller test now runs the real, unmodified BC4 scheduler and these
original handlers. This supersedes the previous call-counter-stub test scope.
All 4,480 cold-start cases still pass, including mixtures of initializing and
already-running palette controllers, actual color changes, stage-1 allocation
and pool exhaustion. Both the native and original paths compare the entire
palette pool and palette RAM in addition to camera/map state. Its fixture uses
only translated handler selectors 0/1/6/7; handlers 2..5 remain outstanding.
Logs: `build/palette-animation-verified.log` and
`build/scroll2-start-real-palette-verified.log`.

No native diagnostic ROM was replaced. Native scheduling, memory storage,
renderer integration and the remaining palette handlers still need work.
This is source-verified module progress, not completed gameplay or a new
whole-game performance claim. All execution stayed headless.

### All palette handlers and integrated scheduler verification

The remaining palette types 2..5 are now translated in
`willow_arcade_palette_animation.h`, completing the eight BC4 handler choices.
This includes descriptor-based three-controller creation (F3A), paired sequence
creation (10CE/114C), event-triggered state changes, byte/word countdowns,
original random-call ordering, sound commands, and shared cycle-event clearing.
Failed allocations retain source partial-allocation behavior; the controller
allocator always selects the first inactive slot. Later slots can execute in
the same ascending scheduler pass, while newly allocated earlier slots wait.

`make test-arcade-palette-animation` passes 65,536 individual handler cases,
comparing every controller/pool byte, 2,048 CPS palette words, RNG state and
scratch byte, sound ring/cursor and mutable event globals. An additional 4,096
successive full BC4 updates across 32 scenarios verify allocation and execution
ordering over 128 updates per scenario. The original scheduler and handler
instructions execute unchanged, aside from the verified palette-RAM address
operand relocations. Freestanding 68000 compilation passes with warnings as
errors. Log: `build/palette-all-handlers-verified.log`.

The 4,480 camera cold-start cases now include all eight palette types, actual
controller spawning, RNG and sound-ring state alongside map/camera/color
comparisons. Log: `build/scroll2-start-all-palettes-verified.log`. This supersedes
the earlier restricted-selector cold-start fixture. Native ROM storage and
scheduling integration are still pending; the diagnostic ROM is unchanged.
No GUI launched. Full port completion and whole-game frame pacing remain
unproven and unfinished.

### Additional native renderer RAM recovery

The Plane A attribute buffer now reserves only 40 visible columns per row,
matching its row-by-row uploads. The Plane B buffer retains all 64 hardware
columns for its timing-sensitive bulk dialogue upload. VRAM layout, source
map data and drawing behavior are unchanged. The explicit Plane B clear size
was retained when replacing the rectangular array with two differently sized
buffers and a constant pointer table.

This recovers another 1,536 native RAM bytes. `make test-intro-probe
test-stage-entry SGDK_ROOT=/tmp/willow-sgdk-resume` passes all prior state,
915-update timing, pool and pixel checks, with zero missed updates or cache
overflows. Remaining heap is 4,894 bytes. Verification log:
`build/renderer-ram-verified.log`. The refreshed diagnostic and manifest are
in `out/diagnostics/intro`. Map/palette runtime storage and scheduling remain
to be integrated; this is not a completed gameplay build. No GUI launched.

### Remove duplicate tile ownership while preserving frame timing

The native renderer no longer keeps a second 5,120-byte array of per-cell
cache slots. Completed background rows already contain those tile indices
in their attribute buffers. A 64-byte row-ownership array marks completed
rows, while half-loaded prefetch rows remain tracked by the existing pending
row state. The release loop reads the stored tile index directly. A 16-word
prefix on the reference array avoids repeated index subtraction in the hot
loop; conversion back to a cache slot happens only when returning a free tile.

Initial versions exceeded the source update budget. Hoisting row pointers,
tracking ownership once per row, and directly indexing reference counts
restored the verified pacing. The ownership audit also exposed offscreen
background references whose attribute storage was later cleared for dialogue.
Those rows now retire once at the first full dialogue clear; repeating the
cleanup loop at the handoff frame exceeded its budget and was removed.

The headless smoke test independently reconstructs references from all owned
rows, partially prepared rows, and retained overlay patterns on every update.
It compares all 1,312 reference entries per update (1,200,480 comparisons over
915 updates), catching lost ownership independently of rendered pixels.
`make test-intro-probe test-stage-entry SGDK_ROOT=/tmp/willow-sgdk-resume` passes
these checks and all prior raw records, allocator stacks and image checkpoints,
with zero missed updates/cache overflows. Net RAM recovery is 5,024 bytes;
remaining heap is 9,918 bytes. Log: `build/renderer-ownership-verified.log`.
The diagnostic ROM, symbols and manifest were refreshed in
`out/diagnostics/intro`. Full gameplay/map/palette integration remains pending;
no GUI was launched and this is not a completion claim.

### Caption DMA and immutable spawn descriptors moved out of RAM

Caption glyphs now upload directly from immutable ROM font data. A ROM-backed
blank buffer supplies the original full caption clear; the renderer preserves
queued transfer order, text presence and sprite rebuilding. The former 2,560
byte writable glyph copy and first/last dirty-range bookkeeping are gone.
The late player patterns still reuse the same lower-caption VRAM locations.
No font pixels, text timing or palette mapping were changed.

`tools/native_stage_entry_data.py` predecodes the 57 fixed Stage 0 descriptors
from the original ROM and its coordinate-filter tables into const native
records. The runtime uses these ROM records directly rather than decoding
them into a 570-byte RAM array during cold setup. All six actor pools remain
at their original capacities; no gameplay record storage was reduced.

The complete native intro/stage-entry suite passes after both changes: 915
updates with zero missed updates/cache overflows, all prior raw-state/pool
checks, per-update cache-reference reconstruction, and all image checkpoints.
Remaining heap is 13,054 bytes, up from 9,918. Logs:
`build/caption-rom-dma-verified.log` and
`build/immutable-runtime-data-verified.log`. The diagnostic ROM and manifest
were refreshed in `out/diagnostics/intro`. Mutable map and palette runtime
integration are still pending; these savings do not complete the port.
No GUI launched.


### Exact mutable map storage and direct ROM-copy integration

`tools/native_scroll2_data.py` extracts every four-byte Scroll2 record in
original ROM `0x80000..0xbffff`: 65,536 records represented by 9,069 distinct
32-bit code/attribute pairs (including zero). No code or attribute bits are
quantized. Immutable pair values and source-address IDs occupy 167,348 ROM
bytes. `src/willow_arcade_map_store.h` represents the 4,096-cell CPS ring with
8,192 bytes of IDs plus caller-owned mutable overrides. Overrides retain
arbitrary individual word writes, share identical values, and track references.
Exhaustion returns failure without changing that cell; it never drops a write
silently. Each override occupies six bytes on the 68000.

The translated Scroll2 controller now accepts an optional original-ROM pair
copy sink through cold start, active update, publication, strip and chunk
transfers. All address traversal and camera logic remain shared with the
previous dense word-write implementation. The packed sink installs the
precomputed pair ID directly, avoiding dictionary searches and transient
mutable entries for ordinary ROM transfers. The word mutation path remains
available for actual gameplay changes. Source/destination alignment and range
checks reject invalid copies before changing storage. A failed multi-cell
transfer returns failure after any earlier completed cells; it is not a
transactional rollback of the whole transfer.

Headless verification:
- `make test-arcade-map-store`: all 65,536 source records, 24,576 arbitrary
  word mutations, 8,192 strip transfers, and 7,168 cold chunks preserve exact
  values and override reference counts. Direct strip/chunk copies run with
  zero override capacity. Invalid copies and explicit mutation exhaustion
  retain previous cells.
- `make test-arcade-map-controller`: the packed representation matches original
  68000 execution in 4,480 cold-start cases and 14,336 complete active updates,
  including the entire tile ring and source camera/player/global state. Cold
  cases also execute and compare real palette scheduling and palette RAM.
  The fixture seeds arbitrary prior RAM using host-only override capacity;
  this does not establish a full-game native override capacity requirement.
- Dense controller regression checks still pass, along with the prior lookup,
  transfer and publication instruction checks. Logs:
  `build/packed-map-controller-verified.log`,
  `build/map-copy-dense-regression.log`, and
  `build/map-copy-source-verified.log`.
- The complete copy-enabled controller and dictionary compile with
  `m68k-elf-gcc -m68000 -ffreestanding -O2 -Wall -Wextra -Werror`;
  a target static assertion verifies the six-byte override layout.

This integrates compact storage with the portable source-derived controller,
not yet the live Genesis runtime. Native allocation, source globals, palette
storage, renderer synchronization, and gameplay mutation capacity remain to
be connected and measured. The diagnostic ROM is unchanged. No full-game
performance or completion claim follows from these module checks. All checks
ran headlessly without launching an emulator application.


### Compact native camera ROM tables

`tools/native_camera_data.py` now generates `camera_data.h` from the original
program: 3,988 bytes in six merged spans. These retain the original addresses
for stage layout pointers/selectors, all seven stages' layout selector domains,
cold chunk offsets, exposed-strip offsets, vertical-bound enable flags,
relative bound-table pointers, and each bound lookup's full 256-byte addressable
domain. Domains follow the translated arithmetic, including wrapping source
coordinates; no frame capture or inferred camera behavior initializes them.
The byte/word readers reject unknown addresses and unaligned words explicitly.
They neither fall back to the full program nor execute its instructions.

The packed-map versions of both original-instruction controller tests now use
this reader for all camera data accesses. Every access additionally checks the
returned data against the original ROM, and an unsupported address fails the
test immediately. Palette handlers retain their separate test ROM reader.
All 4,480 cold-start and 14,336 active-update cases pass, preserving the prior
full state/ring/palette checks. Log: `build/packed-camera-data-verified.log`.
`make test-arcade-map-store` also scans the entire original one-megabyte address
space to verify every exposed byte, word boundary and alignment rejection,
then runs its existing map mutation/copy suite. Log:
`build/camera-table-storage-verified.log`.

The copy-enabled controller compiles for the 68000 with these compact camera
readers (`build/camera-data-target.o`). This removes its dependency on a full
arcade program image when connected to the native runtime. It does not yet
wire that runtime, establish a gameplay mutation allocation bound, or verify
normal gameplay speed. The diagnostic ROM remains unchanged; all checks were
headless.


### Source stage palette initialization

`src/willow_arcade_palette_initialization.h` translates 0978..0A10 and 0A36:
all five source color-copy blocks and the four priority-mask words. It supports
all seven stages, retains every brightness/RGB nibble, and preserves the
unselected half of the final palette bank. The source computes a stage offset
before the Scroll1 copy but never adds it to that copy's pointer; the native
translation therefore retains its fixed E6EA0 source rather than inventing a
stage-dependent palette. This is numeric CPS state, not the final Genesis
palette fit.

`tools/native_palette_initial_data.py` packages the addressed data as 15,964
ROM bytes in seven spans. The native word reader explicitly rejects unsupported
or odd addresses. The instruction-oracle test uses this compact reader for all
translated reads and checks each returned word against the original program.
No captured palette initializes the implementation.

`make test-arcade-palette-initialization` passes 1,792 cases across all seven
stages with varied prior palette contents. Each case compares all 2,048 color
words and four masks against original 68000 entry 0978. Only the five CPS color
RAM destination operands are relocated into test RAM; instructions, tables,
branches and copy lengths remain unchanged. The fixture permits up to four
headless emulator slices for this bulk setup routine to return, without
claiming a gameplay frame-time bound. Log:
`build/palette-initialization-verified.log`. The implementation and compact
reader also compile together for the 68000 as
`build/palette-initialization-target.o`.

This supplies the source-derived initial palette state required by camera and
palette runtime integration. It is not yet called by the Genesis diagnostic;
its existing ROM and measured intro timing are unchanged. Native integration,
palette animation/rendering, and full-game performance remain unfinished.
All validation remained headless.


### Native intro camera and exact Scroll2 map integration

The Genesis diagnostic now executes the complete translated Scroll2 controller
and stores its complete 128-byte record and exact 4,096-cell map. An 8,192-byte
heap allocation holds dictionary references. Its camera position now supplies
the rendered intro/publication position. Initialization follows 2DE6,
A128/A12E and Stage 0's 2D80 tables; captured records are expected test outputs
only. Native gameplay mutation overrides are not allocated yet: this intro
path performs source ROM copies only, and unsupported word-write/allocation
callbacks report diagnostic failure.

`tools/arcade_camera_trace.lua` captures source frames 650..1620 using dummy
SDL drivers, no video and no sound. Continuous host comparison established
that A2F8 stops scheduling camera updates after the descent reaches zero;
A3BA and A428 also bypass the normal update. The native controller uses that
source scheduling, not an unconditional per-frame call. The existing intro
manager remains responsible for target changes and message timing.

Cold chunks and contiguous vertical-strip sections now use range copies into
the compact map. They retain original address traversal/order and release old
mutable references. Range validation occurs before writing; the immutable
source-ID table is validated exhaustively against the original ROM by the
asset/test pipeline rather than revalidating each ROM ID per runtime copy.
Horizontal strips preserve their separate strided-cell transfers. Source
instruction checks across all stages still pass after this change.

Initial integration exposed real missed updates. These were resolved without
altering the camera or source scheduling: advancing viewports scan only their
departing/arriving rows, row loaders advance precomputed pointers, and offscreen
foreground pattern uploads use 4/12/12/12 columns across four frames. Background
rows upload five columns across eight frames. This shifts upload work away
from the frame that also retires a row and transfers a source map strip.
The cache verifier now reconstructs partial-row ownership from the actual
prepared-column count, along with complete rows and retained overlays.

`make test-intro-probe test-stage-entry test-arcade-map-store
 test-arcade-map-controller test-intro-camera SGDK_ROOT=/tmp/willow-sgdk-resume`
passes. Evidence in `build/native-camera-runtime-verified.log` includes:
- 915 native updates, zero missed updates, zero cache overflows.
- All 128 camera bytes on 915 updates, and all 16,384 map bytes on 911 updates,
  match source. Full-map comparison starts at frame 695, after the original
  five-phase cold load completes; preceding unfilled cells retain pre-intro
  CPS contents and are not used to seed native state.
- All existing actor/player records, pool/allocator checks, 61 intro image
  checkpoints, four player-gate image checkpoints and per-update cache
  reference checks remain passing.
- 4,480 cold-start and 14,336 full active-controller instruction comparisons
  pass with the new range sink and compact source tables, as do map mutation
  and range-boundary checks.

Remaining native heap is 4,672 bytes. The verified diagnostic ROM, symbols,
pixel reports, verification log and manifest were refreshed in
`out/diagnostics/intro`; ROM SHA256 is
`65cab082fdb3608e1c04837a289636b5ce97f4f860158125e3eddf594f555be7`.
This is integrated intro camera/map behavior, not complete gameplay: normal
player updates, mutable gameplay map edits, palette execution/rendering,
remaining actors, audio and full campaign are still pending. No GUI launched.


### Original palette brightness steps and global fade coroutines

`src/willow_arcade_palette_fade.h` translates 5260..5294 (one group), 5258
(selected-group addressing), 51E8/524A (all banks), and the global fade-out/in
control paths at 50C6 and 5102. The first color alone controls whether all
sixteen group words advance. Word additions wrap exactly; unequal brightness
within a group is not normalized. Global traversal advances palette addresses
while descending completion-bit indices. It processes 32 groups in each of
four banks and eight groups in each of two further banks, then accumulates
all six masks with longword wrap into the source completion sum.

The task translation preserves early exits, busy/dark flags, layer-control
changes and the final wait before completion. A yielded call requests the
original unsigned-byte delay from task +19; the outer scheduler must resume
it at the correct time. Fade-out's completion sum is exactly 506 (four full
32-bit masks plus two 255 masks modulo 2^32), not a guessed Boolean test.

`make test-arcade-palette-fade` passes 6,144 whole-bank/group/selected-group
cases against original instructions, including varied brightness, eight
positive/negative/wrapping deltas, all 3,072 palette words, six masks and the
completion sum. A further 256 fade jobs match 2,304 yield/exit boundaries and
2,048 requested delays, with all palette contents and shared flags checked
at each boundary. Six CPS destination operands relocate to test RAM. For
coroutine checks only, external scheduler entries 1750 and 165A are replaced
by distinct stop sentinels; yielded calls resume via their original saved
return address. Fade instructions remain unchanged. This verifies requested
waits and state boundaries, not the unimplemented native task scheduler's
wall-clock timing.

The fade helpers compile for the 68000 (`build/palette-fade-target.o`). Log:
`build/palette-fade-verified.log`. They are not yet connected to native palette
storage, scheduler or Genesis palette rendering. The verified native camera
and intro diagnostic ROM are unchanged. All checks ran headlessly.


### Compact native free-tile index

The native renderer now tracks its 1,296 free tile slots with an availability
bitset, a six-word summary and a rotating search cursor. The 176-byte index
replaces 5,188 bytes of linked-list bookkeeping, recovering 5,012 bytes without
reducing the tile cache, actor pools or source map. Occupied slots are never
eligible for replacement. Released cached patterns can be reacquired directly;
otherwise the allocator selects the next free slot in circular index order.
This changes only native cache replacement policy, not original game behavior.
Lookup/mask constants reside in ROM.

The foreground prefetch schedule is now 1/13/13/13 columns across four frames,
leaving the row-retirement/map-transfer frame less work. Background rows still
use five columns over eight frames. The resulting 61 intro and four player-gate
image checkpoints remain identical. The camera, map state, publication timing,
source update scheduling and all actor/player state checks also remain exact.

`make test-native-tile-slots test-intro-probe test-stage-entry
 SGDK_ROOT=/tmp/willow-sgdk-resume` passes. The independent allocator test checks
full exhaustion plus 65,536 randomized release/remove/search operations against
a simple circular reference implementation. The native test reconstructs live
ownership independently and checks all 1,296 availability bits and 81 summary
bits on every update, including partially prepared rows and retained overlays.
All 915 updates complete with zero missed updates/cache overflows. Peak resident
patterns remain 1,226; remaining heap increases from 4,672 to 9,684 bytes.
Logs: `build/tile-slot-index-verified.log` and
`build/tile-slot-ownership-verified.log`.

The verified diagnostic, symbols, image reports and manifest were refreshed in
`out/diagnostics/intro`. Current ROM SHA256: `f5593d8a2013797c99cb721c967f9974ce46915ddcd021729c86912ce28ba8a3`.
This provides RAM for upcoming six-bank palette and gameplay services; those
services are not yet integrated. The full game is still unfinished. All tests
remained headless.

### ROM-derived HUD graphics pack and source-pixel checks

`tools/export_arcade_hud_tiles.c` enumerates keys by executing translated HUD,
label, health, inventory, numeric and charge writes across character/level
variants. `tools/native_hud_assets.py` decodes those original Scroll1 glyphs
and palette words directly from the ROM. It produces 378 code/palette keys
and 329 deduplicated Genesis patterns, preserving both CPS column layouts and
transparent pen 15. The candidate 15-color palette includes the current idle
Willow's original pixels because he shares PAL0; it does not yet cover every
moving-player pose or palette effect. Background palettes are not modified.
Generated assets: `native/intro_probe/src/hud.h` and `res/hud.bin`.

`tools/verify_native_hud_assets.py` independently decodes all packed nibbles,
checks 48,384 pixels for geometry/transparency and nearest fitted color, and
checks key coverage against 399 visible HUD cells from source text RAM at
frames 1588, 1590 and 1600. The captured RAM is verification input only.
23,888 opaque captured pixels have RGB RMSE 21.80 against full-brightness
source palette colors; the broader fitting histogram has RMSE 25.78.
Report: `build/native-hud-asset-verification.json`. The side-by-side
`build/native-hud-palette-preview.png` was visually inspected: source label,
timer, gold, health and charge colors remain distinct. This is a rendered
asset comparison, not a screenshot of the native ROM displaying the HUD.

The display inspection also confirms a pending viewport issue: these labels
span the original 384 pixels, and the diagnostic's 320-pixel center crop would
clip left-side labels. Complete-width presentation must be resolved during
native HUD integration. The graphics pack is not linked into the diagnostic,
and neither runtime performance nor a final 320-pixel layout is established
by these asset checks. `make test-native-hud-assets` reproduces generation and
verification without creating a window; no GUI emulator was launched.

### Full-width HUD composition, native pixels and measured limits

`src/willow_native_hud_surface.h` stores both original words in six HUD rows
(2,3,26,28,29,30), using 1,188 bytes including dirty-tile masks. It rejects
addresses outside that rendering view, retains partial code/attribute writes,
and avoids redraw when a value has not changed. This is a rendering surface,
not a substitute for all CPS text RAM. Source row 30 is retained offscreen.

The complete 384-pixel HUD is sampled to 320 pixels at original pixel centers,
`sourceX=(6*x+3)/5`; the game logic and source tile coordinates are unchanged.
Generated left/right glyph projections preserve column layouts and both flip
directions. Composition combines two precomputed contributions, using eight
aligned longword ORs on the common 68000 path. Direct ROM lookup tables avoid
repeated glyph searches and software division. The projection patterns/index
use 137,320 ROM bytes; direct lookup tables use another 12,008 bytes. This
trades ROM for CPU time without allocating a pixel framebuffer.

`test_native_hud_surface.c` passes 138,240 independent dirty-dependency checks,
unchanged writes, address bounds and source-word retention. Its composed
output passes 92,160 comparisons against an independent ROM pixel sampler
for three captured HUD states with/without mixed flips. All labels remain in
the 320-pixel composition; `build/native-hud-surface-preview.png` was visually
inspected. These are component-generated pixels, not an integrated game screen.

The bare-metal compiled C probe (`tools/hud_surface_probe.c`) runs headlessly
in Genesis Plus GX, exercising the actual SGDK/68000 longword path. All six
240-tile compositions match the host result byte-for-byte (46,080 bytes).
Thirty-six four-tile updates also match and finish within a single emulated
frame interval. A full rebuild originally took 12 intervals; direct lookup,
inlining and the common no-flip path reduce it to **four to five intervals**.
This is still too slow for a synchronous full rebuild at gameplay entry.
Initial composition must be prepared before display and only changed tiles
updated afterward. These intervals exclude VDP upload and other game work;
they do not prove the complete port runs at 60 FPS.

`make test-native-hud-target SGDK_ROOT=/tmp/willow-sgdk-resume` reproduces the
component checks. Report: `build/native-hud-surface-verification.json`; target
log: `build/native-hud-target-verified.log`. Native tile-cache/VDP integration,
source HUD-state initialization and the shared command scheduler remain pending.
The main diagnostic ROM was not rebuilt or relabeled. No GUI was launched.

### Source clock, cold HUD values and precomputed initial tiles

`src/willow_arcade_player_clock.h` translates 4EA2's timer divider, BCD
decrement/borrow, zero clamp, blocking flags and low-time sound gate. It keeps
the 60-update divider separate from the shared global frame byte. Source
9530 calls the clock before player dispatch. The instruction oracle covers
every possible 16-bit time word (including non-BCD values), plus 8,192
divider/flag cases: **73,728 cases** match timer state, all 16,384 HUD bytes,
and the full gated sound queue. Log: `build/player-clock-verified.log`.

The HUD-only cold Stage 0 initializer derives lives/health from ROM DIP tables,
time from the original stage descriptor, and the boot high score from 183C;
it clears the relevant new-player fields. All 16 low-nibble DIP combinations
match original 1B96, 2CCE and 2DFE execution. The boot high-score load is checked
separately against 147A/1482's original data/instruction; this is not a test of
the full boot sequence. The helper is explicitly for cold single-player Stage
0, not continues, other stages, or persistent altered score state. Compact
player data now totals 3,032 bytes in 26 spans.

`test_arcade_hud_clock_trace.c` starts from code/ROM only and matches all 930
source clock states, including 32 updates after handoff. It does not import
captured state for initialization or establish full-boot global frame phase;
low-time frame gating is tested by the separate instruction oracle. Both
clock entry points compile for Genesis 68000.

`export_native_hud_initial.c` executes the translated setup/labels/clock into
a private rendering surface for every DIP combination. The initial surface
now uses the intro handoff's exact 7020 clear code; 4F54 subsequently changes
the lower rows to 7120. The generator emits two states per DIP (initial 3:00
and first clock update), then deduplicates them to **88 patterns / 2,816 ROM
bytes**, 15,360 map bytes and 36,864 retained-word bytes. The default state
uses 73 nonblank patterns. No captured bytes enter these generated ROM assets.

`verify_native_hud_initial.py` checks 245,760 packed output bytes and 36,864
retained-word bytes. For default DIP 5, initial/first-update surfaces match
all 2,304 relevant captured bytes at frames 1588/1589; their composed pixels
also match the independently checked capture-derived output. Reports:
`build/native-hud-initial-assets.json` and
`build/native-hud-initial-verification.json`.

This supplies ROM-prepared initial tiles instead of the four-to-five-frame
runtime composition measured previously. Native VDP upload, cache allocation,
live HUD scheduling and full game timing still need integration/testing;
precomputation alone does not prove 60 FPS. The diagnostic ROM was not rebuilt.
All checks remained headless.


### First native normal-player updates after the ready countdown

The native diagnostic now dispatches source C13E when the authoritative player
record reaches state two, rather than leaving that record frozen after the
ready countdown. It uses the complete translated normal handler, the compact
original player tables, and collision attributes from the live exact Scroll2
map. Its renderer view is republished from the resulting record. Health/cache
fields retain shared gate-state values; default maximum health follows the
same source DIP table used during initialization.

This integration deliberately remains a no-input diagnostic. Effect/shot
allocation, shared RNG, sound, palette accesses and HUD writes are explicit
failure callbacks until their runtime services are connected; none is reached
in the verified idle interval. No substitute data is consumed for those
services. The local native update counter supplies frame input, while full
source boot/global-frame phase remains pending. The verification proves this
idle interval, not parity for input-driven attacks, damage or later gameplay.
BF42's separate charge HUD and other player states are also not integrated.

The continuous host test now reaches frame 1620, deriving dispatch from the
player's state rather than a captured frame trigger. All 128 player bytes
match on the fifteen normal updates after frame 1605. The native test has the
same extension: 930 updates with zero missed updates/cache overflows, all 930
camera records and 926 complete maps, 89,280 captured player bytes plus the
normal interval's full 128-byte records, prior handoff/allocator checks and
per-update tile ownership checks. Every unconnected-service failure counter
remains clear. The added native player-pixel checkpoints are 916, 920 and 930;
all seven player pose/countdown/idle checkpoints match 5,097 visible pixels.
The 61 full intro image checkpoints remain unchanged and passing.

`make test-intro-camera` and `make test-intro-probe test-stage-entry
 SGDK_ROOT=/tmp/willow-sgdk-resume` pass. Log:
`build/normal-player-runtime-verified.log`. Remaining heap is 9,600 bytes.
The diagnostic ROM/symbols, reports and manifest in `out/diagnostics/intro`
were refreshed. ROM SHA256: `350a0231c1d8290be622704dc69bab338b5e9fc237ff73e2b5885d1fd6724cfc`.
Active input, movement rendering, gameplay services, palette execution,
audio and full campaign remain unfinished. All checks stayed headless.


### Native player allocation services and class-specific release preservation

The normal-player callbacks now query and allocate from the existing native
class-2 effects pool, class-8 projectile pool and class-12 shared effect pool.
They use the same source-initialized records/index stacks as stage entry;
allocation retains original pop order and does not clear or activate records.
Unknown requested classes report a diagnostic error. No extra pools or RAM
copies were added.

Source inspection exposed a release distinction: 29EC (class 2) and 2B20
(class 8) omit the word clear at +46 performed by 2A36/2AD2/2BB8/2C54.
The shared helper now preserves those two bytes for class-2/8 records,
identified by their retained source class tag at +30. All previously translated
clear/preserve behavior remains for the other four native pool classes.

`make test-arcade-pool-services` executes original allocator/release routines
for all six classes and original capacities across 768 cases. It compares
allocated addresses, every record byte after release, free-stack contents,
counts/cursors, immediate reuse and complete exhaustion; no source instruction
patch is required. The full C13E instruction verifier now uses the production
`WillowActorPool` implementation instead of its separate test-only allocator.
`make test-player-update` passes its 36,864 full-player cases and 8,192 health
cases, including source pool records and allocation side effects. Log:
`build/player-pool-services-verified.log`.

The native no-input diagnostic still passes 930 updates without missed updates
or cache overflows, all source camera/map/player/entry comparisons, 61 intro
image checkpoints and seven player-pose checkpoints. Remaining heap stays
9,600 bytes. Log: `build/native-player-pools-runtime-verified.log`.
The diagnostic and manifest were refreshed; SHA256:
`40c8c607813ce0927df6c4bb8e080e93a1479d83977e27d7afe9922174cf0e01`.

This connects allocation services, not the newly allocated effects' update or
render handlers. Active input, shared RNG/sound integration, palette/HUD
services, other actors and the full campaign remain pending. Those remaining
service callbacks still signal failure if reached. All checks stayed headless.

### Original player charge meter (4AF2 / 4B0E / 4FBE)

`src/willow_arcade_player_charge.h` translates the post-player charge routine
and the complete empty-meter redraw. It preserves the unsigned cap comparison,
16-bit increment overflow, byte attribute overflow, the capped branch's lack
of tile writes, and the inactive reset guard. Reset emits source command F4
through the caller's sound service. The resulting FF8376 high byte is an input
to the following player update; this routine belongs after the player state
handler, as called by BF42. Character-specific level/length and end-cap
locations come from original ROM tables, not a visual reconstruction.

`tools/native_player_data.py` now includes the three charge table spans:
120 additional bytes, 2,222 bytes total across 18 spans. The instruction
oracle uses this same compact reader for the translated routine.
`make test-player-charge` passes 14,336 cases across both characters and seven
levels, comparing charge/active/flash state, all 16,384 Scroll1 bytes, the full
sound queue and both queue cursors. Inputs include cap boundaries, wrapped
increments, alternate-level byte overflow, release/mode gates, direct redraw,
and both sound gates. Only ten CPS text-memory operands/data pointers are
relocated into Genesis RAM in the oracle; original logic remains unchanged.
Log: `build/player-charge-verified.log`.

The module compiles with the Genesis 68000 compiler (1,202 bytes of code in
the standalone two-entry compile check). `make test-player-update` also passes
the existing 36,864 update and 8,192 health cases after the data expansion;
log: `build/player-charge-regression.log`.

The charge module is not yet wired into the native diagnostic: source-ordered
player dispatch, shared charge state, sound service and the gameplay HUD
renderer still need integration. The diagnostic ROM was not rebuilt or
relabeled by this component change. No GUI emulator was launched.

### Player outer dispatch and full initialization clear

`src/willow_arcade_player_dispatch.h` composes original BF42 states 0, 2 and 4
with the post-handler 4B0E charge update. Charge is read by the player before
being updated for the following frame. State 4 translates CED4's death
animation, byte timer, state-6 transition, and alternate recovery substates,
including the shared lower-bound and screen-bound calls. State 6 removal,
states 8/10/12 stage scripting, state 14, and Madmartigan's separate D4BA
dispatcher remain unsupported and return failure. This is not a complete
player dispatcher or playable campaign.

The combined oracle exposed an existing BFC2 error: ten iterations of two
longword stores clear **80 bytes (48..127)**, not 40. The native gate now
clears the correct range. Previous zero-initialized intro checks concealed
the missing upper-half clear. The expanded oracle initializes all 128 bytes
to nonzero/random data and compares every byte afterward.

`make test-player-dispatch` executes unmodified BF42 logic across 65,536
cases: nine normal/mode routes, three gate substates, four death/recovery
substates. It compares complete player records, charge/active/flash state,
16,384 HUD bytes, player palette, collision coordinates, shared RNG, sound
queue, and three native allocation pools (425 allocations). Only CPS RAM
address operands/pointers are relocated. The compact animation span now
includes D436..D4B9; player ROM data totals 2,354 bytes in 18 spans.
Log: `build/player-dispatch-verified.log`. Genesis 68000 compilation of the
combined entry also passes. Existing update/health/charge component tests
pass (`build/player-dispatch-component-regression.log`).

The native diagnostic's initialization fix passes all 930 existing updates,
source camera/maps/records, 61 full image and seven player pose checkpoints,
with zero missed updates/cache overflows and 9,600 bytes of free heap.
Log: `build/player-dispatch-native-regression.log`. The diagnostic ROM,
symbols, reports and manifest were refreshed. Combined dispatch and death
animation are still component-only; HUD/audio services and remaining states
must be integrated before active gameplay. All verification stayed headless.

### Remaining Willow outer states and stage input playback

The BF42 component dispatcher now handles all eight Willow outer states
(0,2,4,6,8,10,12,14), retaining the post-handler charge update. C03C's scripted
arrival includes same-update substate-0 fallthrough, source motion/floor checks,
wrapped vertical acceleration, readiness, delayed recovery and pause gating,
then the original health/bounds calls. CF8E's stage-dependent paths retain
signed centering comparisons, early returns, input clearing, stage-5 behavior,
and 2020 script playback. Playback decrements the original word timer and skips
exactly one zero-duration record, as the source does. These are translated
instructions, not motions or timings derived from the reference video.

CF6A removal clears the original player fields, resets charge, sets FF83C2,
restores the player palette's RGB while retaining brightness, and calls the
task-cancel service for offset C0. `willowArcadeCancelTask` translates trap-2
handler 1690's active guard and precise clear of task bytes 16..31 and byte 0.
The oracle executes the original trap and RTE through its original vector;
there is no scheduler sentinel substitution in this test.

`make test-player-dispatch` passes **98,304** original-instruction cases across
24 routes. Added comparisons cover script timers/pointers, every relevant
script/input flag and all 32 bytes of the cancelled task record, including
inactive records with nonzero retained fields. All prior record, pool, charge,
HUD, palette, RNG, sound and collision checks remain enabled. Tests exercise
all seven stage selectors, all arrival substates, all three stage-exit outer
states, input playback/flag gates, and signed camera-center overflow cases.
Log: `build/player-script-dispatch-verified.log`.

The combined entry compiles for Genesis 68000 (23,644 bytes in the standalone
compile check). Compact player ROM data now includes 47E8E..4808D for the
stage-input reader, totaling 2,866 bytes across 19 spans. Runtime script
initialization/scheduling and full playback sequences still need integration
and continuous verification. Madmartigan's D4BA dispatcher remains separate
and unimplemented here; completing Willow's outer dispatch does not establish
full game behavior.

This turn changes verified components only. The native diagnostic still uses
its existing gate/C13E path; its ROM was not rebuilt or relabeled. Source HUD
initialization, sound/playback services, live input, other actors and campaign
integration remain pending. All checks ran without GUI windows or focus changes.

### Gameplay HUD setup and command-12 labels

`src/willow_arcade_player_hud.h` translates complete 4F44/4F54/4F98 setup:
the exact three-row/48-column clear, command-12 enqueue, both character meters,
inventory and health, time, lives, player/other/high scores and gold. Numeric
writes preserve nibble rendering, shared leading-zero suppression, forced last
digits, byte-wrapped lives-minus-one, score placement from FF8292 and the
six displayed gold digits. It does not reset caller-owned charge/time/score.
The command producer in `willow_arcade_services.h` implements 1F12/1F14's
command/argument ring and 252-to-0 cursor wrap.

The label path is now identified: command 12 dispatches through 5BAA and table
5C7E to 5F6A. `willowPlayerHudLabels` implements its zero-delay 5BC8 renderer,
including positioned-record separators and NUL termination. This renderer is
distinct from intro text at 7994. The colon's x coordinate is 42 (also '*');
headers must not be mistaken for character separators. The HUD catalog now
records all eight labels, correct glyphs (excluding delimiters), byte-offset
destinations and the verified caller path.

`make test-player-hud` passes 4,480 cases spanning both characters, seven
Willow levels, five alternate-meter levels, all 64 command-queue cursors,
inventory/health combinations, zero values and arbitrary numeric nibbles.
The oracle compares all 16,384 Scroll1 bytes, retained charge/active/flash,
all 128 queue words and cursor after original 4F44. It then compares label
writes through original 5BC8 with zero delay and task byte 20 requesting RTS;
this verifies label rendering, not the asynchronous queue consumer or task
scheduler. Only CPS text-RAM addresses/pointers are relocated. Logs:
`build/player-hud-init-verified.log`, `build/player-hud-catalog-verified.log`.

The HUD/label/command entry points compile for Genesis 68000 (4,376 code bytes
in the standalone check). Original compact data is 3,010 bytes in 22 spans.
This establishes the source charge-flash initialization to 8 and the complete
HUD writes, but native tile/palette presentation and task scheduling are not
integrated yet. The diagnostic ROM was not rebuilt or relabeled. All tests
remained headless.


### Resident Willow with the gameplay HUD palette

`tools/native_hud_assets.py` now also emits `res/hud_player.bin`: eight
resident 16x16 sprite blocks (1,024 bytes), using the original D15A pose,
original CPS graphics, original flip, and the HUD palette's nearest-color
mapping. The block offsets are checked against the existing overlay layout.
This prevents reusing obsolete PAL0 indices when the HUD eventually replaces
the late-intro palette. The asset is prepared, not yet connected to runtime.

`make test-native-hud-assets` now independently reconstructs and checks all
2,304 player pixels (731 opaque) as well as the existing 48,384 HUD pattern
checks and captured HUD coverage. Evidence: `build/native-hud-player-verified.log`
and `build/native-hud-player-parity.json`. This covers the resident idle pose,
not arbitrary gameplay animation palettes.

A selective-row clear experiment failed the native handoff timing check at
update 897, despite reducing DMA bytes. It was reverted completely. The timing regression suggests extra row-call
CPU cost; reducing DMA bytes alone does not establish a faster handoff. The main
renderer remains unchanged from before HUD integration. The rejected result
is retained in `build/hud-handoff-dma-verified.log` for diagnosis.

After reverting, `build/hud-handoff-baseline-verified.log` passes all 930 native
updates with zero missed updates and zero cache overflows, the complete intro
pixel checks, seven player-pose checkpoints, and stage-entry checks. All work
and verification remained headless.


### Live native HUD and first clock updates

The native intro diagnostic now prepares the cold Stage 0 HUD by executing
4F44/4F54/4F98 and the zero-delay command-12 label renderer against the
retained six-row surface. Inputs come from the ROM-derived DIP, high-score
and stage-time tables, not captured state. This private preparation is legal
for this unchanged cold state only: presentation starts at the source handoff,
and 4EA2 begins updating the actual timer on the next source update. The
asynchronous command consumer is still not integrated.

The 88 precompiled initial patterns preload after the final caption switches
to the overlay renderer. They occupy forty retired high-VRAM player slots and
48 caption slots, leaving both scripted-character poses and the remapped
Willow untouched. At handoff the HUD takes PAL0 and Willow receives its
matching 1,024-byte pattern variant. The complete 384-pixel HUD is sampled to
320 pixels so the outer labels remain visible; the world retains its existing
center crop. Palette fitting still has Genesis quantization error.

Subsequent source writes update retained code/attribute words, compose dirty
8x8 output tiles from ROM projections, copy their temporary data into the
DMA queue, and acquire cache slots. Retired overlay pins release three per
update. No captured frame or precomputed first-clock screen drives this live
update path. The first clock transition is rendered through it. Rendering a
large simultaneous batch of gameplay HUD changes is not yet a tested budget
claim; arbitrary active-input gameplay remains pending.

The target build exposed m68k GCC widening a low-nibble byte read into an odd
word address. `willowHudNumber` now forces byte reads, and all 4,480 original
instruction HUD cases still pass. Dedicated bounded callbacks for the palette
and object entry ROM spans avoid unrelated lookup branches during the handoff;
the resulting records still match every source byte at entry.

`make test-intro-probe test-stage-entry test-player-hud test-player-clock
SGDK_ROOT=/tmp/willow-sgdk-resume` passes. The 930-update native run has zero
missed updates and zero cache overflows, all camera/maps/player records still
match, and all 61 intro pixel checkpoints pass. HUD time/divider match all
930 source states; four handoff checkpoints match all 1,152 retained cell
bytes each. Independent ROM glyph/color sampling matches all 38,400 visible
HUD-row pixels across initial display and two first-clock checkpoints,
including transparency over the background. Seven player-pose checkpoints
match 5,097 visible pixels with the HUD palette. Remaining heap: 7,560 bytes.
The 73,728 original clock cases and all 16 cold HUD DIP cases also pass.

Evidence: `build/native-hud-integration-verified.log`,
`build/native-hud-pixel-parity.json`, `build/native-player-gate-pixels.json`,
and the updated diagnostic artifact/manifest in `out/diagnostics/intro`.
This is still a diagnostic, not a completed game: active input, remaining
actor handlers, full scheduling, sound, other stages and the complete campaign
remain unfinished. Every check ran headlessly; no GUI emulator was launched.


### Native unified Willow dispatcher

The diagnostic now calls the verified `willowPlayerDispatch` BF42 path for
its scheduled player updates, replacing the separate direct BF62/C13E calls.
The source handler executes before the charge update; the charge value feeds
attack input and is reflected back into the retained gate globals. The outer
state routes now use the translated dispatcher, rather than silently returning
for non-normal states. Calls into still-unconnected task cancellation, sound,
palette or random services explicitly flag an error. The renderer still lacks
the full set of gameplay poses; integration of a route does not prove that
route works end to end in the game.

`build/native-dispatch-integration-verified.log` passes the full native intro,
HUD and player pixel regression and all 98,304 original-instruction dispatcher
cases across its 24 routes. `build/native-dispatch-state-verified.log` extends
the native check: 33 actual dispatcher calls (initial setup, ready gate and
normal idle), 930 matching source charge value/active/mode/alternate-level
states, source charge flash at handoff, unchanged full player/camera/map
records and zero missed updates or cache overflows. These no-input frames do
not exercise an active charging sequence or the death/exit/arrival paths.
Those paths retain their separate original-instruction evidence. All work
remains headless.


### Native player damage and invulnerability palettes

The native player health callbacks now retain the original two 16-color player
pages (64 bytes), preserving source 16-bit words and brightness. Their private
cold-handoff preparation uses original 076C/07E6 data and full-brightness OR;
this is not a claim that the earlier intro was fully bright. The new isolated
headless capture confirms the player pages match those words from source frame
725 through 1620. Its 971 overlapping globals/player records exactly match the
existing trace. Source frames 691..724 still show the opening fade, whose
scheduling/rendering remains pending in this diagnostic.

For the resident Willow pose, `native_player_palette_assets.py` emits the
normal (E5080), damage (E5000) and invulnerability (E5040) variants at all
16 CPS brightness levels, mapped to the existing HUD palette. This avoids
flashing the HUD when Willow changes color. All 48 variants / 110,592 pixels
pass independent original-pixel/palette checks. The target renderer selects
a matching source palette and replaces the 32 resident patterns through the
VBlank DMA queue. Unsupported mixed-brightness or other palette contents
explicitly fail; full moving-player graphics remain pending.

`make test-native-player-palette SGDK_ROOT=/tmp/willow-sgdk-resume` passes the
baseline 930-update intro/HUD/player regression and two additional isolated
20-update probes. The probes inject a source health timer at native update
930, exercise the original health routine's four-update flash cadence and
expiry, and validate all 32 source color words and the timer. These injected
states test integration; they are not presented as naturally occurring arcade
captures. Across both probes, 29,240 source-derived visible player pixels and
512,000 HUD-row pixels match, including unchanged transparency/background.
Each 950-update run has zero missed updates and zero cache overflows. Remaining
heap is 7,468 bytes. Sound, task cancellation and shared RNG callbacks remain
unconnected; complete gameplay is not yet available.

Evidence: `build/native-player-palette-integration-verified.log`,
`build/native-player-effects-parity.json`, `build/native-player-palette-pixels.json`,
`build/native-player-damage-pixels.json`, and the two
`build/native-hud-during-player-*.json` reports. Capture provenance and brightness
observations are in `build/arcade-player-colors-verification.json`.
All emulator activity was headless with dummy video/audio drivers; no windows
or application-focus operations were used.


### Native gameplay pose renderer for Willow

`native_player_sprites.py` follows the animation tables selected by the
translated Willow handlers, with explicit source-facing domains. It resolves
70 blocked-record poses, including the FFFF held death frames whose controller
removes the actor before the animation timer expires. No frame is inferred
from video or a trace. Source shape tables determine origin and flips; source
CPS codes determine the graphics. There are 238 nonblank source 16x16 tiles,
5,325 deduplicated converted blocks, and 681,600 ROM bytes for all three source
player palettes at sixteen brightness levels. This uses the existing HUD
palette, with its documented Genesis quantization limits.

After HUD handoff, the native renderer resolves the current animation record
and uploads only changed sprite blocks. Both flip directions reuse source
blocks. Eight blocks use the existing player slots and a ninth uses the free
128 bytes immediately after the 80-entry SAT, preserving all HUD patterns.
The initial pose is prepared contiguously for the bounded handoff upload.
Palette changes now select from the same general gameplay block bank; the
old resident-only effect resources are no longer linked into the ROM.

`make test-native-player-sprites SGDK_ROOT=/tmp/willow-sgdk-resume` passes:
all previous intro/HUD/player and flash regressions; 2,924,544 asset pixel
checks across every source tile and all 48 color variants; direct execution
of original sprite writer 3B880 for all 70 pose positions/codes/attributes and
both source sprite budgets; and a headless 70-pose renderer probe with
3,942,400 pixel comparisons and no mismatches. Its 1,210 native updates have
zero missed updates or cache overflows. Remaining heap: 7,448 bytes.

The pose probe injects ROM-selected animation pointers at a fixed position
and holds each for four updates. It verifies rendering, including transparency
and the ninth block, not natural gameplay progression. The existing 930-update
baseline still verifies source scheduling/state; active-input play, projectile
and enemy rendering, sound, other stages and full campaign remain incomplete.
All 48 color variants are asset-verified; the runtime pose sweep uses normal
colors, while separate resident-pose probes exercise both flash sequences.

Evidence: `build/native-player-sprites-integration-verified.log`,
`build/native-player-sprite-assets-parity.json`,
`build/native-player-pose-pixels.json`, and `build/native-player-sprites.json`.
Updated ROM and evidence are in `out/diagnostics/intro`. All checks remained
headless; the previews are saved emulator output, never GUI screenshots.


### Native controller sampling and walking-state trace

`willow_arcade_player_input.h` translates 1F88..1FE4 controller selection and
demo playback: held/previous bytes, second-controller option bits, wrapping
demo timer and the single zero-duration record skip. The BF1E edge expression
is applied before player dispatch. 65,536 cases match the original instruction
routine for held/previous, demo pointer and timer; individual button edges are
also checked. This does not implement demo recording at 1FE6.

The native manager samples Genesis controllers before its gameplay clock,
matching 954E/9552 ordering. Directions map to the original bits 0..3, A to
attack bit 4, and B to jump bit 5. Shared held/previous state is synchronized
with the script dispatcher. BEF8's inactive-player guard now precedes native
dispatch. Input during the intro's skip path remains separate and unfinished.

A new headless original-game capture holds right on source updates 1626..1665,
then releases it. All 34 overlapping baseline records match the existing
camera/player/map trace. The native integration test drives the emulated
Genesis controller interface, not injected player coordinates. All 80 additional
full 128-byte player and camera states, complete 16,384-byte Scroll2 maps,
held/previous input bytes, charge state and HUD clock match the original through
frame 1700. The 1,010-update run has zero missed updates or cache overflows.
The existing intro, palette-flash and 70-pose regressions also pass. Remaining
heap: 7,438 bytes.

This is state fidelity, not a finished walking presentation. The source camera
starts moving immediately (X=2 at frame 1626, X=80 at 1665). The current native
forest was flattened for a stationary intro camera, and horizontal background
streaming/parallax and corresponding sprite-camera publication are not yet
connected. No guessed scroll motion has been substituted. Attack/jump inputs
are sampled but their unconnected sound/RNG services and other gameplay
systems still prevent claiming fully playable active gameplay.

Evidence: `build/player-input-verified.log`,
`build/native-player-input-integration-verified.log`,
`build/native-walk-state-verified.log`, and
`build/arcade-player-walk-verification.json`. Capture source is
`tools/arcade_player_walk_trace.lua`; all emulator runs stayed headless.

### Scroll3 motion derived and verified (2026-10-03)

`src/willow_arcade_scroll3_motion.h` translates original 4310C..431A6:
word-width horizontal/vertical target tracking, the source direction bytes,
negative-result clamp, register-count logical shifts, and stage 5 / phase 6
64-update oscillation. The latter retains the source timer wrap and byte toggle
semantics rather than replacing all stages with a parallax ratio.

`make test-arcade-scroll3-motion` executes the original instructions in the
headless 68000 core for 131,072 cases and compares all 128 controller bytes.
Cases cover both axes, wrapped coordinates, zero/large/masked shift counts,
and all timer/toggle byte values. Result: zero mismatches.

`tools/arcade_scroll3_walk_trace.lua` captures globals and both background
controllers over frames 650..1700 with the existing right-walk input schedule.
The 1,051 records (1,286 bytes each) have SHA-256
`5e12357a429c1bfda8d75ea73d37410c8f19b99725cfe13f60e75b2fcaa94f1d`.
`tools/verify_arcade_scroll3_trace.py` checks 971 intro and 114 walking overlapping
globals/Scroll2 records against the separate original captures.
`tools/test_arcade_scroll3_trace.c` continuously retains translated motion state
from the first active controller and compares 1,005 subsequent original frames:
coordinates, directions, timer/toggle bytes, source targets, and staged scroll
registers all match. Targets come from the captured foreground controller;
Scroll3 expected coordinates are never fed back into the replay.

This confirms the Stage 0 distant forest moves from X=0 to X=40 while foreground
X moves to 80, and confirms the intro Y motion with the same source routines.
It does not verify Scroll3 initialization, map streaming (430B6..4310A and its
callees), register publication latency, or native rendered horizontal movement.
The translated motion is not yet connected to the native renderer. The existing
ROM remains unchanged: its merged stationary forest and plane-B HUD must be
reworked to retain independent background scrolling. No GUI was launched.

### Scroll3 active map streaming verified (2026-10-03)

`src/willow_arcade_scroll3.h` now translates the 43D82 chunk lookup,
43DB4/43E5E column/row transfers, 4309E publication, and 43086 active dispatch.
It preserves 8x8 source chunks, the A00 code adjustment, separate ring wraps,
18-cell columns, 22-cell rows, direction-selected ROM offsets, bit-5 latch
equality, and the stage-4 freeze. Vertical layout stepping deliberately follows
the source's nonzero-selector 32-byte step, without substituting Scroll2's
larger-layout steps or masks.

`make test-arcade-scroll3-trace` passed in
`build/scroll3-integration-verified.log`. In addition to the 131,072 previously
verified motion cases, original instructions now verify 65,536 lookup cases,
4,096 complete strip transfers, and 7,168 active dispatch cases across all seven
stages and four selectors. The active comparisons include all 128 controller
bytes, all 16,384 map bytes, and both staged scroll registers. The oracle redirects
only the verified 43D50 CPS map-base operand into work RAM; original control flow
and ROM data remain intact.

The headless source trace now writes `maps.bin` alongside its unchanged controller
records. Its hash is recorded in `build/arcade-scroll3-trace-verification.json`.
The continuous replay compares the complete controller and map for 1,005 active
updates through intro descent, right movement and release, with zero mismatches.
It seeds one preceding captured state only as a test fixture, retains subsequent
map/controller outputs, and uses the source foreground controller as its input.
No capture bytes are compiled into the native ROM.

Still pending: Scroll3 cold initialization (42F7A..43084 and 5532), renderer
integration, horizontal pixel parity and performance validation. The shipped
diagnostic ROM is unchanged by this component work. All tests were headless.

### Scroll3 initialization and outer controller (2026-10-03)

`src/willow_arcade_scroll3_start.h` translates 5532's complete 8x8 chunk load,
42F7A..43084's five initialization phases, and 42F48's outer target selection
and dispatch. The initial ROM tables supply layout pointers, layout selector,
and shift counts. Stage 1 uses the retained stage-X input. Chunk loading retains
the source position save/restore fields, code adjustment, four-chunk batches,
and the final phase's coordinate reset and oscillator initialization.

The expanded `test-arcade-scroll3` passes 8,960 initialization phase updates
(7 stages × 256 initial states × 5 phases), comparing original instructions
against every controller byte, every map byte, and both staged registers.
The existing lookup, transfer, and active checks pass in the same run;
see `build/scroll3-verified.log`.

`test_arcade_scroll3_trace` now starts before initialization and verifies 1,010
consecutive source updates, frames 691..1700. It applies the two explicit
Stage-0 entry writes from A134/A13A (192,512) before the first call. It then uses
the unified translated controller for initialization and active motion, without
feeding expected Scroll3 output back into the translation. All 128 controller
bytes, 16,384 map bytes, and both staged registers match every update. The initial
frame-690 map/controller are test fixtures only; renderer integration must use
source initialization rather than captured startup assets.

A ROM inventory identifies 1,270 distinct post-A00 Scroll3 code/attribute pairs
including zero across all 16,384 source cells at C0000..CFFFF. Thus an exact
11-bit dictionary map would require 5,632 RAM bytes; the straightforward 8,192-
byte ID map exceeds the diagnostic's last measured 7,438-byte free heap. This is
a sizing result, not an implemented map store or a runtime performance claim.
Independent background rendering, HUD layout/VRAM rework, full gameplay and
60 FPS validation remain unfinished. No GUI was launched and the diagnostic ROM
has not been changed by these source component additions.

### Lossless packed Scroll3 map storage (2026-10-03)

`src/willow_arcade_scroll3_map_store.h` implements the 5,632-byte map identified
above, packing 4,096 exact dictionary IDs into 2,816 aligned words. Every code
and attribute bit is retained. `tools/native_scroll3_data.py` derives the 1,270
pairs and 16,384 source IDs directly from C0000..CFFFF, including the original
A00 code addition and zero pair. No captured data enters generated assets.

The source Scroll3 controller now also exposes copy callbacks, like Scroll2.
These accept complete immutable ROM-pair transfers, avoiding transient half-pair
lookups. Existing word-write entry points remain available. Cold chunks use one
64-pair copy; active strips preserve the original traversal and ordered cell
copies. This compact store does not claim support for arbitrary mutable pairs.

`make test-arcade-scroll3-map-store` passes 8,388,608 exhaustive cell/ID checks,
including both neighboring cells, sentinel bounds, all 16,384 source pairs,
256 chunk copies and rejected range/alignment cases. Address and undefined-
behavior sanitizers report no errors. The packed store also passes the complete
1,010-frame initialization/intro/walk/release replay, comparing every controller
and map byte against MAME, independently of the dense word-write replay.
`make test-arcade-scroll3-trace` passes all original-code regressions after the
callback refactor. Logs: `build/scroll3-map-store-verified.log` and
`build/scroll3-integration-verified.log`.

The packed store saves 2,560 bytes versus a 16-bit ID map. It is not yet linked
into the diagnostic runtime, and its target CPU cost remains unmeasured.
Runtime integration must still budget RAM for wider plane rows and a fixed HUD,
and retain independently scrolling foreground/background planes. Current ROM
and published diagnostic remain unchanged; the full port is still unfinished.

### Scroll3 native runtime integration in progress (2026-10-03)

The diagnostic main now allocates the 5,632-byte Scroll3 map, applies the original
4164E clear pair (A00/zero), initializes the A134/A13A camera position, and runs
the translated distant controller after each scheduled foreground controller.
`native_scroll3_data.py` supplies ROM-backed layout, direction, cold-load and
stage tables, plus aligned packed source IDs for fast 64-cell cold copies.
The headless ROM checker now compares the native full Scroll3 record, staged
registers and all 4,096 map cells with the original source trace.

An initial allocation failure was diagnosed as fragmentation: 7,294 free bytes
but only a 4,212-byte largest block. Allocating both maps before the actor pools
resolves it. Individual 11-bit writes made cold loading too expensive; copying
88 prepacked ROM bytes per complete chunk fixes that early failure. The source
clear pair also matters: zero-filled IDs do not represent the original A00 clear.
All generated packed source IDs and the existing compact-map tests pass in
`build/scroll3-map-store-verified.log`, including the 1,010-frame host replay.

The current native build is **not validated**: `build/scroll3-native-walk.log`
first fails at tick 262, with foreground Y already advanced into the next update
while the completed-update marker still reads 262. Earlier attempts failed sooner.
This establishes an update-boundary/timing regression, not a proved 60 FPS result.
Do not publish this ROM or weaken comparisons to accept partially updated state.
Investigate runtime timing and renderer workload before continuing pixel checks.
The previously published `out/diagnostics/intro` ROM is unchanged.

Experiments with 12-bit storage, different prefetch splits and active-display
waiting did not solve the regression and are not in the current main. Backups
remain in `build/main-scroll3-*`. The retained native main uses the original
1/13/13/13 near-layer prefetch schedule, 11-bit map and original VBlank loop.
Aligned camera word tables avoid duplicate byte lookup scans; they preserve
original ROM values. Source component tests remain passing, but the full native
regression must pass before this integration can be treated as complete.
Independent horizontal rendering and fixed-HUD plane rework are still pending.
All execution stayed headless; no emulator windows or focus changes occurred.

### Native Scroll3 timing regression resolved (2026-10-03)

The preceding update-262 failure is resolved. A temporary V-counter profile
identified map-copy overhead: the foreground update traversed roughly 110 scan
lines on the failing source strip, the distant update another 58, followed by
actor and background rendering. Profiles were removed after diagnosis.

The native immutable Scroll2 sink now has a checked single-cell fast path. More
substantially, Scroll2/Scroll3 transfer helpers expose optional row-copy hooks:
they group adjacent source-row cells within each original chunk (16 or 8 cells),
then retain the original chunk transitions and CPS ring wraps. Native sinks copy
these strided IDs with one bounds check per group rather than full general range
setup for each cell. The default word-write and contiguous-copy APIs remain.
Neither source camera scheduling nor the renderer's prefetch schedule changed.

The updated original-instruction oracles exercise the row hooks across all four
selectors, all strip alignments, complete map contents, and all stage entries:
`build/row-copy-oracles.log` passes for both Scroll2 and Scroll3. The packed map
sanitizer, all original source pairs and prepacked source groups also pass.

`make test-native-player-input SGDK_ROOT=/tmp/willow-sgdk-resume` passes in
`build/native-scroll3-integration-verified.log`. Native walking verifies 1,010
updates, all 128 Scroll3 controller bytes, staged registers from update 2, and
all 4,096 map cells on 1,006 updates from update 5. Existing full Scroll2/player/
pool/input/HUD checks remain. There are zero missed updates and cache overflows.
The full intro, HUD, health-palette, and 70-pose pixel regressions also pass.
The longer pose probe retains 1,210 consecutive updates without misses.

Published diagnostic ROM/symbols/manifest are refreshed under
`out/diagnostics/intro`; remaining heap is 1,660 bytes. The diagnostic still renders
a stationary combined forest after dialogue. Independent horizontal planes and
the fixed HUD's VRAM/plane arrangement are the next renderer work; full-game
60 FPS and campaign completion are not established by these diagnostic checks.
All validation stayed headless, without opening or focusing an emulator.

### Fixed HUD sprite layout preparation (2026-10-03)

A trial using 200 HUD sprite patterns in the opening house's CHARACTER_BASE
region failed the actual intro pixel comparisons at dialogue checkpoints.
The house continues cycling through those patterns during dialogue; the region
is not available for HUD preloading. The trial was removed from the runtime.
The restored ROM hash is exactly the published diagnostic hash
`4b77d5463c7c256dba7354fa57140dda84e30d218f1340fcd1a5a48e3c92ffc5`.

`tools/native_hud_sprite_assets.py` instead prepares a sparse sprite layout from
the existing ROM-derived HUD outputs. It uses single-row sprites for the top
labels and clock, and 4x2 sprites for the two fully opaque bottom rows. Identical
sprite pattern blocks are shared. The cold Stage-0 case needs 26 sprites and 93
unique tile patterns. An exact allocator fits these into the existing 40+40+8
HUD/caption pattern regions plus 8 unused patterns at F700..F7FF. It does not
consume either background plane or evict background-cache tiles. The ninth
Willow sprite remains at F680..F6FF; all scripted actor patterns are preserved.

The planner reconstructs all 409,600 HUD pixels across the 32 original generated
DIP/clock cases exactly. For the cold case, top HUD sprite fetch widths are only
144 and 184 pixels per line, leaving bandwidth for overlapping gameplay actors;
the full-width bottom rows are verified opaque. Maximum HUD-only sprite count
per line is 10. Combined gameplay sprite limits still require runtime tests.
Output: `build/native-hud-sprite-plan.json` and
`native/intro_probe/res/hud_sprite_plan.bin`. `make native-hud-sprite-plan` builds
this preparation; it is deliberately not linked into the current renderer.

Next implementation: prepare those four pattern banks during dialogue, build
HUD sprites ahead of actors, update layouts when glyph occupancy changes, and
copy shared blocks before live mutations. Preserve the exact dirty-cell HUD
surface and verify actual emulator pixels/pose overlays before freeing plane B.
No native HUD migration or independent horizontal rendering is claimed yet.

### Sparse HUD sprites integrated and validated (2026-10-03)

The native HUD now uses the previously verified sparse sprite layout. Four safe
VRAM regions hold its 96 prepared patterns (93 unique patterns in the cold case):
FB00..FFFF, caption patterns 0..39 and 72..79, and F700..F7FF. Original scripted
actor patterns and the ninth Willow block are untouched. The initial layout
uses 26 sprites, drawn before gameplay actors with the source HUD's priority.

`HudSprite` descriptors replace the 400-byte initial plane-attribute cache.
When a source dirty cell changes, its sprite rectangle receives a private,
contiguous cache block copied from immutable initial ROM patterns. Only the
changed cell is then composed by the existing source HUD surface. Other sprites
sharing an initial block remain intact. A newly visible cell outside initial
rectangles gets its own sprite; an omitted blank cell consumes no sprite.
Private cache ownership is checked from sprite descriptors in the headless ROM
test, independently of the allocator's reference counters.

The HUD no longer writes plane B. `build/hud-sprites-plane-release.log` confirms
all 2,048 plane-B cells remain clear after handoff through the walking probe.
This assertion deliberately describes the current intermediate renderer; it will
be replaced with source background checks when Scroll3 graphics occupy the plane.

`make test-native-player-input SGDK_ROOT=/tmp/willow-sgdk-resume` passes in
`build/native-hud-sprites-integration.log`: baseline intro and HUD pixel checks,
source state and pool checks, both health-palette probes, all 70 poses (3,942,400
pixels), and the 1,010-update walking state test. There are zero missed updates
or cache overflows; the pose probe runs 1,210 updates. Remaining heap is 1,816
bytes. Dynamic normal clock writes pass alongside all source clock states.
Large simultaneous HUD mutations and general campaign sprite bandwidth are not
yet established by these bounded probes.

Published diagnostic ROM/symbols/manifest are refreshed. The forest still uses
the stationary merged plane after dialogue; independent horizontal graphics
remain the next renderer step. No source gameplay behavior or camera timing was
changed by the HUD adapter. All tests ran headless without emulator windows.

### Two-plane forest restored before player control (2026-10-03)

After the dialogue clear, the native renderer restores original foreground and
distant layer rows instead of leaving the forest permanently flattened. It
preloads 367 source-derived patterns during quiet dialogue updates, preserving
all flat-forest and dialogue ownership. During the original ready gate, updates
900..913 replace two rows per update. This finishes before the first normal
player update (916); source player/camera timing is unchanged. The rows preserve
the exact original converted tiles, palettes and foreground transparency.

Loading patterns at the handoff exceeded its budget. Preloading resolved that
cost, but exposed the HUD's linear contiguous-slot search when the cache was
nearly full. `willowTileSlotsFindRun` now searches the existing bitmap; 86,272
random and boundary/end-of-cache cases match the linear reference under address
and undefined-behavior sanitizers (`build/tile-runs-verified.log`). This check is
now included in `test-native-player-input` prerequisites.

Restoration applies ROM-generated per-row reference deltas rather than repeatedly
releasing/acquiring identical cells. It then writes both row attribute maps.
The headless test independently reconstructs every reference count and checks all
2,240 restored tile/palette attributes. Pins are retired after restoration.
The HUD continues to use sprites; plane B now contains the original distant
forest rather than HUD cells. The earlier plane-B-clear assertion was replaced
with exact source-layer checks and assertions for not-yet-restored rows.

`build/native-forest-split-integration.log` passes the full native regression:
intro/HUD pixels, both player palette probes, 70 poses (3,942,400 pixels), source
controller/map/pool/clock checks, and 1,010 walking updates. No missed updates or
cache overflows; the longer pose probe passes 1,210 updates. Peak cache residency
is 1,244 of 1,296 patterns, and free heap is 1,808 bytes. Published diagnostic
ROM/symbols/manifest are refreshed.

Horizontal scroll publication, source-driven column streaming and camera-relative
sprite X remain pending. The two-plane stationary handoff is now validated, not
the full campaign or complete 60 FPS gameplay. All checks stayed headless.

### Stage 0 pair-addressed background graphics (2026-10-03)

`tools/native_background_pairs.py` now builds the graphics lookup needed to
render live source Scroll2/Scroll3 maps beyond the initial 320-pixel crop. It
uses the exact existing runtime dictionaries (9,069 Scroll2 pairs and 1,270
Scroll3 pairs), the complete Stage 0 ROM layout selector domains, and original
CPS palette RGB values. The two existing background palettes and all initial
pattern IDs remain unchanged. Original flip bits are baked into row-major
8x8 subtiles; source pen 15 remains native transparent pen 0. Unsupported pairs
use an explicit FFFF offset, not a substitute tile.

The bank covers 25 foreground chunks / 1,815 pairs and 14 distant chunks / 262
pairs. Its 5,513 additional patterns occupy 176,416 bytes. The independent
source-coordinate verifier checks 11,452 subtiles (732,928 pixels), including
flips, transparency, exact palette selection and all 6,080 unchanged intro
cells. Run `make test-native-background-pairs`. Verification evidence is in
`build/native-background-pairs-verification.json`.

These assets are **not yet linked into the diagnostic ROM**. The remaining
renderer work must stream columns from the live source map rings, publish the
matching source horizontal offsets, and subtract those same offsets from
actor positions. The existing direct pattern-to-cache-slot array cannot simply
be enlarged by 5,513 words: that would exceed the remaining native RAM. A
bounded cache lookup is required before integration. These asset checks do not
prove native scrolling, runtime frame timing, visual completeness, or a finished
port. The previously verified diagnostic ROM has not been replaced.

### Bounded background cache lookup (2026-10-03)

The native diagnostic now reuses its direct intro index storage for a bounded
512-bucket, 1,296-slot chained index (`src/willow_native_tile_lookup.h`). Keys
remain the existing per-slot pattern IDs, so additional ROM scenery no longer
requires one new RAM word per pattern. The 3,616-byte chained index shares a
union with the 4,748-byte intro index; this preserves the fast direct path
needed during intro row DMA. Mode/progress bookkeeping leaves 1,804 heap bytes
free (previously 1,808); the union does not yet reclaim its unused tail.

After both forest planes are restored, overlay pins retire 24/update and
forest staging pins 48/update. Conversion begins only once both pin groups are
empty and processes at most 128 resident slots per update. Lookups retain a
linear fallback for the not-yet-indexed suffix during this bounded transition.
The headless walker verifies conversion is complete by update 932, before its
right-input interval starts at 936. It independently reconstructs all resident
IDs, checks every chain for invalid slots/duplicates/wrong buckets, and checks
complete coverage of the converted prefix, alongside all prior ownership and
source-state checks.

The first all-intro hash implementations missed frame deadlines and were
replaced; they are not the published diagnostic. The final full
`make test-native-player-input SGDK_ROOT=/tmp/willow-sgdk-resume` run passes:
930 baseline, 1,010 walking, and 1,210 pose-probe updates, zero missed updates
or cache overflows, the existing 61 intro pixel checkpoints and all 70 Willow
pose comparisons. A sanitizer-backed host test also passes 262,144 cache churn
operations, exhaustive 16-bit key queries, and long collision-chain deletions.
Evidence: `build/native-tile-lookup-integration.log` and
`build/native-tile-lookup-unit.log`.

The Stage 0 pair graphics remain unlinked. Horizontal streaming must handle
input from the first normal-player update (916), including the cache's
transition interval; the current delayed-input walk alone does not establish
that. Do not make new scenery depend on the test's update-936 input timing.
Full gameplay scrolling and performance remain unverified.

### Live source-map background resolver and forest handoff (2026-10-03)

`src/willow_native_background_map.h` resolves a native subtile through the
actual Scroll2 4,096-cell map or packed Scroll3 map. Cell addressing follows
the source transfer destinations (43658 / 43D4E), including each plane's
horizontal/vertical ring masks. It returns a pointer into the original
code/attribute pair's converted subtiles; mutable or unsupported IDs fail
explicitly. It does not query static world geometry to replace the live map.

The sanitizer-backed `test-native-background-map` checks 2,242,240 visible
pair/subtile results against independent original ROM coordinate lookup using
1,001 captured frames (700..1700), including scrolling during the intro and
walking. It also checks complete ring coverage/wraps and all unsupported pair
IDs. Captures are test inputs only, never compiled as game data.

Forest restoration in the native ROM now uses these live map reads instead of
`intro_maps` for its restored near/far attributes. To meet the existing frame
budget it prepares two source pair offsets per quiet dialogue update: 280
foreground cells and 80 distant cells, in a temporary 720-byte allocation.
The map is stationary throughout this source intro phase. Handoff requires all
360 offsets to be ready. Two rows/update still restore at updates 900..913;
grouping four horizontal subtiles avoids repeated offset reads. The temporary
allocation is released at update 914. The generated initial-scene reference
count deltas remain valid for this specific stationary handoff only.

The headless native verifier independently compares each staged offset to the
corresponding captured live-map code/attribute pair on every update, requires
all offsets before handoff, verifies the staging allocation is released, and
retains the prior exact plane-attribute and reference-count checks. Full
`test-native-player-input` passes all baseline, palette, damage, pose, walking,
and pixel checks, with zero missed updates/cache overflows and 1,800 heap bytes
free after handoff. Evidence: `build/native-live-background-integration.log`,
`build/native-live-background-offsets.log`, and
`build/native-background-map-verification.log`.

This connects the pair-addressed lookup metadata to the native renderer;
the additional pattern binary is still unlinked. Horizontal column streaming
and actor camera-X subtraction remain outstanding. Before linking new pattern
bytes, keep their cache IDs distinct from overlay/HUD IDs (the raw asset
pattern index currently starts at 1,990, also used by the overlay namespace).
Resolve immediate normal-player input during cache conversion as noted above;
neither the unchanged stationary output nor the delayed walk state test proves
completed horizontal rendering or full-port performance.

### Extended scenery bank linked and exercised in native VRAM (2026-10-03)

The generated pair entries now reserve the complete existing forest/dialogue
and 200-cell mutable HUD ranges. Existing intro IDs remain 0..1989; new Stage 0
patterns use IDs 2374..7886. The generated extension base is compile-time
checked against the native namespace boundary. Palette bits remain separate
from pattern IDs. The asset verifier checks that no pair entry targets the
reserved gap and still verifies original CPS-derived pixels and all unchanged
intro entries.

`native_background_extra` now links the 176,416-byte bank into the native
ROM. The cache upload path selects this bank for extended immutable patterns.
A normally idle, host-requested diagnostic command loads up to four extended
patterns and keeps them referenced until the next diagnostic request; requests
are accepted only after the bounded cache conversion is complete. This command
is renderer test instrumentation, not an invented gameplay behavior.

`tools/test_native_background_bank.c` runs the real Genesis Plus GX core
headlessly and loads every one of the 5,513 extended patterns through the
native cache and queued VDP DMA. It compares every resulting VRAM byte to the
bank, checks resident IDs/reference counts, verifies both visible background
maps and their protected tile bytes stay unchanged across cache evictions,
and requires every update to advance on schedule without overflow. All
176,416 bytes pass. This establishes native bank addressing, transfer, and
cache coexistence; it does not establish scrolling performance.

Full `make test-native-player-input SGDK_ROOT=/tmp/willow-sgdk-resume` passes,
including existing intro/HUD/palette/70-pose pixel tests, 1,010 walking updates,
1,210 pose updates, map resolver checks, and the native bank test. Final free
heap is 1,784 bytes (diagnostic command state adds 16 bytes). Logs:
`build/native-background-bank-integration.log`,
`build/native-background-bank-dma.log`, and
`build/native-background-bank-assets.log`.

The diagnostic is still stationary horizontally. Remaining work includes
column streaming, camera-X publication/actor positioning, and accepting
movement from the first normal-player update during cache conversion. Do not
call the bank test or stationary pixel regressions a completed scrolling port.

### Cache ready before the first controllable update (2026-10-03)

The bounded cache now finishes conversion at native update **754**, before
handoff at 898 and the first normal-player update at 916. Quiet dialogue
updates prepare the positive reference-count changes for future forest rows
(two commands/update). Handoff requires all 413 commands to have been visited,
all source offsets/patterns loaded, and the bounded index complete. Row swaps
still occur at updates 900..913; their two negative reference changes remain
at the actual row swaps. No source game scheduling or timing was altered.

Restoration retains matching foreground cache slots (the asset generator now
asserts their palette identity), handles transparent tiles through a retained
blank slot, and uses saved slots for its 367 pinned patterns. Those slots use
temporary 512-byte and 222-byte allocations to fit the fragmented heap; both
are freed after pin retirement. The native pool allocation order changed,
while each pool's source class, capacity, record order and allocator indices
remain unchanged and pass full source-state comparisons. Final free heap is
1,772 bytes. The existing 512-bucket chained index remains in use; experimental
open-addressing variants failed timing checks and were not retained.

The headless verifier independently reconstructs future-row references and
pin-slot ownership on every update, verifies buffer release, and requires
conversion before handoff. Full `test-native-player-input` passes all previous
state/pixel checks, including 1,010 walking updates and 1,210 pose updates with
zero missed updates or cache overflows. The strengthened bank test begins
uploads at update 916, loads all 5,513 patterns, compares 176,416 VRAM bytes,
and verifies both visible planes remain intact without missing an update.
This is a stationary-player DMA/cache test, not a horizontal rendering test.

Evidence: `build/native-early-cache-integration.log` and
`build/native-early-cache-first-control.log`. Warning cleanup rebuilt to the
same SHA-256 as the fully tested ROM. This resolves the earlier late-cache
readiness limitation; horizontal column streaming and camera-X positioning
are still pending, along with the remainder of the arcade port.

### Horizontal column storage and first incoming column (2026-10-03)

Plane A now reserves a 43-column CPU ring rather than 40 columns, adding only
192 RAM bytes. The Genesis name table retains its independent 64-column wrap.
`src/willow_native_column_window.h` describes incoming/outgoing spans for a
320-pixel viewport (41 touched tiles when fractional), with one spare column
on either side. Its sanitizer-backed test checks 360,448 transitions, including
all signed 16-bit camera positions, direction reversals, large jumps, both wrap
sizes, and every visible pixel's retained/uploaded column identity. This is a
native renderer storage planner, not a replacement for source camera motion.

After the bounded cache is ready, quiet dialogue updates prepare column 40
(source X=352) on each plane directly from the live original maps. Two cells
are prepared per quiet update, for 56 cells total. Handoff requires completion.
The renderer preserves the offscreen column while clearing the dialogue plane
using a small temporary stack copy around the existing bulk clear. The newly
resident patterns add 33 cache slots: measured peak rises from 1,244 to 1,277,
within the 1,296-slot cache. Preloading both incoming columns during dialogue
would require 1,308 patterns, so the second column must wait for retired
staging graphics to release their slots. Final free heap is 1,580 bytes.

The native verifier independently checks the offscreen cells' captured source
pairs, converted pattern IDs, palette bits, reference counts, actual VDP name
entries, and actual VRAM pattern bytes. The source-map resolver test now covers
one tile left and two tiles right of the aligned viewport: 2,410,408 checks
across 1,001 original frames. The full bank test also protects this incoming
column, including shared references when a tested pattern is already resident.

`make test-native-player-input SGDK_ROOT=/tmp/willow-sgdk-resume` passes in full:
all existing state/pixel checks, 1,010 walking updates, 1,210 pose updates,
all 5,513 extended patterns, and the new column-window checks. No missed updates
or cache overflows. Evidence: `build/native-column-prefetch-final-verification.log`.

**Still pending:** the planner is not yet driving continuous column streaming;
only initial columns 0..40 are owned. Do not treat the 43-column allocation as
an already populated 43-column window. Runtime preparation/cancellation must
handle this partial initial window, direction changes, source publication
latency, and actor camera-X subtraction. The displayed view remains stationary
horizontally; these checks do not establish finished scrolling or a full port.
