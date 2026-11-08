# Stage 5 interior top: plan for ~48 fps at worst (proposal, not started)

Status: build 04i keeps full game speed; the interior top shows ~30 fps,
dipping to ~20. Small approximations were measured and cannot close the gap
(M1_NOTES.md, "the 48 fps goal"). This plan needs the user's approval.

## Budget
One update plus its drawing: ~267 scanlines (enemies 89, collisions/rest 37,
stage events 14, HUD 13, shots 8, drawing 103). Target: <= ~225 so every
frame is drawn (>= 80 of 100 at worst). Cut needed: ~40-45 lines.

## Steps (each exact unless marked; measured before/after)
1. Enemy-vs-shot and enemy-vs-player collisions as one pass over a compact
   per-update list of live, collidable, near-view enemies (box, position,
   flags) built during actors_update, instead of re-reading 64-byte actor
   records and family tables in three loops. Expected 10-15 lines. Exact.
2. Spearman update restructured around its patrol state: walk/stand/home
   phases as a tight loop over the spearmen with the memoized probes, the
   choice test only for spearmen inside the player's row band. Expected
   8-12 lines. Exact (same order of rnd() calls must be kept: verified with
   -DTMEMO_CHECK-style self-checks against the current code).
3. Sprite drawing: per-family prepared SAT pieces with the VRAM base baked
   per resident pose (spearman walk/stand poses resident in stage 5 instead
   of loaded through the cache), removing cache lookups/DMA queueing for the
   common poses. Expected 15-25 lines. Exact visuals; needs a VRAM plan for
   stage 5 (the pose pool is 287 tiles: walk + stand poses must fit).
4. HUD: only changed digits written (score/gold/time), not whole segments.
   Expected 3-5 lines. Exact.
5. If still short: draw the hanging-bat roost every second frame
   (approximation, visible as a 30 Hz bat flutter only).

## Checks
Gates 24/24 and 825/825 after each step; updates per 100 and frames drawn
per 100 at every busy spot (stage 5 start, interior top, boss; stages 1-6);
headless frames of the interior top; M1_NOTES.md entry per step.

## Effort
About 2-3 days of work; steps 1, 2 and 4 are independent, step 3 needs the
VRAM plan first.
