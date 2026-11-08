// Stage 1 music (arcade command 0x13) and Willow's effects, converted
// offline (tools/audio, specs/audio.md). Effects are PCM rendered from the
// arcade's own YM2151 register logs.
XGM2 music_stage1 "../data/audio/music13.vgm"
WAV sfx_shot "../data/audio/render/sfx20_opm.wav" XGM2
WAV sfx_land "../data/audio/render/sfx2b_opm.wav" XGM2
WAV sfx_charge "../data/audio/render/sfx31_opm.wav" XGM2
WAV sfx_charged "../data/audio/render/sfx44_opm.wav" XGM2
WAV sfx_slide "../data/audio/render/sfx2a_opm.wav" XGM2
WAV sfx_time "../data/audio/render/sfx43_opm.wav" XGM2

// Remaining stage-1 cues (specs/audio_stage1.md, tools/audio/run.sh).
// Music: 0x01 boss (loops), 0x10 stage clear, 0x12 map / stage entry,
// 0x14 time over / game over, 0x15 continue (one-shots).
XGM2 music_boss "../data/audio/music01.vgm"
XGM2 music_clear "../data/audio/music10.vgm"
XGM2 music_map "../data/audio/music12.vgm"
XGM2 music_over "../data/audio/music14.vgm"
XGM2 music_continue "../data/audio/music15.vgm"
// 0x1B shop (posted with 0x02FA: its fade-in is in the data), loops.
XGM2 music_shop "../data/audio/music1b.vgm"
// Stage 2: 0x02 the fort, 0x03 the wagon chase (both loop).
XGM2 music_stage2 "../data/audio/music02.vgm"
XGM2 music_chase "../data/audio/music03.vgm"
// FM effects rendered from the original YM2151 writes (ymfm), tail trimmed.
WAV sfx_pickup "../data/audio/render/sfx2d_opm.wav" XGM2
WAV sfx_text "../data/audio/render/sfx2f_opm.wav" XGM2
WAV sfx_count "../data/audio/render/sfx30_opm.wav" XGM2
WAV sfx_enemy_hit "../data/audio/render/sfx32_opm.wav" XGM2
WAV sfx_explode "../data/audio/render/sfx33_opm.wav" XGM2
WAV sfx_arrow "../data/audio/render/sfx34_opm.wav" XGM2
WAV sfx_splash "../data/audio/render/sfx38_opm.wav" XGM2
WAV sfx_countdown "../data/audio/render/sfx42_opm.wav" XGM2
// OKI MSM6295 voice samples (original ADPCM decoded; stored at 6,650 Hz, played
// at half rate: the chip's own rate is about 7.6 kHz).
WAV sfx_hurt "../data/audio/render/oki82.wav" XGM2 6650
WAV sfx_death "../data/audio/render/oki83.wav" XGM2 6650
// Madmartigan (stage 2): sword 0x26, hurt 0x84, death 0x85.
WAV sfx_sword "../data/audio/render/sfx26_opm.wav" XGM2
WAV sfx_mad_hurt "../data/audio/render/oki84.wav" XGM2 6650
WAV sfx_mad_death "../data/audio/render/oki85.wav" XGM2 6650
// Stage 2 (and the stage-1 axe): flame 0x37, chandelier crash 0x3A + voice 0xA1, axe / spear
// throw 0x8B, boarder jumping aboard 0x90, window lurker 0xA0.
WAV sfx_flame "../data/audio/render/sfx37_opm.wav" XGM2
// Willow's charged spells 7-12 (specs/magic.md 4): 0x21 bolt, 0x22 fire walkers, 0x23 time stop,
// 0x24 rising bolt, 0x25 blast.
WAV sfx_spell21 "../data/audio/render/sfx21_opm.wav" XGM2
WAV sfx_spell22 "../data/audio/render/sfx22_opm.wav" XGM2
WAV sfx_spell23 "../data/audio/render/sfx23_opm.wav" XGM2
WAV sfx_spell24 "../data/audio/render/sfx24_opm.wav" XGM2
WAV sfx_spell25 "../data/audio/render/sfx25_opm.wav" XGM2
WAV sfx_crash "../data/audio/render/sfx3a_opm.wav" XGM2
WAV sfx_throw "../data/audio/render/oki8b.wav" XGM2 6650
WAV sfx_board "../data/audio/render/oki90.wav" XGM2 6650
WAV sfx_lurker "../data/audio/render/okia0.wav" XGM2 6650
WAV sfx_crash_voice "../data/audio/render/okia1.wav" XGM2 6650
WAV sfx_roar "../data/audio/render/oki8a.wav" XGM2 6650
WAV sfx_sorcerer "../data/audio/render/oki8c.wav" XGM2 6650
WAV sfx_fire "../data/audio/render/oki8d.wav" XGM2 6650
WAV sfx_boss_dead "../data/audio/render/oki8e.wav" XGM2 6650
// Stage 3: 0x04 the island, 0x05 the lake (both posted with 0x01FA), 0x06
// the boss; the front end's 0x16 ranking and 0x18 name entry (all loop;
// tools/audio/run_s3.sh).
XGM2 music_stage3 "../data/audio/music04.vgm"
XGM2 music_lake "../data/audio/music05.vgm"
XGM2 music_boss3 "../data/audio/music06.vgm"
XGM2 music_rank "../data/audio/music16.vgm"
XGM2 music_name "../data/audio/music18.vgm"
// Stage 3 effects: 0x3E bomb burst (FM, rendered), OKI voices 0x8F thunder,
// 0x91 lance thrust, 0x92 knight hit, 0x93 / 0x94 the boss's summon, 0x95 its death.
WAV sfx_bomb "../data/audio/render/sfx3e_opm.wav" XGM2
WAV sfx_thrust "../data/audio/render/oki91.wav" XGM2 6650
WAV sfx_knight_hit "../data/audio/render/oki92.wav" XGM2 6650
WAV sfx_thunder "../data/audio/render/oki8f.wav" XGM2 6650
WAV sfx_summon "../data/audio/render/oki93.wav" XGM2 6650
WAV sfx_serpents "../data/audio/render/oki94.wav" XGM2 6650
WAV sfx_boss3_dead "../data/audio/render/oki95.wav" XGM2 6650
// The attract loop: 0x81 the WILLOW logo (OKI voice).
WAV sfx_logo "../data/audio/render/oki81.wav" XGM2 6650
// Stage 4: 0x07 the camp, 0x08 the ride, 0x09 the cave (tools/audio/run_s4.sh);
// effects 0x28 clash, 0x39 hoofbeats (FM, rendered), OKI 0x96-0x9A, 0x9E, 0x9F.
XGM2 music_stage4 "../data/audio/music07.vgm"
XGM2 music_ride "../data/audio/music08.vgm"
XGM2 music_cave "../data/audio/music09.vgm"
WAV sfx_clash "../data/audio/render/sfx28_opm.wav" XGM2
WAV sfx_hoof "../data/audio/render/sfx39_opm.wav" XGM2
WAV sfx_slash "../data/audio/render/oki96.wav" XGM2 6650
WAV sfx_thrust4 "../data/audio/render/oki97.wav" XGM2 6650
WAV sfx_sorsha "../data/audio/render/oki98.wav" XGM2 6650
WAV sfx_breath "../data/audio/render/oki99.wav" XGM2 6650
WAV sfx_dragon_dead "../data/audio/render/oki9a.wav" XGM2 6650
WAV sfx_crash4 "../data/audio/render/oki9e.wav" XGM2 6650
WAV sfx_shield_jump "../data/audio/render/oki9f.wav" XGM2 6650
# Stage 5 (specs/stage5.md 9; tools/audio/run_s5.sh).
XGM2 music_stage5 "../data/audio/music0a.vgm"
XGM2 music_boss5 "../data/audio/music0b.vgm"
# Stage 6 and the ending (specs/stage6.md 9; tools/audio/run_s6.sh).
# (Their music, 0x0C-0x0F and 0x1D, is in the stage-6 banks: src/s6data.s.)
WAV sfx_kael_slash "../data/audio/render/okia2.wav" XGM2 6650
WAV sfx_bav_cast "../data/audio/render/okia6.wav" XGM2 6650
WAV sfx_bav_ball "../data/audio/render/okia9.wav" XGM2 6650
WAV sfx_chime86 "../data/audio/render/oki86.wav" XGM2 6650
WAV sfx_chime87 "../data/audio/render/oki87.wav" XGM2 6650
WAV sfx_chime88 "../data/audio/render/oki88.wav" XGM2 6650
WAV sfx_chime89 "../data/audio/render/oki89.wav" XGM2 6650
WAV sfx_troll_hit "../data/audio/render/oki9b.wav" XGM2 6650
WAV sfx_troll_dead "../data/audio/render/oki9c.wav" XGM2 6650
WAV sfx_orb "../data/audio/render/oki9d.wav" XGM2 6650
WAV sfx_pillar "../data/audio/render/okia8.wav" XGM2 6650
