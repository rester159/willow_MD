/* Family registry and the small families: effects, gold pickups, stage
 * objects (chests, rewards, the hidden object), bridge logs and sinking
 * platforms (specs/stage1_enemies.md 4, 5.3-5.5, 5.7, 6). */
#include <genesis.h>
#include "actor.h"
#include "coop.h"
#include "player.h"
#include "hud.h"
#include "shot.h"
#include "sound.h"
#include "s1spr.h"
#include "s2spr.h"
#include "s3spr.h"
#include "s4spr.h"
#include "s5spr.h"
#include "s6spr.h"
#include "magic.h"
#include "spell.h"

#define SET(name, n) static u8 hint_##name[n]; static PoseSet ps_##name = { name##_poses, name##_tiles, name##_anim, n, 0, 0, NULL, hint_##name, 0, name##_sat, 0 }
/* Large families: a VRAM slot per actor (their largest pose). */
#define SLOT(name, n, t) static u8 hint_##name[n]; static PoseSet ps_##name = { name##_poses, name##_tiles, name##_anim, n, t, 0, NULL, hint_##name, t, name##_sat, 0 }
SLOT(soldier, SOLDIER_POSES, SOLDIER_MAX_TILES);
SLOT(beast, BEAST_POSES, BEAST_MAX_TILES);
SLOT(archer, ARCHER_POSES, ARCHER_MAX_TILES);
SET(arrow, ARROW_POSES);
SLOT(sorcerer, SORCERER_POSES, SORCERER_MAX_TILES);
SLOT(spearman, SPEARMAN_POSES, SPEARMAN_MAX_TILES);
SLOT(arenabeast, ARENABEAST_POSES, ARENABEAST_MAX_TILES);
SET(axe, AXE_POSES);
SET(fireball, FIREBALL_POSES);
SET(squiggle, SQUIGGLE_POSES);
SET(groundfire, GROUNDFIRE_POSES);
SET(log, LOG_POSES);
SET(platform, PLATFORM_POSES);
SET(chest, CHEST_POSES);
SET(hidden, HIDDEN_POSES);
SET(gold, GOLD_POSES);
SET(burst, BURST_POSES);
SET(splash, SPLASH_POSES);
SET(sparkle, SPARKLE_POSES);
SET(bosshead, BOSSHEAD_POSES);
SET(bosspaw, BOSSPAW_POSES);
SET(bossfire, BOSSFIRE_POSES);
SET(windmill, WINDMILL_POSES);
SET(aldwin, ALDWIN_POSES);
SET(shopkeeper, SHOPKEEPER_POSES);
SET(companion, COMPANION_POSES);
/* Stage 2's wagon and its parts (tools/pack_stage2.sh). */
static u8 hint_s2wagon[S2_WAGON_POSES], hint_s2driver[S2_DRIVER_POSES], hint_s2horse[S2_HORSE_POSES];
static PoseSet ps_wagon = { s2_wagon_poses, s2_wagon_tiles, s2_wagon_anim, S2_WAGON_POSES, 0, 0, NULL, hint_s2wagon, 0, s2_wagon_sat, 0 };
static PoseSet ps_driver = { s2_driver_poses, s2_driver_tiles, s2_driver_anim, S2_DRIVER_POSES, 0, 0, NULL, hint_s2driver, 0, s2_driver_sat, 0 };
/* The horses: 81-tile poses, each set in one slot of its own (the riders' horses share their pose: src/fam_chase.c). */
static PoseSet ps_horse = { s2_horse_poses, s2_horse_tiles, s2_horse_anim, S2_HORSE_POSES, S2_HORSE_MAX_TILES, 0, NULL, hint_s2horse, S2_HORSE_MAX_TILES, s2_horse_sat, 1 };
static u8 hint_s2rhorse[S2_HORSE_POSES];
static PoseSet ps_rhorse = { s2_horse_poses, s2_horse_tiles, s2_horse_anim, S2_HORSE_POSES, S2_HORSE_MAX_TILES, 0, NULL, hint_s2rhorse, S2_HORSE_MAX_TILES, s2_horse_sat, 1 };
/* Stage 2's own families (specs/stage2.md 5). */
#define SET2(name, N) static u8 hint_s2##name[S2_##N##_POSES]; static PoseSet ps_##name = { s2_##name##_poses, s2_##name##_tiles, s2_##name##_anim, S2_##N##_POSES, 0, 0, NULL, hint_s2##name, 0, s2_##name##_sat, 0 }
SET2(chandelier, CHANDELIER);
SET2(flame, FLAME);
SET2(shutter, SHUTTER);
SET2(debris, DEBRIS);
SET2(doorfx, DOORFX);
#define SLOT2(name, N) static u8 hint_s2##name[S2_##N##_POSES]; static PoseSet ps_##name = { s2_##name##_poses, s2_##name##_tiles, s2_##name##_anim, S2_##N##_POSES, S2_##N##_MAX_TILES, 0, NULL, hint_s2##name, S2_##N##_MAX_TILES, s2_##name##_sat, 0 }
SLOT2(crossbow, CROSSBOW);
SLOT2(lurker, LURKER);
SLOT2(emerge, EMERGE);
SLOT2(rider, RIDER);
SLOT2(boarder, BOARDER);
SLOT2(hawk, HAWK);
SLOT2(hound, HOUND);
SET2(rock, ROCK);
/* Stage 3's own families (specs/stage3.md 5; tools/pack_stage3.sh). */
#define SET3(name, N) static u8 hint_s3##name[S3_##N##_POSES]; static PoseSet ps3_##name = { s3_##name##_poses, s3_##name##_tiles, s3_##name##_anim, S3_##N##_POSES, 0, 0, NULL, hint_s3##name, 0, s3_##name##_sat, 0 }
#define SLOT3(name, N) static u8 hint_s3##name[S3_##N##_POSES]; static PoseSet ps3_##name = { s3_##name##_poses, s3_##name##_tiles, s3_##name##_anim, S3_##N##_POSES, S3_##N##_MAX_TILES, 0, NULL, hint_s3##name, S3_##N##_MAX_TILES, s3_##name##_sat, 0 }
SLOT3(trooper, TROOPER);
SET3(bomb, BOMB);
SLOT3(knight, KNIGHT);
SET3(lance, LANCE);
SET3(pillar, PILLAR);
SET3(boat, BOAT);
SET3(raft, RAFT);
SLOT3(archer, ARCHER);
SLOT3(fish, FISH);
SLOT3(hawk, HAWK);
SET3(rock, ROCK);
SLOT3(boss3, BOSS3);
SET3(bolt3, BOLT3);
SET3(glow, GLOW);
SET3(serpent, SERPENT);
SLOT3(figure, FIGURE);
/* Stage 4's own families (specs/stage4.md 5, 6; tools/pack_stage4.sh). */
#define SET4(name, N) static u8 hint_s4##name[S4_##N##_POSES]; static PoseSet ps4_##name = { s4_##name##_poses, s4_##name##_tiles, s4_##name##_anim, S4_##N##_POSES, 0, 0, NULL, hint_s4##name, 0, s4_##name##_sat, 0 }
#define SLOT4(name, N) static u8 hint_s4##name[S4_##N##_POSES]; static PoseSet ps4_##name = { s4_##name##_poses, s4_##name##_tiles, s4_##name##_anim, S4_##N##_POSES, S4_##N##_MAX_TILES, 0, NULL, hint_s4##name, S4_##N##_MAX_TILES, s4_##name##_sat, 0 }
SLOT4(snowball, SNOWBALL);
SET4(bat, BAT);
SLOT4(hawk, HAWK);
SLOT4(tent, TENT);
SET4(tspear, TSPEAR);
SET4(dragon, DRAGON);
SET4(fire, FIRE);
SLOT4(sorsha, SORSHA);
SET4(clash, CLASH);
SLOT4(shield, SHIELD);
SLOT4(crash, CRASH);
SLOT4(ending, ENDING);
SLOT4(introhorse, INTROHORSE);
/* Stage 5's own families (specs/stage5.md 5; tools/pack_stage5.sh). */
#define SET5(name, N) static u8 hint_s5##name[S5_##N##_POSES]; static PoseSet ps5_##name = { s5_##name##_poses, s5_##name##_tiles, s5_##name##_anim, S5_##N##_POSES, 0, 0, NULL, hint_s5##name, 0, s5_##name##_sat, 0 }
#define SLOT5(name, N) static u8 hint_s5##name[S5_##N##_POSES]; static PoseSet ps5_##name = { s5_##name##_poses, s5_##name##_tiles, s5_##name##_anim, S5_##N##_POSES, S5_##N##_MAX_TILES, 0, NULL, hint_s5##name, S5_##N##_MAX_TILES, s5_##name##_sat, 0 }
SLOT5(troll, TROLL);
SET5(orb, ORB);
SLOT5(pillar, PILLAR);
SLOT5(doorway, DOORWAY);
SLOT5(cart, CART);
SLOT5(head, HEAD);
/* Eborsisk's flames by group (flames 0-2, 3-5, 6-8, 9-10): one VRAM slot a
 * group, the group's flames showing one pose (src/fam_boss5.c). */
static u8 hint_s5flame[4][S5_FLAME_POSES];
static PoseSet ps5_flame[4] = {
    { s5_flame_poses, s5_flame_tiles, s5_flame_anim, S5_FLAME_POSES, 4, 0, NULL, hint_s5flame[0], 4, s5_flame_sat, 1 },
    { s5_flame_poses, s5_flame_tiles, s5_flame_anim, S5_FLAME_POSES, 16, 0, NULL, hint_s5flame[1], 16, s5_flame_sat, 1 },
    { s5_flame_poses, s5_flame_tiles, s5_flame_anim, S5_FLAME_POSES, 36, 0, NULL, hint_s5flame[2], 36, s5_flame_sat, 1 },
    { s5_flame_poses, s5_flame_tiles, s5_flame_anim, S5_FLAME_POSES, 64, 0, NULL, hint_s5flame[3], 64, s5_flame_sat, 1 } };
static const s16 slash4_boxes[8] = { 12, 25, 16, 24, 12, 25, 16, 24 };   /* 0x26756's box (1, 25, 16, 24) centred */
static u8 hint_s2cratearcher[S2_CRATEARCHER_POSES];
static PoseSet ps_cratearcher = { s2_cratearcher_poses, s2_cratearcher_tiles, s2_cratearcher_anim, S2_CRATEARCHER_POSES, S2_CRATEARCHER_MAX_TILES, 0, NULL, hint_s2cratearcher, S2_CRATEARCHER_MAX_TILES, s2_cratearcher_sat, 0 };

extern void soldier_update(Actor *a);
extern void beast_update(Actor *a);
extern void archer_update(Actor *a);
extern void arrow_update(Actor *a);
extern void sorcerer_update(Actor *a);
extern void fireball_update(Actor *a);
extern void log_update(Actor *a);
extern void platform_update(Actor *a);
extern void sage_update(Actor *a);
extern void boss_body_update(Actor *a);
extern void boss_head_update(Actor *a);
extern void boss_paw_update(Actor *a);
extern void boss_fire_update(Actor *a);
extern const s16 boss_body_boxes[5][2][4];
extern void scenery_update(Actor *a);
extern void spearman_update(Actor *a);
extern void axe_update(Actor *a);
extern void arena_beast_update(Actor *a);
extern void shopkeeper_update(Actor *a);
extern void companion_update(Actor *a);
extern void wagon_update(Actor *a);
extern void wagon_part_update(Actor *a);
extern void chandelier_update(Actor *a);
extern void flame_update(Actor *a);
extern void cratearcher_update(Actor *a);
extern void shutter_update(Actor *a);
extern void crossbow_update(Actor *a);
extern void lurker_update(Actor *a);
extern void debris_update(Actor *a);
extern void emerge_update(Actor *a);
extern void rider_update(Actor *a);
extern void rhorse_update(Actor *a);
extern void boarder_update(Actor *a);
extern void hawk_update(Actor *a);
extern void rock_update(Actor *a);
extern void hound_update(Actor *a);
extern void torch_update(Actor *a);
extern void trooper_update(Actor *a);
extern void bomb_update(Actor *a);
extern void knight_update(Actor *a);
extern void lance_update(Actor *a);
extern void pillar_update(Actor *a);
extern void boat_update(Actor *a);
extern void raft_update(Actor *a);
extern void rarcher_update(Actor *a);
extern void rspear_update(Actor *a);
extern void fish_update(Actor *a);
extern void hawk3_update(Actor *a);
extern void rock3_update(Actor *a);
extern void boss3_update(Actor *a);
extern void bolt3_update(Actor *a);
extern void glow_update(Actor *a);
extern void serpent_update(Actor *a);
extern void figure_update(Actor *a);
extern void snowball_update(Actor *a);
extern void bat_update(Actor *a);
extern void hawk4_update(Actor *a);
extern void tent_update(Actor *a);
extern void tspear_update(Actor *a);
extern void dragon_update(Actor *a);
extern void dragonp_update(Actor *a);
extern void fire_update(Actor *a);
extern void sorsha_update(Actor *a);
extern void slash4_update(Actor *a);
extern void clash_update(Actor *a);
extern void shield_update(Actor *a);
extern void crash_update(Actor *a);
extern void ending_update(Actor *a);
extern void introhorse_update(Actor *a);
extern void troll_update(Actor *a);
extern void orb_update(Actor *a);
extern void stone_update(Actor *a);
extern void doorway_update(Actor *a);
extern void cart_update(Actor *a);
extern void ebor_update(Actor *a);
extern void neck5_update(Actor *a);
extern void head5_update(Actor *a);
extern void flame5_update(Actor *a);
extern void gate5_update(Actor *a);
static const s16 neck5_box[8] = { 40, 136, 0, 96, 40, 136, 0, 96 };   /* stage5.md 6.1: the sprite neck's box */
static void effect_update(Actor *a);
static void gold_update(Actor *a);
static void chest_update(Actor *a);
static void reward_update(Actor *a);
static void hidden_update(Actor *a);

#define FAM(name, fn, set, cls) static const Family fam_##name = { fn, set, set ? (const s16 *)name##_boxes : NULL, cls }
static const Family fam_soldier = { soldier_update, &ps_soldier, soldier_boxes, C_ENEMY };
static const Family fam_beast = { beast_update, &ps_beast, beast_boxes, C_ENEMY };
static const Family fam_archer = { archer_update, &ps_archer, archer_boxes, C_ENEMY };
static const Family fam_sorcerer = { sorcerer_update, &ps_sorcerer, sorcerer_boxes, C_ENEMY };
static const Family fam_arrow = { arrow_update, &ps_arrow, arrow_boxes, C_ESHOT };
static const Family fam_fireball = { fireball_update, &ps_fireball, fireball_boxes, C_ESHOT };
static const Family fam_squiggle = { fireball_update, &ps_squiggle, squiggle_boxes, C_ESHOT };
extern void groundfire_update(Actor *a);
static const Family fam_groundfire = { groundfire_update, &ps_groundfire, groundfire_boxes, C_ESHOT };
static const Family fam_log = { log_update, &ps_log, log_boxes, C_SURFACE };
static const Family fam_platform = { platform_update, &ps_platform, platform_boxes, C_SURFACE };
static const Family fam_burst = { effect_update, &ps_burst, burst_boxes, C_EFFECT };
static const Family fam_splash = { effect_update, &ps_splash, splash_boxes, C_EFFECT };
static const Family fam_sparkle = { effect_update, &ps_sparkle, sparkle_boxes, C_EFFECT };
static const Family fam_gold = { gold_update, &ps_gold, gold_boxes, C_PICKUP };
static const Family fam_chest = { chest_update, &ps_chest, chest_boxes, C_OBJECT };
static const Family fam_reward = { reward_update, &ps_gold, gold_boxes, C_OBJECT };
static const Family fam_sage = { sage_update, &ps_sorcerer, sorcerer_boxes, C_ENEMY };
static const Family fam_boss_body = { boss_body_update, NULL, &boss_body_boxes[0][0][0], C_ENEMY };
static const Family fam_boss_head = { boss_head_update, &ps_bosshead, bosshead_boxes, C_ENEMY };
static const Family fam_boss_paw = { boss_paw_update, &ps_bosspaw, bosspaw_boxes, C_ENEMY };
static const Family fam_boss_fire = { boss_fire_update, &ps_bossfire, bossfire_boxes, C_ESHOT };
static const Family fam_windmill = { scenery_update, &ps_windmill, windmill_boxes, C_EFFECT };
static const Family fam_aldwin = { scenery_update, &ps_aldwin, aldwin_boxes, C_EFFECT };
static const Family fam_spearman = { spearman_update, &ps_spearman, spearman_boxes, C_ENEMY };
static const Family fam_axe = { axe_update, &ps_axe, axe_boxes, C_ESHOT };
static const Family fam_arena_beast = { arena_beast_update, &ps_arenabeast, arenabeast_boxes, C_ENEMY };
static const Family fam_companion = { companion_update, &ps_companion, companion_boxes, C_OBJECT };
static const Family fam_wagon = { wagon_update, &ps_wagon, s2_wagon_boxes, C_SURFACE };
static const Family fam_wheel = { wagon_part_update, &ps_wagon, s2_wagon_boxes, C_EFFECT };
static const Family fam_driver = { wagon_part_update, &ps_driver, s2_driver_boxes, C_EFFECT };
static const Family fam_horse = { wagon_part_update, &ps_horse, s2_horse_boxes, C_EFFECT };
static const Family fam_chandelier = { chandelier_update, &ps_chandelier, s2_chandelier_boxes, C_ENEMY };
static const Family fam_cratearcher = { cratearcher_update, &ps_cratearcher, s2_cratearcher_boxes, C_ENEMY };
static const Family fam_shutter = { shutter_update, &ps_shutter, s2_shutter_boxes, C_EFFECT };
static const Family fam_crossbow = { crossbow_update, &ps_crossbow, s2_crossbow_boxes, C_ENEMY };
static const Family fam_lurker = { lurker_update, &ps_lurker, s2_lurker_boxes, C_ENEMY };
static const Family fam_debris = { debris_update, &ps_debris, s2_debris_boxes, C_EFFECT };
static const Family fam_emerge = { emerge_update, &ps_emerge, s2_emerge_boxes, C_ENEMY };
static const Family fam_doorfx = { effect_update, &ps_doorfx, s2_doorfx_boxes, C_EFFECT };
static const Family fam_rider = { rider_update, &ps_rider, s2_rider_boxes, C_ENEMY };
static const Family fam_rhorse = { rhorse_update, &ps_rhorse, s2_horse_boxes, C_OBJECT };   /* drawn behind its rider */
static const Family fam_boarder = { boarder_update, &ps_boarder, s2_boarder_boxes, C_ENEMY };
static const Family fam_hawk = { hawk_update, &ps_hawk, s2_hawk_boxes, C_ENEMY };
static const Family fam_rock = { rock_update, &ps_rock, s2_rock_boxes, C_ESHOT };
static const Family fam_hound = { hound_update, &ps_hound, s2_hound_boxes, C_ENEMY };
static const Family fam_torch = { torch_update, NULL, NULL, C_EFFECT };
static const Family fam_flame = { flame_update, &ps_flame, s2_flame_boxes, C_ESHOT };
static const Family fam_shop = { shopkeeper_update, &ps_shopkeeper, shopkeeper_boxes, C_OBJECT };
static const Family fam_hidden = { hidden_update, &ps_hidden, hidden_boxes, C_OBJECT };
static const Family fam_trooper = { trooper_update, &ps3_trooper, s3_trooper_boxes, C_ENEMY };
static const Family fam_bomb = { bomb_update, &ps3_bomb, s3_bomb_boxes, C_ESHOT };
static const Family fam_knight = { knight_update, &ps3_knight, s3_knight_boxes, C_ENEMY };
static const Family fam_lance = { lance_update, &ps3_lance, s3_lance_boxes, C_ESHOT };
static const Family fam_pillar = { pillar_update, &ps3_pillar, s3_pillar_boxes, C_SURFACE };
static const Family fam_boat = { boat_update, &ps3_boat, s3_boat_boxes, C_SURFACE };
static const Family fam_raft = { raft_update, &ps3_raft, s3_raft_boxes, C_OBJECT };
static const Family fam_rarcher = { rarcher_update, &ps3_archer, s3_archer_boxes, C_ENEMY };
static const Family fam_rspear = { rspear_update, &ps_spearman, s3_spearman_boxes, C_ENEMY };
static const Family fam_fish = { fish_update, &ps3_fish, s3_fish_boxes, C_ENEMY };
static const Family fam_hawk3 = { hawk3_update, &ps3_hawk, s3_hawk_boxes, C_ENEMY };
static const Family fam_rock3 = { rock3_update, &ps3_rock, s3_rock_boxes, C_ESHOT };
static const Family fam_boss3 = { boss3_update, &ps3_boss3, s3_boss3_boxes, C_ENEMY };
static const Family fam_bolt3 = { bolt3_update, &ps3_bolt3, s3_bolt3_boxes, C_ESHOT };
static const Family fam_glow = { glow_update, &ps3_glow, s3_glow_boxes, C_EFFECT };
static const Family fam_serpent = { serpent_update, &ps3_serpent, s3_serpent_boxes, C_ENEMY };   /* bodies: touch only (game.c) */
static const Family fam_figure = { figure_update, &ps3_figure, s3_figure_boxes, C_EFFECT };
static const Family fam_snowball = { snowball_update, &ps4_snowball, s4_snowball_boxes, C_ENEMY };
static const Family fam_bat = { bat_update, &ps4_bat, s4_bat_boxes, C_ENEMY };
static const Family fam_hawk4 = { hawk4_update, &ps4_hawk, s4_hawk_boxes, C_ENEMY };
static const Family fam_tent = { tent_update, &ps4_tent, s4_tent_boxes, C_ENEMY };
static const Family fam_tspear = { tspear_update, &ps4_tspear, s4_tspear_boxes, C_ESHOT };
static const Family fam_dragon = { dragon_update, &ps4_dragon, s4_dragon_boxes, C_ENEMY };
static const Family fam_dragonp = { dragonp_update, &ps4_dragon, s4_dragon_boxes, C_EFFECT };
static const Family fam_fire = { fire_update, &ps4_fire, s4_fire_boxes, C_ESHOT };
static const Family fam_sorsha = { sorsha_update, &ps4_sorsha, s4_sorsha_boxes, C_ENEMY };
/* Sorsha's Quest: Madmartigan as her rival, the boss's logic on his player
 * poses and tiles (tools/rival.py); families_rival swaps him in for chapter IV. */
extern const u8 mad_poses[], mad_tiles[], rival_anim[];
extern const s16 rival_sat[], rival_boxes[];
static u8 hint_rival[70];
static PoseSet ps_rival = { mad_poses, mad_tiles, rival_anim, 70, 43, 0, NULL, hint_rival, 0, rival_sat, 0 };
static const Family fam_rival = { sorsha_update, &ps_rival, rival_boxes, C_ENEMY };
void families_rival(bool on) { families[F_SORSHA] = on ? &fam_rival : &fam_sorsha; }
static const Family fam_slash4 = { slash4_update, NULL, slash4_boxes, C_ESHOT };
static const Family fam_clash = { clash_update, &ps4_clash, s4_clash_boxes, C_EFFECT };
static const Family fam_shield = { shield_update, &ps4_shield, s4_shield_boxes, C_OBJECT };
static const Family fam_crash = { crash_update, &ps4_crash, s4_crash_boxes, C_EFFECT };
static const Family fam_ending = { ending_update, &ps4_ending, s4_ending_boxes, C_EFFECT };
static const Family fam_introhorse = { introhorse_update, &ps4_introhorse, s4_introhorse_boxes, C_EFFECT };
static const Family fam_troll = { troll_update, &ps5_troll, s5_troll_boxes, C_ENEMY };
static const Family fam_orb = { orb_update, &ps5_orb, s5_orb_boxes, C_ESHOT };
static const Family fam_stone = { stone_update, &ps5_pillar, s5_pillar_boxes, C_SURFACE };
static const Family fam_doorway = { doorway_update, &ps5_doorway, s5_doorway_boxes, C_OBJECT };
static const Family fam_gate5 = { gate5_update, NULL, NULL, C_OBJECT };
extern void door6_update(Actor *a);
static const Family fam_door6 = { door6_update, NULL, NULL, C_OBJECT };   /* stage 6's doors: triggers (src/fam_stage6.c) */
static const Family fam_cart = { cart_update, &ps5_cart, s5_cart_boxes, C_ENEMY };
static const Family fam_ebor = { ebor_update, NULL, NULL, C_OBJECT };
static const Family fam_neck5 = { neck5_update, NULL, neck5_box, C_ENEMY };
static const Family fam_head5 = { head5_update, &ps5_head, s5_head_boxes, C_ENEMY };
static const Family fam_flame5[4] = { { flame5_update, &ps5_flame[0], s5_flame_boxes, C_ESHOT }, { flame5_update, &ps5_flame[1], s5_flame_boxes, C_ESHOT },
                                      { flame5_update, &ps5_flame[2], s5_flame_boxes, C_ESHOT }, { flame5_update, &ps5_flame[3], s5_flame_boxes, C_ESHOT } };

/* Stage 6 (tools/pack_stage6.sh): the families it shares with stages 2-5
 * get their own pose sets and boxes (stage 2-5 data is banked out while
 * stage 6 plays: src/bank.c); families_stage swaps them in and back. */
#define SET6(name, N) static u8 hint_s6##name[S6_##N##_POSES]; static PoseSet ps6_##name = { s6_##name##_poses, s6_##name##_tiles, s6_##name##_anim, S6_##N##_POSES, 0, 0, NULL, hint_s6##name, 0, s6_##name##_sat, 0 }
#define SLOT6(name, N) static u8 hint_s6##name[S6_##N##_POSES]; static PoseSet ps6_##name = { s6_##name##_poses, s6_##name##_tiles, s6_##name##_anim, S6_##N##_POSES, S6_##N##_MAX_TILES, 0, NULL, hint_s6##name, S6_##N##_MAX_TILES, s6_##name##_sat, 0 }
SLOT6(crossbow, CROSSBOW);
SLOT6(knight, KNIGHT);
SET6(flail, FLAIL);
SLOT6(kael, KAEL);
SET6(kslash, KSLASH);
SLOT6(doorway, DOORWAY);
SLOT6(cart, CART);
SET6(stone, STONE);
SET6(crusher, CRUSHER);
SET6(chand, CHAND);
SLOT6(rarch, RARCH);
SLOT6(bav, BAV);
SET6(smoke, SMOKE);
SET6(spell, SPELL);
SLOT6(altar, ALTAR);
SLOT6(mortar, MORTAR);
SLOT6(trooper, TROOPER);
SET6(bomb, BOMB);
static const Family fam6_crossbow = { crossbow_update, &ps6_crossbow, s6_crossbow_boxes, C_ENEMY };
static const Family fam6_knight = { knight_update, &ps6_knight, s6_knight_boxes, C_ENEMY };
static const Family fam6_lance = { lance_update, &ps6_flail, s6_flail_boxes, C_ESHOT };
extern void kael_update(Actor *a), kslash_update(Actor *a);
static const Family fam6_kael = { kael_update, &ps6_kael, s6_kael_boxes, C_ENEMY };
static const Family fam6_kslash = { kslash_update, &ps6_kslash, s6_kslash_boxes, C_ESHOT };
static const Family fam6_doorway = { doorway_update, &ps6_doorway, s6_doorway_boxes, C_OBJECT };
static const Family fam6_cart = { cart_update, &ps6_cart, s6_cart_boxes, C_ENEMY };
extern void stone6_update(Actor *a), crusher6_update(Actor *a);
static const Family fam_stone6 = { stone6_update, &ps6_stone, NULL, C_EFFECT };
static const Family fam_crush6 = { crusher6_update, &ps6_crusher, s6_crusher_boxes, C_SURFACE };
extern void whirl6_update(Actor *a), smoke6_update(Actor *a);
static const Family fam_whirl6 = { whirl6_update, &ps6_bav, NULL, C_EFFECT };
static const Family fam_smoke6 = { smoke6_update, &ps6_smoke, NULL, C_EFFECT };
extern void bav_update(Actor *a), sglow6_update(Actor *a), spark6_update(Actor *a), ball6_update(Actor *a), glow6_update(Actor *a),
    bolt6_update(Actor *a), candle6_update(Actor *a), flame6_update(Actor *a), brazier6_update(Actor *a), fire6_update(Actor *a);
static const Family fam_bav6 = { bav_update, &ps6_bav, s6_bav_boxes, C_ENEMY };
static const Family fam_sglow6 = { sglow6_update, &ps6_spell, s6_spell_boxes, C_EFFECT };
static const Family fam_spark6 = { spark6_update, &ps6_spell, s6_spell_boxes, C_ESHOT };
static const Family fam_ball6 = { ball6_update, &ps6_spell, s6_spell_boxes, C_ESHOT };
static const Family fam_glow6 = { glow6_update, &ps6_spell, s6_spell_boxes, C_EFFECT };
static const Family fam_bolt6 = { bolt6_update, &ps6_spell, s6_spell_boxes, C_ESHOT };
static const Family fam_candle6 = { candle6_update, &ps6_spell, s6_spell_boxes, C_EFFECT };
static const Family fam_flame6 = { flame6_update, &ps6_spell, s6_spell_boxes, C_ESHOT };
static const Family fam_brazier6 = { brazier6_update, &ps6_altar, s6_altar_boxes, C_ESHOT };
static const Family fam_fire6 = { fire6_update, &ps6_spell, s6_spell_boxes, C_ESHOT };
extern void mortar6_update(Actor *a), mpart6_update(Actor *a), trooper6_update(Actor *a);
static const Family fam_mortar6 = { mortar6_update, &ps6_mortar, s6_mortar_boxes, C_ENEMY };
static const Family fam_mpart6 = { mpart6_update, &ps6_mortar, s6_mortar_boxes, C_EFFECT };
static const Family fam_troop6 = { trooper6_update, &ps6_trooper, s6_trooper_boxes, C_ENEMY };
static const Family fam6_bomb = { bomb_update, &ps6_bomb, s6_bomb_boxes, C_ESHOT };
extern void pspark6_update(Actor *a);
static const Family fam_pspark6 = { pspark6_update, &ps6_spell, s6_spell_boxes, C_EFFECT };
extern void rarch6_update(Actor *a);
static const Family fam_rarch6 = { rarch6_update, &ps6_rarch, s6_rarch_boxes, C_ENEMY };

static const struct { u8 f; const Family *fam; } swap6[] = {
    { F_CROSSBOW, &fam6_crossbow }, { F_KNIGHT, &fam6_knight }, { F_LANCE, &fam6_lance },
    { F_KAEL, &fam6_kael }, { F_KSLASH, &fam6_kslash }, { F_DOORWAY, &fam6_doorway }, { F_CART, &fam6_cart },
    { F_STONE6, &fam_stone6 }, { F_CRUSH6, &fam_crush6 }, { F_WHIRL6, &fam_whirl6 }, { F_SMOKE6, &fam_smoke6 },
    { F_BAV, &fam_bav6 }, { F_SGLOW6, &fam_sglow6 }, { F_SPARK6, &fam_spark6 }, { F_BALL6, &fam_ball6 }, { F_LGLOW6, &fam_glow6 },
    { F_CGLOW6, &fam_glow6 }, { F_BOLT6, &fam_bolt6 }, { F_CANDLE6, &fam_candle6 }, { F_FLAME6, &fam_flame6 },
    { F_BRAZIER6, &fam_brazier6 }, { F_FIRE6, &fam_fire6 },
    { F_MORTAR6, &fam_mortar6 }, { F_MPART6, &fam_mpart6 }, { F_TROOP6, &fam_troop6 }, { F_BOMB, &fam6_bomb }, { F_PSPARK6, &fam_pspark6 }, { F_RARCH6, &fam_rarch6 },
};
static const Family *saved6[sizeof swap6 / sizeof swap6[0]];

static void families_swap6(bool on)
{
    u16 i;
    for (i = 0; i < sizeof swap6 / sizeof swap6[0]; i++)
    {
        if (on && !saved6[i]) { saved6[i] = families[swap6[i].f]; families[swap6[i].f] = swap6[i].fam; }
        else if (!on && saved6[i]) { families[swap6[i].f] = saved6[i]; saved6[i] = NULL; }
    }
}

const Family *families[F_COUNT] = {
    [F_SOLDIER] = &fam_soldier, [F_BEAST] = &fam_beast, [F_ARCHER] = &fam_archer,
    [F_SORCERER] = &fam_sorcerer, [F_APPARITION] = &fam_sorcerer,
    [F_ARROW] = &fam_arrow, [F_FIREBALL] = &fam_fireball, [F_SQUIGGLE] = &fam_squiggle, [F_GROUNDFIRE] = &fam_groundfire,
    [F_LOG] = &fam_log, [F_PLATFORM] = &fam_platform,
    [F_BURST] = &fam_burst, [F_SPLASH] = &fam_splash, [F_SPARKLE] = &fam_sparkle,
    [F_GOLD] = &fam_gold,
    [F_CHEST] = &fam_chest, [F_REWARD] = &fam_reward, [F_HEART] = &fam_reward, [F_HIDDEN] = &fam_hidden,
    [F_SAGE] = &fam_sage, [F_BOSS_BODY] = &fam_boss_body, [F_BOSS_HEAD] = &fam_boss_head,
    [F_BOSS_PAW] = &fam_boss_paw, [F_BOSS_FIRE] = &fam_boss_fire,
    [F_WINDMILL] = &fam_windmill, [F_ALDWIN] = &fam_aldwin,
    [F_SPEARMAN] = &fam_spearman, [F_AXE] = &fam_axe, [F_ARENA_BEAST] = &fam_arena_beast,
    [F_SHOP] = &fam_shop, [F_COMPANION] = &fam_companion,
    [F_WAGON] = &fam_wagon, [F_WHEEL] = &fam_wheel, [F_DRIVER] = &fam_driver, [F_HORSE] = &fam_horse,
    [F_CHANDELIER] = &fam_chandelier, [F_FLAME] = &fam_flame, [F_CRATEARCHER] = &fam_cratearcher,
    [F_SHUTTER] = &fam_shutter, [F_CROSSBOW] = &fam_crossbow, [F_LURKER] = &fam_lurker, [F_DEBRIS] = &fam_debris,
    [F_EMERGE] = &fam_emerge, [F_DOORFX] = &fam_doorfx,
    [F_RIDER] = &fam_rider, [F_RHORSE] = &fam_rhorse, [F_BOARDER] = &fam_boarder, [F_HAWK] = &fam_hawk,
    [F_ROCK] = &fam_rock, [F_HOUND] = &fam_hound, [F_TORCH] = &fam_torch,
    [F_TROOPER] = &fam_trooper, [F_BOMB] = &fam_bomb, [F_KNIGHT] = &fam_knight, [F_LANCE] = &fam_lance,
    [F_PILLAR] = &fam_pillar, [F_BOAT] = &fam_boat, [F_RAFT] = &fam_raft, [F_RARCHER] = &fam_rarcher,
    [F_RSPEAR] = &fam_rspear, [F_FISH] = &fam_fish, [F_HAWK3] = &fam_hawk3, [F_ROCK3] = &fam_rock3,
    [F_BOSS3] = &fam_boss3, [F_BOLT3] = &fam_bolt3, [F_GLOW] = &fam_glow, [F_SERPENT] = &fam_serpent, [F_SERPENTB] = &fam_serpent,
    [F_FIGURE] = &fam_figure,
    [F_SNOWBALL] = &fam_snowball, [F_BAT] = &fam_bat, [F_HAWK4] = &fam_hawk4, [F_TENT] = &fam_tent, [F_TSPEAR] = &fam_tspear,
    [F_DRAGON] = &fam_dragon, [F_DRAGONP] = &fam_dragonp, [F_FIRE] = &fam_fire, [F_SORSHA] = &fam_sorsha,
    [F_SLASH4] = &fam_slash4, [F_CLASH] = &fam_clash, [F_SHIELD] = &fam_shield, [F_CRASH] = &fam_crash, [F_ENDING] = &fam_ending, [F_INTROHORSE] = &fam_introhorse,
    [F_TROLL] = &fam_troll, [F_ORB] = &fam_orb, [F_STONE] = &fam_stone, [F_DOORWAY] = &fam_doorway,
    [F_CART] = &fam_cart, [F_EBOR] = &fam_ebor, [F_NECK5] = &fam_neck5, [F_HEAD5] = &fam_head5, [F_FLAME5] = &fam_flame5[0], [F_FLAME5B] = &fam_flame5[1], [F_FLAME5C] = &fam_flame5[2], [F_FLAME5D] = &fam_flame5[3], [F_GATE5] = &fam_gate5, [F_DOOR6] = &fam_door6,
};

extern const u8 shot_poses[], shot_tiles[], spark_poses[], spark_tiles[];
extern const s16 shot_sat[], spark_sat[];

/* Another stage's art for the families it shares with stage 1 (same poses,
 * that stage's colours: tools/pack_stage2.sh): stage 0 or 1. */
static void art(PoseSet *ps, const u8 *poses, const u8 *tiles, const s16 *sat)
{
    ps->poses = poses; ps->tiles = tiles; ps->sat = sat;
}

void families_stage(u16 stage)
{
    families_swap6(stage == 5);
#ifndef SPEAR_SLOTS5
    /* Stage 5's interior has up to 8 spearmen on screen: per-actor slots
     * (54 tiles each) ran out of VRAM and left some undrawn; the shared
     * cache keeps one copy per pose in use (and the last pose when short). */
    ps_spearman.slot_tiles = stage == 4 ? 0 : ps_spearman.max_tiles;
#endif
    if (stage == 1)
    {
        art(&ps_soldier, s2_soldier_poses, s2_soldier_tiles, s2_soldier_sat);
        art(&ps_archer, s2_archer_poses, s2_archer_tiles, s2_archer_sat);
        art(&ps_arrow, s2_arrow_poses, s2_arrow_tiles, s2_arrow_sat);
        art(&ps_chest, s2_chest_poses, s2_chest_tiles, s2_chest_sat);
        art(&ps_gold, s2_gold_poses, s2_gold_tiles, s2_gold_sat);
        art(&ps_burst, s2_burst_poses, s2_burst_tiles, s2_burst_sat);
        art(&ps_spearman, s2_spearman_poses, s2_spearman_tiles, s2_spearman_sat);
        art(&ps_axe, s2_axe_poses, s2_axe_tiles, s2_axe_sat);
        art(&ps_flame, s2_flame_poses, s2_flame_tiles, s2_flame_sat);
    }
    else if (stage == 3)
    {
        /* Stage 4 (tools/pack_stage4.sh). */
        art(&ps_soldier, s4_soldier_poses, s4_soldier_tiles, s4_soldier_sat);
        art(&ps_archer, s4_archer_poses, s4_archer_tiles, s4_archer_sat);
        art(&ps_arrow, s4_arrow_poses, s4_arrow_tiles, s4_arrow_sat);
        art(&ps_chest, s4_chest_poses, s4_chest_tiles, s4_chest_sat);
        art(&ps_gold, s4_gold_poses, s4_gold_tiles, s4_gold_sat);
        art(&ps_burst, s4_burst_poses, s4_burst_tiles, s4_burst_sat);
        art(&ps_spearman, s4_spearman_poses, s4_spearman_tiles, s4_spearman_sat);
        art(&ps_axe, s4_axe_poses, s4_axe_tiles, s4_axe_sat);
        art(&ps_hidden, s4_hidden_poses, s4_hidden_tiles, s4_hidden_sat);
        art(&ps_companion, s4_companion_poses, s4_companion_tiles, s4_companion_sat);
        art(&ps3_trooper, s4_trooper_poses, s4_trooper_tiles, s4_trooper_sat);
        art(&ps3_bomb, s4_bomb_poses, s4_bomb_tiles, s4_bomb_sat);
    }
    else if (stage == 4)
    {
        /* Stage 5 (tools/pack_stage5.sh). */
        art(&ps_soldier, s5_soldier_poses, s5_soldier_tiles, s5_soldier_sat);
        art(&ps_archer, s5_archer_poses, s5_archer_tiles, s5_archer_sat);
        art(&ps_arrow, s5_arrow_poses, s5_arrow_tiles, s5_arrow_sat);
        art(&ps_chest, s5_chest_poses, s5_chest_tiles, s5_chest_sat);
        art(&ps_gold, s5_gold_poses, s5_gold_tiles, s5_gold_sat);
        art(&ps_burst, s5_burst_poses, s5_burst_tiles, s5_burst_sat);
        art(&ps_spearman, s5_spearman_poses, s5_spearman_tiles, s5_spearman_sat);
        art(&ps_axe, s5_axe_poses, s5_axe_tiles, s5_axe_sat);
        art(&ps4_bat, s5_bat_poses, s5_bat_tiles, s5_bat_sat);
    }
    else if (stage == 5)
    {
        /* Stage 6 (tools/pack_stage6.sh; section .s6, banked in). */
        art(&ps_soldier, s6_soldier_poses, s6_soldier_tiles, s6_soldier_sat);
        art(&ps_archer, s6_archer_poses, s6_archer_tiles, s6_archer_sat);
        art(&ps_arrow, s6_arrow_poses, s6_arrow_tiles, s6_arrow_sat);
        art(&ps_chest, s6_chest_poses, s6_chest_tiles, s6_chest_sat);
        art(&ps_gold, s6_gold_poses, s6_gold_tiles, s6_gold_sat);
        art(&ps_burst, s6_burst_poses, s6_burst_tiles, s6_burst_sat);
        art(&ps_spearman, s6_spearman_poses, s6_spearman_tiles, s6_spearman_sat);
        art(&ps_axe, s6_axe_poses, s6_axe_tiles, s6_axe_sat);
    }
    else if (stage == 2)
    {
        /* Stage 3 (tools/pack_stage3.sh): no soldiers, archers or beasts are placed. */
        art(&ps_arrow, s3_arrow_poses, s3_arrow_tiles, s3_arrow_sat);
        art(&ps_chest, s3_chest_poses, s3_chest_tiles, s3_chest_sat);
        art(&ps_gold, s3_gold_poses, s3_gold_tiles, s3_gold_sat);
        art(&ps_burst, s3_burst_poses, s3_burst_tiles, s3_burst_sat);
        art(&ps_spearman, s3_spearman_poses, s3_spearman_tiles, s3_spearman_sat);
        art(&ps_axe, s3_axe_poses, s3_axe_tiles, s3_axe_sat);
        art(&ps_splash, s3_splash_poses, s3_splash_tiles, s3_splash_sat);
        art(&ps_hidden, s3_hidden_poses, s3_hidden_tiles, s3_hidden_sat);
        art(&ps_companion, s3_companion_poses, s3_companion_tiles, s3_companion_sat);
        art(&ps3_trooper, s3_trooper_poses, s3_trooper_tiles, s3_trooper_sat);
        art(&ps3_bomb, s3_bomb_poses, s3_bomb_tiles, s3_bomb_sat);
    }
    else
    {
        art(&ps_soldier, soldier_poses, soldier_tiles, soldier_sat);
        art(&ps_archer, archer_poses, archer_tiles, archer_sat);
        art(&ps_arrow, arrow_poses, arrow_tiles, arrow_sat);
        art(&ps_chest, chest_poses, chest_tiles, chest_sat);
        art(&ps_gold, gold_poses, gold_tiles, gold_sat);
        art(&ps_burst, burst_poses, burst_tiles, burst_sat);
        art(&ps_spearman, spearman_poses, spearman_tiles, spearman_sat);
        art(&ps_axe, axe_poses, axe_tiles, axe_sat);
        art(&ps_splash, splash_poses, splash_tiles, splash_sat);
        art(&ps_hidden, hidden_poses, hidden_tiles, hidden_sat);
        art(&ps_companion, companion_poses, companion_tiles, companion_sat);
        art(&ps_flame, flame_poses, flame_tiles, flame_sat);   /* the sorcerer's ground fire */
    }
    if (stage == 3) art(&ps4_bat, s4_bat_poses, s4_bat_tiles, s4_bat_sat);
    if (stage == 5)
    {
        shot_art(s6_shot_poses, s6_shot_tiles, s6_shot_sat);
        spark_art(s6_spark_poses, s6_spark_tiles, s6_spark_sat);
        spell_art(s6_spell1_poses, s6_spell1_tiles, s6_spell1_sat);
    }
    else if (stage == 4)
    {
        shot_art(s5_shot_poses, s5_shot_tiles, s5_shot_sat);
        spark_art(s5_spark_poses, s5_spark_tiles, s5_spark_sat);
        spell_art(s5_spell_poses, s5_spell_tiles, s5_spell_sat);
    }
    else if (stage == 2)
    {
        shot_art(s3_shot_poses, s3_shot_tiles, s3_shot_sat);
        spark_art(s3_spark_poses, s3_spark_tiles, s3_spark_sat);
        spell_art(s3_spell_poses, s3_spell_tiles, s3_spell_sat);
    }
    else
    {
        shot_art(shot_poses, shot_tiles, shot_sat);
        spark_art(spark_poses, spark_tiles, spark_sat);
        spell_art(spell_poses, spell_tiles, spell_sat);
    }
    /* Stages 2 and 4 (Madmartigan only): the sparks of his charged sword's
     * extension in the stage's colours. */
    if (stage == 1) spark_art(s2_spark_poses, s2_spark_tiles, s2_spark_sat);
    if (stage == 3) spark_art(s4_spark_poses, s4_spark_tiles, s4_spark_sat);
    if (stage == 3) sword_art_set(s4_sword_poses, s4_sword_tiles, s4_sword_sat);
    else if (stage == 4) sword_art_set(s5_sword_poses, s5_sword_tiles, s5_sword_sat);
    else if (stage == 5) sword_art_set(s6_sword_poses, s6_sword_tiles, s6_sword_sat);
    else sword_art_set(s2_sword_poses, s2_sword_tiles, s2_sword_sat);
    poseset_reload(&ps_gold);
    poseset_reload(&ps_splash);
    if (stage == 3 || stage == 4) poseset_reload(&ps4_bat);
    posecache_flush();
}

const PoseSet *bat_poseset(void) { return &ps4_bat; }
const PoseSet *chain6_poses(void) { return &ps6_chand; }   /* stage 6's crusher chains (src/fam_stage6b.c) */   /* stage 5's roost (src/fam_stage5.c) */

/* The bridge logs' one sprite per pose, resolved once (logs_draw): y, size,
 * attribute (with the resident tile), x; size 0xFF: an empty pose. */
static struct { s16 dy; u16 size; u16 attr; s16 dx; } log_spr[LOG_POSES];

static void logs_prepare(void)
{
    u16 p;
    for (p = 0; p < LOG_POSES; p++)
    {
        const u8 *d = ps_log.poses + p * 6;
        const s16 *q = ps_log.sat + (((d[4] << 8) | d[5]) << 2);
        log_spr[p].size = d[3] ? (u16)q[1] : 0xFF;
        log_spr[p].dy = q[0];
        log_spr[p].attr = q[2] + ps_log.vram_of[p];
        log_spr[p].dx = q[3];
    }
}

/* Resident art (call before posecache_init takes the rest of the sprite
 * VRAM): the bridge logs, up to 30 at once. */
void families_init(void)
{
    static u16 vram_log[LOG_POSES], vram_sparkle[SPARKLE_POSES];
    ps_log.vram_of = vram_log;
    poseset_resident(&ps_log);
    logs_prepare();
    static u16 vram_gold[GOLD_POSES], vram_splash[SPLASH_POSES];
    ps_sparkle.vram_of = vram_sparkle;      /* the sorcerer's entrance: a new one every update */
    poseset_resident(&ps_sparkle);
    ps_gold.vram_of = vram_gold;            /* showers of up to 15 */
    poseset_resident(&ps_gold);
    ps_splash.vram_of = vram_splash;        /* the bridge collapse: one per log */
    poseset_resident(&ps_splash);
    {
        /* Bats (stages 4 and 5): stage 5's flock hangs ~20 on screen at once. */
        static u16 vram_bat[S4_BAT_POSES > S5_BAT_POSES ? S4_BAT_POSES : S5_BAT_POSES];
        ps4_bat.vram_of = vram_bat;
        poseset_resident(&ps4_bat);
    }
    { extern void flakes_init(void); flakes_init(); }   /* stage 4's falling snow (src/fam_stage4.c) */
    { extern void spray_init(void); spray_init(); }     /* stage 4's sled spray (src/fam_stage4.c) */
}

/* The bridge logs (up to 30 on screen): one sprite each from resident art,
 * written in a tight loop (the general draw path cost ~700 cycles a log). */
/* The log slots in slot order, rebuilt when a surface starts or is freed
 * (src/actor.c sets surf_dirty): the draw walks only these. */
u8 surf_dirty = 1;
static u8 log_list[34], log_n;

u16 logs_draw(VDPSprite *sat, u16 first, u16 max, s16 view_x, s16 view_y)
{
    Actor *a = actors + pool_lo[C_SURFACE], *end = actors + pool_hi[C_SURFACE];
    s16 ox = 128 - view_x, oy = 128 + 511 - view_y;
    u16 k = first;
    VDPSprite *o = sat + first;
    if (surf_dirty)
    {
        Actor *e = actors + pool_lo[C_SURFACE + 1];
        surf_dirty = 0;
        log_n = 0;
        for (; a < e; a++) if (a->fam == F_LOG) log_list[log_n++] = a - actors;
        a = actors + pool_lo[C_SURFACE];
    }
    /* The logs: x never changes (v[1]); one prepared sprite a pose
     * (68000 loop: ~190 cycles a log, GCC's ~300). Actor offsets: active 1,
     * y 16, v[1] 26, pose 46, shown 48; log_spr 8 bytes: dy, size, attr, dx. */
    if (log_n)
    {
        const u8 *li = log_list;
        u16 n = log_n - 1, full = 0;
        Actor *base = actors;
        __asm__ volatile (
            "1:\n\t"
            "moveq #0,%%d0\n\t"
            "move.b (%[li])+,%%d0\n\t"
            "lsl.w #6,%%d0\n\t"
            "lea 0(%[base],%%d0.w),%%a0\n\t"
            "tst.b 1(%%a0)\n\t"
            "beq.s 2f\n\t"
            "move.w 26(%%a0),%%d1\n\t"          /* sx */
            "add.w %[ox],%%d1\n\t"
            "move.w %%d1,%%d0\n\t"
            "sub.w #32,%%d0\n\t"
            "cmp.w #512,%%d0\n\t"
            "bhi.s 2f\n\t"
            "move.l 16(%%a0),%%d0\n\t"           /* sy = oy - (y >> 8) */
            "asr.l #8,%%d0\n\t"
            "move.w %[oy],%%d2\n\t"
            "sub.w %%d0,%%d2\n\t"
            "move.w %%d2,%%d0\n\t"
            "sub.w #32,%%d0\n\t"
            "cmp.w #416,%%d0\n\t"
            "bhi.s 2f\n\t"
            "move.w 46(%%a0),%%d0\n\t"           /* the pose's sprite */
            "lsl.w #3,%%d0\n\t"
            "lea 0(%[spr],%%d0.w),%%a1\n\t"
            "cmp.w #255,2(%%a1)\n\t"
            "beq.s 2f\n\t"
            "cmp.w %[max],%[k]\n\t"
            "bcc.s 3f\n\t"
            "addq.w #1,%[k]\n\t"
            "add.w (%%a1),%%d2\n\t"
            "move.w %%d2,(%[o])+\n\t"
            "move.w 2(%%a1),%%d0\n\t"
            "or.w %[k],%%d0\n\t"
            "move.w %%d0,(%[o])+\n\t"
            "move.w 4(%%a1),(%[o])+\n\t"
            "add.w 6(%%a1),%%d1\n\t"
            "move.w %%d1,(%[o])+\n\t"
            "move.w 46(%%a0),48(%%a0)\n"
            "2:\n\t"
            "dbra %[n],1b\n\t"
            "bra.s 4f\n"
            "3:\n\t"
            "moveq #1,%[full]\n"
            "4:"
            : [li] "+a" (li), [o] "+a" (o), [k] "+d" (k), [n] "+d" (n), [full] "+d" (full)
            : [base] "a" (base), [spr] "a" (log_spr), [ox] "d" (ox), [oy] "d" (oy), [max] "g" (max)
            : "d0", "d1", "d2", "a0", "a1", "cc", "memory");
        if (full) return k - first;
    }
    /* The other surfaces (platforms, the wagon: the general path), in slot
     * order: the slots in surf_other (clear bits skipped 16, 8, 4 at a
     * time), then those past the mask's 32. */
    {
        extern u32 surf_other;
        u32 m = surf_other;
        u16 j = 0;
        Actor *lo = a;
        for (;;)
        {
            s16 sx, sy;
            u16 f;
            if (j < 32)
            {
                if (!m) { j = 32; continue; }
                if (!(u16)m) { m >>= 16; j += 16; }
                if (!(m & 0xFF)) { m >>= 8; j += 8; }
                if (!(m & 0xF)) { m >>= 4; j += 4; }
                while (!(m & 1)) { m >>= 1; j++; }
                m >>= 1;
            }
            a = lo + j++;
            if (a >= end) break;
            f = a->fam;
            if (!f || f == F_LOG || !a->active || f == F_BOAT) continue;   /* the boat: drawn first (actors_draw) */
            sx = AX(a) + ox;
            if ((u16)(sx - 32) > 512) continue;
            sy = oy - AY(a);
            if ((u16)(sy - 32) > 416) continue;
            if (k + 6 > max) break;
            k += pose_draw(families[f]->ps, a->pose, &a->shown, sat, k, sx - 128, sy - 128, a->hflip, &a->slot);
        }
    }
    return k - first;
}

/* Sparkles (class 12 kind 2, around the materialising sorcerer, one an
 * update, 16 updates each: chain 0x15498, 4 poses x 4): a particle list,
 * not actors (12-16 alive at once cost ~25,000 cycles a frame as actors). */
#define SPARKLES 24
static struct { s16 x, y; u8 t; } spk[SPARKLES];
static u8 nspk;

void sparkle_add(s16 x, s16 y)
{
    if (nspk == SPARKLES) return;
    spk[nspk].x = x; spk[nspk].y = y; spk[nspk].t = 0;
    nspk++;
}

void sparkles_clear(void)
{
    nspk = 0;
}

void sparkles_update(void)
{
    u16 i = 0;
    while (i < nspk)
        if (++spk[i].t >= 16) spk[i] = spk[--nspk];
        else i++;
}

u16 sparkles_draw(VDPSprite *sat, u16 first, u16 max, s16 view_x, s16 view_y)
{
    u16 i, k = first;
    s16 ox = 128 - view_x, oy = 128 + 511 - view_y;
    for (i = 0; i < nspk && k < max; i++)
    {
        u16 pose = (spk[i].t >> 2) & 3;
        const u8 *d = ps_sparkle.poses + pose * 6;
        const s16 *q = ps_sparkle.sat + (((d[4] << 8) | d[5]) << 2);
        u16 ns = d[3], base = ps_sparkle.vram_of[pose];
        s16 sx = spk[i].x + ox, sy = oy - spk[i].y;
        while (ns-- && k < max)
        {
            VDPSprite *o = sat + k++;
            o->y = sy + q[0];
            o->size_link = q[1] | k;
            o->attribut = q[2] + base;
            o->x = sx + q[3];
            q += 4;
        }
    }
    return k - first;
}

/* ---- Effects (class 12 kind 0 / 2; routine 3222E): play once, then go. */
static const u8 effect_life[3] = { 32, 32, 30 };    /* burst v4, v5; splash v0 (0x32260) */

static void effect_update(Actor *a)
{
    if (!a->state)
    {
        a->state = 2;
        if (a->fam == F_BURST) { actor_anim(a, a->variant == 5); a->timer = effect_life[a->variant == 5]; }
        else if (a->fam == F_SPLASH) { actor_anim(a, 0); a->timer = 30; }
        else if (a->fam == F_DOORFX) { actor_anim(a, 0); a->timer = 90; }     /* class 12 kind 0 variant 7 (0x32260) */
        else { actor_anim(a, 0); a->timer = 16; }
        return;
    }
    actor_step(a);
    if (!--a->timer) actor_free(a);
}

/* ---- Gold pickups (class 14 kind 0, routine 2CFC2; section 6). */
static const s16 spread[16] = { 0, 64, -64, 128, -128, 192, -192, 256, -256, 320, -320, 384, -384, 448, -448, 512 };

/* The player's hurt box grown by 12 px each side touches point (x, y). */
static bool player_takes(s16 x, s16 y)
{
    const s16 *b = player_box();
    s16 px = player.x >> 8, py = player.y >> 8;
    return x >= px - b[0] - 12 && x <= px - b[0] + b[1] + 12 && y >= py - b[2] - 12 && y <= py - b[2] + b[3] + 12;
}

static void gold_update(Actor *a)
{
    u16 slot = a - actors;
    s16 top;
    switch (a->state)
    {
    case 0:                                         /* launch */
        a->state = 2;
        actor_anim(a, a->variant);
        a->vx = spread[a->sub];
        a->vy = 768;
        a->v[0] = 768;                              /* bounce speed */
        a->timer = 16;
        a->active = 1;
        return;
    case 2:                                         /* rise */
        a->y += a->vy;
        a->vy -= 32;
        if (a->vy < 0) { a->state = 4; a->vy = 0; }
        break;
    case 4:                                         /* fall */
        a->y -= a->vy;
        if (a->vy < 896) a->vy += 32;
        /* the foot probe is 8 px below the origin (0x3C74, size 0): the coin rests 8 px above the surface */
        if (!a->timer && !(game_frame & 1) && terrain_floor(AX(a), AY(a) - 8, &top))
        {
            a->y = (s32)(top + 8) << 8;
            a->vx >>= 1;
            a->v[0] = (a->v[0] >> 1) * 3 >> 1;
            if (a->v[0]) { a->state = 2; a->vy = a->v[0]; }
            else { a->state = 6; a->vx = 0; }
        }
        if (AY(a) < CAM_Y - 32) { actor_free(a); return; }   /* fallen into the lake / a pit */
        break;
    }
    a->x += a->vx;
    if (a->timer) a->timer--;
    if (a->vx && !(game_frame & 3))
    {
        s16 sx = AX(a) - CAM_X;
        if ((sx < 8 && a->vx < 0) || (sx > 376 && a->vx > 0)) a->vx = -a->vx;
    }
    actor_step_i(a);
    if (++a->t2 >= 300)                             /* expiry: blink 12 frames */
    {
        a->active = (a->t2 & 2) ? 2 : 0;
        if (a->t2 >= 312) { actor_free(a); return; }
    }
    if (!((game_frame + slot) & 7) && (player_takes(AX(a), AY(a)) || (two_players && coop_takes(AX(a), AY(a)))))
    {
        sound_post(SND_PICKUP);
        if (two_players) coop_gold(a->variant, !player_takes(AX(a), AY(a)));   /* co-op: to the one who took it */
        else add_gold(a->variant);
        actor_free(a);
        return;
    }
    if (!((game_frame + slot) & 63))
    {
        s16 sx = AX(a) - CAM_X, sy = AY(a) - CAM_Y;
        if (sx < -96 || sx > 384 + 96 || sy < -96 || sy > 256 + 96) actor_free(a);
    }
}

/* ---- Treasure chest (class 16 kind 4, routine 37A08; section 5.4). */
static const s16 chest_box[4] = { 12, 24, 14, 17 };
/* Rewards (ROM 0x37EB2), set (FF83A4, rotating per new game) x chest:
 * gold variant, or 0xFF = heart. */
static const u8 chest_reward[4][5] = {
    { 0xFF, 0, 5, 0, 6 }, { 0, 0, 0xFF, 5, 6 }, { 6, 0, 0, 0xFF, 5 }, { 5, 6, 0, 0, 0xFF }
};
/* Stage 3 (stage3.md 8.1, ROM 0x37EA6 + 0x5C): chests 0-2. */
static const u8 chest_reward3[4][3] = { { 5, 0, 0xFF }, { 0, 5, 0xFF }, { 0xFF, 0, 5 }, { 5, 0xFF, 0 } };
extern u8 stage_index;
u8 chest_broken;            /* FF8387: bit per chest index */
u8 reward_set;              /* FF83A4 */

static void chest_update(Actor *a)
{
    u16 power;
    switch (a->state)
    {
    case 0:
        if (chest_broken & (1 << a->facing)) { actor_free(a); return; }
        a->state = 2;
        a->hp = 6;
        a->pose = 0;
        a->active = 1;
        break;
    case 2:
        if (a->timer) a->timer--;
        else if ((power = shot_test(AX(a), AY(a), chest_box, FALSE)) != 0)
        {
            sound_post(SND_ENEMY_HIT);
            a->timer = 12;
            a->hp = a->hp > power ? a->hp - power : 0;
            if (!a->hp)
            {
                chest_broken |= 1 << a->facing;
                a->state = 4;
                a->pose = 1;
                a->timer = 8;
            }
        }
        break;
    case 4:
        if (!--a->timer)
        {
            sound_post(SND_EXPLODE);
            effect(F_BURST, AX(a), AY(a), 4);
            a->state = 6;
            a->timer = 8;
        }
        break;
    case 6:
        if (!--a->timer)
        {
            u8 r = stage_index == 2 ? chest_reward3[reward_set][a->facing] : chest_reward[reward_set][a->facing];
            Actor *o = actor_new(r == 0xFF ? F_HEART : F_REWARD);
            if (o)
            {
                o->x = a->x;
                o->y = a->y - (r == 0xFF ? (4 << 8) : (r ? 0 : (8 << 8)));
                o->variant = r == 0xFF ? 0 : r;
            }
            actor_free(a);
        }
        break;
    }
}

/* ---- Chest rewards (class 16 kind 5 gold, kind 6 heart): wait to be
 * collected (checked every 8 frames). The stage-object art for these is not
 * extracted yet: the gold uses the pickup coin of the same value. */
static void reward_update(Actor *a)
{
    if (!a->state)
    {
        a->state = 2;
        actor_anim(a, a->fam == F_HEART ? 5 : a->variant);
        a->active = 2;
    }
    actor_step(a);
    if ((game_frame + (a - actors)) & 7) return;
    {
        u8 who = 0;
        if (!player_takes(AX(a), AY(a)))
        {
            if (!two_players || !coop_takes(AX(a), AY(a))) return;
            who = 1;                                /* co-op: the second player took it */
        }
        if (a->pad[2]) who = a->pad[2] - 1;          /* co-op: stage 4's ride coins alternate */
        sound_post(SND_PICKUP);
        if (two_players)
        {
            if (a->fam == F_HEART) coop_heart(a->variant, who);
            else coop_gold(a->variant, who);
            actor_free(a);
            return;
        }
    }
    if (a->fam == F_HEART)
    {
        hud.hp += a->variant ? 2 : 1;
        if (hud.hp > hud.max_hp) hud.hp = hud.max_hp;
        hud_dirty |= HUD_VITAL;
    }
    else add_gold(a->variant);
    actor_free(a);
}

/* ---- Hidden object (class 16 kind 9, routine 374FA; 5.5): invisible
 * without item 3; 20 hits uncover the companion (not in this build). */
u8 hidden_found;            /* FF83A5: bit 0 stage 1's, bit 1 stage 3's, bit 2 stage 4's */
static const s16 hidden_pos[3][2] = { { 896, 160 }, { 1248, 128 }, { 1312, 224 } };
static const s16 hidden_pos3[3][2] = { { 816, 400 }, { 848, 224 }, { 1264, 288 } };   /* stage3.md 8.3 (y - 256) */
static const s16 hidden_pos4[3][2] = { { 3280, 4704 }, { 3712, 5136 }, { 4016, 4832 } };   /* stage4.md 8.3: 0x37526 */
static const u8 hidden_pick[16] = { 0, 2, 1, 1, 0, 2, 0, 2, 0, 1, 1, 2, 0, 0, 2, 1 };       /* 0x37532 (byte offsets / 4), every stage */
static u8 hidden_bit(void) { return stage_index == 2 ? 2 : stage_index == 3 ? 4 : 1; }
static const s16 hidden_box[4] = { 10, 20, 16, 20 };

static void hidden_update(Actor *a)
{
    u16 power;
    if (!a->state)
    {
        const s16 *p;
        if (hidden_found & hidden_bit()) { actor_free(a); return; }
        if (stage_index == 3) p = hidden_pos4[hidden_pick[rnd() & 15]];
        else p = (stage_index == 2 ? hidden_pos3 : hidden_pos)[hidden_pick[rnd() & 15]];
        a->x = (s32)p[0] << 8;
        a->y = (s32)p[1] << 8;
        a->hp = 20;
#ifdef TEST_HIDDEN
        a->x = (s32)TEST_HIDDEN << 8;           /* test builds: on the path, one hit, a Quest-Stone */
        a->y = (s32)(TEST_HIDDEN_Y) << 8;
        a->hp = 1;
        hud.items[3] = 1;
#endif
        a->state = 2;
        actor_anim(a, 0);
        a->active = hud.items[3] ? 1 : 0;       /* shown only with the reveal item */
        return;
    }
    actor_step(a);
    if (a->timer) { a->timer--; return; }
    if (game_frame & 1) return;             /* shots tested on alternate frames, as enemies */
    if ((power = shot_test(AX(a), AY(a), hidden_box, FALSE)) != 0)
    {
        sound_post(SND_ENEMY_HIT);
        a->timer = 16;
        a->hp = a->hp > power ? a->hp - power : 0;
        if (!a->hp)
        {
            Actor *c;
            hidden_found |= hidden_bit();       /* FF83A5: the companion appears here */
            if ((c = actor_new(F_COMPANION)) != NULL)
            {
                c->x = a->x;
                c->y = a->y;
                c->state = c->phase = 0;
            }
            actor_free(a);
        }
    }
}
