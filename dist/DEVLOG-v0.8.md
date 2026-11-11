V0.8: stage 2 shop door fixed, browner stage 1 path

A small update with fixes from your V0.7 reports. Download willow-v0.8.bin (5 MB; SHA-1 374f6afb04a9ecb5c28906607723467ac37c8498).

Important: no build since V0.5 has been run on a real console or cart yet. Everything below was tested in emulators only.

## Fixed in V0.8

- Stage 2 shopkeeper. He was hard to reach because the game handled stage 2's shop like stage 1's, where you walk into the shopkeeper. Stage 2's shop works the way it does in the arcade now: nobody stands at the door. Walk up to it, hold Up and the shop opens after a moment. You can't be hit while it opens. This also removes the shopkeeper who floated in the doorway. The door-opening animation isn't in yet.
- Stage 1 earth. The upper band of the path is brown now, so it stands out from the stone. The lower band is still olive because there's no free colour for it.

## Looked at, not changed

- Scenery in front of the characters on stages 1 and 3, as on stage 2. I tried it, but on the Genesis that scenery also covers the bottom HUD bar. Every way of keeping the bar visible slowed the game down, so those stages stay as they were.
- Fewer enemies in stage 2's fight. That would change the arcade's content, so it's on hold.

## Still open, and what I need to close it

1. Real hardware. If you have a console and a flashcart, please try V0.8 and tell me:
   - your console model, cart model and firmware version;
   - whether it boots, and whether you see a warning screen;
   - whether the top of stage 1's background is clean and the stage 1 boss looks right;
   - whether stage 6 and the ending play;
   - whether high scores survive turning the console off and on.

2. The bridge collapse in stage 1 still slows down (about 60 of 100 frames drawn at the worst moment). Each frame there needs about 15% less work to reach full smoothness, and that's the next speed job. Nothing needed from you.

3. Corrupted world map after a death. I can't reproduce it. If it still happens on V0.8, please tell me the stage and your emulator or cart, and send a screenshot.

4. Stage 3 "scrolls wildly, then gets stuck". I can't reproduce it. Please tell me where in stage 3 it happened, what you did just before, and your emulator and version. A save state helps a lot.

5. Random tiles on the stage 1 boss during the attract demo. I can't reproduce it. It looks like an emulator or cart that doesn't do the bank switching a 5 MB ROM needs. Please tell me your emulator and version.

6. Small things I know about:
   - GOLD and TIME show in blue in the shop.
   - The stage 5 interior top slows down when the big spearman crowd is on screen.
   - Two-player mode is slower at the busiest spots.

Please keep the reports coming!
