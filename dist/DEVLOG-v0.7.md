V0.7: your bug reports, fixed (and what we still need from you)

Thanks to everyone who played V0.6 and sent reports. V0.7 fixes most of them. Download willow-v0.7.bin (5 MB; SHA-1 ec097c205116c37f8cdc3b16f7bfb0ef7360af87).

Important: no build since V0.5 has been run on a real console or cart yet. Everything below was tested in emulators only.

## Fixed in V0.7

- Missing platforms and holes in the scenery are filled on every stage: stage 2's invisible platforms, stage 3's tree island, stage 5's tower, the top of stage 1's canopy. The platform after stage 1's first shopkeeper is probably the same fault, but I haven't checked that exact spot.
- Stage 1 boss: his head and paw stay on his body. No more "disembodied limb".
- Stage 1 sorceress: her graphics are no longer corrupted when she first appears (no missing head, no white strip).
- Stage 2: handrails, balcony fronts and the big pillar are drawn in front of the characters, as in the arcade.
- Bavmorda's story picture: the light-blue squares are gone and the sky and rock colours match the arcade.
- Shop picture: the grey strips are gone and the red curtain stripes are back.
- Speed: the slowdown when the bridge collapses after the sorceress is much smaller (frames drawn per 100 at the worst moment: 47 to 62). Stage 6's corridor and two-player mode are smoother too. Game logic is unchanged and runs at full speed.

## Still open, and what I need to close it

1. Real hardware. If you have a console and a flashcart, please try V0.7 and tell me:
   - your console model, cart model and firmware version;
   - whether it boots, and whether you see any warning screen;
   - whether the top of stage 1's background is clean and the stage 1 boss looks right;
   - whether stage 6 and the ending play;
   - whether high scores survive turning the console off and on.

   V0.5 had trouble on the EverDrive MD v2.6 and did not boot on the Mega EverDrive PRO. The fixes for both have been in every build since V0.6, but nobody has confirmed them on a cart yet.

2. The bridge collapse in stage 1 is better but still uneven. I'm still working on it; nothing needed from you.

3. Corrupted world map after a death. I can't reproduce it, and I've removed two likely causes. If it still happens on V0.7, please tell me the stage, your emulator or cart, and send a screenshot.

4. Stage 3 "scrolls wildly, then gets stuck". I can't reproduce it. Please tell me:
   - where in stage 3 it happened;
   - what you did just before;
   - your emulator and its version (a save state helps a lot).

5. Random tiles on the stage 1 boss during the attract demo. I can't reproduce this either. It looks like an emulator or cart that doesn't handle the bank switching a 5 MB ROM needs. Please tell me your emulator and version.

6. The stage 2 shopkeeper is hard to reach. Not looked at yet. Which shopkeeper is it, and what stops you reaching him?

7. Under consideration:
   - more contrast on stage 1's earth tiles;
   - fewer enemies in stage 2's fight (that one would change the arcade's content);
   - scenery in front of the characters on stages 1 and 3, as on stage 2.

8. Small things I know about: GOLD and TIME show in blue in the shop, and the stage 5 interior top still slows down when the big spearman crowd is on screen.

Please keep the reports coming!
