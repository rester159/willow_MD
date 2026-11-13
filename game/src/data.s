/* Prepared stage data (tools/stagepack.py); the game only copies it. */
    .section .rodata
    .align 2
    .global s1_sprpal, s1_cycle, s1_pal, s1_tiles, s1_map, s1_loads, s1_bd_map, s1_bd_tiles
    .global mad_poses, mad_nodes, mad_boxes, mad_tiles, mad_pal
    .global willow_boxes, s1_col, s1_coldesc, willow_pal, willow_poses, willow_tiles, willow_nodes
    .global shot_pal, shot_poses, shot_tiles, shot_anim, hud_tiles, hud_partial, hud_partline
    .global spark_poses, spark_tiles, spark_anim, spark_sat, shot_sat
s1_pal:      .incbin "data/stage1/pal.bin"
    .balign 2
s1_map:      .incbin "data/stage1/map_meta.bin"
s1_loads:    .incbin "data/stage1/loads.bin"
    .align 2
s1_bd_map:   .incbin "data/stage1/bd_map.bin"
s1_bd_tiles: .incbin "data/stage1/bd_tiles.bin"
s1_cycle:    .incbin "data/stage1/cycle.bin"
s1_sprpal:   .incbin "data/s1spr/sprpal.bin"
    .global boss_body, boss_slots, boss_maps, boss_fur, boss_furflash
    /* Art pass: the boss body lives in banks 8-9 with stage 6 (section .s6,
     * linked at 0x300000): they are mapped in for all of stage 1
     * (game_stage_reset; game_next_stage maps stage 2-5's data back). */
    .pushsection .s6,"a"
    .balign 2
boss_body:   .incbin "data/boss1/body.bin"
    .popsection
    .balign 2
boss_slots:  .incbin "data/boss1/slots.bin"
    .balign 2
boss_maps:   .incbin "data/boss1/maps.bin"
boss_fur:    .incbin "data/s1spr/furpal.bin"
boss_furflash: .incbin "data/boss1/furflash.bin"
    .global intro_map_tiles, intro_map_map, intro_map_pal, intro_sky_tiles, intro_sky_map, intro_sky_pal, intro_font
    .balign 2
intro_map_tiles: .incbin "data/intro/map_tiles.apj"
intro_map_map:   .incbin "data/intro/share/map_map.apj"
intro_map_pal:   .incbin "data/intro/map_pal.bin"
intro_sky_tiles: .incbin "data/intro/sky_tiles.bin"
intro_sky_map:   .incbin "data/intro/sky_map.bin"
intro_sky_pal:   .incbin "data/intro/sky_pal.bin"
intro_font:      .incbin "data/intro/font.bin"
    .global intro_font_spr
intro_font_spr:  .incbin "data/intro/font_spr.bin"   /* tools/fontspr.py */
    .global intro_box_tiles, intro_box_map, intro_map_alt, story_tiles, story_map, story_pal
    .global story2_tiles, story2_map, story2_pal, font_tiles, font_pal, font_big, font_textpal
intro_box_tiles: .incbin "data/intro/box_tiles.bin"
intro_box_map:   .incbin "data/intro/box_map.bin"
intro_map_alt:   .incbin "data/intro/map_alt.bin"
story_map:       .incbin "data/intro/share/intro_story_map.apj"
    .global intro_map2_tiles, intro_map2_map, intro_map2_pal, intro_map2_alt
intro_map2_map:  .incbin "data/intro/share/map2_map.apj"
intro_map2_alt:  .incbin "data/intro/share/map2_alt.bin"
intro_map2_pal:
intro_map2_tiles: .incbin "data/intro/share/map2_xtiles.apj"
    .global intro_map3_tiles, intro_map3_map, intro_map3_pal, intro_map3_alt
intro_map3_map:  .incbin "data/intro/share/map3_map.apj"
intro_map3_alt:  .incbin "data/intro/share/map3_alt.bin"
intro_map3_pal:
intro_map3_tiles: .incbin "data/intro/share/map3_xtiles.apj"
    .global intro_map4_tiles, intro_map4_map, intro_map4_pal, intro_map4_alt
intro_map4_map:  .incbin "data/intro/share/map4_map.apj"
intro_map4_alt:  .incbin "data/intro/share/map4_alt.bin"
intro_map4_pal:
intro_map4_tiles: .incbin "data/intro/share/map4_xtiles.apj"
story_pal:       .incbin "data/intro/story_pal.bin"
story_tiles:     .incbin "data/intro/story_tiles.apj"
/* The menus' font (tools/fontpack.py). */
    .align 2
font_pal:        .incbin "data/font/pal.bin"
font_tiles:      .incbin "data/font/font.bin"
font_big:        .incbin "data/font/big.bin"
font_textpal:    .incbin "data/font/textpal.bin"
/* Stage 2's story page: Cherlindrea (tools/storypack2.py). */
    .align 2
story2_map:      .incbin "data/intro/share/intro_story2_map.apj"
story2_pal:      .incbin "data/intro/story2_pal.bin"
story2_tiles:    .incbin "data/intro/story2_tiles.apj"
/* Stage 3's story page: Fin Raziel (tools/storypack2.py, STORY=3). */
    .global story3_tiles, story3_map, story3_pal
    .align 2
story3_map:      .incbin "data/intro/share/intro_story3_map.apj"
story3_pal:      .incbin "data/intro/story3_pal.bin"
story3_tiles:    .incbin "data/intro/story3_tiles.apj"
    .align 2
    .global shop_tiles, shop_map, shop_alt, shop_pal, shop_spr
shop_map:        .incbin "data/intro/share/shop_map.apj"
shop_alt:        .incbin "data/shop/alt.bin"
shop_pal:        .incbin "data/shop/pal.bin"
shop_tiles:      .incbin "data/shop/tiles.apj"
shop_spr:        .incbin "data/shop/spr.bin"
    .global coop_line0
coop_line0:  .incbin "data/coop/line0.bin"    /* co-op: line 0 with both heroes (game/coop/tools/line0.py) */
willow_pal:  .incbin "data/willow/pal.bin"
willow_poses: .incbin "data/willow/poses.bin"
willow_nodes: .incbin "data/willow/nodes.bin"
    .align 2
willow_boxes: .incbin "data/willow/boxes.bin"
    .align 2
willow_tiles: .incbin "data/willow/tiles.bin"
/* Madmartigan (stage 2: tools/posepack.py NODE_FIRST=70, specs/madmartigan.md 4); his colours are
 * the stage's sprite line 0 (tools/pack_stage2.sh). */
mad_pal:     .incbin "data/mad/pal.bin"
    .global sorsha_poses, sorsha_nodes, sorsha_pal, sorsha_flash
    .align 2
sorsha_poses: .incbin "data/sorsha/poses.bin"   /* Home-only Sorsha (tools/sorsha_player.py) */
    .align 2
sorsha_nodes: .incbin "data/sorsha/nodes.bin"
    .align 2
sorsha_pal:   .incbin "data/sorsha/pal.bin"
sorsha_flash: .incbin "data/sorsha/flash.bin"
    .global sorsha_poses2p, sorsha_tiles2p, sorsha_line0_2p
    .align 2
sorsha_line0_2p: .incbin "data/sorsha/line0_2p.bin"   /* co-op Willow + Sorsha line 0 */
sorsha_poses2p: .incbin "data/sorsha/poses2p.bin"
    .align 2
sorsha_tiles2p: .incbin "data/sorsha/tiles2p.bin"   /* her player tiles in that line's pens */
mad_poses:   .incbin "data/mad/poses.bin"
mad_nodes:   .incbin "data/mad/nodes.bin"
    .align 2
mad_boxes:   .incbin "data/mad/boxes.bin"
    .align 2
mad_tiles:   .incbin "data/mad/tiles.bin"
    .align 2
shot_pal:    .incbin "data/shot/pal.bin"
shot_poses:  .incbin "data/shot/poses.bin"
shot_anim:   .incbin "data/shot/anim.bin"
    .align 2
shot_tiles:  .incbin "data/shot/tiles.bin"
hud_partline: .incbin "data/hud/partline.bin"
    .align 2
hud_tiles:   .incbin "data/hud/tiles.bin"
hud_partial: .incbin "data/hud/partial.bin"
    .global hud2_tiles, hud2_partial, hud2_partline
hud2_partline: .incbin "data/hud2/partline.bin"
    .align 2
hud2_tiles:  .incbin "data/hud2/tiles.bin"
hud2_partial: .incbin "data/hud2/partial.bin"
    .global hud3_tiles, hud3_partial, hud3_partline
hud3_partline: .incbin "data/hud3/partline.bin"
    .balign 2
hud3_tiles:  .incbin "data/hud3/tiles.bin"
hud3_partial: .incbin "data/hud3/partial.bin"
spark_poses: .incbin "data/spark/poses.bin"
spark_anim:  .incbin "data/spark/anim.bin"
    .align 2
spark_sat:   .incbin "data/spark/sat.bin"
shot_sat:    .incbin "data/shot/sat.bin"
    .align 2
spark_tiles: .incbin "data/spark/tiles.bin"
s1_coldesc:  .incbin "data/stage1/coldesc.bin"
    .balign 2
s1_col:      .incbin "data/stage1/col_trim.bin"
    .align 2
s1_tiles:    .incbin "data/stage1/tiles.bin"
/* From here, data only stages 2-5 use (section .stagedata: linked last, so
 * the common data stays below 0x300000, which stage 6's banks replace;
 * tools/md_willow.ld) - except the attract loop's pictures below. */
    .section .stagedata,"a"
    .global s2_pal, s2_map, s2_loads, s2_bd_map, s2_bd_tiles, s2_col, s2_coldesc, s2_tiles, s2_sprpal
s2_sprpal:   .incbin "data/s2spr/sprpal.bin"
    .global road_tiles, road_blocks, road_seq, s2_far_pal, torch_tiles, torch_words
s2_far_pal:  .incbin "data/stage2/far_pal.bin"
road_blocks: .incbin "data/stage2/road_blocks.bin"
road_seq:    .incbin "data/stage2/road_seq.bin"
    .balign 2
road_tiles:  .incbin "data/stage2/road_tiles.bin"
/* Stage 2's wall torches (tools/torchpack.py). */
    .align 2
torch_words: .incbin "data/stage2/torch_words.bin"
torch_tiles: .incbin "data/stage2/torch_tiles.bin"
    .align 2
/* Stage 5's interior wall torches (tools/torchpack5.py). */
    .global torch5_tiles, torch5_words
torch5_words: .incbin "data/stage5/torch5_words.bin"
torch5_tiles: .incbin "data/stage5/torch5_tiles.bin"
/* Stage 5's doorway wipe (tools/wipepack5.py). */
    .global wipe5_tiles, wipe5_words, wipe5_frame
    .balign 2
wipe5_words: .incbin "data/stage5/wipe5_words.bin"
wipe5_tiles: .incbin "data/stage5/wipe5_tiles.bin"
wipe5_frame: .incbin "data/stage5/wipe5_frame.bin"
    .balign 2
    .align 2
    .balign 2
s2_pal:      .incbin "data/stage2/pal.bin"
    .balign 2
s2_map:      .incbin "data/stage2/map_meta.bin"
s2_loads:    .incbin "data/stage2/loads.bin"
s2_bd_map:   .incbin "data/stage2/bd_map.bin"
s2_bd_tiles: .incbin "data/stage2/bd_tiles.bin"
s2_coldesc:  .incbin "data/stage2/coldesc.bin"
    .balign 2
s2_col:      .incbin "data/stage2/col_trim.bin"
    .balign 2
s2_tiles:    .incbin "data/stage2/tiles.bin"

/* Stage 3: Fin Raziel's island (tools/stagepack.py, colpack.py; specs/stage3.md). */
    .global s3_pal, s3_map, s3_loads, s3_bd_map, s3_bd_tiles, s3_col, s3_coldesc, s3_tiles, s3_sprpal
    .balign 2
s3_sprpal:   .incbin "data/s3spr/sprpal.bin"
    .global s3_storm, s3_cycle
s3_storm:    .incbin "data/stage3/storm.bin"
s3_cycle:    .incbin "data/stage3/cycle.bin"
s3_pal:      .incbin "data/stage3/pal.bin"
    .balign 2
s3_map:      .incbin "data/stage3/map_meta.bin"
s3_loads:    .incbin "data/stage3/loads.bin"
s3_bd_map:   .incbin "data/stage3/bd_map.bin"
s3_bd_tiles: .incbin "data/stage3/bd_tiles.bin"
s3_coldesc:  .incbin "data/stage3/coldesc.bin"
    .balign 2
s3_col:      .incbin "data/stage3/col_trim.bin"
    .balign 2
s3_tiles:    .incbin "data/stage3/tiles.bin"

    .section .rodata
/* The Arcade attract loop's pictures (tools/attractpack.py). */
    .global att_cave_tiles, att_cave_map, att_cave_pal, att_willow_tiles, att_willow_map, att_willow_pal, att_mad_tiles, att_mad_map, att_mad_pal, att_logo_tiles, att_logo_map, att_logo_pal, att_capcom_tiles, att_capcom_map, att_capcom_pal, att_fbi_tiles, att_fbi_map, att_fbi_pal, att_dragon_tiles, att_dragon_map, att_dragon_pal
    .balign 2
att_cave_map:  .incbin "data/intro/share/att_cave_map.apj"
att_cave_pal:  .incbin "data/attract/att_cave_pal.bin"
att_cave_tiles: .incbin "data/attract/att_cave_tiles.apj"
    .balign 2
att_willow_map:  .incbin "data/intro/share/att_willow_map.apj"
att_willow_pal:  .incbin "data/attract/att_willow_pal.bin"
att_willow_tiles: .incbin "data/attract/att_willow_tiles.apj"
    .balign 2
att_mad_map:  .incbin "data/intro/share/att_mad_map.apj"
att_mad_pal:  .incbin "data/attract/att_mad_pal.bin"
att_mad_tiles: .incbin "data/attract/att_mad_tiles.apj"
    .balign 2
att_logo_map:  .incbin "data/intro/share/att_logo_map.apj"
att_logo_pal:  .incbin "data/attract/att_logo_pal.bin"
att_logo_tiles: .incbin "data/attract/att_logo_tiles.apj"
    .balign 2
att_capcom_map:  .incbin "data/intro/share/att_capcom_map.apj"
att_capcom_pal:  .incbin "data/attract/att_capcom_pal.bin"
att_capcom_tiles: .incbin "data/attract/att_capcom_tiles.apj"
    .balign 2
att_fbi_map:  .incbin "data/intro/share/att_fbi_map.apj"
att_fbi_pal:  .incbin "data/attract/att_fbi_pal.bin"
att_fbi_tiles: .incbin "data/attract/att_fbi_tiles.apj"
    .balign 2
att_dragon_map:  .incbin "data/intro/share/att_dragon_map.apj"
att_dragon_pal:  .incbin "data/attract/att_dragon_pal.bin"
att_dragon_tiles: .incbin "data/attract/att_dragon_tiles.apj"

    .section .stagedata,"a"
/* Stage 4: Sorsha's camp (tools/stagepack.py WORLD_TOP=8191 FAR_SHIFT=3,
 * colpack.py, maptrim.py; specs/stage4.md). */
    .global s4_pal, s4_map, s4_loads, s4_bd_map, s4_bd_tiles, s4_col, s4_coldesc, s4_tiles, s4_sprpal
    .global hud4_tiles, hud4_partial, hud4_partline, story4_tiles, story4_map, story4_pal
    .balign 2
s4_sprpal:   .incbin "data/s4spr/sprpal.bin"
s4_pal:      .incbin "data/stage4/pal.bin"
    .balign 2
s4_map:      .incbin "data/stage4/map_meta.bin"
s4_loads:    .incbin "data/stage4/loads.bin"
s4_bd_map:   .incbin "data/stage4/bd_map.bin"
s4_bd_tiles: .incbin "data/stage4/bd_tiles.bin"
s4_coldesc:  .incbin "data/stage4/coldesc.bin"
    .balign 2
s4_col:      .incbin "data/stage4/col_trim.bin"
    .balign 2
s4_tiles:    .incbin "data/stage4/tiles.bin"
hud4_partline: .incbin "data/hud4/partline.bin"
    .balign 2
hud4_tiles:  .incbin "data/hud4/tiles.bin"
hud4_partial: .incbin "data/hud4/partial.bin"
    .balign 2

/* Stage 5: Tir Asleen castle (tools/stagepack.py WORLD_TOP=767 FAR_SHIFT=1,
 * colpack.py, maptrim.py; specs/stage5.md). */
    .global s5_pal, s5_map, s5_loads, s5_bd_map, s5_bd_tiles, s5_col, s5_coldesc, s5_tiles, s5_sprpal
    .global hud5_tiles, hud5_partial, hud5_partline, story5_tiles, story5_map, story5_pal
    .global intro_map5_tiles, intro_map5_map, intro_map5_pal, intro_map5_alt
    .balign 2
s5_sprpal:   .incbin "data/s5spr/sprpal.bin"
s5_pal:      .incbin "data/stage5/pal.bin"
    .balign 2
s5_map:      .incbin "data/stage5/map_meta.bin"
s5_loads:    .incbin "data/stage5/loads.bin"
s5_bd_map:   .incbin "data/stage5/bd_map.bin"
s5_bd_tiles: .incbin "data/stage5/bd_tiles.bin"
s5_coldesc:  .incbin "data/stage5/coldesc.bin"
    .balign 2
s5_col:      .incbin "data/stage5/col_trim.bin"
    .balign 2
s5_tiles:    .incbin "data/stage5/tiles.bin"
hud5_partline: .incbin "data/hud5/partline.bin"
    .balign 2
hud5_tiles:  .incbin "data/hud5/tiles.bin"
hud5_partial: .incbin "data/hud5/partial.bin"
    .balign 2
    .balign 2
intro_map5_map:  .incbin "data/intro/share/map5_map.apj"
intro_map5_alt:  .incbin "data/intro/share/map5_alt.bin"
intro_map5_pal:
intro_map5_tiles: .incbin "data/intro/share/map5_xtiles.apj"

/* Stage 5's boss: the re-tiled arena and the necks' pictures (tools/bosspack5.py). */
    .global boss5_arena_map, boss5_neck_maps, boss5_arena_pal, boss5_neck_pal, boss5_arena_tiles, boss5_neck_tiles
    .balign 2
boss5_arena_pal:   .incbin "data/boss5/arena_pal.bin"
boss5_neck_pal:    .incbin "data/boss5/neck_pal.bin"
boss5_arena_map:   .incbin "data/boss5/arena_map.bin"
boss5_neck_maps:   .incbin "data/boss5/neck_maps.bin"
boss5_arena_tiles: .incbin "data/boss5/arena_tiles.bin"
boss5_neck_tiles:  .incbin "data/boss5/neck_tiles.bin"
    .global boss5_neck_relay
boss5_neck_relay:  .incbin "data/boss5/neck_relay.bin"


/* Stages 4 and 5's story pictures: in the common data, the ending shows
 * them again with stage 6's banks in (specs/stage6.md 7.3). */
    .section .rodata
    .balign 2
story4_map:  .incbin "data/intro/share/intro_story4_map.apj"
story4_pal:  .incbin "data/intro/story4_pal.bin"
story4_tiles: .incbin "data/intro/story4_tiles.apj"
    .balign 2
story5_map:  .incbin "data/intro/share/intro_story5_map.apj"
story5_pal:  .incbin "data/intro/story5_pal.bin"
story5_tiles: .incbin "data/intro/story5_tiles.apj"
    .balign 2

/* The world map before stage 6 (stage6.md 1.2; tools/intropack.py MAP6):
 * common data (shown while stage 5's data is still mapped). */
    .section .rodata
    .global intro_map6_tiles, intro_map6_map, intro_map6_pal, intro_map6_alt
    .balign 2
intro_map6_map:  .incbin "data/intro/share/map6_map.apj"
intro_map6_alt:  .incbin "data/intro/share/map6_alt.bin"
intro_map6_pal:
intro_map6_tiles: .incbin "data/intro/share/map6_xtiles.apj"
    .balign 2
    .section .rodata
/* The continue screen's figures (tools/contpack.py; tiles aPLib-packed). */
    .global contfig_poses, contfig_anim, contfig_tiles0, contfig_tiles1, contfig_pal0, contfig_pal1
    .balign 2
contfig_poses:  .incbin "data/contfig/poses.bin"
    .balign 2
contfig_pal0:   .incbin "data/contfig/pal0.bin"
contfig_pal1:   .incbin "data/contfig/pal1.bin"
contfig_tiles0: .incbin "data/contfig/tiles0.apj"
    .balign 2
contfig_tiles1: .incbin "data/contfig/tiles1.apj"
    .balign 2
contfig_anim:   .incbin "data/contfig/anim.bin"
    .balign 2
    .section .rodata
/* The opening screen's arcade title (tools/logopack.py). */
    .global logo_tiles, logo_map, logo_pal
    .balign 2
logo_tiles: .incbin "data/frontend/logo_tiles.bin"
logo_map:   .incbin "data/frontend/logo_map.bin"
logo_pal:   .incbin "data/frontend/logo_pal.bin"
    .balign 2
/* Sorsha's Quest: her world map (tools/questmap.py, tools/questpack.sh). */
    .global quest_map_map, quest_map_xtiles, quest_map_diff, quest_map_alt
    .balign 2
quest_map_map:    .incbin "data/quest/qmap_map.apj"
    .balign 2
quest_map_xtiles: .incbin "data/quest/qmap_xtiles.apj"
    .balign 2
quest_map_diff:   .incbin "data/quest/qmap_diff.bin"
    .balign 2
quest_map_alt:    .incbin "data/quest/qmap_alt.bin"
    .balign 2
/* Sorsha's Quest: Madmartigan as her rival (tools/rival.py). */
    .global rival_anim, rival_sat, rival_boxes
    .balign 2
rival_anim:  .incbin "data/rival/anim.bin"
    .balign 2
rival_sat:   .incbin "data/rival/sat.bin"
rival_boxes: .incbin "data/rival/boxes.bin"
    .balign 2
/* Sorsha's figure on her map (tools/questfig.py). */
    .global quest_fig_tiles, quest_fig_spr
    .balign 2
quest_fig_tiles: .incbin "data/quest/qfig_tiles.bin"
quest_fig_spr:   .incbin "data/quest/qfig_spr.bin"
    .balign 2
/* Sorsha's Quest's picture before chapter VI (stage 2-5 data is in then). */
    .global qpic6_tiles, qpic6_map, qpic6_pal
    .balign 2
qpic6_tiles: .incbin "data/quest/p6_return_tiles.apj"
    .balign 2
qpic6_map:   .incbin "data/quest/p6_return_map.apj"
    .balign 2
qpic6_pal:   .incbin "data/quest/p6_return_pal.bin"
    .balign 2
