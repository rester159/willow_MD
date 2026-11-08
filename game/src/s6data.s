/* Stage 6 and the ending's data (section .s6: linked at 0x300000, stored
 * from 0x400000; tools/md_willow.ld, src/bank.c). Readable only while
 * bank_stage6(TRUE). */
    .section .s6,"a"
    .global s6_marker
    .balign 2
s6_marker:  .ascii "STAGE6BANK"

/* The seven areas (tools/pack_stage6_areas.py; specs/stage6.md 2.1), each an
 * ordinary scenery set in its own native coordinates. */
    .global s6a0_pal, s6a0_map, s6a0_loads, s6a0_bd_map, s6a0_bd_tiles, s6a0_col, s6a0_coldesc, s6a0_tiles
    .balign 2
s6a0_pal: .incbin "data/stage6/a0/pal.bin"
    .balign 2
s6a0_map: .incbin "data/stage6/a0/map_meta.bin"
    .balign 2
s6a0_loads: .incbin "data/stage6/a0/loads.bin"
    .balign 2
s6a0_bd_map: .incbin "data/stage6/a0/bd_map.bin"
    .balign 2
s6a0_bd_tiles: .incbin "data/stage6/a0/bd_tiles.bin"
    .balign 2
s6a0_coldesc: .incbin "data/stage6/a0/coldesc.bin"
    .balign 2
s6a0_col: .incbin "data/stage6/a0/col_trim.bin"
    .balign 2
s6a0_tiles: .incbin "data/stage6/a0/tiles.bin"
    .balign 2
    .global s6a1_pal, s6a1_map, s6a1_loads, s6a1_bd_map, s6a1_bd_tiles, s6a1_col, s6a1_coldesc, s6a1_tiles
    .balign 2
s6a1_pal: .incbin "data/stage6/a1/pal.bin"
    .balign 2
s6a1_map: .incbin "data/stage6/a1/map_meta.bin"
    .balign 2
s6a1_loads: .incbin "data/stage6/a1/loads.bin"
    .balign 2
s6a1_bd_map: .incbin "data/stage6/a1/bd_map.bin"
    .balign 2
s6a1_bd_tiles: .incbin "data/stage6/a1/bd_tiles.bin"
    .balign 2
s6a1_coldesc: .incbin "data/stage6/a1/coldesc.bin"
    .balign 2
s6a1_col: .incbin "data/stage6/a1/col_trim.bin"
    .balign 2
s6a1_tiles: .incbin "data/stage6/a1/tiles.bin"
    .balign 2
    .global s6a2_pal, s6a2_map, s6a2_loads, s6a2_bd_map, s6a2_bd_tiles, s6a2_col, s6a2_coldesc, s6a2_tiles
    .balign 2
s6a2_pal: .incbin "data/stage6/a2/pal.bin"
    .balign 2
s6a2_map: .incbin "data/stage6/a2/map_meta.bin"
    .balign 2
s6a2_loads: .incbin "data/stage6/a2/loads.bin"
    .balign 2
s6a2_bd_map: .incbin "data/stage6/a2/bd_map.bin"
    .balign 2
s6a2_bd_tiles: .incbin "data/stage6/a2/bd_tiles.bin"
    .balign 2
s6a2_coldesc: .incbin "data/stage6/a2/coldesc.bin"
    .balign 2
s6a2_col: .incbin "data/stage6/a2/col_trim.bin"
    .balign 2
s6a2_tiles: .incbin "data/stage6/a2/tiles.bin"
    .balign 2
    .global s6a3_pal, s6a3_map, s6a3_loads, s6a3_bd_map, s6a3_bd_tiles, s6a3_col, s6a3_coldesc, s6a3_tiles
    .balign 2
s6a3_pal: .incbin "data/stage6/a3/pal.bin"
    .balign 2
s6a3_map: .incbin "data/stage6/a3/map_meta.bin"
    .balign 2
s6a3_loads: .incbin "data/stage6/a3/loads.bin"
    .balign 2
s6a3_bd_map: .incbin "data/stage6/a3/bd_map.bin"
    .balign 2
s6a3_bd_tiles: .incbin "data/stage6/a3/bd_tiles.bin"
    .balign 2
s6a3_coldesc: .incbin "data/stage6/a3/coldesc.bin"
    .balign 2
s6a3_col: .incbin "data/stage6/a3/col_trim.bin"
    .balign 2
s6a3_tiles: .incbin "data/stage6/a3/tiles.bin"
    .balign 2
    .global s6a4_pal, s6a4_map, s6a4_loads, s6a4_bd_map, s6a4_bd_tiles, s6a4_col, s6a4_coldesc, s6a4_tiles
    .balign 2
s6a4_pal: .incbin "data/stage6/a4/pal.bin"
    .balign 2
s6a4_map: .incbin "data/stage6/a4/map_meta.bin"
    .balign 2
s6a4_loads: .incbin "data/stage6/a4/loads.bin"
    .balign 2
s6a4_bd_map: .incbin "data/stage6/a4/bd_map.bin"
    .balign 2
s6a4_bd_tiles: .incbin "data/stage6/a4/bd_tiles.bin"
    .balign 2
s6a4_coldesc: .incbin "data/stage6/a4/coldesc.bin"
    .balign 2
s6a4_col: .incbin "data/stage6/a4/col_trim.bin"
    .balign 2
s6a4_tiles: .incbin "data/stage6/a4/tiles.bin"
    .balign 2
    .global s6a5_pal, s6a5_map, s6a5_loads, s6a5_bd_map, s6a5_bd_tiles, s6a5_col, s6a5_coldesc, s6a5_tiles
    .balign 2
s6a5_pal: .incbin "data/stage6/a5/pal.bin"
    .balign 2
s6a5_map: .incbin "data/stage6/a5/map_meta.bin"
    .balign 2
s6a5_loads: .incbin "data/stage6/a5/loads.bin"
    .balign 2
s6a5_bd_map: .incbin "data/stage6/a5/bd_map.bin"
    .balign 2
s6a5_bd_tiles: .incbin "data/stage6/a5/bd_tiles.bin"
    .balign 2
s6a5_coldesc: .incbin "data/stage6/a5/coldesc.bin"
    .balign 2
s6a5_col: .incbin "data/stage6/a5/col_trim.bin"
    .balign 2
s6a5_tiles: .incbin "data/stage6/a5/tiles.bin"
    .balign 2
    .global s6a6_pal, s6a6_map, s6a6_loads, s6a6_bd_map, s6a6_bd_tiles, s6a6_col, s6a6_coldesc, s6a6_tiles
    .balign 2
s6a6_pal: .incbin "data/stage6/a6/pal.bin"
    .balign 2
s6a6_map: .incbin "data/stage6/a6/map_meta.bin"
    .balign 2
s6a6_loads: .incbin "data/stage6/a6/loads.bin"
    .balign 2
s6a6_bd_map: .incbin "data/stage6/a6/bd_map.bin"
    .balign 2
s6a6_bd_tiles: .incbin "data/stage6/a6/bd_tiles.bin"
    .balign 2
s6a6_coldesc: .incbin "data/stage6/a6/coldesc.bin"
    .balign 2
s6a6_col: .incbin "data/stage6/a6/col_trim.bin"
    .balign 2
s6a6_tiles: .incbin "data/stage6/a6/tiles.bin"
    .balign 2

/* The stage's sprite CRAM and HUD recolour (tools/pack_stage6.sh). */
    .global s6_sprpal, hud6_tiles, hud6_partial, hud6_partline
    .balign 2
s6_sprpal:   .incbin "data/s6spr/sprpal.bin"
hud6_partline: .incbin "data/hud6/partline.bin"
    .balign 2
hud6_tiles:  .incbin "data/hud6/tiles.bin"
hud6_partial: .incbin "data/hud6/partial.bin"
    .balign 2


/* Bavmorda's picture after Kael (stage6.md 2.5, door variant 10; tools/storypack2.py STORY=6). */
    .global story6_tiles, story6_map, story6_pal
    .balign 2
story6_map:  .incbin "data/intro/story6_map.bin"
story6_pal:  .incbin "data/intro/story6_pal.bin"
story6_tiles: .incbin "data/intro/story6_tiles.apj"
    .balign 2

/* The ending (specs/stage6.md 7; tools/endpack.py, storypack2.py STORY=e0..e5). */
    .global end_por_map, end_por_pal, end_por_tiles
    .global end_sky_tiles, end_sky_map, end_castle_tiles, end_castle_map, end_seq, end_card_tiles, end_card_map, end_card_pal
    .balign 2
end_por_map:
    .incbin "data/ending/storye0_map.bin"
    .incbin "data/ending/storye1_map.bin"
    .incbin "data/ending/storye2_map.bin"
    .incbin "data/ending/storye3_map.bin"
    .incbin "data/ending/storye4_map.bin"
    .incbin "data/ending/storye5_map.bin"
end_por_pal:
    .incbin "data/ending/storye0_pal.bin"
    .incbin "data/ending/storye1_pal.bin"
    .incbin "data/ending/storye2_pal.bin"
    .incbin "data/ending/storye3_pal.bin"
    .incbin "data/ending/storye4_pal.bin"
    .incbin "data/ending/storye5_pal.bin"
    .balign 2
end_por_tiles:
    .long por0 - end_por_tiles
    .long por1 - end_por_tiles
    .long por2 - end_por_tiles
    .long por3 - end_por_tiles
    .long por4 - end_por_tiles
    .long por5 - end_por_tiles
por0: .incbin "data/ending/storye0_tiles.apj"
    .balign 2
por1: .incbin "data/ending/storye1_tiles.apj"
    .balign 2
por2: .incbin "data/ending/storye2_tiles.apj"
    .balign 2
por3: .incbin "data/ending/storye3_tiles.apj"
    .balign 2
por4: .incbin "data/ending/storye4_tiles.apj"
    .balign 2
por5: .incbin "data/ending/storye5_tiles.apj"
    .balign 2
end_sky_tiles:    .incbin "data/ending/end_sky_tiles.apj"
    .balign 2
end_sky_map:      .incbin "data/ending/end_sky_map.bin"
end_castle_tiles: .incbin "data/ending/end_castle_tiles.apj"
    .balign 2
end_castle_map:   .incbin "data/ending/end_castle_map.bin"
end_seq:          .incbin "data/ending/end_seq.bin"
end_card_tiles:   .incbin "data/ending/end_card_tiles.apj"
    .balign 2
end_card_map:     .incbin "data/ending/end_card_map.bin"
end_card_pal:     .incbin "data/ending/end_card_pal.bin"
    .balign 2

/* Stage 6's and the ending's music (tools/audio/run_s6.sh: XGM2 songs),
 * here so the common data stays within 4 MB: the Z80 reads them through
 * the mapper, so they play only while the banks are in (stage 6). */
    .global music_stage6, music_tower6, music_kael, music_bav, music_ending
    .balign 256
music_stage6: .incbin "data/audio/music0c.xgc"
    .balign 256
music_tower6: .incbin "data/audio/music0d.xgc"
    .balign 256
music_kael:   .incbin "data/audio/music0e.xgc"
    .balign 256
music_bav:    .incbin "data/audio/music0f.xgc"
    .balign 256
music_ending: .incbin "data/audio/music1d.xgc"
    .balign 2

/* Torch and chandelier frames per area (tools/torchpack6.py). */
    .global s6a0_anim_tiles, s6a0_anim_words, s6a1_anim_tiles, s6a1_anim_words, s6a2_anim_tiles, s6a2_anim_words, s6a3_anim_tiles, s6a3_anim_words
    .balign 2
s6a0_anim_words: .incbin "data/stage6/a0/anim_words.bin"
    .balign 2
s6a0_anim_tiles: .incbin "data/stage6/a0/anim_tiles.bin"
    .balign 2
s6a1_anim_words: .incbin "data/stage6/a1/anim_words.bin"
    .balign 2
s6a1_anim_tiles: .incbin "data/stage6/a1/anim_tiles.bin"
    .balign 2
s6a2_anim_words: .incbin "data/stage6/a2/anim_words.bin"
    .balign 2
s6a2_anim_tiles: .incbin "data/stage6/a2/anim_tiles.bin"
    .balign 2
s6a3_anim_words: .incbin "data/stage6/a3/anim_words.bin"
    .balign 2
s6a3_anim_tiles: .incbin "data/stage6/a3/anim_tiles.bin"
    .balign 2

/* Area 6's storm (tools/far6.py): the lit cells and each state's words. */
    .global far6_cells, far6_words
    .balign 2
far6_words: .incbin "data/stage6/a6/far6_words.bin"
far6_cells: .incbin "data/stage6/a6/far6_cells.bin"
    .balign 2
