SGDK_ROOT ?=
# Development commands must never create a window or take desktop focus.
# In particular, MAME's SDL initialization must stay headless even before its
# command-line video settings take effect. GUI launches are separate and explicit.
override SDL_VIDEODRIVER := dummy
override SDL_AUDIODRIVER := dummy
export SDL_VIDEODRIVER SDL_AUDIODRIVER
PYTHON ?= python3
SORSHA_PYTHON ?= $(if $(wildcard .tools/sorsha-python/bin/python),.tools/sorsha-python/bin/python,$(PYTHON))
XGM_PCM_RATE ?= 14000
ASSET_BROWSER ?= 0
JAVA ?= $(if $(JAVA_HOME),$(JAVA_HOME)/bin/java,$(shell if test -x /opt/homebrew/opt/openjdk@17/bin/java; then echo /opt/homebrew/opt/openjdk@17/bin/java; else command -v java 2>/dev/null || echo java; fi))

ifeq ($(ASSET_BROWSER),1)
ASSET_BROWSER_FLAG := -DWILLOW_ENABLE_ASSET_BROWSER
endif

.PHONY: all assets audio-assets audio-catalog stage-assets reference-maincpu animation-catalog collision-catalog scroll1-map-catalog layer-map-catalog stage-catalog object-catalog hud-catalog hud-font-assets story-catalog release clean

all: release

.PHONY: sorsha-assets
sorsha-assets: build/sorsha_assets.h build/sorsha_sprites.bin build/sorsha_backgrounds.bin

build/sorsha_assets.h: assets/sorsha/sheet.png assets/sorsha/layout.json tools/sorsha_assets.py tools/native_palette_fit.py tools/cps1_gfx.py tools/cps1_palette.py tools/cps1_stage_catalog.py tools/cps1_gameplay_maps.py tools/cps1_program.py willow.zip
	$(SORSHA_PYTHON) tools/sorsha_assets.py

release: sorsha-assets
assets: sorsha-assets

.PHONY: companion-assets
companion-assets: build/companion_assets.h build/companion_sprites.bin
build/companion_assets.h: assets/companion/sheet-v2.png tools/companion_assets.py tools/cps1_gfx.py tools/cps1_palette.py tools/cps1_program.py willow.zip
	$(SORSHA_PYTHON) tools/companion_assets.py
release assets: companion-assets

assets: build/willow_genesis_tiles.bin build/willow_scroll1_left.bin build/willow_scroll1_right.bin build/willow_scroll2_tiles.bin build/willow_scroll3_tiles.bin build/willow_preview_tiles.bin build/willow_palettes.h stage-assets audio-assets

audio-assets: build/oki_wav/index.json

audio-catalog: build/willow_audio_commands.json

build/willow_audio_commands.json: willow.zip tools/cps1_audio_catalog.py tools/cps1_program.py
	$(PYTHON) tools/cps1_audio_catalog.py --output build/willow_audio_commands.json --header build/willow_audio.h

build/willow_audio.h: build/willow_audio_commands.json
	@test -f $@ || $(PYTHON) tools/cps1_audio_catalog.py --output build/willow_audio_commands.json --header $@

build/oki_wav/index.json: willow.zip tools/oki_samples.py
	$(PYTHON) tools/oki_samples.py --output-rate $(XGM_PCM_RATE) --output build/oki_wav

build/willow_genesis_tiles.bin: willow.zip tools/cps1_gfx.py
	$(PYTHON) tools/cps1_gfx.py --count 32768 --output $@

build/willow_preview_tiles.bin: willow.zip tools/cps1_gfx.py
	$(PYTHON) tools/cps1_gfx.py --count 208 --output $@

# CPS Scroll 2 uses 16x16 decoded tiles; Scroll 3 uses 32x32 tiles. These
# native Genesis streams are retained offline; stage-assets packs the referenced
# codes uncompressed for the cartridge ROM.
build/willow_scroll1_left.bin: willow.zip tools/cps1_gfx.py
	$(PYTHON) tools/cps1_gfx.py --layer scroll1 --first 28672 --count 4096 --scroll1-layout 0 --output $@

build/willow_scroll1_right.bin: willow.zip tools/cps1_gfx.py
	$(PYTHON) tools/cps1_gfx.py --layer scroll1 --first 28672 --count 4096 --scroll1-layout 1 --output $@

build/willow_scroll2_tiles.bin: willow.zip tools/cps1_gfx.py
	$(PYTHON) tools/cps1_gfx.py --layer scroll2 --first 0 --count 8192 --output $@

build/willow_scroll3_tiles.bin: willow.zip tools/cps1_gfx.py
	$(PYTHON) tools/cps1_gfx.py --layer scroll3 --first 2560 --count 1024 --output $@

# Offline reverse-engineering aid only; no release dependency references this file.
reference-maincpu: build/reference_maincpu.bin

animation-catalog: build/willow_animations.json build/willow_walk.h

collision-catalog: build/willow_collision.json build/willow_collision.h

scroll1-map-catalog: build/willow_scroll1_maps.json

layer-map-catalog: build/willow_layer_maps.json

# Gameplay stage selector grids, extracted for native map streaming.
stage-catalog: build/willow_stage_maps.h build/willow_stage_maps.json

object-catalog: build/willow_objects.json build/willow_score.h

hud-catalog: build/willow_hud_candidates.json

hud-font-assets: build/willow_hud_font.json
	@test -f build/willow_hud_font_left.bin -a -f build/willow_hud_font_right.bin -a -f build/willow_hud_timer_digits.bin

story-catalog: build/willow_story.h

build/willow_story.h: willow.zip tools/cps1_story_catalog.py tools/cps1_program.py
	$(PYTHON) tools/cps1_story_catalog.py --output $@

build/willow_hud_font.json: build/willow_scroll1_left.bin build/willow_scroll1_right.bin tools/cps1_hud_font.py
	$(PYTHON) tools/cps1_hud_font.py --output-dir build
	@test -f build/willow_frontend_font.bin

build/willow_frontend_font.bin: build/willow_hud_font.json
	@test -f $@

build/willow_hud_candidates.json: willow.zip tools/cps1_hud_catalog.py tools/cps1_program.py
	$(PYTHON) tools/cps1_hud_catalog.py --output $@

stage-assets: build/willow_stage_assets.stamp

build/willow_stage_assets.stamp: willow.zip tools/cps1_stage_assets.py tools/cps1_stage_catalog.py tools/cps1_animation_catalog.py tools/cps1_gfx.py tools/cps1_program.py build/willow_animations.json build/willow_stage_maps.json
	$(PYTHON) tools/cps1_stage_assets.py --output-dir build --manifest build/willow_stage_assets.json
	touch $@

build/willow_stage_sprite_tiles.bin build/willow_stage_scroll2_tiles.bin build/willow_stage_scroll3_tiles.bin build/willow_stage_scroll2_maps.bin build/willow_stage_scroll3_maps.bin build/willow_stage_tile_index.h: build/willow_stage_assets.stamp
	@test -f $@

build/willow_scroll1_maps.json: willow.zip tools/cps1_scroll1_maps.py tools/cps1_program.py
	$(PYTHON) tools/cps1_scroll1_maps.py --output build/willow_scroll1_maps.json --header build/willow_frontend.h

build/willow_frontend.h: build/willow_scroll1_maps.json
	@test -f $@ || $(PYTHON) tools/cps1_scroll1_maps.py --output build/willow_scroll1_maps.json --header $@

build/willow_layer_maps.json: willow.zip tools/cps1_layer_maps.py tools/cps1_program.py
	$(PYTHON) tools/cps1_layer_maps.py --output $@

build/willow_score.h: willow.zip tools/cps1_object_catalog.py tools/cps1_program.py
	$(PYTHON) tools/cps1_object_catalog.py --output build/willow_objects.json --header $@

build/willow_objects.json: build/willow_score.h
	@test -f $@ || $(PYTHON) tools/cps1_object_catalog.py --output $@ --header build/willow_score.h

build/willow_stage_maps.h: willow.zip tools/cps1_stage_catalog.py tools/cps1_animation_catalog.py tools/cps1_program.py
	$(PYTHON) tools/cps1_stage_catalog.py --json build/willow_stage_maps.json --header $@

build/willow_stage_maps.json: build/willow_stage_maps.h
	@test -f $@

build/willow_animations.json: willow.zip tools/cps1_animation_catalog.py tools/cps1_program.py
	$(PYTHON) tools/cps1_animation_catalog.py --output $@ --header build/willow_walk.h

build/willow_walk.h: build/willow_animations.json
	@test -f $@ || $(PYTHON) tools/cps1_animation_catalog.py --output build/willow_animations.json --header $@

build/willow_collision.json: willow.zip tools/cps1_collision_catalog.py tools/cps1_program.py
	$(PYTHON) tools/cps1_collision_catalog.py --output $@ --header build/willow_collision.h

build/willow_collision.h: build/willow_collision.json
	@test -f $@ || $(PYTHON) tools/cps1_collision_catalog.py --output build/willow_collision.json --header $@

build/reference_maincpu.bin: willow.zip tools/cps1_program.py
	$(PYTHON) tools/cps1_program.py --output $@

build/willow_palettes.h: willow.zip tools/cps1_palette.py tools/cps1_program.py
	$(PYTHON) tools/cps1_palette.py --output $@

build/willow_native_bg.h: willow.zip tools/native_stage_background.py tools/cps1_gameplay_maps.py tools/native_palette_fit.py tools/cps1_palette.py
	$(PYTHON) tools/native_stage_background.py

build/willow_native_bg.bin: build/willow_native_bg.h
	@test -f $@ || $(PYTHON) tools/native_stage_background.py

release: build/willow_native_bg.h build/willow_native_bg.bin tools/enable_sgdk_mapper.py build/willow_stage_sprite_tiles.bin build/willow_stage_scroll2_tiles.bin build/willow_stage_scroll3_tiles.bin build/willow_stage_scroll2_maps.bin build/willow_stage_scroll3_maps.bin build/willow_stage_tile_index.h build/willow_stage_maps.h build/willow_collision.h build/willow_palettes.h build/willow_walk.h build/oki_wav/index.json build/willow_audio_commands.json build/willow_audio.h build/willow_hud_font.json build/willow_frontend_font.bin build/willow_story.h build/willow_score.h build/willow_frontend.h
	@test -n "$(SGDK_ROOT)" || (echo "Set SGDK_ROOT to an SGDK checkout"; exit 1)
	$(PYTHON) tools/enable_sgdk_mapper.py "$(SGDK_ROOT)"
	$(MAKE) -C "$(SGDK_ROOT)" JAVA="$(JAVA)" -f makelib.gen release
	# SGDK's generated project rule does not track config.h for the ROM header.
	rm -f out/rom_head.o out/rom_head.bin out/sega.o
	# The stock SGDK library contains GCC 13 LTO bytecode. Use the no-LTO
	# project path with a library rebuilt for the active m68k-elf-gcc.
	$(MAKE) -f "$(SGDK_ROOT)/makefile_nolto.gen" JAVA="$(JAVA)" EXTRA_FLAGS="$(EXTRA_FLAGS) $(ASSET_BROWSER_FLAG)" release
	mkdir -p out/candidate
	cp out/rom.bin out/candidate/rom.bin
	$(PYTHON) tools/finalize_genesis_rom.py out/candidate/rom.bin --max-bytes 5242880
	$(PYTHON) tools/native_rom_audit.py out/candidate/rom.bin --max-bytes 5242880 --symbols out/symbol.txt --audio-index build/oki_wav/index.json --audio-catalog build/willow_audio_commands.json
	mkdir -p out/release
	cp out/candidate/rom.bin out/release/rom.bin
	cp out/release/rom.bin out/rom.bin

out/rom_head.o: $(SGDK_ROOT)/inc/config.h

clean:
	@test -n "$(SGDK_ROOT)" || (echo "Set SGDK_ROOT to an SGDK checkout"; exit 1)
	$(MAKE) -f "$(SGDK_ROOT)/makefile.gen" clean-all
	rm -rf build

build/native_magic_assets.h: tools/native_magic_assets.py tools/cps1_animation_catalog.py tools/cps1_palette.py willow.zip
	$(PYTHON) tools/native_magic_assets.py

release: build/native_magic_assets.h

build/sorsha_sprites.bin build/sorsha_backgrounds.bin: build/sorsha_assets.h
	@test -f $@ || $(SORSHA_PYTHON) tools/sorsha_assets.py
build/companion_sprites.bin: build/companion_assets.h
	@test -f $@ || $(SORSHA_PYTHON) tools/companion_assets.py

.PHONY: test-native
test-native: build/native_magic_assets.h build/willow_native_bg.h build/willow_collision.h
	$(PYTHON) tools/test_native.py

.PHONY: intro-probe test-intro-probe
native/intro_probe/src/overlay.h: tools/native_intro_overlay.py tools/cps1_text_task.py tools/cps1_gfx.py tools/cps1_palette.py tools/native_palette_fit.py native/intro_probe/src/actor_sprites.h willow.zip
	$(SORSHA_PYTHON) tools/native_intro_overlay.py
native/intro_probe/res/overlay.bin native/intro_probe/res/overlay_player.bin: native/intro_probe/src/overlay.h
	@test -f $@ || $(SORSHA_PYTHON) tools/native_intro_overlay.py
intro-probe: native-player-sprites native-player-palette-assets native-hud-initial native/intro_probe/src/overlay.h native/intro_probe/res/overlay.bin native/intro_probe/res/overlay_player.bin
native/intro_probe/src/layers.h: tools/native_intro_layers.py tools/cps1_source_layers.py tools/native_palette_fit.py tools/cps1_palette.py willow.zip
	$(SORSHA_PYTHON) tools/native_intro_layers.py
native/intro_probe/res/layers.bin: native/intro_probe/src/layers.h
	@test -f $@ || $(SORSHA_PYTHON) tools/native_intro_layers.py
build/arcade_intro_text.h: tools/native_intro_text.py tools/cps1_text_task.py willow.zip
	$(PYTHON) tools/native_intro_text.py
native/intro_probe/src/font.h: tools/native_intro_font.py tools/cps1_gfx.py tools/cps1_palette.py tools/native_palette_fit.py willow.zip
	$(SORSHA_PYTHON) tools/native_intro_font.py
native/intro_probe/res/font.bin: native/intro_probe/src/font.h
	@test -f $@ || $(SORSHA_PYTHON) tools/native_intro_font.py
native/intro_probe/src/actor_data.h: tools/native_intro_actor_data.py willow.zip
	$(PYTHON) tools/native_intro_actor_data.py
native/intro_probe/src/entry_data.h: tools/native_stage_entry_data.py willow.zip
	$(PYTHON) tools/native_stage_entry_data.py
intro-probe: native/intro_probe/src/scroll3_data.h native/intro_probe/src/entry_data.h native/intro_probe/src/camera_data.h native/intro_probe/src/scroll2_data.h native/intro_probe/src/player_data.h
native/intro_probe/src/actor_sprites.h: native/intro_probe/src/font.h tools/native_intro_sprites.py tools/cps1_intro_actor_render.py tools/cps1_palette.py tools/native_palette_fit.py willow.zip
	$(SORSHA_PYTHON) tools/native_intro_sprites.py
native/intro_probe/res/actors.bin: native/intro_probe/src/actor_sprites.h
	@test -f $@ || $(SORSHA_PYTHON) tools/native_intro_sprites.py
native/intro_probe/src/forest.h: tools/native_intro_forest.py tools/cps1_source_layers.py tools/cps1_palette.py tools/cps1_gfx.py tools/native_palette_fit.py native/intro_probe/src/layers.h native/intro_probe/res/layers.bin willow.zip
	$(SORSHA_PYTHON) tools/native_intro_forest.py
native/intro_probe/res/forest.bin: native/intro_probe/src/forest.h
	@test -f $@ || $(SORSHA_PYTHON) tools/native_intro_forest.py
intro-probe: native/intro_probe/src/forest.h native/intro_probe/res/forest.bin native/intro_probe/src/actor_sprites.h native/intro_probe/res/actors.bin native/intro_probe/src/actor_data.h native/intro_probe/src/font.h native/intro_probe/res/font.bin native/intro_probe/src/layers.h native/intro_probe/res/layers.bin build/arcade_intro_text.h
	@test -n "$(SGDK_ROOT)" || (echo "Set SGDK_ROOT to an SGDK checkout"; exit 1)
	$(MAKE) -C native/intro_probe -f "$(SGDK_ROOT)/makefile_nolto.gen" JAVA="$(JAVA)" release
test-intro-probe: intro-probe build/arcade-trace/camera-native/frames.bin build/arcade-trace/scroll3-walk/frames.bin build/arcade-trace/scroll3-walk/maps.bin
	$(SORSHA_PYTHON) tools/verify_intro_overlay_player.py
	$(PYTHON) tools/verify_arcade_intro_sequence.py
	$(PYTHON) tools/verify_intro_overlay_commands.py
	$(PYTHON) tools/verify_arcade_intro.py
	$(PYTHON) tools/verify_arcade_intro_player.py
	$(PYTHON) tools/verify_arcade_text.py
	$(SORSHA_PYTHON) tools/verify_intro_glyphs.py
	$(SORSHA_PYTHON) tools/verify_source_layers.py
	clang -O2 tools/intro_rom_smoke.c -o build/intro_rom_smoke
	build/intro_rom_smoke "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" native/intro_probe/out/rom.bin native/intro_probe/out/symbol.txt
	$(SORSHA_PYTHON) tools/verify_native_intro_pixels.py
	$(SORSHA_PYTHON) tools/verify_native_player_gate_pixels.py
	$(SORSHA_PYTHON) tools/verify_native_hud_pixels.py

.PHONY: test-intro-actors
test-intro-actors:
	$(PYTHON) tools/verify_arcade_intro_actor.py
	$(PYTHON) tools/cps1_intro_actor_render.py
	$(SORSHA_PYTHON) tools/verify_intro_actor_pixels.py

.PHONY: test-intro-dialogue
test-intro-dialogue:
	$(PYTHON) tools/verify_arcade_dialogue.py

.PHONY: test-intro-sequence
test-intro-sequence: native/intro_probe/src/actor_data.h
	$(PYTHON) tools/verify_arcade_intro_sequence.py

.PHONY: test-stage-entry
test-stage-entry: build/arcade_intro_text.h
	$(PYTHON) tools/verify_arcade_stage_objects.py
	clang -Wall -Wextra -Werror tools/test_arcade_stage_spawns.c -o build/test_arcade_stage_spawns
	build/test_arcade_stage_spawns
	clang -Wall -Wextra -Werror tools/test_arcade_actor_pool.c -o build/test_arcade_actor_pool
	build/test_arcade_actor_pool
	clang -Wall -Wextra -Werror tools/test_arcade_player_handoff.c -o build/test_arcade_player_handoff
	build/test_arcade_player_handoff

.PHONY: intro-scroll1-assets test-intro-scroll1-assets
intro-scroll1-assets:
	$(SORSHA_PYTHON) tools/native_intro_scroll1.py
test-intro-scroll1-assets: intro-scroll1-assets
	$(SORSHA_PYTHON) tools/verify_intro_scroll1_assets.py

.PHONY: test-player-motion
test-player-motion:
	clang -Wall -Wextra -Werror -O2 tools/test_arcade_player_motion.c -o build/test_arcade_player_motion
	build/test_arcade_player_motion "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-player-collision
test-player-collision:
	$(PYTHON) tools/verify_stage0_collision_map.py
	clang -Wall -Wextra -Werror -O2 tools/test_arcade_player_collision.c -o build/test_arcade_player_collision
	build/test_arcade_player_collision "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-player-ground
test-player-ground:
	$(PYTHON) tools/verify_stage0_collision_map.py
	clang -Wall -Wextra -Werror -O2 tools/test_arcade_player_ground.c -o build/test_arcade_player_ground
	build/test_arcade_player_ground "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-player-drop
test-player-drop:
	$(PYTHON) tools/verify_stage0_collision_map.py
	clang -Wall -Wextra -Werror -O2 tools/test_arcade_player_drop.c -o build/test_arcade_player_drop
	build/test_arcade_player_drop "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-player-jump
test-player-jump:
	$(PYTHON) tools/verify_stage0_collision_map.py
	clang -Wall -Wextra -Werror -O2 tools/test_arcade_player_jump.c -o build/test_arcade_player_jump
	build/test_arcade_player_jump "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"
	clang -Wall -Wextra -Werror -O2 tools/test_arcade_player_climb.c -o build/test_arcade_player_climb
	build/test_arcade_player_climb "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-player-attack
test-player-attack:
	$(PYTHON) tools/verify_stage0_collision_map.py
	clang -Wall -Wextra -Werror -O2 tools/test_arcade_player_attack.c -o build/test_arcade_player_attack
	build/test_arcade_player_attack "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-player-movement
test-player-movement:
	$(PYTHON) tools/verify_stage0_collision_map.py
	clang -Wall -Wextra -Werror -O2 tools/test_arcade_player_slide.c -o build/test_arcade_player_slide
	build/test_arcade_player_slide "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"
	clang -Wall -Wextra -Werror -O2 tools/test_arcade_player_movement.c -o build/test_arcade_player_movement
	build/test_arcade_player_movement "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-player-update
.PHONY: test-arcade-services
.PHONY: test-arcade-scroll2
.PHONY: test-arcade-scroll2-motion
.PHONY: test-arcade-scroll2-active
.PHONY: test-arcade-scroll2-start
.PHONY: test-arcade-palette-animation
.PHONY: test-arcade-map-store
native/intro_probe/src/scroll2_data.h: tools/native_scroll2_data.py willow.zip
	$(PYTHON) tools/native_scroll2_data.py
test-arcade-map-store: native/intro_probe/src/scroll2_data.h native/intro_probe/src/camera_data.h build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 tools/test_arcade_map_store.c -o build/test_arcade_map_store
	build/test_arcade_map_store

.PHONY: test-arcade-map-controller
native/intro_probe/src/camera_data.h: tools/native_camera_data.py willow.zip
	$(PYTHON) tools/native_camera_data.py
test-arcade-map-controller: native/intro_probe/src/scroll2_data.h native/intro_probe/src/camera_data.h build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -DWILLOW_TEST_PACKED_MAP tools/test_arcade_scroll2_start.c -o build/test_packed_scroll2_start
	build/test_packed_scroll2_start "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"
	clang -Wall -Wextra -Werror -O2 -DWILLOW_TEST_PACKED_MAP tools/test_arcade_scroll2_active.c -o build/test_packed_scroll2_active
	build/test_packed_scroll2_active "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-arcade-palette-animation: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 tools/test_arcade_palette_animation.c -o build/test_arcade_palette_animation
	build/test_arcade_palette_animation "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-arcade-scroll2-start: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 tools/test_arcade_scroll2_start.c -o build/test_arcade_scroll2_start
	build/test_arcade_scroll2_start "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-arcade-scroll2-active: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 tools/test_arcade_scroll2_active.c -o build/test_arcade_scroll2_active
	build/test_arcade_scroll2_active "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-arcade-scroll2-motion: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 tools/test_arcade_scroll2_motion.c -o build/test_arcade_scroll2_motion
	build/test_arcade_scroll2_motion "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-arcade-scroll2: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 tools/test_arcade_scroll2.c -o build/test_arcade_scroll2
	build/test_arcade_scroll2 "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-arcade-services: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 tools/test_arcade_services.c -o build/test_arcade_services
	build/test_arcade_services "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

native/intro_probe/src/player_data.h: tools/native_player_data.py willow.zip
	$(PYTHON) tools/native_player_data.py

native/intro_probe/src/input_data.h: native/intro_probe/src/player_data.h
	@test -f $@ || $(PYTHON) tools/native_player_data.py

test-player-update: native/intro_probe/src/player_data.h
	$(PYTHON) tools/verify_stage0_collision_map.py
	clang -Wall -Wextra -Werror -O2 tools/test_arcade_player_health.c -o build/test_arcade_player_health
	build/test_arcade_player_health "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"
	clang -Wall -Wextra -Werror -O2 tools/test_arcade_player_update.c -o build/test_arcade_player_update
	build/test_arcade_player_update "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-palette-initialization
native/intro_probe/src/palette_initial_data.h: tools/native_palette_initial_data.py willow.zip
	$(PYTHON) tools/native_palette_initial_data.py
test-arcade-palette-initialization: native/intro_probe/src/palette_initial_data.h build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 tools/test_arcade_palette_initialization.c -o build/test_arcade_palette_initialization
	build/test_arcade_palette_initialization "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-intro-camera
test-intro-camera: native/intro_probe/src/player_data.h native/intro_probe/src/camera_data.h native/intro_probe/src/scroll2_data.h native/intro_probe/src/actor_data.h build/arcade-trace/camera-native/frames.bin
	clang -Wall -Wextra -Werror -O2 tools/test_arcade_intro_camera.c -o build/test_arcade_intro_camera
	build/test_arcade_intro_camera

build/arcade-trace/camera-native/frames.bin: tools/arcade_camera_trace.lua willow.zip
	mkdir -p build/arcade-trace/camera-native
	@if [ "$$(uname -s)" = Darwin ]; then echo "MAME capture disabled on macOS to prevent window/focus changes. Use existing reference traces and headless libretro checks." >&2; exit 1; fi
	SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy mame willowu -rompath . -noreadconfig -window -video none -sound none -nothrottle -skip_gameinfo -autoboot_delay 0 -autoboot_script tools/arcade_camera_trace.lua -cfg_directory build/arcade-trace/camera-native/cfg -nvram_directory build/arcade-trace/camera-native/nvram -seconds_to_run 40 > build/camera-trace-capture.log 2>&1

.PHONY: test-arcade-palette-fade
test-arcade-palette-fade: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 tools/test_arcade_palette_fade.c -o build/test_arcade_palette_fade
	build/test_arcade_palette_fade "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"
	build/test_arcade_palette_fade "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" --grouped

.PHONY: test-native-tile-slots
test-native-tile-slots:
	clang -Wall -Wextra -Werror -O2 tools/test_native_tile_slots.c -o build/test_native_tile_slots
	build/test_native_tile_slots

.PHONY: test-arcade-pool-services
test-arcade-pool-services: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 tools/test_arcade_pool_services.c -o build/test_arcade_pool_services
	build/test_arcade_pool_services "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-player-charge
test-player-charge: build/reference_maincpu.bin native/intro_probe/src/player_data.h
	clang -Wall -Wextra -Werror -O2 tools/test_arcade_player_charge.c -o build/test_arcade_player_charge
	build/test_arcade_player_charge "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-player-dispatch
test-player-dispatch: build/reference_maincpu.bin native/intro_probe/src/player_data.h
	$(PYTHON) tools/verify_stage0_collision_map.py
	clang -Wall -Wextra -Werror -O2 -DWILLOW_TEST_DISPATCH tools/test_arcade_player_update.c -o build/test_arcade_player_dispatch
	build/test_arcade_player_dispatch "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-player-hud
test-player-hud: build/reference_maincpu.bin native/intro_probe/src/player_data.h
	clang -Wall -Wextra -Werror -O2 tools/test_arcade_player_hud.c -o build/test_arcade_player_hud
	build/test_arcade_player_hud "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-player-clock
test-player-clock: build/reference_maincpu.bin native/intro_probe/src/player_data.h build/arcade-trace/camera-native/frames.bin
	clang -Wall -Wextra -Werror -O2 tools/test_arcade_player_clock.c -o build/test_arcade_player_clock
	build/test_arcade_player_clock "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"
	clang -Wall -Wextra -Werror -O2 tools/test_arcade_hud_clock_trace.c -o build/test_arcade_hud_clock_trace
	build/test_arcade_hud_clock_trace

.PHONY: native-hud-assets test-native-hud-assets
native-hud-assets: native/intro_probe/src/player_data.h native/intro_probe/src/actor_sprites.h native/intro_probe/src/overlay.h
	clang -Wall -Wextra -Werror -O2 tools/export_arcade_hud_tiles.c -o build/export_arcade_hud_tiles
	build/export_arcade_hud_tiles > build/arcade-hud-tile-keys.csv
	$(SORSHA_PYTHON) tools/native_hud_assets.py
test-native-hud-assets: native-hud-assets
	$(SORSHA_PYTHON) tools/verify_native_hud_assets.py
	$(SORSHA_PYTHON) tools/verify_intro_overlay_player.py --hud

.PHONY: test-native-hud-surface test-native-hud-target
test-native-hud-surface: native-hud-assets
	clang -Wall -Wextra -Werror -O2 tools/test_native_hud_surface.c -o build/test_native_hud_surface
	build/test_native_hud_surface
	$(SORSHA_PYTHON) tools/verify_native_hud_surface.py
test-native-hud-target: test-native-hud-surface
	@test -n "$(SGDK_ROOT)" || (echo "Set SGDK_ROOT to an SGDK checkout"; exit 1)
	m68k-elf-gcc -m68000 -O2 -Wall -Wextra -Werror -DSGDK_GCC -I"$(SGDK_ROOT)/inc" -nostdlib -Wl,-Ttext=0x100010,-e,hudProbe tools/hud_surface_probe.c -o build/hud_surface_probe.elf
	m68k-elf-objcopy -O binary build/hud_surface_probe.elf build/hud_surface_probe.bin
	m68k-elf-nm -n build/hud_surface_probe.elf > build/hud_surface_probe.sym
	clang -Wall -Wextra -Werror -O2 tools/test_native_hud_target.c -o build/test_native_hud_target
	build/test_native_hud_target "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: native-hud-initial test-native-hud-initial
native-hud-initial: native-hud-assets
	clang -Wall -Wextra -Werror -O2 tools/export_native_hud_initial.c -o build/export_native_hud_initial
	build/export_native_hud_initial
	$(PYTHON) tools/native_hud_initial.py
test-native-hud-initial: native-hud-initial test-native-hud-surface
	$(PYTHON) tools/verify_native_hud_initial.py

.PHONY: native-player-palette-assets
native-player-palette-assets: native-hud-assets
	$(SORSHA_PYTHON) tools/native_player_palette_assets.py

.PHONY: test-native-player-palette
test-native-player-palette: test-intro-probe build/arcade-trace/player-colors/frames.bin
	$(SORSHA_PYTHON) tools/verify_native_player_effects.py
	$(PYTHON) tools/verify_arcade_player_colors.py
	build/intro_rom_smoke "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" native/intro_probe/out/rom.bin native/intro_probe/out/symbol.txt --palette-probe
	$(SORSHA_PYTHON) tools/verify_native_player_gate_pixels.py --palette-probe
	$(SORSHA_PYTHON) tools/verify_native_hud_pixels.py --palette-probe
	build/intro_rom_smoke "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" native/intro_probe/out/rom.bin native/intro_probe/out/symbol.txt --damage-probe
	$(SORSHA_PYTHON) tools/verify_native_player_gate_pixels.py --damage-probe
	$(SORSHA_PYTHON) tools/verify_native_hud_pixels.py --damage-probe

build/arcade-trace/player-colors/frames.bin: tools/arcade_player_color_trace.lua willow.zip
	mkdir -p build/arcade-trace/player-colors/cfg build/arcade-trace/player-colors/nvram
	@if [ "$$(uname -s)" = Darwin ]; then echo "MAME capture disabled on macOS to prevent window/focus changes. Use existing reference traces and headless libretro checks." >&2; exit 1; fi
	SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy mame willowu -rompath . -noreadconfig -window -video none -sound none -nothrottle -skip_gameinfo -autoboot_delay 0 -autoboot_script tools/arcade_player_color_trace.lua -cfg_directory build/arcade-trace/player-colors/cfg -nvram_directory build/arcade-trace/player-colors/nvram -seconds_to_run 45 > build/player-color-trace-capture.log 2>&1

.PHONY: native-player-sprites
native-player-sprites: native-hud-assets
	$(SORSHA_PYTHON) tools/native_player_sprites.py

.PHONY: test-native-player-sprites
test-native-player-sprites: test-native-player-palette
	$(SORSHA_PYTHON) tools/verify_native_player_sprite_assets.py
	clang -O2 tools/test_arcade_player_sprite_geometry.c -o build/test_arcade_player_sprite_geometry
	build/test_arcade_player_sprite_geometry "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"
	build/intro_rom_smoke "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" native/intro_probe/out/rom.bin native/intro_probe/out/symbol.txt --pose-probe
	$(SORSHA_PYTHON) tools/verify_native_player_poses.py

.PHONY: test-player-input
test-player-input: build/reference_maincpu.bin native/intro_probe/src/player_data.h
	clang -O2 tools/test_arcade_player_input.c -o build/test_arcade_player_input
	build/test_arcade_player_input "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

build/arcade-trace/player-walk/frames.bin: tools/arcade_player_walk_trace.lua willow.zip
	mkdir -p build/arcade-trace/player-walk/cfg build/arcade-trace/player-walk/nvram
	@if [ "$$(uname -s)" = Darwin ]; then echo "MAME capture disabled on macOS to prevent window/focus changes. Use existing reference traces and headless libretro checks." >&2; exit 1; fi
	SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy mame willowu -rompath . -noreadconfig -window -video none -sound none -nothrottle -skip_gameinfo -autoboot_delay 0 -autoboot_script tools/arcade_player_walk_trace.lua -cfg_directory build/arcade-trace/player-walk/cfg -nvram_directory build/arcade-trace/player-walk/nvram -seconds_to_run 45 > build/player-walk-capture.log 2>&1

.PHONY: test-native-player-input
test-native-player-input: test-native-player-sprites test-player-input build/arcade-trace/player-walk/frames.bin
	$(PYTHON) tools/verify_arcade_player_walk.py
	build/intro_rom_smoke "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" native/intro_probe/out/rom.bin native/intro_probe/out/symbol.txt --walk-probe
	$(SORSHA_PYTHON) tools/verify_native_walk_pixels.py

.PHONY: test-arcade-scroll3-motion
test-arcade-scroll3-motion: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 tools/test_arcade_scroll3_motion.c -o build/test_arcade_scroll3_motion
	build/test_arcade_scroll3_motion "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

build/arcade-trace/scroll3-walk/frames.bin: tools/arcade_scroll3_walk_trace.lua willow.zip
	mkdir -p build/arcade-trace/scroll3-walk
	@if [ "$$(uname -s)" = Darwin ]; then echo "MAME capture disabled on macOS to prevent window/focus changes. Use existing reference traces and headless libretro checks." >&2; exit 1; fi
	SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy mame willowu -rompath . -noreadconfig -window -video none -sound none -nothrottle -skip_gameinfo -autoboot_delay 0 -autoboot_script tools/arcade_scroll3_walk_trace.lua -cfg_directory build/arcade-trace/scroll3-walk/cfg -nvram_directory build/arcade-trace/scroll3-walk/nvram -seconds_to_run 45 > build/scroll3-walk-capture.log 2>&1

.PHONY: test-arcade-scroll3-trace
test-arcade-scroll3-trace: test-arcade-scroll3-motion test-arcade-scroll3 build/arcade-trace/scroll3-walk/frames.bin build/arcade-trace/scroll3-walk/maps.bin build/arcade-trace/player-walk/frames.bin
	clang -Wall -Wextra -Werror -O2 tools/test_arcade_scroll3_trace.c -o build/test_arcade_scroll3_trace
	$(PYTHON) tools/verify_arcade_scroll3_trace.py
	build/test_arcade_scroll3_trace

.PHONY: test-arcade-scroll3
test-arcade-scroll3: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 tools/test_arcade_scroll3.c -o build/test_arcade_scroll3
	build/test_arcade_scroll3 "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

build/arcade-trace/scroll3-walk/maps.bin: build/arcade-trace/scroll3-walk/frames.bin
	@test -f "$@" || $(MAKE) -W tools/arcade_scroll3_walk_trace.lua build/arcade-trace/scroll3-walk/frames.bin

native/intro_probe/src/scroll3_data.h: tools/native_scroll3_data.py willow.zip
	$(PYTHON) tools/native_scroll3_data.py

.PHONY: test-arcade-scroll3-map-store
.PHONY: test-arcade-demo-manager-task
.PHONY: test-arcade-demo-auxiliary-tasks
test-arcade-demo-auxiliary-tasks: build/reference_maincpu.bin
	$(PYTHON) tools/test_arcade_demo_auxiliary.py

.PHONY: test-arcade-demo-post-sequence
test-arcade-demo-post-sequence: build/reference_maincpu.bin
	$(PYTHON) tools/test_arcade_demo_post_sequence.py

.PHONY: test-arcade-attract-interlude-setup test-arcade-interlude-setup-sequence
.PHONY: test-arcade-attract-interlude-wait
test-arcade-attract-interlude-wait: build/reference_maincpu.bin
	mkdir -p build/interlude-setup-investigation
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_attract_interlude_wait.c -o build/interlude-setup-investigation/wait-test
	build/interlude-setup-investigation/wait-test "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-arcade-attract-interlude-setup: test-arcade-interlude-clear-sequence native/intro_probe/src/opening_message_data.h
	mkdir -p build/interlude-setup-investigation
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_attract_interlude_setup.c -o build/interlude-setup-investigation/setup-test
	build/interlude-setup-investigation/setup-test "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_INTERLUDE_TASK tools/test_arcade_attract_interlude_setup.c -o build/interlude-setup-investigation/manager-test
	build/interlude-setup-investigation/manager-test "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_OPENING_MESSAGE_DATA -DWILLOW_INTERLUDE_SETUP_PROBE tools/test_native_message_task.c -o build/interlude-setup-investigation/message-test
	build/interlude-setup-investigation/message-test "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-arcade-interlude-setup-sequence: test-arcade-interlude-clear-sequence test-arcade-attract-interlude-setup
	mkdir -p build/interlude-setup-investigation
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_DEMO_FIRST_FRAME -DWILLOW_TEST_INTERLUDE_SETUP_SEQUENCE -DWILLOW_TEST_DEMO_UPDATE_SEQUENCE=1300 '-DDEMO_OUTPUT="build/interlude-setup-investigation/sequence"' tools/test_arcade_demo_first_actors.c -o build/interlude-setup-investigation/reference-test
	WILLOW_DEMO_CONTINUATION_INPUT=build/demo-auxiliary-investigation/checkpoint build/interlude-setup-investigation/reference-test "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-attract-interlude-reset
.PHONY: test-arcade-attract-interlude-clear
.PHONY: test-arcade-interlude-clear-sequence
test-arcade-interlude-clear-sequence: test-arcade-attract-interlude-reset test-arcade-attract-interlude-clear
	mkdir -p build/interlude-clear-investigation
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_DEMO_FIRST_FRAME -DWILLOW_TEST_INTERLUDE_CLEAR_SEQUENCE -DWILLOW_TEST_DEMO_UPDATE_SEQUENCE=1300 '-DDEMO_OUTPUT="build/interlude-clear-investigation/sequence"' tools/test_arcade_demo_first_actors.c -o build/interlude-clear-investigation/reference-test
	WILLOW_DEMO_CONTINUATION_INPUT=build/demo-auxiliary-investigation/checkpoint build/interlude-clear-investigation/reference-test "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-arcade-attract-interlude-clear: build/reference_maincpu.bin
	mkdir -p build/interlude-reset-investigation
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_attract_interlude_clear.c -o build/interlude-reset-investigation/clear-test
	build/interlude-reset-investigation/clear-test "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-arcade-attract-interlude-reset: test-arcade-demo-post-sequence
	mkdir -p build/interlude-reset-investigation
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_attract_interlude_reset.c -o build/interlude-reset-investigation/reference-test
	build/interlude-reset-investigation/reference-test "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_INTERLUDE_TASK tools/test_arcade_attract_interlude_reset.c -o build/interlude-reset-investigation/manager-test
	build/interlude-reset-investigation/manager-test "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_DEMO_FIRST_FRAME -DWILLOW_TEST_INTERLUDE_RESET_SEQUENCE -DWILLOW_TEST_DEMO_UPDATE_SEQUENCE=1200 '-DDEMO_OUTPUT="build/interlude-reset-investigation/sequence"' tools/test_arcade_demo_first_actors.c -o build/interlude-reset-investigation/sequence-test
	WILLOW_DEMO_CONTINUATION_INPUT=build/demo-auxiliary-investigation/checkpoint build/interlude-reset-investigation/sequence-test "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-arcade-demo-manager-task: build/reference_maincpu.bin
	@test -s build/opening-demo-start-0-state.bin && test -s build/opening-title-context-scroll1.bin
	mkdir -p build/demo-task-routing-investigation
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -DWILLOW_TEST_DEMO_FIRST_FRAME -DWILLOW_TEST_DEMO_MANAGER_TASK -DWILLOW_TEST_DEMO_UPDATE_SEQUENCE=900 '-DDEMO_OUTPUT="build/demo-task-routing-investigation/sequence"' tools/test_arcade_demo_first_actors.c -o build/test_arcade_demo_manager_task
	build/test_arcade_demo_manager_task "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-demo-live-registers
test-arcade-demo-live-registers: build/reference_maincpu.bin
	@test -s build/opening-demo-start-0-state.bin && test -s build/opening-title-context-scroll1.bin
	mkdir -p build/demo-live-registers-investigation
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined tools/test_arcade_player_sprite_geometry.c -o build/test_arcade_sprite_live_registers
	build/test_arcade_sprite_live_registers "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -DWILLOW_TEST_DEMO_FIRST_FRAME -DWILLOW_TEST_DEMO_LIVE_REGISTERS -DWILLOW_TEST_DEMO_UPDATE_SEQUENCE=900 '-DDEMO_OUTPUT="build/demo-live-registers-investigation/sequence"' tools/test_arcade_demo_first_actors.c -o build/test_arcade_demo_live_registers
	build/test_arcade_demo_live_registers "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-native-scroll3-packed-source test-arcade-attract-demo-finish
test-arcade-attract-demo-finish: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined tools/test_arcade_attract_demo_finish.c -o build/test_arcade_attract_demo_finish
	build/test_arcade_attract_demo_finish "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -DWILLOW_TEST_NATIVE_DEMO_FINISH_TASK tools/test_arcade_attract_demo_finish.c -o build/test_native_attract_demo_finish_task
	build/test_native_attract_demo_finish_task "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined tools/test_arcade_attract_demo_finish_sequence.c -o build/test_arcade_attract_demo_finish_sequence
	build/test_arcade_attract_demo_finish_sequence "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -DWILLOW_TEST_NATIVE_DEMO_FINISH_SCHEDULER tools/test_arcade_attract_demo_finish_sequence.c -o build/test_native_attract_demo_finish_scheduler
	build/test_native_attract_demo_finish_scheduler "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

# Existing trace files only: this target never starts a reference emulator.
test-native-scroll3-packed-source: native/intro_probe/src/scroll3_data.h build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined tools/test_arcade_scroll3_map_store.c -o build/test_native_scroll3_packed_source
	build/test_native_scroll3_packed_source
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -DWILLOW_TEST_PACKED_MAP -DWILLOW_PACKED_SCROLL3_SOURCE_PROBE tools/test_arcade_scroll3_trace.c -o build/test_native_scroll3_packed_source_trace
	build/test_native_scroll3_packed_source_trace

test-arcade-scroll3-map-store: native/intro_probe/src/scroll3_data.h build/reference_maincpu.bin build/arcade-trace/scroll3-walk/maps.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined tools/test_arcade_scroll3_map_store.c -o build/test_arcade_scroll3_map_store
	build/test_arcade_scroll3_map_store
	clang -Wall -Wextra -Werror -O2 -DWILLOW_TEST_PACKED_MAP tools/test_arcade_scroll3_trace.c -o build/test_arcade_scroll3_packed_trace
	build/test_arcade_scroll3_packed_trace

.PHONY: native-hud-sprite-plan
native-hud-sprite-plan: native-hud-initial
	$(PYTHON) tools/native_hud_sprite_assets.py

native/intro_probe/src/hud_sprite_plan.h: native/intro_probe/src/hud_initial.h tools/native_hud_sprite_assets.py
	$(PYTHON) tools/native_hud_sprite_assets.py
native/intro_probe/res/hud_sprite_plan.bin: native/intro_probe/src/hud_sprite_plan.h
	@test -f $@ || $(PYTHON) tools/native_hud_sprite_assets.py
intro-probe: native/intro_probe/src/hud_sprite_plan.h native/intro_probe/res/hud_sprite_plan.bin

.PHONY: test-native-tile-runs
test-native-tile-runs:
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined tools/test_native_tile_runs.c -o build/test_native_tile_runs
	build/test_native_tile_runs

test-native-player-input: test-native-tile-runs

# Stage 0 pair-addressed art. Its IDs reserve the dialogue and HUD ranges.
native/intro_probe/src/background_pairs.h: native/intro_probe/src/scroll3_data.h native/intro_probe/src/forest.h native/intro_probe/src/overlay.h tools/native_background_pairs.py tools/cps1_source_layers.py tools/cps1_gfx.py tools/cps1_palette.py tools/native_palette_fit.py native/intro_probe/src/layers.h native/intro_probe/res/layers.bin willow.zip
	$(SORSHA_PYTHON) tools/native_background_pairs.py
native/intro_probe/res/background_pairs.bin: native/intro_probe/src/background_pairs.h
	@test -f $@ || $(SORSHA_PYTHON) tools/native_background_pairs.py

.PHONY: test-native-background-pairs
test-native-background-pairs: native/intro_probe/src/background_pairs.h native/intro_probe/res/background_pairs.bin native/intro_probe/src/scroll2_data.h native/intro_probe/src/scroll3_data.h
	$(SORSHA_PYTHON) tools/verify_native_background_pairs.py

.PHONY: test-native-tile-lookup
test-native-tile-lookup:
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined tools/test_native_tile_lookup.c -o build/test_native_tile_lookup
	build/test_native_tile_lookup

test-native-player-input: test-native-tile-lookup

intro-probe: native/intro_probe/src/background_pairs.h native/intro_probe/res/background_pairs.bin
.PHONY: test-native-background-map
test-native-background-map: test-native-background-pairs build/reference_maincpu.bin build/arcade-trace/camera-native/frames.bin build/arcade-trace/player-walk/frames.bin build/arcade-trace/scroll3-walk/maps.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined tools/test_native_background_map.c -o build/test_native_background_map
	build/test_native_background_map

test-native-player-input: test-native-background-map

.PHONY: test-native-background-bank
test-native-background-bank: intro-probe test-native-background-map
	clang -Wall -Wextra -Werror -O2 tools/test_native_background_bank.c -o build/test_native_background_bank
	build/test_native_background_bank "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" native/intro_probe/out/rom.bin native/intro_probe/out/symbol.txt

test-native-player-input: test-native-background-bank

.PHONY: test-native-column-window
test-native-column-window:
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined tools/test_native_column_window.c -o build/test_native_column_window
	build/test_native_column_window

test-native-player-input: test-native-column-window

.PHONY: test-native-column-stream
test-native-column-stream: test-native-background-map
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined tools/test_native_column_stream.c -o build/test_native_column_stream
	build/test_native_column_stream
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined tools/test_native_column_stream_trace.c -o build/test_native_column_stream_trace
	build/test_native_column_stream_trace

test-native-player-input: test-native-column-stream

.PHONY: test-native-column-adjacent
test-native-column-adjacent: test-native-column-stream
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -DWILLOW_ADJACENT_COLUMNS tools/test_native_column_stream.c -o build/test_native_column_adjacent
	build/test_native_column_adjacent
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -DWILLOW_ADJACENT_COLUMNS tools/test_native_column_stream_trace.c -o build/test_native_column_adjacent_trace
	build/test_native_column_adjacent_trace

test-native-player-input: test-native-column-adjacent

.PHONY: test-native-movement-stress
test-native-movement-stress: intro-probe test-native-background-map
	clang -Wall -Wextra -Werror -O2 tools/test_native_movement_stress.c -o build/test_native_movement_stress
	build/test_native_movement_stress "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" native/intro_probe/out/rom.bin native/intro_probe/out/symbol.txt

test-native-player-input: test-native-movement-long

.PHONY: test-native-movement-long
test-native-movement-long: test-native-movement-stress
	build/test_native_movement_stress "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" native/intro_probe/out/rom.bin native/intro_probe/out/symbol.txt --long

.PHONY: test-arcade-scroll-publication
test-arcade-scroll-publication: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 tools/test_arcade_scroll_publication.c -o build/test_arcade_scroll_publication
	build/test_arcade_scroll_publication "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-player-input: test-arcade-scroll-publication test-arcade-pool-services

.PHONY: test-native-class18-pool
test-native-class18-pool: test-arcade-pool-services test-native-player-input
	build/intro_rom_smoke "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" native/intro_probe/out/rom.bin native/intro_probe/out/symbol.txt --class18-probe

.PHONY: test-native-class10-pool
test-native-class10-pool: test-arcade-pool-services test-native-player-input
	build/intro_rom_smoke "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" native/intro_probe/out/rom.bin native/intro_probe/out/symbol.txt --class10-probe

.PHONY: test-native-class14-pool
test-native-class14-pool: test-arcade-pool-services test-native-player-input
	build/intro_rom_smoke "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" native/intro_probe/out/rom.bin native/intro_probe/out/symbol.txt --class14-probe

.PHONY: test-native-class12-source-release
test-native-class12-source-release: test-arcade-pool-services intro-probe
	clang -O2 tools/intro_rom_smoke.c -o build/intro_rom_class12_bus
	build/intro_rom_class12_bus "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" native/intro_probe/out/rom.bin native/intro_probe/out/symbol.txt --class12-release-probe

.PHONY: test-native-all-pool-source-bus
test-native-all-pool-source-bus: test-arcade-pool-services intro-probe
	clang -O2 tools/intro_rom_smoke.c -o build/intro_rom_all_pool_bus
	build/intro_rom_all_pool_bus "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" native/intro_probe/out/rom.bin native/intro_probe/out/symbol.txt --all-pool-allocate-probe

# Representation experiment only; sparse counters are not enabled in the ROM.
.PHONY: test-native-tile-refs
test-native-tile-refs:
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_native_tile_refs.c -o build/test_native_tile_refs
	build/test_native_tile_refs

.PHONY: test-native-tile-release-m68k
test-native-tile-release-m68k:
	@test -n "$(SGDK_ROOT)" || (echo "Set SGDK_ROOT to an SGDK checkout"; exit 1)
	m68k-elf-gcc -m68000 -O3 -Wall -Wextra -Werror -fno-builtin -DSGDK_GCC -I"$(SGDK_ROOT)/inc" -nostdlib -Wl,-Ttext=0x100000,-e,releaseTransition tools/native_tile_release_probe.c -o build/native_tile_release_probe.elf
	m68k-elf-objcopy -O binary build/native_tile_release_probe.elf build/native_tile_release_probe.bin
	m68k-elf-nm -n build/native_tile_release_probe.elf > build/native_tile_release_probe.sym
	clang -Wall -Wextra -Werror -O2 tools/test_native_tile_release_m68k.c -o build/test_native_tile_release_m68k
	build/test_native_tile_release_m68k "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" build/native_tile_release_probe.bin build/native_tile_release_probe.sym

native/intro_probe/src/intro_restore_pairs.h: tools/native_intro_restore_pairs.py tools/cps1_program.py native/intro_probe/src/background_pairs.h native/intro_probe/src/scroll2_data.h native/intro_probe/src/scroll3_data.h willow.zip
	$(SORSHA_PYTHON) tools/native_intro_restore_pairs.py

intro-probe: native/intro_probe/src/intro_restore_pairs.h

.PHONY: test-arcade-actor-scheduler
test-arcade-actor-scheduler: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor_scheduler.c -o build/test_arcade_actor_scheduler
	build/test_arcade_actor_scheduler "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-player-input: test-arcade-actor-scheduler

.PHONY: test-arcade-frame-clock
test-arcade-frame-clock: build/reference_maincpu.bin build/arcade-trace/camera-native/frames.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_frame_clock.c -o build/test_arcade_frame_clock
	build/test_arcade_frame_clock "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-player-input: test-arcade-frame-clock

.PHONY: test-arcade-attract-state
test-arcade-attract-state: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_attract_state.c -o build/test_arcade_attract_state
	build/test_arcade_attract_state "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-player-input: test-arcade-attract-state

.PHONY: test-arcade-actor-selector1
test-arcade-actor-selector1: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor_selector1.c -o build/test_arcade_actor_selector1
	build/test_arcade_actor_selector1 "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-player-input: test-arcade-actor-selector1

.PHONY: test-arcade-actor-selector3
test-arcade-actor-selector3: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor_selector3.c -o build/test_arcade_actor_selector3
	build/test_arcade_actor_selector3 "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-player-input: test-arcade-actor-selector3

.PHONY: test-native-class12-visibility
test-native-class12-visibility: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_native_class12_visibility.c -o build/test_native_class12_visibility
	build/test_native_class12_visibility "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-player-input: test-native-class12-visibility

.PHONY: test-arcade-actor-selector8
test-arcade-actor-selector8: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor_selector8.c -o build/test_arcade_actor_selector8
	build/test_arcade_actor_selector8 "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-player-input: test-arcade-actor-selector8

.PHONY: test-arcade-event-route
test-arcade-event-route: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_event_route.c -o build/test_arcade_event_route
	build/test_arcade_event_route "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-event-resume
test-arcade-event-resume: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_EVENT_RESUME tools/test_arcade_event_route.c -o build/test_arcade_event_resume
	build/test_arcade_event_resume "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-player-input: test-arcade-event-route test-arcade-event-resume

.PHONY: test-arcade-gameplay-routed test-native-gameplay-routed
.PHONY: test-arcade-timeout-manager
test-arcade-timeout-manager: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_timeout_manager.c -o build/test_arcade_timeout_manager
	build/test_arcade_timeout_manager "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-timeout-scheduler
test-arcade-timeout-scheduler: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_timeout_scheduler.c -o build/test_arcade_timeout_scheduler
	@for region in 0 1 2 3 4; do build/test_arcade_timeout_scheduler "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" $$region || exit $$?; done

.PHONY: test-native-timeout-scheduler
.PHONY: test-arcade-gameplay-task
test-arcade-gameplay-task: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_gameplay_task.c -o build/test_arcade_gameplay_task
	build/test_arcade_gameplay_task "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-timeout-scheduler: build/reference_maincpu.bin
	python3 tools/build_timeout_runtime_probe.py
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_native_timeout_scheduler.c -o build/test_native_timeout_scheduler
	@for region in 0 1 2 3 4; do build/test_native_timeout_scheduler "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" $$region || exit $$?; done

test-arcade-gameplay-routed: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_GAMEPLAY_ROUTED tools/test_arcade_stage0_full_frame.c -o build/test_arcade_gameplay_routed
	build/test_arcade_gameplay_routed "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-gameplay-routed: build/reference_maincpu.bin
	python3 tools/build_gameplay_runtime_probe.py --routed
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_GAMEPLAY_ROUTED -DWILLOW_TEST_NATIVE_TARGET -DWILLOW_TEST_NATIVE_SAMPLES=16 tools/test_arcade_stage0_full_frame.c -o build/test_native_gameplay_routed
	build/test_native_gameplay_routed "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" --cycles

.PHONY: test-arcade-event-setup
test-arcade-event-setup: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_event_setup.c -o build/test_arcade_event_setup
	build/test_arcade_event_setup "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-player-input: test-arcade-event-setup

.PHONY: test-arcade-event-ui
test-arcade-event-ui: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_event_ui.c -o build/test_arcade_event_ui
	build/test_arcade_event_ui "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"
	build/test_arcade_event_ui "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" --real-tasks
	build/test_arcade_event_ui "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" --real-services

test-native-player-input: test-arcade-event-ui

.PHONY: test-arcade-tasks
test-arcade-tasks: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_tasks.c -o build/test_arcade_tasks
	build/test_arcade_tasks "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-player-input: test-arcade-tasks

.PHONY: test-arcade-task-startup
.PHONY: test-arcade-high-score
test-arcade-high-score: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_high_score.c -o build/test_arcade_high_score
	build/test_arcade_high_score "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-startup-latched
.PHONY: test-arcade-reset-ram
test-arcade-reset-ram: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_reset_ram.c -o build/test_arcade_reset_ram
	build/test_arcade_reset_ram "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-arcade-startup-latched: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_startup_latched.c -o build/test_arcade_startup_latched
	build/test_arcade_startup_latched "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-arcade-task-startup: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_task_startup.c -o build/test_arcade_task_startup
	build/test_arcade_task_startup "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-player-input: test-arcade-task-startup

.PHONY: test-arcade-boot-state
test-arcade-boot-state: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_boot_state.c -o build/test_arcade_boot_state
	build/test_arcade_boot_state "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"
	build/test_arcade_boot_state "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" --real-options

test-native-player-input: test-arcade-boot-state

.PHONY: test-native-boot-task
test-native-boot-task: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_native_boot_task.c -o build/test_native_boot_task
	build/test_native_boot_task "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-player-input: test-native-boot-task

.PHONY: test-native-queued-text
test-native-queued-text: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_native_queued_text.c -o build/test_native_queued_text
	build/test_native_queued_text "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-player-input: test-native-queued-text

.PHONY: test-native-boot-text-chain test-native-queued-text-scheduler
test-native-boot-text-chain: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_native_boot_text_chain.c -o build/test_native_boot_text_chain
	@for region in 0 1 2 3 4; do build/test_native_boot_text_chain "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" $$region || exit $$?; done

test-native-queued-text-scheduler: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_native_queued_text_scheduler.c -o build/test_native_queued_text_scheduler
	build/test_native_queued_text_scheduler "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" 0

test-native-player-input: test-native-boot-text-chain test-native-queued-text-scheduler

.PHONY: test-native-boot-handoff-chain
test-native-boot-handoff-chain: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_native_boot_handoff_chain.c -o build/test_native_boot_handoff_chain
	@for region in 0 1 2 3 4; do build/test_native_boot_handoff_chain "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" $$region || exit $$?; done

test-native-player-input: test-native-boot-handoff-chain

.PHONY: test-arcade-attract-init
test-arcade-attract-init: native/intro_probe/src/palette_initial_data.h build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_attract_init.c -o build/test_arcade_attract_init
	build/test_arcade_attract_init "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-player-input: test-arcade-attract-init

.PHONY: test-arcade-attract-fade
test-arcade-attract-fade: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_attract_fade.c -o build/test_arcade_attract_fade
	@for region in 0 1 2 3 4; do build/test_arcade_attract_fade "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" $$region || exit $$?; done

test-native-player-input: test-arcade-attract-fade

.PHONY: test-arcade-attract-input
test-arcade-attract-input: native/intro_probe/src/palette_initial_data.h build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_attract_input.c -o build/test_arcade_attract_input
	build/test_arcade_attract_input "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-player-input: test-arcade-attract-input

.PHONY: test-native-attract-task
test-native-attract-task: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_native_attract_task.c -o build/test_native_attract_task
	build/test_native_attract_task "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-player-input: test-native-attract-task

.PHONY: test-native-boot-attract-chain
.PHONY: test-native-reset-boot-chain
.PHONY: test-native-reset-boot-storage
test-native-reset-boot-storage: test-native-reset-boot-chain
	$(PYTHON) tools/report_reset_boot_storage.py

test-native-reset-boot-chain: test-arcade-reset-ram test-arcade-startup-latched
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_native_reset_boot_chain.c -o build/test_native_reset_boot_chain
	@for region in 0 1 2 3 4; do build/test_native_reset_boot_chain "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" $$region || exit $$?; done

test-native-boot-attract-chain: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_native_boot_attract_chain.c -o build/test_native_boot_attract_chain
	@for region in 0 1 2 3 4; do build/test_native_boot_attract_chain "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" $$region || exit $$?; done

test-native-player-input: test-native-boot-attract-chain

.PHONY: test-arcade-attract-scene-init
test-arcade-attract-scene-init: native/intro_probe/src/palette_initial_data.h build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_attract_scene_init.c -o build/test_arcade_attract_scene_init
	build/test_arcade_attract_scene_init "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-player-input: test-arcade-attract-scene-init

.PHONY: test-arcade-attract-scene-spawn
test-arcade-attract-scene-spawn: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_attract_scene_spawn.c -o build/test_arcade_attract_scene_spawn
	build/test_arcade_attract_scene_spawn "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-player-input: test-arcade-attract-scene-spawn

.PHONY: test-arcade-palette-bank-task test-native-palette-bank-scheduler
test-arcade-palette-bank-task: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_palette_bank_task.c -o build/test_arcade_palette_bank_task
	build/test_arcade_palette_bank_task "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-palette-bank-scheduler: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_native_palette_bank_scheduler.c -o build/test_native_palette_bank_scheduler
	build/test_native_palette_bank_scheduler "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" 0

test-native-player-input: test-arcade-palette-bank-task test-native-palette-bank-scheduler

.PHONY: test-arcade-attract-backdrop
test-arcade-attract-backdrop: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_attract_backdrop.c -o build/test_arcade_attract_backdrop
	build/test_arcade_attract_backdrop "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-player-input: test-arcade-attract-backdrop

.PHONY: test-arcade-title-actor
test-arcade-title-actor: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_title_actor.c -o build/test_arcade_title_actor
	build/test_arcade_title_actor "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-player-input: test-arcade-title-actor

.PHONY: test-arcade-title-publication
test-arcade-title-publication: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_title_publication.c -o build/test_arcade_title_publication
	build/test_arcade_title_publication "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-player-input: test-arcade-title-publication

.PHONY: native-title-sprites test-native-title-sprites
native-title-sprites: willow.zip
	$(SORSHA_PYTHON) tools/native_title_sprites.py

test-native-title-sprites: native-title-sprites test-arcade-title-publication
	$(SORSHA_PYTHON) tools/verify_native_title_sprites.py

test-native-player-input: test-native-title-sprites

.PHONY: test-native-boot-scheduler
test-native-boot-scheduler: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_native_boot_scheduler.c -o build/test_native_boot_scheduler
	build/test_native_boot_scheduler "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-player-input: test-native-boot-scheduler

.PHONY: test-native-boot-target
test-native-boot-target: build/reference_maincpu.bin
	@test -n "$(SGDK_ROOT)" || (echo "Set SGDK_ROOT to an SGDK checkout"; exit 1)
	m68k-elf-gcc -m68000 -O2 -Wall -Wextra -Werror -fno-builtin -DSGDK_GCC -I"$(SGDK_ROOT)/inc" -nostdlib -Wl,-Ttext=0x100010,-e,bootRuntimeProbe tools/boot_runtime_probe.c -o build/boot_runtime_probe.elf
	m68k-elf-objcopy -O binary build/boot_runtime_probe.elf build/boot_runtime_probe.bin
	m68k-elf-nm -n build/boot_runtime_probe.elf > build/boot_runtime_probe.sym
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_native_boot_target.c -o build/test_native_boot_target
	build/test_native_boot_target "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-player-input: test-native-boot-target

.PHONY: test-arcade-transition-clear
test-arcade-transition-clear: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_transition_clear.c -o build/test_arcade_transition_clear
	@for region in 0 1 2 3 4; do build/test_arcade_transition_clear "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" $$region || exit $$?; done

test-native-player-input: test-arcade-transition-clear

.PHONY: test-native-boot-clear-chain
test-native-boot-clear-chain: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_native_boot_clear_chain.c -o build/test_native_boot_clear_chain
	@for region in 0 1 2 3 4; do build/test_native_boot_clear_chain "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" $$region || exit $$?; done

test-native-player-input: test-native-boot-clear-chain

.PHONY: test-native-scene-clear
test-native-scene-clear:
	$(PYTHON) tools/audit_transition_clear_graphics.py
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_native_scene_clear.c -o build/test_native_scene_clear
	build/test_native_scene_clear

test-native-player-input: test-native-scene-clear

.PHONY: test-native-scene-memory
test-native-scene-memory: test-arcade-transition-clear
	@test -n "$(SGDK_ROOT)" || (echo "Set SGDK_ROOT to an SGDK checkout"; exit 1)
	m68k-elf-gcc -m68000 -O2 -Wall -Wextra -Werror -fno-builtin -DSGDK_GCC -I"$(SGDK_ROOT)/inc" -nostdlib -Wl,-Ttext=0x100010,-e,sceneMemoryProbe tools/scene_memory_probe.c -o build/scene_memory_probe.elf
	m68k-elf-objcopy -O binary build/scene_memory_probe.elf build/scene_memory_probe.bin
	m68k-elf-nm -n build/scene_memory_probe.elf > build/scene_memory_probe.sym
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_scene_memory_target.c -o build/test_scene_memory_target
	build/test_scene_memory_target "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-native-scroll3-fill-target
test-native-scroll3-fill-target: build/reference_maincpu.bin
	@test -n "$(SGDK_ROOT)" || (echo "Set SGDK_ROOT to an SGDK checkout"; exit 1)
	m68k-elf-gcc -m68000 -O2 -Wall -Wextra -Werror -fno-builtin -DSGDK_GCC -I"$(SGDK_ROOT)/inc" -nostdlib -Wl,-Ttext=0x100010,-e,scroll3FillProbe tools/scroll3_fill_probe.c -o build/scroll3_fill_probe.elf
	m68k-elf-objcopy -O binary build/scroll3_fill_probe.elf build/scroll3_fill_probe.bin
	m68k-elf-nm -n build/scroll3_fill_probe.elf > build/scroll3_fill_probe.sym
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_scroll3_fill_target.c -o build/test_scroll3_fill_target
	build/test_scroll3_fill_target "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-player-input: test-native-scroll3-fill-target

# Always rebuild the normal diagnostic after this terminal-only experiment,
# including when validation fails. Keep its ROM in build/, never release/.
.PHONY: test-native-scene-publication
test-native-scene-publication: test-native-word-fill-target
	@test -n "$(SGDK_ROOT)" || (echo "Set SGDK_ROOT to an SGDK checkout"; exit 1)
	@set -e; trap 'rm -f native/intro_probe/out/src/main.o; $(MAKE) intro-probe EXTRA_FLAGS="$(EXTRA_FLAGS)"' EXIT; \
	 rm -f native/intro_probe/out/src/main.o; \
	 $(MAKE) intro-probe EXTRA_FLAGS="$(EXTRA_FLAGS) -DWILLOW_SCENE_CLEAR_PROBE"; \
	 cp native/intro_probe/out/rom.bin build/scene-clear-experiment.bin; \
	 cp native/intro_probe/out/symbol.txt build/scene-clear-experiment.sym; \
	 clang -O2 tools/intro_rom_smoke.c -o build/intro_rom_scene_clear; \
	 build/intro_rom_scene_clear "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" build/scene-clear-experiment.bin build/scene-clear-experiment.sym --scene-clear-probe; \
	 build/intro_rom_scene_clear "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" build/scene-clear-experiment.bin build/scene-clear-experiment.sym --scene-clear-walk-probe

.PHONY: test-arcade-task-continuation
test-arcade-task-continuation: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_task_continuation.c -o build/test_arcade_task_continuation
	build/test_arcade_task_continuation "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-player-input: test-arcade-task-continuation

.PHONY: test-arcade-text-quad
test-arcade-text-quad: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_text_quad.c -o build/test_arcade_text_quad
	build/test_arcade_text_quad "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-player-input: test-arcade-text-quad

.PHONY: test-arcade-event-animation
test-arcade-event-animation: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_event_animation.c -o build/test_arcade_event_animation
	build/test_arcade_event_animation "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-player-input: test-arcade-event-animation

.PHONY: test-arcade-event-consumer
test-arcade-event-consumer: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_event_consumer.c -o build/test_arcade_event_consumer
	build/test_arcade_event_consumer "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" --real-services
	build/test_arcade_event_consumer "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" --real-health

test-native-player-input: test-arcade-event-consumer

.PHONY: test-arcade-event-sprites
test-arcade-event-sprites: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_event_sprites.c -o build/test_arcade_event_sprites
	build/test_arcade_event_sprites "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-player-input: test-arcade-event-sprites

.PHONY: test-arcade-event-price
test-arcade-event-price: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_event_price.c -o build/test_arcade_event_price
	build/test_arcade_event_price "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-player-input: test-arcade-event-price

.PHONY: test-arcade-event-messages
test-arcade-event-messages: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_event_messages.c -o build/test_arcade_event_messages
	build/test_arcade_event_messages "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-player-input: test-arcade-event-messages

.PHONY: test-arcade-event-purchase
test-arcade-event-purchase: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_event_purchase.c -o build/test_arcade_event_purchase
	build/test_arcade_event_purchase "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-player-input: test-arcade-event-purchase

.PHONY: test-arcade-event-grant
test-arcade-event-grant: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_event_grant.c -o build/test_arcade_event_grant
	build/test_arcade_event_grant "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" --real-services
	build/test_arcade_event_grant "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" --real-health

test-native-player-input: test-arcade-event-grant

.PHONY: test-arcade-event-display
test-arcade-event-display: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_event_display.c -o build/test_arcade_event_display
	build/test_arcade_event_display "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-player-input: test-arcade-event-display

.PHONY: test-arcade-inventory-attribute
test-arcade-inventory-attribute: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_inventory_attribute.c -o build/test_arcade_inventory_attribute
	build/test_arcade_inventory_attribute "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-player-input: test-arcade-inventory-attribute

.PHONY: test-arcade-event-commands
test-arcade-event-commands: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_event_commands.c -o build/test_arcade_event_commands
	build/test_arcade_event_commands "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-player-input: test-arcade-event-commands

.PHONY: test-arcade-event-hud-band
test-arcade-event-hud-band: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_event_hud_band.c -o build/test_arcade_event_hud_band
	build/test_arcade_event_hud_band "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-player-input: test-arcade-event-hud-band

.PHONY: test-arcade-event-hud-rebuild
test-arcade-event-hud-rebuild: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_event_hud_rebuild.c -o build/test_arcade_event_hud_rebuild
	build/test_arcade_event_hud_rebuild "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-player-input: test-arcade-event-hud-rebuild

.PHONY: test-arcade-event-command-wait
test-arcade-event-command-wait: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_event_command_wait.c -o build/test_arcade_event_command_wait
	build/test_arcade_event_command_wait "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-player-input: test-arcade-event-command-wait

.PHONY: test-arcade-event-command-consumer
test-arcade-event-command-consumer: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_event_command_consumer.c -o build/test_arcade_event_command_consumer
	build/test_arcade_event_command_consumer "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-player-input: test-arcade-event-command-consumer

.PHONY: test-arcade-transition-reveal
test-arcade-transition-reveal: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_transition_reveal.c -o build/test_arcade_transition_reveal
	build/test_arcade_transition_reveal "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-player-input: test-arcade-transition-reveal

.PHONY: test-arcade-palette-task
test-arcade-palette-task: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_palette_task.c -o build/test_arcade_palette_task
	build/test_arcade_palette_task "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-player-input: test-arcade-palette-task

.PHONY: test-arcade-transition-sequence
test-arcade-transition-sequence: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_transition_sequence.c -o build/test_arcade_transition_sequence
	build/test_arcade_transition_sequence "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-player-input: test-arcade-transition-sequence

.PHONY: test-arcade-task-scheduler
test-arcade-task-scheduler: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_task_scheduler.c -o build/test_arcade_task_scheduler
	build/test_arcade_task_scheduler "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-player-input: test-arcade-task-scheduler

.PHONY: test-arcade-task-context
test-arcade-task-context: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_task_context.c -o build/test_arcade_task_context
	build/test_arcade_task_context "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-player-input: test-arcade-task-context

.PHONY: test-arcade-task-sleep-cycle
test-arcade-task-sleep-cycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_task_sleep_cycle.c -o build/test_arcade_task_sleep_cycle
	build/test_arcade_task_sleep_cycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-player-input: test-arcade-task-sleep-cycle

.PHONY: test-arcade-scheduler-return
test-arcade-scheduler-return: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_scheduler_return.c -o build/test_arcade_scheduler_return
	build/test_arcade_scheduler_return "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-player-input: test-arcade-scheduler-return

.PHONY: test-arcade-scheduler-pass
test-arcade-scheduler-pass: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_scheduler_pass.c -o build/test_arcade_scheduler_pass
	build/test_arcade_scheduler_pass "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-player-input: test-arcade-scheduler-pass

.PHONY: test-arcade-scheduler-sleep
test-arcade-scheduler-sleep: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_scheduler_sleep.c -o build/test_arcade_scheduler_sleep
	build/test_arcade_scheduler_sleep "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-player-input: test-arcade-scheduler-sleep

.PHONY: test-native-task-store
test-native-task-store: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_native_task_store.c -o build/test_native_task_store
	build/test_native_task_store

test-native-player-input: test-native-task-store

.PHONY: test-native-task-target
test-native-task-target: build/reference_maincpu.bin
	@test -n "$(SGDK_ROOT)" || (echo "Set SGDK_ROOT to an SGDK checkout"; exit 1)
	m68k-elf-gcc -m68000 -O2 -Wall -Wextra -Werror -DSGDK_GCC -I"$(SGDK_ROOT)/inc" -nostdlib -Wl,-Ttext=0x100010,-e,taskStoreProbe tools/task_store_probe.c -o build/task_store_probe.elf
	m68k-elf-objcopy -O binary build/task_store_probe.elf build/task_store_probe.bin
	m68k-elf-nm -n build/task_store_probe.elf > build/task_store_probe.sym
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_native_task_target.c -o build/test_native_task_target
	build/test_native_task_target "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-player-input: test-native-task-target

.PHONY: test-native-task-runtime
test-native-task-runtime: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_native_task_runtime.c -o build/test_native_task_runtime
	build/test_native_task_runtime

test-native-player-input: test-native-task-runtime

.PHONY: test-native-task-differential
test-native-task-differential: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_native_task_differential.c -o build/test_native_task_differential
	build/test_native_task_differential "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-player-input: test-native-task-differential

.PHONY: test-native-task-runtime-target
test-native-task-runtime-target: build/reference_maincpu.bin
	@test -n "$(SGDK_ROOT)" || (echo "Set SGDK_ROOT to an SGDK checkout"; exit 1)
	m68k-elf-gcc -m68000 -O2 -Wall -Wextra -Werror -DSGDK_GCC -I"$(SGDK_ROOT)/inc" -nostdlib -Wl,-Ttext=0x100010,-e,taskRuntimeProbe $(TASK_RUNTIME_CFLAGS) tools/task_runtime_probe.c -o build/task_runtime_probe.elf
	m68k-elf-objcopy -O binary build/task_runtime_probe.elf build/task_runtime_probe.bin
	m68k-elf-nm -n build/task_runtime_probe.elf > build/task_runtime_probe.sym
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all $(TASK_RUNTIME_HOST_CFLAGS) tools/test_native_task_runtime_target.c -o build/test_native_task_runtime_target
	build/test_native_task_runtime_target "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"
	build/test_native_task_runtime_target "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" --cycles

test-native-player-input: test-native-task-runtime-target

.PHONY: test-native-palette-view
test-native-palette-view:
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_native_palette_view.c -o build/test_native_palette_view
	build/test_native_palette_view

test-native-player-input: test-native-palette-view

.PHONY: test-native-task-runtime-lazy
test-native-task-runtime-lazy:
	$(MAKE) test-native-task-runtime-target TASK_RUNTIME_CFLAGS=-DWILLOW_PROBE_LAZY_PALETTE

.PHONY: test-native-palette-sources
test-native-palette-sources: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_native_palette_sources.c -o build/test_native_palette_sources
	build/test_native_palette_sources

test-native-player-input: test-native-palette-sources

.PHONY: test-native-palette-owner
test-native-palette-owner: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_native_palette_owner.c -o build/test_native_palette_owner
	build/test_native_palette_owner

test-native-player-input: test-native-palette-owner

.PHONY: test-native-palette-mutable
test-native-palette-mutable: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_native_palette_mutable.c -o build/test_native_palette_mutable
	build/test_native_palette_mutable

test-native-player-input: test-native-palette-mutable

.PHONY: audit-palette-storage
audit-palette-storage: build/reference_maincpu.bin
	$(PYTHON) tools/audit_palette_snapshot_storage.py

.PHONY: test-native-palette-owner-target
test-native-palette-owner-target: build/reference_maincpu.bin
	@test -n "$(SGDK_ROOT)" || (echo "Set SGDK_ROOT to an SGDK checkout"; exit 1)
	m68k-elf-gcc -m68000 -O2 -Wall -Wextra -Werror -DSGDK_GCC -I"$(SGDK_ROOT)/inc" -nostdlib -Wl,-Ttext=0x100010,-e,paletteOwnerProbe $(PALETTE_OWNER_CFLAGS) tools/palette_owner_probe.c -o build/palette_owner_probe.elf
	m68k-elf-objcopy -O binary build/palette_owner_probe.elf build/palette_owner_probe.bin
	m68k-elf-nm -n build/palette_owner_probe.elf > build/palette_owner_probe.sym
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all $(PALETTE_OWNER_HOST_CFLAGS) tools/test_native_palette_owner_target.c -o build/test_native_palette_owner_target
	build/test_native_palette_owner_target "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-player-input: test-native-palette-owner-target

.PHONY: test-native-task-runtime-rom-palette
test-native-task-runtime-rom-palette:
	$(MAKE) test-native-task-runtime-target TASK_RUNTIME_CFLAGS=-DWILLOW_PROBE_ROM_PALETTE TASK_RUNTIME_HOST_CFLAGS=-DWILLOW_TEST_ROM_OWNER

.PHONY: test-native-task-runtime-mutable-palette
test-native-task-runtime-mutable-palette:
	$(MAKE) test-native-task-runtime-target TASK_RUNTIME_CFLAGS=-DWILLOW_PROBE_ROM_PALETTE TASK_RUNTIME_HOST_CFLAGS='-DWILLOW_TEST_ROM_OWNER -DWILLOW_TEST_MUTABLE_SEED'

.PHONY: test-native-palette-whole-fade
test-native-palette-whole-fade: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_palette_fade.c -o build/test_arcade_palette_fade
	build/test_arcade_palette_fade "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" --owned
	$(MAKE) test-native-task-runtime-target TASK_RUNTIME_CFLAGS='-DWILLOW_PROBE_ROM_PALETTE -DWILLOW_PROBE_OWNER_FADE_ALL' TASK_RUNTIME_HOST_CFLAGS='-DWILLOW_TEST_ROM_OWNER -DWILLOW_TEST_MUTABLE_SEED'

.PHONY: test-native-task-segments
test-native-task-segments: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_SEGMENTED tools/test_native_task_store.c -o build/test_native_task_segments
	build/test_native_task_segments

test-native-player-input: test-native-task-segments

.PHONY: test-native-runtime-memory
test-native-runtime-memory: intro-probe
	clang -O2 tools/intro_rom_smoke.c -o build/intro_rom_heap_audit
	build/intro_rom_heap_audit "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" native/intro_probe/out/rom.bin native/intro_probe/out/symbol.txt --runtime-memory-probe

.PHONY: test-native-palette-split
test-native-palette-split: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_native_palette_split.c -o build/test_native_palette_split
	build/test_native_palette_split
	$(MAKE) test-native-palette-owner-target PALETTE_OWNER_CFLAGS='-DWILLOW_PROBE_SPLIT_PALETTE -fno-builtin' PALETTE_OWNER_HOST_CFLAGS=-DWILLOW_TEST_SPLIT_PALETTE

.PHONY: test-native-task-runtime-split-palette
test-native-task-runtime-split-palette:
	$(MAKE) test-native-task-runtime-target TASK_RUNTIME_CFLAGS='-DWILLOW_PROBE_ROM_PALETTE -DWILLOW_PROBE_OWNER_FADE_ALL -DWILLOW_PROBE_SPLIT_PALETTE -fno-builtin -fstack-usage' TASK_RUNTIME_HOST_CFLAGS='-DWILLOW_TEST_ROM_OWNER -DWILLOW_TEST_MUTABLE_SEED -DWILLOW_TEST_SPLIT_PALETTE'

.PHONY: test-native-runtime-split-storage
test-native-runtime-split-storage:
	$(MAKE) test-native-task-runtime-target TASK_RUNTIME_CFLAGS='-DWILLOW_PROBE_ROM_PALETTE -DWILLOW_PROBE_OWNER_FADE_ALL -DWILLOW_PROBE_SPLIT_PALETTE -DWILLOW_PROBE_SEGMENTED_TASKS -fno-builtin -fstack-usage' TASK_RUNTIME_HOST_CFLAGS='-DWILLOW_TEST_ROM_OWNER -DWILLOW_TEST_MUTABLE_SEED -DWILLOW_TEST_SPLIT_PALETTE -DWILLOW_TEST_SEGMENTED_TASKS'

.PHONY: test-arcade-attract-scene-wait
test-arcade-attract-scene-wait: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_attract_scene_wait.c -o build/test_arcade_attract_scene_wait
	build/test_arcade_attract_scene_wait "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-player-input: test-arcade-attract-scene-wait

.PHONY: test-native-message-task
test-native-message-task: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_native_message_task.c -o build/test_native_message_task
	build/test_native_message_task "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-player-input: test-native-message-task

.PHONY: test-native-message-scheduler
test-native-message-scheduler: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_native_message_scheduler.c -o build/test_native_message_scheduler
	build/test_native_message_scheduler "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-player-input: test-native-message-scheduler

.PHONY: test-arcade-opening-actors
test-arcade-opening-actors: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_opening_actors.c -o build/test_arcade_opening_actors
	build/test_arcade_opening_actors "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-native-title-resources
test-native-title-resources: test-arcade-title-publication native/intro_probe/src/actor_data.h
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_NATIVE_TITLE_DATA tools/test_arcade_title_publication.c -o build/test_native_title_publication
	build/test_native_title_publication "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-native-opening-resources
test-native-opening-resources: test-arcade-opening-actors native/intro_probe/src/actor_data.h native/intro_probe/src/palette_initial_data.h
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_NATIVE_OPENING_DATA tools/test_arcade_opening_actors.c -o build/test_native_opening_actors
	build/test_native_opening_actors "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-player-input: test-arcade-opening-actors

.PHONY: test-arcade-opening-scene
test-arcade-opening-scene: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_opening_scene.c -o build/test_arcade_opening_scene
	@for region in 0 1 2; do build/test_arcade_opening_scene "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" $$region || exit $$?; done

test-native-player-input: test-arcade-opening-scene

.PHONY: test-arcade-opening-frame
test-arcade-opening-frame: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_opening_frame.c -o build/test_arcade_opening_frame
	build/test_arcade_opening_frame "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-player-input: test-arcade-opening-frame

.PHONY: test-arcade-opening-scene-run
test-arcade-opening-scene-run: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_opening_scene_run.c -o build/test_arcade_opening_scene_run
	build/test_arcade_opening_scene_run "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-player-input: test-arcade-opening-scene-run

.PHONY: test-native-opening-task
test-native-opening-task: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_OPENING_TASK tools/test_arcade_opening_scene_run.c -o build/test_native_opening_task
	build/test_native_opening_task "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-scene-boot: test-native-opening-task

.PHONY: test-arcade-opening-lifecycle
test-arcade-opening-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_opening_lifecycle.c -o build/test_arcade_opening_lifecycle
	build/test_arcade_opening_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"
	build/test_arcade_opening_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" --pan-setup
	build/test_arcade_opening_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" --pan-move
	build/test_arcade_opening_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" --pan-fade
	build/test_arcade_opening_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" --phase-two
	build/test_arcade_opening_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" --phase-four-text
	build/test_arcade_opening_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" --phase-four-panel
	build/test_arcade_opening_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" --phase-four-hold
	build/test_arcade_opening_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" --phase-four-map
	build/test_arcade_opening_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" --phase-four-scroll
	build/test_arcade_opening_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" --phase-four-blink
	build/test_arcade_opening_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" --phase-four-replace
	build/test_arcade_opening_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" --phase-four-exit
	build/test_arcade_opening_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" --phase-four
	build/test_arcade_opening_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" --demo-init-0
	build/test_arcade_opening_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" --demo-init-1
	build/test_arcade_opening_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" --demo-start-0
	build/test_arcade_opening_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" --demo-start-1

test-native-player-input: test-arcade-opening-lifecycle

.PHONY: test-native-opening-target
test-native-opening-target: build/reference_maincpu.bin
	@test -n "$(SGDK_ROOT)" || (echo "Set SGDK_ROOT to an SGDK checkout"; exit 1)
	m68k-elf-gcc -m68000 -O2 -Wall -Wextra -Werror -fno-builtin -DSGDK_GCC -I"$(SGDK_ROOT)/inc" -nostdlib -Wl,-Ttext=0x100010,-e,openingRuntimeProbe tools/opening_runtime_probe.c -o build/opening_runtime_probe.elf
	m68k-elf-objcopy -O binary build/opening_runtime_probe.elf build/opening_runtime_probe.bin
	m68k-elf-nm -n build/opening_runtime_probe.elf > build/opening_runtime_probe.sym
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_native_opening_target.c -o build/test_native_opening_target
	build/test_native_opening_target "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"
	build/test_native_opening_target "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" --zero-retained
	build/test_native_opening_target "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" --pan-setup
	build/test_native_opening_target "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" --pan-move
	build/test_native_opening_target "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" --pan-fade
	build/test_native_opening_target "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" --phase-two
	build/test_native_opening_target "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" --phase-four-text
	build/test_native_opening_target "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" --phase-four-panel
	build/test_native_opening_target "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" --phase-four-hold
	build/test_native_opening_target "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" --phase-four-map
	build/test_native_opening_target "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" --phase-four-scroll
	build/test_native_opening_target "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" --phase-four-blink
	build/test_native_opening_target "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" --phase-four-replace
	build/test_native_opening_target "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" --phase-four-exit
	build/test_native_opening_target "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" --phase-four
	build/test_native_opening_target "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" --demo-init-0
	build/test_native_opening_target "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" --demo-init-1
	build/test_native_opening_target "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" --demo-start-0
	build/test_native_opening_target "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" --demo-start-1

test-native-player-input: test-native-opening-target

.PHONY: test-arcade-attract-pan-init
test-arcade-attract-pan-init: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_attract_pan_init.c -o build/test_arcade_attract_pan_init
	@for region in 0 1; do build/test_arcade_attract_pan_init "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" $$region || exit $$?; done
	@for region in 0 1; do build/test_arcade_attract_pan_init "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" $$region --second || exit $$?; done

# Same original instructions, using native-compatible whole-chunk sinks.
.PHONY: test-arcade-attract-pan-copy
test-arcade-attract-pan-copy: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_PAN_COPY tools/test_arcade_attract_pan_init.c -o build/test_arcade_attract_pan_copy
	@for region in 0 1; do build/test_arcade_attract_pan_copy "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" $$region || exit $$?; done
	@for region in 0 1; do build/test_arcade_attract_pan_copy "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" $$region --second || exit $$?; done

test-native-scene-boot: test-arcade-attract-pan-init test-arcade-attract-pan-copy
test-native-player-input: test-arcade-attract-pan-init

.PHONY: test-arcade-attract-pan-step
test-arcade-attract-pan-step: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_attract_pan_step.c -o build/test_arcade_attract_pan_step
	build/test_arcade_attract_pan_step "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-player-input: test-arcade-attract-pan-step

.PHONY: test-arcade-attract-pan-wait
test-arcade-attract-pan-wait: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_attract_pan_wait.c -o build/test_arcade_attract_pan_wait
	build/test_arcade_attract_pan_wait "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-player-input: test-arcade-attract-pan-wait

.PHONY: test-arcade-attract-text-sequence
test-arcade-attract-text-sequence: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_attract_text_sequence.c -o build/test_arcade_attract_text_sequence
	build/test_arcade_attract_text_sequence "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-player-input: test-arcade-attract-text-sequence

.PHONY: test-arcade-attract-panel
test-arcade-attract-panel: build/reference_maincpu.bin native/intro_probe/src/intro_panel_data.h
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_attract_panel.c -o build/test_arcade_attract_panel
	build/test_arcade_attract_panel "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-player-input: test-arcade-attract-panel

.PHONY: test-arcade-attract-panel-wait
test-arcade-attract-panel-wait: build/reference_maincpu.bin native/intro_probe/src/task_startup_data.h
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_attract_panel_wait.c -o build/test_arcade_attract_panel_wait
	build/test_arcade_attract_panel_wait "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-player-input: test-arcade-attract-panel-wait

.PHONY: test-arcade-palette-selected-task
test-arcade-palette-selected-task: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_palette_selected_task.c -o build/test_arcade_palette_selected_task
	build/test_arcade_palette_selected_task "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-native-palette-selected-scheduler
test-native-palette-selected-scheduler: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_native_palette_selected_scheduler.c -o build/test_native_palette_selected_scheduler
	build/test_native_palette_selected_scheduler "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" 0

test-native-player-input: test-arcade-palette-selected-task test-native-palette-selected-scheduler

.PHONY: test-arcade-attract-panel-map
test-arcade-attract-panel-map: build/reference_maincpu.bin native/intro_probe/src/task_startup_data.h
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_attract_panel_map.c -o build/test_arcade_attract_panel_map
	build/test_arcade_attract_panel_map "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-player-input: test-arcade-attract-panel-map

.PHONY: test-arcade-attract-panel-scroll
test-arcade-attract-panel-scroll: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_attract_panel_scroll.c -o build/test_arcade_attract_panel_scroll
	build/test_arcade_attract_panel_scroll "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-player-input: test-arcade-attract-panel-scroll

.PHONY: test-native-blink-text
test-native-blink-text: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_native_blink_text.c -o build/test_native_blink_text
	build/test_native_blink_text "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-native-blink-text-scheduler
test-native-blink-text-scheduler: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_native_blink_text_scheduler.c -o build/test_native_blink_text_scheduler
	build/test_native_blink_text_scheduler "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" 0

test-native-player-input: test-native-blink-text test-native-blink-text-scheduler

.PHONY: test-arcade-attract-panel-text
test-arcade-attract-panel-text: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_attract_panel_text.c -o build/test_arcade_attract_panel_text
	build/test_arcade_attract_panel_text "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-player-input: test-arcade-attract-panel-text

.PHONY: test-arcade-attract-panel-replace
test-arcade-attract-panel-replace: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_attract_panel_replace.c -o build/test_arcade_attract_panel_replace
	build/test_arcade_attract_panel_replace "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-player-input: test-arcade-attract-panel-replace

.PHONY: test-arcade-attract-panel-exit
test-arcade-attract-panel-exit: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_attract_panel_exit.c -o build/test_arcade_attract_panel_exit
	build/test_arcade_attract_panel_exit "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-player-input: test-arcade-attract-panel-exit

.PHONY: test-arcade-attract-final-panel
test-arcade-attract-final-panel: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_attract_final_panel.c -o build/test_arcade_attract_final_panel
	build/test_arcade_attract_final_panel "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-player-input: test-arcade-attract-final-panel

.PHONY: test-arcade-attract-demo-equipment
test-arcade-attract-demo-equipment: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_attract_demo_equipment.c -o build/test_arcade_attract_demo_equipment
	build/test_arcade_attract_demo_equipment "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-player-reset
test-arcade-player-reset: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_player_reset.c -o build/test_arcade_player_reset
	build/test_arcade_player_reset "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-player-input: test-arcade-attract-demo-equipment test-arcade-player-reset

.PHONY: test-arcade-player-new
test-arcade-player-new: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_player_new.c -o build/test_arcade_player_new
	build/test_arcade_player_new "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-attract-demo-prepare
test-arcade-attract-demo-prepare: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_attract_demo_prepare.c -o build/test_arcade_attract_demo_prepare
	build/test_arcade_attract_demo_prepare "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-player-input: test-arcade-player-new test-arcade-attract-demo-prepare

.PHONY: test-arcade-player-start
test-arcade-player-start: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_player_start.c -o build/test_arcade_player_start
	build/test_arcade_player_start "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-attract-demo-init
test-arcade-attract-demo-init: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_attract_demo_init.c -o build/test_arcade_attract_demo_init
	build/test_arcade_attract_demo_init "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-player-input: test-arcade-player-start test-arcade-attract-demo-init

.PHONY: test-arcade-stage-reset
test-arcade-stage-reset: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_stage_reset.c -o build/test_arcade_stage_reset
	build/test_arcade_stage_reset "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-player-input: test-arcade-stage-reset

.PHONY: test-arcade-stage-spawn
test-arcade-stage-spawn: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_stage_spawn.c -o build/test_arcade_stage_spawn
	build/test_arcade_stage_spawn "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-player-input: test-arcade-stage-spawn

.PHONY: test-arcade-hud-init
test-arcade-hud-init: build/reference_maincpu.bin native/intro_probe/src/player_data.h
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_hud_init.c -o build/test_arcade_hud_init
	build/test_arcade_hud_init "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-demo-statistics test-arcade-stage-objects-bus
test-arcade-demo-statistics: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_demo_statistics.c -o build/test_arcade_demo_statistics
	build/test_arcade_demo_statistics "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-arcade-stage-objects-bus: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_stage_objects_bus.c -o build/test_arcade_stage_objects_bus
	build/test_arcade_stage_objects_bus "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-attract-demo-start
test-arcade-attract-demo-start: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_attract_demo_start.c -o build/test_arcade_attract_demo_start
	build/test_arcade_attract_demo_start "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-gameplay-tick
test-arcade-gameplay-tick: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_gameplay_tick.c -o build/test_arcade_gameplay_tick
	build/test_arcade_gameplay_tick "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-stage-update
test-arcade-stage-update: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_stage_update.c -o build/test_arcade_stage_update
	build/test_arcade_stage_update "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-stage-object7
.PHONY: test-arcade-stage-object3 test-arcade-stage-object9 test-arcade-stage-object14 test-arcade-stage-update-mixed
test-arcade-stage-object3: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_stage_object3.c -o build/test_arcade_stage_object3
	build/test_arcade_stage_object3 "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-arcade-stage-object9: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_stage_object9.c -o build/test_arcade_stage_object9
	build/test_arcade_stage_object9 "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-arcade-stage-object14: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_stage_object14.c -o build/test_arcade_stage_object14
	build/test_arcade_stage_object14 "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-arcade-stage-update-mixed: test-arcade-stage-object3 test-arcade-stage-object9 test-arcade-stage-object14 test-arcade-stage-object15 test-arcade-stage-object20 test-arcade-stage-object21_22 test-arcade-stage-object0_alternate test-arcade-stage-object1_alternate test-arcade-stage-family6 test-arcade-stage-object25_32 test-arcade-stage-family12 test-arcade-stage-object27 test-arcade-stage-object26 test-arcade-stage-object31
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_stage_update_mixed.c -o build/test_arcade_stage_update_mixed
	build/test_arcade_stage_update_mixed "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-native-stage-object3
test-native-stage-object3: build/reference_maincpu.bin
	@test -n "$(SGDK_ROOT)" || (echo "Set SGDK_ROOT to an SGDK checkout"; exit 1)
	m68k-elf-gcc -m68000 -O2 -Wall -Wextra -Werror -fno-builtin -DSGDK_GCC -I"$(SGDK_ROOT)/inc" -nostdlib -Wl,-Ttext=0x110000,-e,stageObject3Probe tools/native_stage_object3_probe.c -o build/native_stage_object3_probe.elf
	m68k-elf-nm -n build/native_stage_object3_probe.elf > build/native_stage_object3_probe.sym
	m68k-elf-objcopy -O binary build/native_stage_object3_probe.elf build/native_stage_object3_probe.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_COMPILED_STAGE3 tools/test_arcade_stage_object3.c -o build/test_native_stage_object3
	build/test_native_stage_object3 "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-native-stage-object9
test-native-stage-object9: build/reference_maincpu.bin
	@test -n "$(SGDK_ROOT)" || (echo "Set SGDK_ROOT to an SGDK checkout"; exit 1)
	m68k-elf-gcc -m68000 -O2 -Wall -Wextra -Werror -fno-builtin -DSGDK_GCC -I"$(SGDK_ROOT)/inc" -nostdlib -Wl,-Ttext=0x110000,-e,stageObject9Probe tools/native_stage_object9_probe.c -o build/native_stage_object9_probe.elf
	m68k-elf-nm -n build/native_stage_object9_probe.elf > build/native_stage_object9_probe.sym
	m68k-elf-objcopy -O binary build/native_stage_object9_probe.elf build/native_stage_object9_probe.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_COMPILED_STAGE9 tools/test_arcade_stage_object9.c -o build/test_native_stage_object9
	build/test_native_stage_object9 "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-native-stage-object14
test-native-stage-object14: build/reference_maincpu.bin
	@test -n "$(SGDK_ROOT)" || (echo "Set SGDK_ROOT to an SGDK checkout"; exit 1)
	m68k-elf-gcc -m68000 -O2 -Wall -Wextra -Werror -fno-builtin -DSGDK_GCC -I"$(SGDK_ROOT)/inc" -nostdlib -Wl,-Ttext=0x110000,-e,stageObject14Probe tools/native_stage_object14_probe.c -o build/native_stage_object14_probe.elf
	m68k-elf-nm -n build/native_stage_object14_probe.elf > build/native_stage_object14_probe.sym
	m68k-elf-objcopy -O binary build/native_stage_object14_probe.elf build/native_stage_object14_probe.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_COMPILED_STAGE14 tools/test_arcade_stage_object14.c -o build/test_native_stage_object14
	build/test_native_stage_object14 "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-arcade-stage-object7: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_stage_object7.c -o build/test_arcade_stage_object7
	build/test_arcade_stage_object7 "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-stage-object1
test-arcade-stage-object1: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_stage_object1.c -o build/test_arcade_stage_object1
	build/test_arcade_stage_object1 "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-stage-object0 test-arcade-stage-update0
test-arcade-stage-object0: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_stage_object0.c -o build/test_arcade_stage_object0
	build/test_arcade_stage_object0 "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-arcade-stage-update0: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_stage_update0.c -o build/test_arcade_stage_update0
	build/test_arcade_stage_update0 "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-platform-contact
test-arcade-platform-contact: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_platform_contact.c -o build/test_arcade_platform_contact
	build/test_arcade_platform_contact "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-obstacle-contact
test-arcade-obstacle-contact: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_obstacle_contact.c -o build/test_arcade_obstacle_contact
	build/test_arcade_obstacle_contact "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-solid-contact test-arcade-contact-dispatch
test-arcade-solid-contact: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_solid_contact.c -o build/test_arcade_solid_contact
	build/test_arcade_solid_contact "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-arcade-contact-dispatch: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_contact_dispatch.c -o build/test_arcade_contact_dispatch
	build/test_arcade_contact_dispatch "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor-platforms
test-arcade-actor-platforms: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor_platforms.c -o build/test_arcade_actor_platforms
	build/test_arcade_actor_platforms "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-attack-contact
test-arcade-attack-contact: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_attack_contact.c -o build/test_arcade_attack_contact
	build/test_arcade_attack_contact "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-attack-dispatch
test-arcade-attack-dispatch: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_attack_dispatch.c -o build/test_arcade_attack_dispatch
	build/test_arcade_attack_dispatch "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-player-contact
test-arcade-player-contact: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_player_contact.c -o build/test_arcade_player_contact
	build/test_arcade_player_contact "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-contact-protection
test-arcade-contact-protection: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_contact_protection.c -o build/test_arcade_contact_protection
	build/test_arcade_contact_protection "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-player-contact-dispatch
test-arcade-player-contact-dispatch: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_player_contact_dispatch.c -o build/test_arcade_player_contact_dispatch
	build/test_arcade_player_contact_dispatch "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-enemy-player-contact
test-arcade-enemy-player-contact: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_enemy_player_contact.c -o build/test_arcade_enemy_player_contact
	build/test_arcade_enemy_player_contact "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-enemy-player-dispatch
test-arcade-enemy-player-dispatch: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_enemy_player_dispatch.c -o build/test_arcade_enemy_player_dispatch
	build/test_arcade_enemy_player_dispatch "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-secondary-player-dispatch
test-arcade-secondary-player-dispatch: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_secondary_player_dispatch.c -o build/test_arcade_secondary_player_dispatch
	build/test_arcade_secondary_player_dispatch "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor-obstacles
test-arcade-actor-obstacles: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor_obstacles.c -o build/test_arcade_actor_obstacles
	build/test_arcade_actor_obstacles "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-collision-manager
test-arcade-collision-manager: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_collision_manager.c -o build/test_arcade_collision_manager
	build/test_arcade_collision_manager "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-scroll3-controller
test-arcade-scroll3-controller: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_scroll3_controller.c -o build/test_arcade_scroll3_controller
	build/test_arcade_scroll3_controller "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-scroll2-controller
test-arcade-scroll2-controller: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_scroll2_controller.c -o build/test_arcade_scroll2_controller
	build/test_arcade_scroll2_controller "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-palette-scheduler
test-arcade-palette-scheduler: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_palette_scheduler.c -o build/test_arcade_palette_scheduler
	build/test_arcade_palette_scheduler "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-scroll2-start-controller
test-arcade-scroll2-start-controller: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_scroll2_start_controller.c -o build/test_arcade_scroll2_start_controller
	build/test_arcade_scroll2_start_controller "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-frame-scrolls
test-arcade-frame-scrolls: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_frame_scrolls.c -o build/test_arcade_frame_scrolls
	build/test_arcade_frame_scrolls "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-gameplay-frame
test-arcade-gameplay-frame: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_gameplay_frame.c -o build/test_arcade_gameplay_frame
	build/test_arcade_gameplay_frame "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor16-triggers
test-arcade-actor16-triggers: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor16_triggers.c -o build/test_arcade_actor16_triggers
	build/test_arcade_actor16_triggers "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-health-pickup
test-arcade-health-pickup: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_health_pickup.c -o build/test_arcade_health_pickup
	build/test_arcade_health_pickup "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-gold-pickup
test-arcade-gold-pickup: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_gold_pickup.c -o build/test_arcade_gold_pickup
	build/test_arcade_gold_pickup "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor16
test-arcade-actor16: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor16.c -o build/test_arcade_actor16
	build/test_arcade_actor16 "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor16-sequence
test-arcade-actor16-sequence: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor16_sequence.c -o build/test_arcade_actor16_sequence
	build/test_arcade_actor16_sequence "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-breakable-hits
test-arcade-breakable-hits: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_breakable_hits.c -o build/test_arcade_breakable_hits
	build/test_arcade_breakable_hits "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-breakable-object
test-arcade-breakable-object: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_breakable_object.c -o build/test_arcade_breakable_object
	build/test_arcade_breakable_object "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-hidden-breakable
test-arcade-hidden-breakable: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_hidden_breakable.c -o build/test_arcade_hidden_breakable
	build/test_arcade_hidden_breakable "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor16-attack
test-arcade-actor16-attack: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor16_attack.c -o build/test_arcade_actor16_attack
	build/test_arcade_actor16_attack "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor-floor
test-arcade-actor-floor: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor_floor.c -o build/test_arcade_actor_floor
	build/test_arcade_actor_floor "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor-landing
test-arcade-actor-landing: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor_landing.c -o build/test_arcade_actor_landing
	build/test_arcade_actor_landing "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor16-zero
test-arcade-actor16-zero: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor16_zero.c -o build/test_arcade_actor16_zero
	build/test_arcade_actor16_zero "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-pickup-landing
test-arcade-pickup-landing: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_pickup_landing.c -o build/test_arcade_pickup_landing
	build/test_arcade_pickup_landing "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor14-pickups
test-arcade-actor14-pickups: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor14_pickups.c -o build/test_arcade_actor14_pickups
	build/test_arcade_actor14_pickups "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor14
test-arcade-actor14: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor14.c -o build/test_arcade_actor14
	build/test_arcade_actor14 "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-transition-services
test-arcade-transition-services: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_transition_services.c -o build/test_arcade_transition_services
	build/test_arcade_transition_services "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-transition-destination
test-arcade-transition-destination: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_transition_destination.c -o build/test_arcade_transition_destination
	build/test_arcade_transition_destination "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-transition-admission
test-arcade-transition-admission: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_transition_admission.c -o build/test_arcade_transition_admission
	build/test_arcade_transition_admission "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-transition-actor
test-arcade-transition-actor: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_transition_actor.c -o build/test_arcade_transition_actor
	build/test_arcade_transition_actor "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor14-resume
test-arcade-actor14-resume: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor14_resume.c -o build/test_arcade_actor14_resume
	build/test_arcade_actor14_resume "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-transition-presentation
test-arcade-transition-presentation: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_transition_presentation.c -o build/test_arcade_transition_presentation
	build/test_arcade_transition_presentation "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor14-presentation
test-arcade-actor14-presentation: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor14_presentation.c -o build/test_arcade_actor14_presentation
	build/test_arcade_actor14_presentation "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor14-fade-scheduler
test-arcade-actor14-fade-scheduler: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor14_fade_scheduler.c -o build/test_arcade_actor14_fade_scheduler
	@for region in 0 1 2 3 4; do build/test_arcade_actor14_fade_scheduler "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" $$region || exit $$?; done

.PHONY: test-arcade-actor18-entry
test-arcade-actor18-entry: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor18_entry.c -o build/test_arcade_actor18_entry
	build/test_arcade_actor18_entry "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor18-active
test-arcade-actor18-active: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor18_active.c -o build/test_arcade_actor18_active
	build/test_arcade_actor18_active "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor18-death
test-arcade-actor18-death: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor18_death.c -o build/test_arcade_actor18_death
	build/test_arcade_actor18_death "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor18-zero
test-arcade-actor18-zero: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor18_zero.c -o build/test_arcade_actor18_zero
	build/test_arcade_actor18_zero "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor18-two-entry
test-arcade-actor18-two-entry: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor18_two_entry.c -o build/test_arcade_actor18_two_entry
	build/test_arcade_actor18_two_entry "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-native-message-panel
test-native-message-panel: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_native_message_panel.c -o build/test_native_message_panel
	build/test_native_message_panel "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-native-message-panel-task
test-native-message-panel-task: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_native_message_panel_task.c -o build/test_native_message_panel_task
	build/test_native_message_panel_task "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-native-message-panel-scheduler
test-native-message-panel-scheduler: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_native_message_panel_scheduler.c -o build/test_native_message_panel_scheduler
	build/test_native_message_panel_scheduler "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-native-message-sound-scheduler
test-native-message-sound-scheduler: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_native_message_sound_scheduler.c -o build/test_native_message_sound_scheduler
	build/test_native_message_sound_scheduler "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor18-two-entrance
test-arcade-actor18-two-entrance: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor18_two_entrance.c -o build/test_arcade_actor18_two_entrance
	build/test_arcade_actor18_two_entrance "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor-direction
test-arcade-actor-direction: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor_direction.c -o build/test_arcade_actor_direction
	build/test_arcade_actor_direction "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor18-two-motion
test-arcade-actor18-two-motion: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor18_two_motion.c -o build/test_arcade_actor18_two_motion
	build/test_arcade_actor18_two_motion "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor18-two-attack
test-arcade-actor18-two-attack: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor18_two_attack.c -o build/test_arcade_actor18_two_attack
	build/test_arcade_actor18_two_attack "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor18-two-route-state
test-arcade-actor18-two-route-state: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor18_two_route_state.c -o build/test_arcade_actor18_two_route_state
	build/test_arcade_actor18_two_route_state "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor18-two-bob
test-arcade-actor18-two-bob: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor18_two_bob.c -o build/test_arcade_actor18_two_bob
	build/test_arcade_actor18_two_bob "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor18-two-active
test-arcade-actor18-two-active: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor18_two_active.c -o build/test_arcade_actor18_two_active
	build/test_arcade_actor18_two_active "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor18-two-death
test-arcade-actor18-two-death: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor18_two_death.c -o build/test_arcade_actor18_two_death
	build/test_arcade_actor18_two_death "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor18-two-lifecycle
test-arcade-actor18-two-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor18_two_lifecycle.c -o build/test_arcade_actor18_two_lifecycle
	build/test_arcade_actor18_two_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor18-three-entry
test-arcade-actor18-three-entry: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor18_three_entry.c -o build/test_arcade_actor18_three_entry
	build/test_arcade_actor18_three_entry "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor18-three-health
test-arcade-actor18-three-health: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor18_three_health.c -o build/test_arcade_actor18_three_health
	build/test_arcade_actor18_three_health "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor18-three-decision
test-arcade-actor18-three-decision: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor18_three_decision.c -o build/test_arcade_actor18_three_decision
	build/test_arcade_actor18_three_decision "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor18-three-motion
test-arcade-actor18-three-motion: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor18_three_motion.c -o build/test_arcade_actor18_three_motion
	build/test_arcade_actor18_three_motion "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor18-three-effects
test-arcade-actor18-three-effects: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor18_three_effects.c -o build/test_arcade_actor18_three_effects
	build/test_arcade_actor18_three_effects "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor18-three-attack
test-arcade-actor18-three-attack: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor18_three_attack.c -o build/test_arcade_actor18_three_attack
	build/test_arcade_actor18_three_attack "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor-forward-floor
test-arcade-actor-forward-floor: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor_forward_floor.c -o build/test_arcade_actor_forward_floor
	build/test_arcade_actor_forward_floor "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor18-three-jump
test-arcade-actor18-three-jump: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor18_three_jump.c -o build/test_arcade_actor18_three_jump
	build/test_arcade_actor18_three_jump "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor18-three-active
test-arcade-actor18-three-active: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor18_three_active.c -o build/test_arcade_actor18_three_active
	build/test_arcade_actor18_three_active "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor18-three-death
test-arcade-actor18-three-death: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor18_three_death.c -o build/test_arcade_actor18_three_death
	build/test_arcade_actor18_three_death "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor18-three
test-arcade-actor18-three: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor18_three.c -o build/test_arcade_actor18_three
	build/test_arcade_actor18_three "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor18-three-lifecycle
test-arcade-actor18-three-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor18_three_lifecycle.c -o build/test_arcade_actor18_three_lifecycle
	build/test_arcade_actor18_three_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor18-four-entry
test-arcade-actor18-four-entry: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor18_four_entry.c -o build/test_arcade_actor18_four_entry
	build/test_arcade_actor18_four_entry "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor18-four-health
test-arcade-actor18-four-health: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor18_four_health.c -o build/test_arcade_actor18_four_health
	build/test_arcade_actor18_four_health "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor18-four-steering
test-arcade-actor18-four-steering: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor18_four_steering.c -o build/test_arcade_actor18_four_steering
	build/test_arcade_actor18_four_steering "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor18-four-patrol
test-arcade-actor18-four-patrol: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor18_four_patrol.c -o build/test_arcade_actor18_four_patrol
	build/test_arcade_actor18_four_patrol "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor18-four-target
test-arcade-actor18-four-target: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor18_four_target.c -o build/test_arcade_actor18_four_target
	build/test_arcade_actor18_four_target "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor18-four-actions
test-arcade-actor18-four-actions: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor18_four_actions.c -o build/test_arcade_actor18_four_actions
	build/test_arcade_actor18_four_actions "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor18-four-cast
test-arcade-actor18-four-cast: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor18_four_cast.c -o build/test_arcade_actor18_four_cast
	build/test_arcade_actor18_four_cast "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor18-four-enraged
test-arcade-actor18-four-enraged: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor18_four_enraged.c -o build/test_arcade_actor18_four_enraged
	build/test_arcade_actor18_four_enraged "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor18-four-hover
test-arcade-actor18-four-hover: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor18_four_hover.c -o build/test_arcade_actor18_four_hover
	build/test_arcade_actor18_four_hover "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor18-four-ground
test-arcade-actor18-four-ground: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor18_four_ground.c -o build/test_arcade_actor18_four_ground
	build/test_arcade_actor18_four_ground "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor18-four-ranged
test-arcade-actor18-four-ranged: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor18_four_ranged.c -o build/test_arcade_actor18_four_ranged
	build/test_arcade_actor18_four_ranged "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor18-four-flight
test-arcade-actor18-four-flight: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor18_four_flight.c -o build/test_arcade_actor18_four_flight
	build/test_arcade_actor18_four_flight "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor18-four-active
test-arcade-actor18-four-active: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor18_four_active.c -o build/test_arcade_actor18_four_active
	build/test_arcade_actor18_four_active "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor18-four-death
test-arcade-actor18-four-death: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor18_four_death.c -o build/test_arcade_actor18_four_death
	build/test_arcade_actor18_four_death "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor18-four
test-arcade-actor18-four: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor18_four.c -o build/test_arcade_actor18_four
	build/test_arcade_actor18_four "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor18-four-lifecycle
test-arcade-actor18-four-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor18_four_lifecycle.c -o build/test_arcade_actor18_four_lifecycle
	build/test_arcade_actor18_four_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor18-five-entry
test-arcade-actor18-five-entry: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor18_five_entry.c -o build/test_arcade_actor18_five_entry
	build/test_arcade_actor18_five_entry "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor18-five-children
test-arcade-actor18-five-children: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor18_five_children.c -o build/test_arcade_actor18_five_children
	build/test_arcade_actor18_five_children "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor18-five-active
test-arcade-actor18-five-active: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor18_five_active.c -o build/test_arcade_actor18_five_active
	build/test_arcade_actor18_five_active "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor18-five-death
test-arcade-actor18-five-death: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor18_five_death.c -o build/test_arcade_actor18_five_death
	build/test_arcade_actor18_five_death "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor18-five
test-arcade-actor18-five: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor18_five.c -o build/test_arcade_actor18_five
	build/test_arcade_actor18_five "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor18-five-lifecycle
test-arcade-actor18-five-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor18_five_lifecycle.c -o build/test_arcade_actor18_five_lifecycle
	build/test_arcade_actor18_five_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor18-six-setup
test-arcade-actor18-six-setup: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor18_six_setup.c -o build/test_arcade_actor18_six_setup
	build/test_arcade_actor18_six_setup "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor18-six-tiles
test-arcade-actor18-six-tiles: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor18_six_tiles.c -o build/test_arcade_actor18_six_tiles
	build/test_arcade_actor18_six_tiles "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor18-six-motion
test-arcade-actor18-six-motion: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor18_six_motion.c -o build/test_arcade_actor18_six_motion
	build/test_arcade_actor18_six_motion "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor18-six-active
test-arcade-actor18-six-active: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor18_six_active.c -o build/test_arcade_actor18_six_active
	build/test_arcade_actor18_six_active "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor18-six-zero
test-arcade-actor18-six-zero: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor18_six_zero.c -o build/test_arcade_actor18_six_zero
	build/test_arcade_actor18_six_zero "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor18-six-one
test-arcade-actor18-six-one: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor18_six_one.c -o build/test_arcade_actor18_six_one
	build/test_arcade_actor18_six_one "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor18-six-two-entry
test-arcade-actor18-six-two-entry: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor18_six_two_entry.c -o build/test_arcade_actor18_six_two_entry
	build/test_arcade_actor18_six_two_entry "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor18-six-two-active
test-arcade-actor18-six-two-active: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor18_six_two_active.c -o build/test_arcade_actor18_six_two_active
	build/test_arcade_actor18_six_two_active "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor18-six-two-death
test-arcade-actor18-six-two-death: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor18_six_two_death.c -o build/test_arcade_actor18_six_two_death
	build/test_arcade_actor18_six_two_death "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor18-six-two
test-arcade-actor18-six-two: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor18_six_two.c -o build/test_arcade_actor18_six_two
	build/test_arcade_actor18_six_two "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor18-six-three
test-arcade-actor18-six-three: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor18_six_three.c -o build/test_arcade_actor18_six_three
	build/test_arcade_actor18_six_three "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor18-six
test-arcade-actor18-six: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -DWILLOW_TEST_SIX_DISPATCH -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor18_six_zero.c -o build/test_arcade_actor18_six_dispatch_zero
	build/test_arcade_actor18_six_dispatch_zero "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"
	clang -Wall -Wextra -Werror -O2 -DWILLOW_TEST_SIX_DISPATCH -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor18_six_one.c -o build/test_arcade_actor18_six_dispatch_one
	build/test_arcade_actor18_six_dispatch_one "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"
	clang -Wall -Wextra -Werror -O2 -DWILLOW_TEST_SIX_DISPATCH -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor18_six_two.c -o build/test_arcade_actor18_six_dispatch_two
	build/test_arcade_actor18_six_dispatch_two "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"
	clang -Wall -Wextra -Werror -O2 -DWILLOW_TEST_SIX_DISPATCH -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor18_six_three.c -o build/test_arcade_actor18_six_dispatch_three
	build/test_arcade_actor18_six_dispatch_three "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor18-six-lifecycle
test-arcade-actor18-six-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor18_six_lifecycle.c -o build/test_arcade_actor18_six_lifecycle
	build/test_arcade_actor18_six_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor18-pool
test-arcade-actor18-pool: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -DWILLOW_TEST_ACTOR18_POOL -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor18_five_lifecycle.c -o build/test_arcade_actor18_pool_five
	build/test_arcade_actor18_pool_five "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"
	clang -Wall -Wextra -Werror -O2 -DWILLOW_TEST_ACTOR18_POOL -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor18_six_lifecycle.c -o build/test_arcade_actor18_pool_six
	build/test_arcade_actor18_pool_six "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor12-explosion
test-arcade-actor12-explosion: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor12_explosion.c -o build/test_arcade_actor12_explosion
	build/test_arcade_actor12_explosion "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-boss-effects
test-arcade-boss-effects: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -DWILLOW_TEST_ACTOR18_POOL -DWILLOW_TEST_BOSS_EFFECTS -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor18_five_lifecycle.c -o build/test_arcade_boss_effects_five
	build/test_arcade_boss_effects_five "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"
	clang -Wall -Wextra -Werror -O2 -DWILLOW_TEST_ACTOR18_POOL -DWILLOW_TEST_BOSS_EFFECTS -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor18_six_lifecycle.c -o build/test_arcade_boss_effects_six
	build/test_arcade_boss_effects_six "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor12-one
test-arcade-actor12-one: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor12_one.c -o build/test_arcade_actor12_one
	build/test_arcade_actor12_one "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor12-three-zero
test-arcade-actor12-three-zero: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor12_three_zero.c -o build/test_arcade_actor12_three_zero
	build/test_arcade_actor12_three_zero "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor12-three
test-arcade-actor12-three: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor12_three.c -o build/test_arcade_actor12_three
	build/test_arcade_actor12_three "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor12-presentation
test-arcade-actor12-presentation: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor12_presentation.c -o build/test_arcade_actor12_presentation
	build/test_arcade_actor12_presentation "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor12-four
test-arcade-actor12-four: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor12_four.c -o build/test_arcade_actor12_four
	build/test_arcade_actor12_four "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor12-five
test-arcade-actor12-five: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor12_five.c -o build/test_arcade_actor12_five
	build/test_arcade_actor12_five "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor12-six-zero
test-arcade-actor12-six-zero: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor12_six_zero.c -o build/test_arcade_actor12_six_zero
	build/test_arcade_actor12_six_zero "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor12-six-trigger
test-arcade-actor12-six-trigger: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor12_six_trigger.c -o build/test_arcade_actor12_six_trigger
	build/test_arcade_actor12_six_trigger "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor12-six-falling
test-arcade-actor12-six-falling: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor12_six_falling.c -o build/test_arcade_actor12_six_falling
	build/test_arcade_actor12_six_falling "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor12-six-lifecycle
test-arcade-actor12-six-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor12_six_lifecycle.c -o build/test_arcade_actor12_six_lifecycle
	build/test_arcade_actor12_six_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor12-six-tiles
test-arcade-actor12-six-tiles: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor12_six_tiles.c -o build/test_arcade_actor12_six_tiles
	build/test_arcade_actor12_six_tiles "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor12-six-eight
test-arcade-actor12-six-eight: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor12_six_eight.c -o build/test_arcade_actor12_six_eight
	build/test_arcade_actor12_six_eight "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor12-seven
test-arcade-actor12-seven: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor12_seven.c -o build/test_arcade_actor12_seven
	build/test_arcade_actor12_seven "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor12-seven-lifecycle
test-arcade-actor12-seven-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor12_seven_lifecycle.c -o build/test_arcade_actor12_seven_lifecycle
	build/test_arcade_actor12_seven_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor12-eight
test-arcade-actor12-eight: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor12_eight.c -o build/test_arcade_actor12_eight
	build/test_arcade_actor12_eight "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor12-eight-lifecycle
test-arcade-actor12-eight-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor12_eight_lifecycle.c -o build/test_arcade_actor12_eight_lifecycle
	build/test_arcade_actor12_eight_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor12-ten
test-arcade-actor12-ten: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor12_ten.c -o build/test_arcade_actor12_ten
	build/test_arcade_actor12_ten "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor12-ten-lifecycle
test-arcade-actor12-ten-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor12_ten_lifecycle.c -o build/test_arcade_actor12_ten_lifecycle
	build/test_arcade_actor12_ten_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor12-twelve
test-arcade-actor12-twelve: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor12_twelve.c -o build/test_arcade_actor12_twelve
	build/test_arcade_actor12_twelve "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor12-twelve-lifecycle
test-arcade-actor12-twelve-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor12_twelve_lifecycle.c -o build/test_arcade_actor12_twelve_lifecycle
	build/test_arcade_actor12_twelve_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor12-eleven
test-arcade-actor12-eleven: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor12_eleven.c -o build/test_arcade_actor12_eleven
	build/test_arcade_actor12_eleven "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor12-eleven-lifecycle
test-arcade-actor12-eleven-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor12_eleven_lifecycle.c -o build/test_arcade_actor12_eleven_lifecycle
	build/test_arcade_actor12_eleven_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor12-thirteen
test-arcade-actor12-thirteen: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor12_thirteen.c -o build/test_arcade_actor12_thirteen
	build/test_arcade_actor12_thirteen "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor12-thirteen-lifecycle
test-arcade-actor12-thirteen-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor12_thirteen_lifecycle.c -o build/test_arcade_actor12_thirteen_lifecycle
	build/test_arcade_actor12_thirteen_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor12-fourteen
test-arcade-actor12-fourteen: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor12_fourteen.c -o build/test_arcade_actor12_fourteen
	build/test_arcade_actor12_fourteen "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor12-fourteen-lifecycle
test-arcade-actor12-fourteen-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor12_fourteen_lifecycle.c -o build/test_arcade_actor12_fourteen_lifecycle
	build/test_arcade_actor12_fourteen_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor12-fifteen
test-arcade-actor12-fifteen: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor12_fifteen.c -o build/test_arcade_actor12_fifteen
	build/test_arcade_actor12_fifteen "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor12-fifteen-lifecycle
test-arcade-actor12-fifteen-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor12_fifteen_lifecycle.c -o build/test_arcade_actor12_fifteen_lifecycle
	build/test_arcade_actor12_fifteen_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor12-timed
test-arcade-actor12-timed: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor12_timed.c -o build/test_arcade_actor12_timed
	build/test_arcade_actor12_timed "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor12-timed-lifecycle
test-arcade-actor12-timed-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor12_timed_lifecycle.c -o build/test_arcade_actor12_timed_lifecycle
	build/test_arcade_actor12_timed_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor12-spray
test-arcade-actor12-spray: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor12_spray.c -o build/test_arcade_actor12_spray
	build/test_arcade_actor12_spray "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor12-spray-lifecycle
test-arcade-actor12-spray-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor12_spray_lifecycle.c -o build/test_arcade_actor12_spray_lifecycle
	build/test_arcade_actor12_spray_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor12-markers
test-arcade-actor12-markers: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor12_markers.c -o build/test_arcade_actor12_markers
	build/test_arcade_actor12_markers "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor12-markers-lifecycle
test-arcade-actor12-markers-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor12_markers_lifecycle.c -o build/test_arcade_actor12_markers_lifecycle
	build/test_arcade_actor12_markers_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor12-twenty-two
test-arcade-actor12-twenty-two: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor12_twenty_two.c -o build/test_arcade_actor12_twenty_two
	build/test_arcade_actor12_twenty_two "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-stage-marker-lifecycle
test-arcade-stage-marker-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_stage_marker_lifecycle.c -o build/test_arcade_stage_marker_lifecycle
	build/test_arcade_stage_marker_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor12-burst
test-arcade-actor12-burst: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor12_burst.c -o build/test_arcade_actor12_burst
	build/test_arcade_actor12_burst "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor12-burst-lifecycle
test-arcade-actor12-burst-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor12_burst_lifecycle.c -o build/test_arcade_actor12_burst_lifecycle
	build/test_arcade_actor12_burst_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor12-twenty-five
test-arcade-actor12-twenty-five: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor12_twenty_five.c -o build/test_arcade_actor12_twenty_five
	build/test_arcade_actor12_twenty_five "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor12-twenty-five-lifecycle
test-arcade-actor12-twenty-five-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor12_twenty_five_lifecycle.c -o build/test_arcade_actor12_twenty_five_lifecycle
	build/test_arcade_actor12_twenty_five_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor12-twenty-seven
test-arcade-actor12-twenty-seven: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor12_twenty_seven.c -o build/test_arcade_actor12_twenty_seven
	build/test_arcade_actor12_twenty_seven "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor12-scripted-pair-lifecycle
test-arcade-actor12-scripted-pair-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor12_scripted_pair_lifecycle.c -o build/test_arcade_actor12_scripted_pair_lifecycle
	build/test_arcade_actor12_scripted_pair_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-scripted-effects-items
test-arcade-scripted-effects-items: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_scripted_effects_items.c -o build/test_arcade_scripted_effects_items
	build/test_arcade_scripted_effects_items "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor12-presentation-pair
test-arcade-actor12-presentation-pair: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor12_presentation_pair.c -o build/test_arcade_actor12_presentation_pair
	build/test_arcade_actor12_presentation_pair "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor12-presentation-pair-lifecycle
test-arcade-actor12-presentation-pair-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor12_presentation_pair_lifecycle.c -o build/test_arcade_actor12_presentation_pair_lifecycle
	build/test_arcade_actor12_presentation_pair_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor12-selection
test-arcade-actor12-selection: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor12_selection.c -o build/test_arcade_actor12_selection
	build/test_arcade_actor12_selection "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor12-selection-lifecycle
test-arcade-actor12-selection-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor12_selection_lifecycle.c -o build/test_arcade_actor12_selection_lifecycle
	build/test_arcade_actor12_selection_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor12-thirty-five
test-arcade-actor12-thirty-five: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor12_thirty_five.c -o build/test_arcade_actor12_thirty_five
	build/test_arcade_actor12_thirty_five "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor12-thirty-five-boundaries
test-arcade-actor12-thirty-five-boundaries: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor12_thirty_five_boundaries.c -o build/test_arcade_actor12_thirty_five_boundaries
	build/test_arcade_actor12_thirty_five_boundaries "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor12-segments
test-arcade-actor12-segments: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor12_segments.c -o build/test_arcade_actor12_segments
	build/test_arcade_actor12_segments "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor12-segments-lifecycle
test-arcade-actor12-segments-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor12_segments_lifecycle.c -o build/test_arcade_actor12_segments_lifecycle
	build/test_arcade_actor12_segments_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor12-thirty-eight
test-arcade-actor12-thirty-eight: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor12_thirty_eight.c -o build/test_arcade_actor12_thirty_eight
	build/test_arcade_actor12_thirty_eight "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor12-thirty-eight-lifecycle
test-arcade-actor12-thirty-eight-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor12_thirty_eight_lifecycle.c -o build/test_arcade_actor12_thirty_eight_lifecycle
	build/test_arcade_actor12_thirty_eight_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor12-thirty-nine
test-arcade-actor12-thirty-nine: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor12_thirty_nine.c -o build/test_arcade_actor12_thirty_nine
	build/test_arcade_actor12_thirty_nine "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor12-thirty-nine-lifecycle
test-arcade-actor12-thirty-nine-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor12_thirty_nine_lifecycle.c -o build/test_arcade_actor12_thirty_nine_lifecycle
	build/test_arcade_actor12_thirty_nine_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor12-short-effects
test-arcade-actor12-short-effects: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor12_short_effects.c -o build/test_arcade_actor12_short_effects
	build/test_arcade_actor12_short_effects "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor12-short-effects-lifecycle
test-arcade-actor12-short-effects-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor12_short_effects_lifecycle.c -o build/test_arcade_actor12_short_effects_lifecycle
	build/test_arcade_actor12_short_effects_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor12-tail
test-arcade-actor12-tail: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor12_tail.c -o build/test_arcade_actor12_tail
	build/test_arcade_actor12_tail "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor12-tail-lifecycle
test-arcade-actor12-tail-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor12_tail_lifecycle.c -o build/test_arcade_actor12_tail_lifecycle
	build/test_arcade_actor12_tail_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor6-one
test-arcade-actor6-one: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor6_one.c -o build/test_arcade_actor6_one
	build/test_arcade_actor6_one "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor6-one-lifecycle
test-arcade-actor6-one-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor6_one_lifecycle.c -o build/test_arcade_actor6_one_lifecycle
	build/test_arcade_actor6_one_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"


.PHONY: test-arcade-actor6-zero
test-arcade-actor6-zero: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor6_zero.c -o build/test_arcade_actor6_zero
	build/test_arcade_actor6_zero "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"


.PHONY: test-arcade-actor6-zero-lifecycle
test-arcade-actor6-zero-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor6_zero_lifecycle.c -o build/test_arcade_actor6_zero_lifecycle
	build/test_arcade_actor6_zero_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"



.PHONY: test-arcade-actor6-two
test-arcade-actor6-two: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor6_two.c -o build/test_arcade_actor6_two
	build/test_arcade_actor6_two "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"



.PHONY: test-arcade-actor6-two-lifecycle
test-arcade-actor6-two-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor6_two_lifecycle.c -o build/test_arcade_actor6_two_lifecycle
	build/test_arcade_actor6_two_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"




.PHONY: test-arcade-actor6-oscillation
test-arcade-actor6-oscillation: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor6_oscillation.c -o build/test_arcade_actor6_oscillation
	build/test_arcade_actor6_oscillation "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"


.PHONY: test-arcade-actor6-oscillation-lifecycle
test-arcade-actor6-oscillation-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor6_oscillation_lifecycle.c -o build/test_arcade_actor6_oscillation_lifecycle
	build/test_arcade_actor6_oscillation_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"


.PHONY: test-arcade-actor6-three
test-arcade-actor6-three: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor6_three.c -o build/test_arcade_actor6_three
	build/test_arcade_actor6_three "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"




.PHONY: test-arcade-actor6-three-lifecycle
test-arcade-actor6-three-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor6_three_lifecycle.c -o build/test_arcade_actor6_three_lifecycle
	build/test_arcade_actor6_three_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"



.PHONY: test-arcade-actor-slope-floor
test-arcade-actor-slope-floor: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor_slope_floor.c -o build/test_arcade_actor_slope_floor
	build/test_arcade_actor_slope_floor "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"





.PHONY: test-arcade-actor6-four-speed
test-arcade-actor6-four-speed: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor6_four_speed.c -o build/test_arcade_actor6_four_speed
	build/test_arcade_actor6_four_speed "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"





.PHONY: test-arcade-actor14-scene-spawn
test-arcade-actor14-scene-spawn: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor14_scene_spawn.c -o build/test_arcade_actor14_scene_spawn
	build/test_arcade_actor14_scene_spawn "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"


.PHONY: test-arcade-actor14-scene-spawn-lifecycle
test-arcade-actor14-scene-spawn-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor14_scene_spawn_lifecycle.c -o build/test_arcade_actor14_scene_spawn_lifecycle
	build/test_arcade_actor14_scene_spawn_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"



.PHONY: test-arcade-actor6-four
test-arcade-actor6-four: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor6_four.c -o build/test_arcade_actor6_four
	build/test_arcade_actor6_four "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"





.PHONY: test-arcade-actor6-four-lifecycle
test-arcade-actor6-four-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor6_four_lifecycle.c -o build/test_arcade_actor6_four_lifecycle
	build/test_arcade_actor6_four_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"




.PHONY: test-arcade-actor6-five
test-arcade-actor6-five: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor6_five.c -o build/test_arcade_actor6_five
	build/test_arcade_actor6_five "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"





.PHONY: test-arcade-actor6-five-lifecycle
test-arcade-actor6-five-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor6_five_lifecycle.c -o build/test_arcade_actor6_five_lifecycle
	build/test_arcade_actor6_five_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"



.PHONY: test-arcade-actor6-six
test-arcade-actor6-six: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor6_six.c -o build/test_arcade_actor6_six
	build/test_arcade_actor6_six "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"


.PHONY: test-arcade-actor6-six-lifecycle
test-arcade-actor6-six-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor6_six_lifecycle.c -o build/test_arcade_actor6_six_lifecycle
	build/test_arcade_actor6_six_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"



.PHONY: test-arcade-actor6-seven
test-arcade-actor6-seven: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor6_seven.c -o build/test_arcade_actor6_seven
	build/test_arcade_actor6_seven "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"





.PHONY: test-arcade-actor6-seven-lifecycle
test-arcade-actor6-seven-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor6_seven_lifecycle.c -o build/test_arcade_actor6_seven_lifecycle
	build/test_arcade_actor6_seven_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"



.PHONY: test-arcade-actor6-eight
test-arcade-actor6-eight: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor6_eight.c -o build/test_arcade_actor6_eight
	build/test_arcade_actor6_eight "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"





.PHONY: test-arcade-actor6-eight-lifecycle
test-arcade-actor6-eight-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor6_eight_lifecycle.c -o build/test_arcade_actor6_eight_lifecycle
	build/test_arcade_actor6_eight_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor6-nine
test-arcade-actor6-nine: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor6_nine.c -o build/test_arcade_actor6_nine
	build/test_arcade_actor6_nine "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor6-nine-lifecycle
test-arcade-actor6-nine-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor6_nine_lifecycle.c -o build/test_arcade_actor6_nine_lifecycle
	build/test_arcade_actor6_nine_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor6-ten
test-arcade-actor6-ten: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor6_ten.c -o build/test_arcade_actor6_ten
	build/test_arcade_actor6_ten "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor6-ten-lifecycle
test-arcade-actor6-ten-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor6_ten_lifecycle.c -o build/test_arcade_actor6_ten_lifecycle
	build/test_arcade_actor6_ten_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor2
test-arcade-actor2: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor2.c -o build/test_arcade_actor2
	build/test_arcade_actor2 "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor2-lifecycle
test-arcade-actor2-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor2_lifecycle.c -o build/test_arcade_actor2_lifecycle
	build/test_arcade_actor2_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor8-one
test-arcade-actor8-one: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor8_one.c -o build/test_arcade_actor8_one
	build/test_arcade_actor8_one "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor8-one-lifecycle
test-arcade-actor8-one-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor8_one_lifecycle.c -o build/test_arcade_actor8_one_lifecycle
	build/test_arcade_actor8_one_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor8-two-three
test-arcade-actor8-two-three: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor8_two_three.c -o build/test_arcade_actor8_two_three
	build/test_arcade_actor8_two_three "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor8-two-three-lifecycle
test-arcade-actor8-two-three-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor8_two_three_lifecycle.c -o build/test_arcade_actor8_two_three_lifecycle
	build/test_arcade_actor8_two_three_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor8-signals
test-arcade-actor8-signals: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor8_signals.c -o build/test_arcade_actor8_signals
	build/test_arcade_actor8_signals "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor8-signals-lifecycle
test-arcade-actor8-signals-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor8_signals_lifecycle.c -o build/test_arcade_actor8_signals_lifecycle
	build/test_arcade_actor8_signals_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor8-five
test-arcade-actor8-five: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor8_five.c -o build/test_arcade_actor8_five
	build/test_arcade_actor8_five "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor8-five-lifecycle
test-arcade-actor8-five-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor8_five_lifecycle.c -o build/test_arcade_actor8_five_lifecycle
	build/test_arcade_actor8_five_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor8-seven
test-arcade-actor8-seven: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor8_seven.c -o build/test_arcade_actor8_seven
	build/test_arcade_actor8_seven "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor8-seven-lifecycle
test-arcade-actor8-seven-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor8_seven_lifecycle.c -o build/test_arcade_actor8_seven_lifecycle
	build/test_arcade_actor8_seven_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor8-eight
test-arcade-actor8-eight: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor8_eight.c -o build/test_arcade_actor8_eight
	build/test_arcade_actor8_eight "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor8-eight-lifecycle
test-arcade-actor8-eight-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor8_eight_lifecycle.c -o build/test_arcade_actor8_eight_lifecycle
	build/test_arcade_actor8_eight_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-projectile-targets
test-arcade-projectile-targets: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_projectile_targets.c -o build/test_arcade_projectile_targets
	build/test_arcade_projectile_targets "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor8-zero
test-arcade-actor8-zero: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor8_zero.c -o build/test_arcade_actor8_zero
	build/test_arcade_actor8_zero "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor8-zero-lifecycle
test-arcade-actor8-zero-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor8_zero_lifecycle.c -o build/test_arcade_actor8_zero_lifecycle
	build/test_arcade_actor8_zero_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-enemy-platforms
test-arcade-enemy-platforms: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_enemy_platforms.c -o build/test_arcade_enemy_platforms
	build/test_arcade_enemy_platforms "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor-terrain-contact
test-arcade-actor-terrain-contact: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor_terrain_contact.c -o build/test_arcade_actor_terrain_contact
	build/test_arcade_actor_terrain_contact "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor10-zero
test-arcade-actor10-zero: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor10_zero.c -o build/test_arcade_actor10_zero
	build/test_arcade_actor10_zero "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor10-zero-lifecycle
test-arcade-actor10-zero-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor10_zero_lifecycle.c -o build/test_arcade_actor10_zero_lifecycle
	build/test_arcade_actor10_zero_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor10-effect
test-arcade-actor10-effect: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor10_effect.c -o build/test_arcade_actor10_effect
	build/test_arcade_actor10_effect "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor10-pool-lifecycle
test-arcade-actor10-pool-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor10_pool_lifecycle.c -o build/test_arcade_actor10_pool_lifecycle
	build/test_arcade_actor10_pool_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor10-one-two
test-arcade-actor10-one-two: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor10_one_two.c -o build/test_arcade_actor10_one_two
	build/test_arcade_actor10_one_two "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor10-one-two-lifecycle
test-arcade-actor10-one-two-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor10_one_two_lifecycle.c -o build/test_arcade_actor10_one_two_lifecycle
	build/test_arcade_actor10_one_two_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor10-three
test-arcade-actor10-three: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor10_three.c -o build/test_arcade_actor10_three
	build/test_arcade_actor10_three "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor10-three-lifecycle
test-arcade-actor10-three-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor10_three_lifecycle.c -o build/test_arcade_actor10_three_lifecycle
	build/test_arcade_actor10_three_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor-support
test-arcade-actor-support: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor_support.c -o build/test_arcade_actor_support
	build/test_arcade_actor_support "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor10-five
test-arcade-actor10-five: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor10_five.c -o build/test_arcade_actor10_five
	build/test_arcade_actor10_five "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor10-five-lifecycle
test-arcade-actor10-five-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor10_five_lifecycle.c -o build/test_arcade_actor10_five_lifecycle
	build/test_arcade_actor10_five_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor10-six
test-arcade-actor10-six: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor10_six.c -o build/test_arcade_actor10_six
	build/test_arcade_actor10_six "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor10-six-lifecycle
test-arcade-actor10-six-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor10_six_lifecycle.c -o build/test_arcade_actor10_six_lifecycle
	build/test_arcade_actor10_six_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor10-seven
test-arcade-actor10-seven: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor10_seven.c -o build/test_arcade_actor10_seven
	build/test_arcade_actor10_seven "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor10-seven-lifecycle
test-arcade-actor10-seven-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor10_seven_lifecycle.c -o build/test_arcade_actor10_seven_lifecycle
	build/test_arcade_actor10_seven_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor10-eight
test-arcade-actor10-eight: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor10_eight.c -o build/test_arcade_actor10_eight
	build/test_arcade_actor10_eight "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor10-eight-lifecycle
test-arcade-actor10-eight-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor10_eight_lifecycle.c -o build/test_arcade_actor10_eight_lifecycle
	build/test_arcade_actor10_eight_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor10-nine
test-arcade-actor10-nine: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor10_nine.c -o build/test_arcade_actor10_nine
	build/test_arcade_actor10_nine "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor10-nine-lifecycle
test-arcade-actor10-nine-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor10_nine_lifecycle.c -o build/test_arcade_actor10_nine_lifecycle
	build/test_arcade_actor10_nine_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-enemy-platforms-back
test-arcade-enemy-platforms-back: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_enemy_platforms.c -o build/test_arcade_enemy_platforms_back
	build/test_arcade_enemy_platforms_back "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" 1

.PHONY: test-arcade-actor10-ten
test-arcade-actor10-ten: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor10_ten.c -o build/test_arcade_actor10_ten
	build/test_arcade_actor10_ten "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor10-ten-lifecycle
test-arcade-actor10-ten-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor10_ten_lifecycle.c -o build/test_arcade_actor10_ten_lifecycle
	build/test_arcade_actor10_ten_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor10-eleven
test-arcade-actor10-eleven: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor10_eleven.c -o build/test_arcade_actor10_eleven
	build/test_arcade_actor10_eleven "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor10-eleven-lifecycle
test-arcade-actor10-eleven-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor10_eleven_lifecycle.c -o build/test_arcade_actor10_eleven_lifecycle
	build/test_arcade_actor10_eleven_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor10-thirteen
test-arcade-actor10-thirteen: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor10_thirteen.c -o build/test_arcade_actor10_thirteen
	build/test_arcade_actor10_thirteen "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor10-thirteen-lifecycle
test-arcade-actor10-thirteen-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor10_thirteen_lifecycle.c -o build/test_arcade_actor10_thirteen_lifecycle
	build/test_arcade_actor10_thirteen_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor10-fourteen
test-arcade-actor10-fourteen: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor10_fourteen.c -o build/test_arcade_actor10_fourteen
	build/test_arcade_actor10_fourteen "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor10-fourteen-lifecycle
test-arcade-actor10-fourteen-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor10_fourteen_lifecycle.c -o build/test_arcade_actor10_fourteen_lifecycle
	build/test_arcade_actor10_fourteen_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor10-fifteen
test-arcade-actor10-fifteen: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor10_fifteen.c -o build/test_arcade_actor10_fifteen
	build/test_arcade_actor10_fifteen "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor10-fifteen-lifecycle
test-arcade-actor10-fifteen-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor10_fifteen_lifecycle.c -o build/test_arcade_actor10_fifteen_lifecycle
	build/test_arcade_actor10_fifteen_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor10-sixteen
test-arcade-actor10-sixteen: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor10_sixteen.c -o build/test_arcade_actor10_sixteen
	build/test_arcade_actor10_sixteen "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor10-sixteen-lifecycle
test-arcade-actor10-sixteen-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor10_sixteen_lifecycle.c -o build/test_arcade_actor10_sixteen_lifecycle
	build/test_arcade_actor10_sixteen_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor10-seventeen-eighteen
test-arcade-actor10-seventeen-eighteen: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor10_seventeen_eighteen.c -o build/test_arcade_actor10_seventeen_eighteen
	build/test_arcade_actor10_seventeen_eighteen "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor10-seventeen-eighteen-lifecycle
test-arcade-actor10-seventeen-eighteen-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor10_seventeen_eighteen_lifecycle.c -o build/test_arcade_actor10_seventeen_eighteen_lifecycle
	build/test_arcade_actor10_seventeen_eighteen_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor10-nineteen
test-arcade-actor10-nineteen: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor10_nineteen.c -o build/test_arcade_actor10_nineteen
	build/test_arcade_actor10_nineteen "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor10-nineteen-lifecycle
test-arcade-actor10-nineteen-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor10_nineteen_lifecycle.c -o build/test_arcade_actor10_nineteen_lifecycle
	build/test_arcade_actor10_nineteen_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor10-twenty
test-arcade-actor10-twenty: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor10_twenty.c -o build/test_arcade_actor10_twenty
	build/test_arcade_actor10_twenty "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor10-twenty-lifecycle
test-arcade-actor10-twenty-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor10_twenty_lifecycle.c -o build/test_arcade_actor10_twenty_lifecycle
	build/test_arcade_actor10_twenty_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor10-twentyone
test-arcade-actor10-twentyone: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor10_twentyone.c -o build/test_arcade_actor10_twentyone
	build/test_arcade_actor10_twentyone "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor10-twentyone-lifecycle
test-arcade-actor10-twentyone-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor10_twentyone_lifecycle.c -o build/test_arcade_actor10_twentyone_lifecycle
	build/test_arcade_actor10_twentyone_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor10-twentythree
test-arcade-actor10-twentythree: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor10_twentythree.c -o build/test_arcade_actor10_twentythree
	build/test_arcade_actor10_twentythree "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor10-twentythree-lifecycle
test-arcade-actor10-twentythree-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor10_twentythree_lifecycle.c -o build/test_arcade_actor10_twentythree_lifecycle
	build/test_arcade_actor10_twentythree_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor10-twentytwo
test-arcade-actor10-twentytwo: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor10_twentytwo.c -o build/test_arcade_actor10_twentytwo
	build/test_arcade_actor10_twentytwo "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor10-twentytwo-lifecycle
test-arcade-actor10-twentytwo-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor10_twentytwo_lifecycle.c -o build/test_arcade_actor10_twentytwo_lifecycle
	build/test_arcade_actor10_twentytwo_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-zero-entry
test-arcade-actor4-zero-entry: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_zero_entry.c -o build/test_arcade_actor4_zero_entry
	build/test_arcade_actor4_zero_entry "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-pool-services
test-arcade-actor4-pool-services: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_pool_services.c -o build/test_arcade_actor4_pool_services
	build/test_arcade_actor4_pool_services "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-zero-common
test-arcade-actor4-zero-common: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_zero_common.c -o build/test_arcade_actor4_zero_common
	build/test_arcade_actor4_zero_common "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-zero-turning
test-arcade-actor4-zero-turning: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_zero_turning.c -o build/test_arcade_actor4_zero_turning
	build/test_arcade_actor4_zero_turning "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-zero-recovery
test-arcade-actor4-zero-recovery: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_zero_recovery.c -o build/test_arcade_actor4_zero_recovery
	build/test_arcade_actor4_zero_recovery "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-zero-side-recovery
test-arcade-actor4-zero-side-recovery: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_zero_side_recovery.c -o build/test_arcade_actor4_zero_side_recovery
	build/test_arcade_actor4_zero_side_recovery "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-zero-falling
test-arcade-actor4-zero-falling: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_zero_falling.c -o build/test_arcade_actor4_zero_falling
	build/test_arcade_actor4_zero_falling "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-zero-falling-lifecycle
test-arcade-actor4-zero-falling-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_zero_falling_lifecycle.c -o build/test_arcade_actor4_zero_falling_lifecycle
	build/test_arcade_actor4_zero_falling_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-zero-edges
test-arcade-actor4-zero-edges: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_zero_edges.c -o build/test_arcade_actor4_zero_edges
	build/test_arcade_actor4_zero_edges "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-zero-step
test-arcade-actor4-zero-step: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_zero_step.c -o build/test_arcade_actor4_zero_step
	build/test_arcade_actor4_zero_step "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-zero-step-lifecycle
test-arcade-actor4-zero-step-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_zero_step_lifecycle.c -o build/test_arcade_actor4_zero_step_lifecycle
	build/test_arcade_actor4_zero_step_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-zero-jump
test-arcade-actor4-zero-jump: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_zero_jump.c -o build/test_arcade_actor4_zero_jump
	build/test_arcade_actor4_zero_jump "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-zero-jump-lifecycle
test-arcade-actor4-zero-jump-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_zero_jump_lifecycle.c -o build/test_arcade_actor4_zero_jump_lifecycle
	build/test_arcade_actor4_zero_jump_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor-wall
test-arcade-actor-wall: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor_wall.c -o build/test_arcade_actor_wall
	build/test_arcade_actor_wall "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-zero-dive
test-arcade-actor4-zero-dive: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_zero_dive.c -o build/test_arcade_actor4_zero_dive
	build/test_arcade_actor4_zero_dive "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-zero-dive-lifecycle
test-arcade-actor4-zero-dive-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_zero_dive_lifecycle.c -o build/test_arcade_actor4_zero_dive_lifecycle
	build/test_arcade_actor4_zero_dive_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-zero-search
test-arcade-actor4-zero-search: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_zero_search.c -o build/test_arcade_actor4_zero_search
	build/test_arcade_actor4_zero_search "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-zero-search-lifecycle
test-arcade-actor4-zero-search-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_zero_search_lifecycle.c -o build/test_arcade_actor4_zero_search_lifecycle
	build/test_arcade_actor4_zero_search_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-zero-walk
test-arcade-actor4-zero-walk: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_zero_walk.c -o build/test_arcade_actor4_zero_walk
	build/test_arcade_actor4_zero_walk "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-zero-active
test-arcade-actor4-zero-active: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_zero_active.c -o build/test_arcade_actor4_zero_active
	build/test_arcade_actor4_zero_active "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-zero-active-lifecycle
test-arcade-actor4-zero-active-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_zero_active_lifecycle.c -o build/test_arcade_actor4_zero_active_lifecycle
	build/test_arcade_actor4_zero_active_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-zero-death
test-arcade-actor4-zero-death: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_zero_death.c -o build/test_arcade_actor4_zero_death
	build/test_arcade_actor4_zero_death "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-zero-lifecycle
test-arcade-actor4-zero-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_zero_lifecycle.c -o build/test_arcade_actor4_zero_lifecycle
	build/test_arcade_actor4_zero_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-one-setup
test-arcade-actor4-one-setup: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_one_setup.c -o build/test_arcade_actor4_one_setup
	build/test_arcade_actor4_one_setup "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-one-scripted
test-arcade-actor4-one-scripted: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_one_scripted.c -o build/test_arcade_actor4_one_scripted
	build/test_arcade_actor4_one_scripted "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-one-active
test-arcade-actor4-one-active: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_one_active.c -o build/test_arcade_actor4_one_active
	build/test_arcade_actor4_one_active "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-one-active-lifecycle
test-arcade-actor4-one-active-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_one_active_lifecycle.c -o build/test_arcade_actor4_one_active_lifecycle
	build/test_arcade_actor4_one_active_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-one-variant2
test-arcade-actor4-one-variant2: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_one_variant2.c -o build/test_arcade_actor4_one_variant2
	build/test_arcade_actor4_one_variant2 "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-one-variant2-lifecycle
test-arcade-actor4-one-variant2-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_one_variant2_lifecycle.c -o build/test_arcade_actor4_one_variant2_lifecycle
	build/test_arcade_actor4_one_variant2_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-one-death
test-arcade-actor4-one-death: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_one_death.c -o build/test_arcade_actor4_one_death
	build/test_arcade_actor4_one_death "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-one-lifecycle
test-arcade-actor4-one-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_one_lifecycle.c -o build/test_arcade_actor4_one_lifecycle
	build/test_arcade_actor4_one_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-two
test-arcade-actor4-two: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_two.c -o build/test_arcade_actor4_two
	build/test_arcade_actor4_two "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-two-lifecycle
test-arcade-actor4-two-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_two_lifecycle.c -o build/test_arcade_actor4_two_lifecycle
	build/test_arcade_actor4_two_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-three-setup
test-arcade-actor4-three-setup: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_three_setup.c -o build/test_arcade_actor4_three_setup
	build/test_arcade_actor4_three_setup "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-three-common
test-arcade-actor4-three-common: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_three_common.c -o build/test_arcade_actor4_three_common
	build/test_arcade_actor4_three_common "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-three-decisions
test-arcade-actor4-three-decisions: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_three_decisions.c -o build/test_arcade_actor4_three_decisions
	build/test_arcade_actor4_three_decisions "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-three-patrol
test-arcade-actor4-three-patrol: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_three_patrol.c -o build/test_arcade_actor4_three_patrol
	build/test_arcade_actor4_three_patrol "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-three-walk
test-arcade-actor4-three-walk: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_three_walk.c -o build/test_arcade_actor4_three_walk
	build/test_arcade_actor4_three_walk "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-three-ranged-lifecycle
test-arcade-actor4-three-ranged-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_three_ranged_lifecycle.c -o build/test_arcade_actor4_three_ranged_lifecycle
	build/test_arcade_actor4_three_ranged_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-three-active
test-arcade-actor4-three-active: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_three_active.c -o build/test_arcade_actor4_three_active
	build/test_arcade_actor4_three_active "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-three-active-lifecycle
test-arcade-actor4-three-active-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_three_active_lifecycle.c -o build/test_arcade_actor4_three_active_lifecycle
	build/test_arcade_actor4_three_active_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-three-death
test-arcade-actor4-three-death: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_three_death.c -o build/test_arcade_actor4_three_death
	build/test_arcade_actor4_three_death "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-three-ordinary-lifecycle
test-arcade-actor4-three-ordinary-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_three_ordinary_lifecycle.c -o build/test_arcade_actor4_three_ordinary_lifecycle
	build/test_arcade_actor4_three_ordinary_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-three-higher
test-arcade-actor4-three-higher: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_three_higher.c -o build/test_arcade_actor4_three_higher
	build/test_arcade_actor4_three_higher "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-three-higher-death
test-arcade-actor4-three-higher-death: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_three_higher_death.c -o build/test_arcade_actor4_three_higher_death
	build/test_arcade_actor4_three_higher_death "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-three-lifecycle
test-arcade-actor4-three-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_three_lifecycle.c -o build/test_arcade_actor4_three_lifecycle
	build/test_arcade_actor4_three_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-four
test-arcade-actor4-four: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_four.c -o build/test_arcade_actor4_four
	build/test_arcade_actor4_four "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-four-lifecycle
test-arcade-actor4-four-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_four_lifecycle.c -o build/test_arcade_actor4_four_lifecycle
	build/test_arcade_actor4_four_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-five
test-arcade-actor4-five: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_five.c -o build/test_arcade_actor4_five
	build/test_arcade_actor4_five "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-spawner-pool
test-arcade-actor4-spawner-pool: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_spawner_pool.c -o build/test_arcade_actor4_spawner_pool
	build/test_arcade_actor4_spawner_pool "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-six
test-arcade-actor4-six: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_six.c -o build/test_arcade_actor4_six
	build/test_arcade_actor4_six "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-six-lifecycle
test-arcade-actor4-six-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_six_lifecycle.c -o build/test_arcade_actor4_six_lifecycle
	build/test_arcade_actor4_six_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-seven-setup
test-arcade-actor4-seven-setup: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_seven_setup.c -o build/test_arcade_actor4_seven_setup
	build/test_arcade_actor4_seven_setup "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-seven-appearance
test-arcade-actor4-seven-appearance: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_seven_appearance.c -o build/test_arcade_actor4_seven_appearance
	build/test_arcade_actor4_seven_appearance "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-seven-tracking
test-arcade-actor4-seven-tracking: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_seven_tracking.c -o build/test_arcade_actor4_seven_tracking
	build/test_arcade_actor4_seven_tracking "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-seven-move
test-arcade-actor4-seven-move: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_seven_move.c -o build/test_arcade_actor4_seven_move
	build/test_arcade_actor4_seven_move "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-seven-move-lifecycle
test-arcade-actor4-seven-move-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_seven_move_lifecycle.c -o build/test_arcade_actor4_seven_move_lifecycle
	build/test_arcade_actor4_seven_move_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-seven-transition
test-arcade-actor4-seven-transition: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_seven_transition.c -o build/test_arcade_actor4_seven_transition
	build/test_arcade_actor4_seven_transition "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-seven-transition-lifecycle
test-arcade-actor4-seven-transition-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_seven_transition_lifecycle.c -o build/test_arcade_actor4_seven_transition_lifecycle
	build/test_arcade_actor4_seven_transition_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-seven-attack
test-arcade-actor4-seven-attack: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_seven_attack.c -o build/test_arcade_actor4_seven_attack
	build/test_arcade_actor4_seven_attack "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-seven-attack-lifecycle
test-arcade-actor4-seven-attack-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_seven_attack_lifecycle.c -o build/test_arcade_actor4_seven_attack_lifecycle
	build/test_arcade_actor4_seven_attack_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-seven-death
test-arcade-actor4-seven-death: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_seven_death.c -o build/test_arcade_actor4_seven_death
	build/test_arcade_actor4_seven_death "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-seven-lifecycle
test-arcade-actor4-seven-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_seven_lifecycle.c -o build/test_arcade_actor4_seven_lifecycle
	build/test_arcade_actor4_seven_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-eight
test-arcade-actor4-eight: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_eight.c -o build/test_arcade_actor4_eight
	build/test_arcade_actor4_eight "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-eight-pool
test-arcade-actor4-eight-pool: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_eight_pool.c -o build/test_arcade_actor4_eight_pool
	build/test_arcade_actor4_eight_pool "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-nine-entry
test-arcade-actor4-nine-entry: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_nine_entry.c -o build/test_arcade_actor4_nine_entry
	build/test_arcade_actor4_nine_entry "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-nine-entry-lifecycle
test-arcade-actor4-nine-entry-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_nine_entry_lifecycle.c -o build/test_arcade_actor4_nine_entry_lifecycle
	build/test_arcade_actor4_nine_entry_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-nine-active
test-arcade-actor4-nine-active: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_nine_active.c -o build/test_arcade_actor4_nine_active
	build/test_arcade_actor4_nine_active "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-nine-active-lifecycle
test-arcade-actor4-nine-active-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_nine_active_lifecycle.c -o build/test_arcade_actor4_nine_active_lifecycle
	build/test_arcade_actor4_nine_active_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-nine-death
test-arcade-actor4-nine-death: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_nine_death.c -o build/test_arcade_actor4_nine_death
	build/test_arcade_actor4_nine_death "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-nine-lifecycle
test-arcade-actor4-nine-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_nine_lifecycle.c -o build/test_arcade_actor4_nine_lifecycle
	build/test_arcade_actor4_nine_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-ten-attack
test-arcade-actor4-ten-attack: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_ten_attack.c -o build/test_arcade_actor4_ten_attack
	build/test_arcade_actor4_ten_attack "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-ten-attack-lifecycle
test-arcade-actor4-ten-attack-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_ten_attack_lifecycle.c -o build/test_arcade_actor4_ten_attack_lifecycle
	build/test_arcade_actor4_ten_attack_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-ten
test-arcade-actor4-ten: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_ten.c -o build/test_arcade_actor4_ten
	build/test_arcade_actor4_ten "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-ten-lifecycle
test-arcade-actor4-ten-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_ten_lifecycle.c -o build/test_arcade_actor4_ten_lifecycle
	build/test_arcade_actor4_ten_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-eleven
test-arcade-actor4-eleven: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_eleven.c -o build/test_arcade_actor4_eleven
	build/test_arcade_actor4_eleven "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-eleven-lifecycle
test-arcade-actor4-eleven-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_eleven_lifecycle.c -o build/test_arcade_actor4_eleven_lifecycle
	build/test_arcade_actor4_eleven_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-twelve
test-arcade-actor4-twelve: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_twelve.c -o build/test_arcade_actor4_twelve
	build/test_arcade_actor4_twelve "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-twelve-pool
test-arcade-actor4-twelve-pool: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_twelve_pool.c -o build/test_arcade_actor4_twelve_pool
	build/test_arcade_actor4_twelve_pool "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-thirteen-motion
test-arcade-actor4-thirteen-motion: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_thirteen_motion.c -o build/test_arcade_actor4_thirteen_motion
	build/test_arcade_actor4_thirteen_motion "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-thirteen-paths
test-arcade-actor4-thirteen-paths: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_thirteen_paths.c -o build/test_arcade_actor4_thirteen_paths
	build/test_arcade_actor4_thirteen_paths "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-thirteen
test-arcade-actor4-thirteen: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_thirteen.c -o build/test_arcade_actor4_thirteen
	build/test_arcade_actor4_thirteen "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-thirteen-pool
test-arcade-actor4-thirteen-pool: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_thirteen_pool.c -o build/test_arcade_actor4_thirteen_pool
	build/test_arcade_actor4_thirteen_pool "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-fourteen-entry
test-arcade-actor4-fourteen-entry: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_fourteen_entry.c -o build/test_arcade_actor4_fourteen_entry
	build/test_arcade_actor4_fourteen_entry "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-fourteen-entry-lifecycle
test-arcade-actor4-fourteen-entry-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_fourteen_entry_lifecycle.c -o build/test_arcade_actor4_fourteen_entry_lifecycle
	build/test_arcade_actor4_fourteen_entry_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-fourteen-helpers
test-arcade-actor4-fourteen-helpers: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_fourteen_helpers.c -o build/test_arcade_actor4_fourteen_helpers
	build/test_arcade_actor4_fourteen_helpers "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-fourteen-actions
test-arcade-actor4-fourteen-actions: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_fourteen_actions.c -o build/test_arcade_actor4_fourteen_actions
	build/test_arcade_actor4_fourteen_actions "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-fourteen-attack-lifecycle
test-arcade-actor4-fourteen-attack-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_fourteen_attack_lifecycle.c -o build/test_arcade_actor4_fourteen_attack_lifecycle
	build/test_arcade_actor4_fourteen_attack_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-fourteen-walk
test-arcade-actor4-fourteen-walk: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_fourteen_walk.c -o build/test_arcade_actor4_fourteen_walk
	build/test_arcade_actor4_fourteen_walk "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-fourteen-walk-lifecycle
test-arcade-actor4-fourteen-walk-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_fourteen_walk_lifecycle.c -o build/test_arcade_actor4_fourteen_walk_lifecycle
	build/test_arcade_actor4_fourteen_walk_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-fourteen-projectile
test-arcade-actor4-fourteen-projectile: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_fourteen_projectile.c -o build/test_arcade_actor4_fourteen_projectile
	build/test_arcade_actor4_fourteen_projectile "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-fourteen-projectile-lifecycle
test-arcade-actor4-fourteen-projectile-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_fourteen_projectile_lifecycle.c -o build/test_arcade_actor4_fourteen_projectile_lifecycle
	build/test_arcade_actor4_fourteen_projectile_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-fourteen-return
test-arcade-actor4-fourteen-return: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_fourteen_return.c -o build/test_arcade_actor4_fourteen_return
	build/test_arcade_actor4_fourteen_return "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-fourteen-air
test-arcade-actor4-fourteen-air: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_fourteen_air.c -o build/test_arcade_actor4_fourteen_air
	build/test_arcade_actor4_fourteen_air "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-fourteen-script-lifecycle
test-arcade-actor4-fourteen-script-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_fourteen_script_lifecycle.c -o build/test_arcade_actor4_fourteen_script_lifecycle
	build/test_arcade_actor4_fourteen_script_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-fourteen-recovery
test-arcade-actor4-fourteen-recovery: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_fourteen_recovery.c -o build/test_arcade_actor4_fourteen_recovery
	build/test_arcade_actor4_fourteen_recovery "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-fourteen-post
test-arcade-actor4-fourteen-post: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_fourteen_post.c -o build/test_arcade_actor4_fourteen_post
	build/test_arcade_actor4_fourteen_post "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-fourteen-active
test-arcade-actor4-fourteen-active: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_fourteen_active.c -o build/test_arcade_actor4_fourteen_active
	build/test_arcade_actor4_fourteen_active "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-fourteen-death
test-arcade-actor4-fourteen-death: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_fourteen_death.c -o build/test_arcade_actor4_fourteen_death
	build/test_arcade_actor4_fourteen_death "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-fourteen
test-arcade-actor4-fourteen: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_fourteen.c -o build/test_arcade_actor4_fourteen
	build/test_arcade_actor4_fourteen "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-fourteen-pool
test-arcade-actor4-fourteen-pool: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_fourteen_pool.c -o build/test_arcade_actor4_fourteen_pool
	build/test_arcade_actor4_fourteen_pool "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-fifteen-entry
test-arcade-actor4-fifteen-entry: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_fifteen_entry.c -o build/test_arcade_actor4_fifteen_entry
	build/test_arcade_actor4_fifteen_entry "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-fifteen-entry-lifecycle
test-arcade-actor4-fifteen-entry-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_fifteen_entry_lifecycle.c -o build/test_arcade_actor4_fifteen_entry_lifecycle
	build/test_arcade_actor4_fifteen_entry_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-fifteen-motion
test-arcade-actor4-fifteen-motion: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_fifteen_motion.c -o build/test_arcade_actor4_fifteen_motion
	build/test_arcade_actor4_fifteen_motion "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-fifteen-path-lifecycle
test-arcade-actor4-fifteen-path-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_fifteen_path_lifecycle.c -o build/test_arcade_actor4_fifteen_path_lifecycle
	build/test_arcade_actor4_fifteen_path_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-fifteen-attack
test-arcade-actor4-fifteen-attack: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_fifteen_attack.c -o build/test_arcade_actor4_fifteen_attack
	build/test_arcade_actor4_fifteen_attack "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-fifteen-attack-lifecycle
test-arcade-actor4-fifteen-attack-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_fifteen_attack_lifecycle.c -o build/test_arcade_actor4_fifteen_attack_lifecycle
	build/test_arcade_actor4_fifteen_attack_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-fifteen-ordinary
test-arcade-actor4-fifteen-ordinary: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_fifteen_ordinary.c -o build/test_arcade_actor4_fifteen_ordinary
	build/test_arcade_actor4_fifteen_ordinary "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-fifteen-ordinary-lifecycle
test-arcade-actor4-fifteen-ordinary-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_fifteen_ordinary_lifecycle.c -o build/test_arcade_actor4_fifteen_ordinary_lifecycle
	build/test_arcade_actor4_fifteen_ordinary_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-fifteen-one
test-arcade-actor4-fifteen-one: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_fifteen_one.c -o build/test_arcade_actor4_fifteen_one
	build/test_arcade_actor4_fifteen_one "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-fifteen-one-death
test-arcade-actor4-fifteen-one-death: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_fifteen_one_death.c -o build/test_arcade_actor4_fifteen_one_death
	build/test_arcade_actor4_fifteen_one_death "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-fifteen-death
test-arcade-actor4-fifteen-death: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_fifteen_death.c -o build/test_arcade_actor4_fifteen_death
	build/test_arcade_actor4_fifteen_death "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-fifteen
test-arcade-actor4-fifteen: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_fifteen.c -o build/test_arcade_actor4_fifteen
	build/test_arcade_actor4_fifteen "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-fifteen-pool
test-arcade-actor4-fifteen-pool: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_fifteen_pool.c -o build/test_arcade_actor4_fifteen_pool
	build/test_arcade_actor4_fifteen_pool "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-sixteen-active
test-arcade-actor4-sixteen-active: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_sixteen_active.c -o build/test_arcade_actor4_sixteen_active
	build/test_arcade_actor4_sixteen_active "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-sixteen-active-lifecycle
test-arcade-actor4-sixteen-active-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_sixteen_active_lifecycle.c -o build/test_arcade_actor4_sixteen_active_lifecycle
	build/test_arcade_actor4_sixteen_active_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-sixteen-death
test-arcade-actor4-sixteen-death: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_sixteen_death.c -o build/test_arcade_actor4_sixteen_death
	build/test_arcade_actor4_sixteen_death "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-sixteen-death-lifecycle
test-arcade-actor4-sixteen-death-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_sixteen_death_lifecycle.c -o build/test_arcade_actor4_sixteen_death_lifecycle
	build/test_arcade_actor4_sixteen_death_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-sixteen
test-arcade-actor4-sixteen: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_sixteen.c -o build/test_arcade_actor4_sixteen
	build/test_arcade_actor4_sixteen "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-sixteen-pool
test-arcade-actor4-sixteen-pool: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_sixteen_pool.c -o build/test_arcade_actor4_sixteen_pool
	build/test_arcade_actor4_sixteen_pool "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-seventeen-active
test-arcade-actor4-seventeen-active: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_seventeen_active.c -o build/test_arcade_actor4_seventeen_active
	build/test_arcade_actor4_seventeen_active "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-seventeen-active-lifecycle
test-arcade-actor4-seventeen-active-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_seventeen_active_lifecycle.c -o build/test_arcade_actor4_seventeen_active_lifecycle
	build/test_arcade_actor4_seventeen_active_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-seventeen
test-arcade-actor4-seventeen: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_seventeen.c -o build/test_arcade_actor4_seventeen
	build/test_arcade_actor4_seventeen "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-seventeen-pool
test-arcade-actor4-seventeen-pool: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_seventeen_pool.c -o build/test_arcade_actor4_seventeen_pool
	build/test_arcade_actor4_seventeen_pool "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-eighteen
test-arcade-actor4-eighteen: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_eighteen.c -o build/test_arcade_actor4_eighteen
	build/test_arcade_actor4_eighteen "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-eighteen-pool
test-arcade-actor4-eighteen-pool: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_eighteen_pool.c -o build/test_arcade_actor4_eighteen_pool
	build/test_arcade_actor4_eighteen_pool "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-nineteen-entry
test-arcade-actor4-nineteen-entry: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_nineteen_entry.c -o build/test_arcade_actor4_nineteen_entry
	build/test_arcade_actor4_nineteen_entry "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-nineteen-damage
test-arcade-actor4-nineteen-damage: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_nineteen_damage.c -o build/test_arcade_actor4_nineteen_damage
	build/test_arcade_actor4_nineteen_damage "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-nineteen-active
test-arcade-actor4-nineteen-active: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_nineteen_active.c -o build/test_arcade_actor4_nineteen_active
	build/test_arcade_actor4_nineteen_active "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-nineteen-active-lifecycle
test-arcade-actor4-nineteen-active-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_nineteen_active_lifecycle.c -o build/test_arcade_actor4_nineteen_active_lifecycle
	build/test_arcade_actor4_nineteen_active_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-nineteen
test-arcade-actor4-nineteen: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_nineteen.c -o build/test_arcade_actor4_nineteen
	build/test_arcade_actor4_nineteen "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-nineteen-pool
test-arcade-actor4-nineteen-pool: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_nineteen_pool.c -o build/test_arcade_actor4_nineteen_pool
	build/test_arcade_actor4_nineteen_pool "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-twenty-entry
test-arcade-actor4-twenty-entry: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_twenty_entry.c -o build/test_arcade_actor4_twenty_entry
	build/test_arcade_actor4_twenty_entry "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-twenty-entry-lifecycle
test-arcade-actor4-twenty-entry-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_twenty_entry_lifecycle.c -o build/test_arcade_actor4_twenty_entry_lifecycle
	build/test_arcade_actor4_twenty_entry_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-twenty-zero
test-arcade-actor4-twenty-zero: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_twenty_zero.c -o build/test_arcade_actor4_twenty_zero
	build/test_arcade_actor4_twenty_zero "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-twenty-zero-lifecycle
test-arcade-actor4-twenty-zero-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_twenty_zero_lifecycle.c -o build/test_arcade_actor4_twenty_zero_lifecycle
	build/test_arcade_actor4_twenty_zero_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-twenty-one
test-arcade-actor4-twenty-one: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_twenty_one.c -o build/test_arcade_actor4_twenty_one
	build/test_arcade_actor4_twenty_one "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-twenty-one-lifecycle
test-arcade-actor4-twenty-one-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_twenty_one_lifecycle.c -o build/test_arcade_actor4_twenty_one_lifecycle
	build/test_arcade_actor4_twenty_one_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-twenty-death
test-arcade-actor4-twenty-death: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_twenty_death.c -o build/test_arcade_actor4_twenty_death
	build/test_arcade_actor4_twenty_death "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-twenty
test-arcade-actor4-twenty: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_twenty.c -o build/test_arcade_actor4_twenty
	build/test_arcade_actor4_twenty "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-twenty-pool
test-arcade-actor4-twenty-pool: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_twenty_pool.c -o build/test_arcade_actor4_twenty_pool
	build/test_arcade_actor4_twenty_pool "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-twentyone-entry
test-arcade-actor4-twentyone-entry: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_twentyone_entry.c -o build/test_arcade_actor4_twentyone_entry
	build/test_arcade_actor4_twentyone_entry "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-twentyone-entry-lifecycle
test-arcade-actor4-twentyone-entry-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_twentyone_entry_lifecycle.c -o build/test_arcade_actor4_twentyone_entry_lifecycle
	build/test_arcade_actor4_twentyone_entry_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-twentyone-choose
test-arcade-actor4-twentyone-choose: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_twentyone_choose.c -o build/test_arcade_actor4_twentyone_choose
	build/test_arcade_actor4_twentyone_choose "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-twentyone-close
test-arcade-actor4-twentyone-close: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_twentyone_close.c -o build/test_arcade_actor4_twentyone_close
	build/test_arcade_actor4_twentyone_close "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-twentyone-close-lifecycle
test-arcade-actor4-twentyone-close-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_twentyone_close_lifecycle.c -o build/test_arcade_actor4_twentyone_close_lifecycle
	build/test_arcade_actor4_twentyone_close_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-twentyone-air
test-arcade-actor4-twentyone-air: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_twentyone_air.c -o build/test_arcade_actor4_twentyone_air
	build/test_arcade_actor4_twentyone_air "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-twentyone-platform
test-arcade-actor4-twentyone-platform: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_twentyone_platform.c -o build/test_arcade_actor4_twentyone_platform
	build/test_arcade_actor4_twentyone_platform "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-twentyone-platform-lifecycle
test-arcade-actor4-twentyone-platform-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_twentyone_platform_lifecycle.c -o build/test_arcade_actor4_twentyone_platform_lifecycle
	build/test_arcade_actor4_twentyone_platform_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-twentyone-active
test-arcade-actor4-twentyone-active: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_twentyone_active.c -o build/test_arcade_actor4_twentyone_active
	build/test_arcade_actor4_twentyone_active "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-twentyone-death
test-arcade-actor4-twentyone-death: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_twentyone_death.c -o build/test_arcade_actor4_twentyone_death
	build/test_arcade_actor4_twentyone_death "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-twentyone
test-arcade-actor4-twentyone: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_twentyone.c -o build/test_arcade_actor4_twentyone
	build/test_arcade_actor4_twentyone "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-twentyone-pool
test-arcade-actor4-twentyone-pool: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_twentyone_pool.c -o build/test_arcade_actor4_twentyone_pool
	build/test_arcade_actor4_twentyone_pool "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-twentytwo-entry
test-arcade-actor4-twentytwo-entry: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_twentytwo_entry.c -o build/test_arcade_actor4_twentytwo_entry
	build/test_arcade_actor4_twentytwo_entry "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-twentytwo-move-setup
test-arcade-actor4-twentytwo-move-setup: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_twentytwo_move_setup.c -o build/test_arcade_actor4_twentytwo_move_setup
	build/test_arcade_actor4_twentytwo_move_setup "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-twentytwo-turn
test-arcade-actor4-twentytwo-turn: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_twentytwo_turn.c -o build/test_arcade_actor4_twentytwo_turn
	build/test_arcade_actor4_twentytwo_turn "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-twentytwo-turn-lifecycle
test-arcade-actor4-twentytwo-turn-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_twentytwo_turn_lifecycle.c -o build/test_arcade_actor4_twentytwo_turn_lifecycle
	build/test_arcade_actor4_twentytwo_turn_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-twentytwo-move
test-arcade-actor4-twentytwo-move: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_twentytwo_move.c -o build/test_arcade_actor4_twentytwo_move
	build/test_arcade_actor4_twentytwo_move "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-twentytwo-move-lifecycle
test-arcade-actor4-twentytwo-move-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_twentytwo_move_lifecycle.c -o build/test_arcade_actor4_twentytwo_move_lifecycle
	build/test_arcade_actor4_twentytwo_move_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-twentytwo-attack
test-arcade-actor4-twentytwo-attack: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_twentytwo_attack.c -o build/test_arcade_actor4_twentytwo_attack
	build/test_arcade_actor4_twentytwo_attack "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-twentytwo-attack-lifecycle
test-arcade-actor4-twentytwo-attack-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_twentytwo_attack_lifecycle.c -o build/test_arcade_actor4_twentytwo_attack_lifecycle
	build/test_arcade_actor4_twentytwo_attack_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-twentytwo-active
test-arcade-actor4-twentytwo-active: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_twentytwo_active.c -o build/test_arcade_actor4_twentytwo_active
	build/test_arcade_actor4_twentytwo_active "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-twentytwo-death
test-arcade-actor4-twentytwo-death: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_twentytwo_death.c -o build/test_arcade_actor4_twentytwo_death
	build/test_arcade_actor4_twentytwo_death "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-twentytwo
test-arcade-actor4-twentytwo: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_twentytwo.c -o build/test_arcade_actor4_twentytwo
	build/test_arcade_actor4_twentytwo "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-twentytwo-pool
test-arcade-actor4-twentytwo-pool: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_twentytwo_pool.c -o build/test_arcade_actor4_twentytwo_pool
	build/test_arcade_actor4_twentytwo_pool "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-twentythree-entry
test-arcade-actor4-twentythree-entry: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_twentythree_entry.c -o build/test_arcade_actor4_twentythree_entry
	build/test_arcade_actor4_twentythree_entry "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-twentythree-shoot
test-arcade-actor4-twentythree-shoot: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_twentythree_shoot.c -o build/test_arcade_actor4_twentythree_shoot
	build/test_arcade_actor4_twentythree_shoot "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-twentythree-shoot-lifecycle
test-arcade-actor4-twentythree-shoot-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_twentythree_shoot_lifecycle.c -o build/test_arcade_actor4_twentythree_shoot_lifecycle
	build/test_arcade_actor4_twentythree_shoot_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-twentythree-fall
test-arcade-actor4-twentythree-fall: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_twentythree_fall.c -o build/test_arcade_actor4_twentythree_fall
	build/test_arcade_actor4_twentythree_fall "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-twentythree-move
test-arcade-actor4-twentythree-move: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_twentythree_move.c -o build/test_arcade_actor4_twentythree_move
	build/test_arcade_actor4_twentythree_move "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-twentythree-jump
test-arcade-actor4-twentythree-jump: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_twentythree_jump.c -o build/test_arcade_actor4_twentythree_jump
	build/test_arcade_actor4_twentythree_jump "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-twentythree-jump-lifecycle
test-arcade-actor4-twentythree-jump-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_twentythree_jump_lifecycle.c -o build/test_arcade_actor4_twentythree_jump_lifecycle
	build/test_arcade_actor4_twentythree_jump_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-twentythree
test-arcade-actor4-twentythree: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_twentythree.c -o build/test_arcade_actor4_twentythree
	build/test_arcade_actor4_twentythree "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-twentythree-pool
test-arcade-actor4-twentythree-pool: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_twentythree_pool.c -o build/test_arcade_actor4_twentythree_pool
	build/test_arcade_actor4_twentythree_pool "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-twentyfour-entry
test-arcade-actor4-twentyfour-entry: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_twentyfour_entry.c -o build/test_arcade_actor4_twentyfour_entry
	build/test_arcade_actor4_twentyfour_entry "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-twentyfour-entry-lifecycle
test-arcade-actor4-twentyfour-entry-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_twentyfour_entry_lifecycle.c -o build/test_arcade_actor4_twentyfour_entry_lifecycle
	build/test_arcade_actor4_twentyfour_entry_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-twentyfour-move
test-arcade-actor4-twentyfour-move: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_twentyfour_move.c -o build/test_arcade_actor4_twentyfour_move
	build/test_arcade_actor4_twentyfour_move "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-twentyfour-move-lifecycle
test-arcade-actor4-twentyfour-move-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_twentyfour_move_lifecycle.c -o build/test_arcade_actor4_twentyfour_move_lifecycle
	build/test_arcade_actor4_twentyfour_move_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-twentyfour-attack
test-arcade-actor4-twentyfour-attack: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_twentyfour_attack.c -o build/test_arcade_actor4_twentyfour_attack
	build/test_arcade_actor4_twentyfour_attack "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-twentyfour-attack-lifecycle
test-arcade-actor4-twentyfour-attack-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_twentyfour_attack_lifecycle.c -o build/test_arcade_actor4_twentyfour_attack_lifecycle
	build/test_arcade_actor4_twentyfour_attack_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-twentyfour-leap
test-arcade-actor4-twentyfour-leap: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_twentyfour_leap.c -o build/test_arcade_actor4_twentyfour_leap
	build/test_arcade_actor4_twentyfour_leap "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-twentyfour-leap-lifecycle
test-arcade-actor4-twentyfour-leap-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_twentyfour_leap_lifecycle.c -o build/test_arcade_actor4_twentyfour_leap_lifecycle
	build/test_arcade_actor4_twentyfour_leap_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-twentyfour-target-jump
test-arcade-actor4-twentyfour-target-jump: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_twentyfour_target_jump.c -o build/test_arcade_actor4_twentyfour_target_jump
	build/test_arcade_actor4_twentyfour_target_jump "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-twentyfour-shove
test-arcade-actor4-twentyfour-shove: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_twentyfour_shove.c -o build/test_arcade_actor4_twentyfour_shove
	build/test_arcade_actor4_twentyfour_shove "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-twentyfour-target-jump-lifecycle
test-arcade-actor4-twentyfour-target-jump-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_twentyfour_target_jump_lifecycle.c -o build/test_arcade_actor4_twentyfour_target_jump_lifecycle
	build/test_arcade_actor4_twentyfour_target_jump_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-twentyfour-defense
test-arcade-actor4-twentyfour-defense: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_twentyfour_defense.c -o build/test_arcade_actor4_twentyfour_defense
	build/test_arcade_actor4_twentyfour_defense "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-twentyfour-health
test-arcade-actor4-twentyfour-health: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_twentyfour_health.c -o build/test_arcade_actor4_twentyfour_health
	build/test_arcade_actor4_twentyfour_health "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-twentyfour-active
test-arcade-actor4-twentyfour-active: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_twentyfour_active.c -o build/test_arcade_actor4_twentyfour_active
	build/test_arcade_actor4_twentyfour_active "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-twentyfour
test-arcade-actor4-twentyfour: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_twentyfour.c -o build/test_arcade_actor4_twentyfour
	build/test_arcade_actor4_twentyfour "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-twentyfour-pool
test-arcade-actor4-twentyfour-pool: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_twentyfour_pool.c -o build/test_arcade_actor4_twentyfour_pool
	build/test_arcade_actor4_twentyfour_pool "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-twentyfive-entry
test-arcade-actor4-twentyfive-entry: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_twentyfive_entry.c -o build/test_arcade_actor4_twentyfive_entry
	build/test_arcade_actor4_twentyfive_entry "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-twentyfive-entry-lifecycle
test-arcade-actor4-twentyfive-entry-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_twentyfive_entry_lifecycle.c -o build/test_arcade_actor4_twentyfive_entry_lifecycle
	build/test_arcade_actor4_twentyfive_entry_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-twentyfive-move
test-arcade-actor4-twentyfive-move: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_twentyfive_move.c -o build/test_arcade_actor4_twentyfive_move
	build/test_arcade_actor4_twentyfive_move "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-twentyfive-move-lifecycle
test-arcade-actor4-twentyfive-move-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_twentyfive_move_lifecycle.c -o build/test_arcade_actor4_twentyfive_move_lifecycle
	build/test_arcade_actor4_twentyfive_move_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-twentyfive-attack
test-arcade-actor4-twentyfive-attack: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_twentyfive_attack.c -o build/test_arcade_actor4_twentyfive_attack
	build/test_arcade_actor4_twentyfive_attack "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-twentyfive
test-arcade-actor4-twentyfive: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_twentyfive.c -o build/test_arcade_actor4_twentyfive
	build/test_arcade_actor4_twentyfive "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-twentyfive-pool
test-arcade-actor4-twentyfive-pool: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_twentyfive_pool.c -o build/test_arcade_actor4_twentyfive_pool
	build/test_arcade_actor4_twentyfive_pool "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-twentysix-entry
test-arcade-actor4-twentysix-entry: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_twentysix_entry.c -o build/test_arcade_actor4_twentysix_entry
	build/test_arcade_actor4_twentysix_entry "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-actor4-twentysix-entry-lifecycle
test-arcade-actor4-twentysix-entry-lifecycle: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_actor4_twentysix_entry_lifecycle.c -o build/test_arcade_actor4_twentysix_entry_lifecycle
	build/test_arcade_actor4_twentysix_entry_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-gameplay-manager
test-arcade-gameplay-manager: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_gameplay_manager.c -o build/test_arcade_gameplay_manager
	build/test_arcade_gameplay_manager "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-life-loss
test-arcade-life-loss: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_life_loss.c -o build/test_arcade_life_loss
	build/test_arcade_life_loss "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-stage-entry
test-arcade-stage-entry: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_stage_entry.c -o build/test_arcade_stage_entry
	build/test_arcade_stage_entry "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-gameplay-flow
test-arcade-gameplay-flow: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_gameplay_flow.c -o build/test_arcade_gameplay_flow
	build/test_arcade_gameplay_flow "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-gameplay-main test-native-gameplay-main
test-arcade-gameplay-main: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_GAMEPLAY_MAIN tools/test_arcade_stage0_full_frame.c -o build/test_arcade_gameplay_main
	build/test_arcade_gameplay_main "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-gameplay-main: build/reference_maincpu.bin
	python3 tools/build_gameplay_runtime_probe.py --main
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_GAMEPLAY_MAIN -DWILLOW_TEST_NATIVE_TARGET -DWILLOW_TEST_NATIVE_SAMPLES=16 tools/test_arcade_stage0_full_frame.c -o build/test_native_gameplay_main
	build/test_native_gameplay_main "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" --cycles

.PHONY: test-arcade-gameplay-initial-wait
test-arcade-gameplay-initial-wait: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_gameplay_initial_wait.c -o build/test_arcade_gameplay_initial_wait
	build/test_arcade_gameplay_initial_wait "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-gameplay-initial-scheduler
test-arcade-gameplay-initial-scheduler: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_gameplay_initial_scheduler.c -o build/test_arcade_gameplay_initial_scheduler
	@for region in 0 1 2 3 4; do build/test_arcade_gameplay_initial_scheduler "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" $$region || exit $$?; done

.PHONY: test-native-gameplay-initial-scheduler
test-native-gameplay-initial-scheduler: build/reference_maincpu.bin
	python3 tools/build_timeout_runtime_probe.py --initial
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_native_gameplay_initial_scheduler.c -o build/test_native_gameplay_initial_scheduler
	@for region in 0 1 2 3 4; do build/test_native_gameplay_initial_scheduler "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" $$region || exit $$?; done

.PHONY: test-arcade-gameplay-init
test-arcade-gameplay-init: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_gameplay_init.c -o build/test_arcade_gameplay_init
	@for region in 0 1 2; do build/test_arcade_gameplay_init "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" $$region || exit $$?; done

.PHONY: test-arcade-gameplay-reset-entry
test-arcade-gameplay-reset-entry: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_gameplay_reset_entry.c -o build/test_arcade_gameplay_reset_entry
	@for region in 0 1 2; do build/test_arcade_gameplay_reset_entry "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" $$region || exit $$?; done

.PHONY: test-native-gameplay-reset-scheduler
test-native-gameplay-reset-scheduler: build/reference_maincpu.bin
	python3 tools/build_timeout_runtime_probe.py --reset
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_native_gameplay_reset_scheduler.c -o build/test_native_gameplay_reset_scheduler
	@for region in 0 1 2 3 4; do build/test_native_gameplay_reset_scheduler "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" $$region || exit $$?; done

.PHONY: test-arcade-scene-manager
test-arcade-scene-manager: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_scene_manager.c -o build/test_arcade_scene_manager
	build/test_arcade_scene_manager "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-scene-setup
test-arcade-scene-setup: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_scene_setup.c -o build/test_arcade_scene_setup
	@for region in 0 1; do build/test_arcade_scene_setup "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" $$region || exit $$?; done

.PHONY: test-arcade-scene-sound
test-arcade-scene-sound: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_scene_sound.c -o build/test_arcade_scene_sound
	build/test_arcade_scene_sound "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-native-source-input
test-native-source-input: test-arcade-attract-state test-native-controller intro-probe
	build/test_arcade_attract_state "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" --native-input-fixtures
	clang -O2 tools/intro_rom_smoke.c -o build/intro_rom_smoke
	build/intro_rom_smoke "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" native/intro_probe/out/rom.bin native/intro_probe/out/symbol.txt --source-input-probe

.PHONY: test-native-replay-phase
test-native-replay-phase: test-arcade-attract-state test-arcade-attract-demo-prepare
	@test -n "$(SGDK_ROOT)" || (echo "Set SGDK_ROOT to an SGDK checkout"; exit 1)
	build/test_arcade_attract_state "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" --native-fixtures
	@set -e; trap 'rm -f native/intro_probe/out/src/main.o; $(MAKE) intro-probe EXTRA_FLAGS="$(EXTRA_FLAGS)"' EXIT; \
	 rm -f native/intro_probe/out/src/main.o; \
	 $(MAKE) intro-probe EXTRA_FLAGS="$(EXTRA_FLAGS) -DWILLOW_REPLAY_PHASE_PROBE -DWILLOW_FRAME_CLOCK_PROBE"; \
	 cp native/intro_probe/out/rom.bin build/replay-phase-experiment.bin; \
	 cp native/intro_probe/out/symbol.txt build/replay-phase-experiment.sym; \
	 clang -O2 tools/intro_rom_smoke.c -o build/intro_rom_smoke; \
	 build/intro_rom_smoke "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" build/replay-phase-experiment.bin build/replay-phase-experiment.sym --replay-phase-probe

.PHONY: test-native-frame-clock
test-native-frame-clock: test-arcade-frame-clock
	@test -n "$(SGDK_ROOT)" || (echo "Set SGDK_ROOT to an SGDK checkout"; exit 1)
	build/test_arcade_frame_clock "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" --native-fixtures
	@set -e; trap 'rm -f native/intro_probe/out/src/main.o; $(MAKE) intro-probe EXTRA_FLAGS="$(EXTRA_FLAGS)"' EXIT; \
	 rm -f native/intro_probe/out/src/main.o; \
	 $(MAKE) intro-probe EXTRA_FLAGS="$(EXTRA_FLAGS) -DWILLOW_FRAME_CLOCK_PROBE"; \
	 cp native/intro_probe/out/rom.bin build/source-clock-experiment.bin; \
	 cp native/intro_probe/out/symbol.txt build/source-clock-experiment.sym; \
	 clang -O2 tools/intro_rom_smoke.c -o build/intro_rom_smoke; \
	 build/intro_rom_smoke "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" build/source-clock-experiment.bin build/source-clock-experiment.sym --frame-clock-probe

.PHONY: test-native-player-cancel
test-native-player-cancel: test-arcade-tasks intro-probe
	build/test_arcade_tasks "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" --native-cancel-fixtures
	clang -O2 tools/intro_rom_smoke.c -o build/intro_rom_smoke
	build/intro_rom_smoke "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" native/intro_probe/out/rom.bin native/intro_probe/out/symbol.txt --player-cancel-probe

.PHONY: test-native-player-rng
test-native-player-rng: test-arcade-services intro-probe
	build/test_arcade_services "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" --native-rng-fixtures
	clang -O2 tools/intro_rom_smoke.c -o build/intro_rom_smoke
	build/intro_rom_smoke "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" native/intro_probe/out/rom.bin native/intro_probe/out/symbol.txt --player-rng-probe

.PHONY: test-native-player-sound
test-native-player-sound: test-arcade-scene-sound intro-probe
	build/test_arcade_scene_sound "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" --native-fixtures
	clang -O2 tools/intro_rom_smoke.c -o build/intro_rom_smoke
	build/intro_rom_smoke "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" native/intro_probe/out/rom.bin native/intro_probe/out/symbol.txt --player-sound-probe

.PHONY: test-arcade-scene-intro-entry
test-arcade-scene-intro-entry: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_scene_intro_entry.c -o build/test_arcade_scene_intro_entry
	build/test_arcade_scene_intro_entry "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-scene-handoff
test-arcade-scene-handoff: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_scene_handoff.c -o build/test_arcade_scene_handoff
	build/test_arcade_scene_handoff "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-native-gameplay-scene-scheduler
test-native-gameplay-scene-scheduler: build/reference_maincpu.bin
	python3 tools/build_timeout_runtime_probe.py --scene
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_native_gameplay_scene_scheduler.c -o build/test_native_gameplay_scene_scheduler
	@for region in 0 1 2 3 4; do build/test_native_gameplay_scene_scheduler "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" $$region || exit $$?; done

.PHONY: test-arcade-map-scene-scheduler
test-arcade-map-scene-scheduler: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_map_scene_scheduler.c -o build/test_arcade_map_scene_scheduler
	@for region in 0 1 2 3 4; do build/test_arcade_map_scene_scheduler "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" $$region || exit $$?; done

.PHONY: test-arcade-forest-scene-scheduler
test-arcade-forest-scene-scheduler: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_forest_scene_scheduler.c -o build/test_arcade_forest_scene_scheduler
	@for region in 0 1 2 3 4; do build/test_arcade_forest_scene_scheduler "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" $$region || exit $$?; done

.PHONY: test-native-forest-scheduler
.PHONY: test-arcade-score-award test-native-first-level-scheduler
test-arcade-score-award: build/reference_maincpu.bin
	mkdir -p build/first-level-runtime-investigation
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_score_award.c -o build/first-level-runtime-investigation/score-test
	build/first-level-runtime-investigation/score-test "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-first-level-scheduler: test-arcade-score-award
	python3 tools/build_timeout_runtime_probe.py --forest
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_FIRST_LEVEL tools/test_native_forest_scheduler.c -o build/first-level-runtime-investigation/continuous-test
	build/first-level-runtime-investigation/continuous-test "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" 1

.PHONY: test-native-first-level-combat
test-native-first-level-combat: test-arcade-score-award
	python3 tools/build_timeout_runtime_probe.py --forest
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_FIRST_LEVEL -DWILLOW_TEST_BOSS_INPUT tools/test_native_forest_scheduler.c -o build/first-level-runtime-investigation/normal-play-test
	build/first-level-runtime-investigation/normal-play-test "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" 1

test-native-forest-scheduler: build/reference_maincpu.bin
	python3 tools/build_timeout_runtime_probe.py --forest
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_native_forest_scheduler.c -o build/test_native_forest_scheduler
	@for region in 0 1 2 3 4; do build/test_native_forest_scheduler "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" $$region || exit $$?; done

.PHONY: test-native-map14
test-native-map14:
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_native_map14.c -o build/test_native_map14
	build/test_native_map14

.PHONY: test-native-map14-store
test-native-map14-store: test-native-map14 build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_MAP14 tools/test_arcade_map_store.c -o build/test_arcade_map_store14
	build/test_arcade_map_store14
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_MAP14 tools/test_native_scene_clear.c -o build/test_native_scene_clear14
	build/test_native_scene_clear14

.PHONY: test-native-column-slot-m68k
test-native-column-slot-m68k:
	@test -n "$(SGDK_ROOT)" || (echo "Set SGDK_ROOT to an SGDK checkout"; exit 1)
	m68k-elf-gcc -m68000 -DSGDK_GCC -I"$(SGDK_ROOT)/inc" -O3 -Wall -Wextra -Werror -fno-builtin -nostdlib -Wl,-Ttext=0x100000,-e,slotFold tools/native_column_slot_probe.c -o build/native-column-slot.out
	m68k-elf-objcopy -O binary build/native-column-slot.out build/native-column-slot.bin
	m68k-elf-nm -n build/native-column-slot.out > build/native-column-slot.sym
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_native_column_slot_m68k.c -o build/test_native_column_slot_m68k
	build/test_native_column_slot_m68k "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" build/native-column-slot.bin build/native-column-slot.sym

.PHONY: test-native-actor-pool-bus-m68k
test-native-actor-pool-bus-m68k: build/reference_maincpu.bin
	@test -n "$(SGDK_ROOT)" || (echo "Set SGDK_ROOT to an SGDK checkout"; exit 1)
	m68k-elf-gcc -m68000 -DSGDK_GCC -I"$(SGDK_ROOT)/inc" -O3 -Wall -Wextra -Werror -fno-builtin -nostdlib -Wl,-Ttext=0x110000,-e,nativePoolProbe tools/native_actor_pool_bus_probe.c -o build/native-actor-pool-bus.out
	m68k-elf-objcopy -O binary build/native-actor-pool-bus.out build/native-actor-pool-bus.bin
	m68k-elf-nm -n build/native-actor-pool-bus.out > build/native-actor-pool-bus.sym
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_COMPILED_POOL_BUS tools/test_arcade_pool_services.c -o build/test_native_pool_bus_m68k
	build/test_native_pool_bus_m68k "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" build/native-actor-pool-bus.bin build/native-actor-pool-bus.sym
.PHONY: test-native-stage-object15
test-native-stage-object15: build/reference_maincpu.bin
	@test -n "$(SGDK_ROOT)" || (echo "Set SGDK_ROOT to an SGDK checkout"; exit 1)
	m68k-elf-gcc -m68000 -O2 -Wall -Wextra -Werror -fno-builtin -DSGDK_GCC -I"$(SGDK_ROOT)/inc" -nostdlib -Wl,-Ttext=0x110000,-e,stageObject15Probe tools/native_stage_object15_probe.c -o build/native_stage_object15_probe.elf
	m68k-elf-nm -n build/native_stage_object15_probe.elf > build/native_stage_object15_probe.sym
	m68k-elf-objcopy -O binary build/native_stage_object15_probe.elf build/native_stage_object15_probe.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_COMPILED_STAGE15 tools/test_arcade_stage_object15.c -o build/test_native_stage_object15
	build/test_native_stage_object15 "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-stage-object15
test-arcade-stage-object15: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_stage_object15.c -o build/test_arcade_stage_object15
	build/test_arcade_stage_object15 "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-native-stage-object20
test-native-stage-object20: build/reference_maincpu.bin
	@test -n "$(SGDK_ROOT)" || (echo "Set SGDK_ROOT to an SGDK checkout"; exit 1)
	m68k-elf-gcc -m68000 -O2 -Wall -Wextra -Werror -fno-builtin -DSGDK_GCC -I"$(SGDK_ROOT)/inc" -nostdlib -Wl,-Ttext=0x110000,-e,stageObject20Probe tools/native_stage_object20_probe.c -o build/native_stage_object20_probe.elf
	m68k-elf-nm -n build/native_stage_object20_probe.elf > build/native_stage_object20_probe.sym
	m68k-elf-objcopy -O binary build/native_stage_object20_probe.elf build/native_stage_object20_probe.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_COMPILED_STAGE20 tools/test_arcade_stage_object20.c -o build/test_native_stage_object20
	build/test_native_stage_object20 "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-stage-object20
test-arcade-stage-object20: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_stage_object20.c -o build/test_arcade_stage_object20
	build/test_arcade_stage_object20 "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-native-stage-object21_22
test-native-stage-object21_22: build/reference_maincpu.bin
	@test -n "$(SGDK_ROOT)" || (echo "Set SGDK_ROOT to an SGDK checkout"; exit 1)
	m68k-elf-gcc -m68000 -O2 -Wall -Wextra -Werror -fno-builtin -DSGDK_GCC -I"$(SGDK_ROOT)/inc" -nostdlib -Wl,-Ttext=0x110000,-e,stageObject21Or22Probe tools/native_stage_object21_22_probe.c -o build/native_stage_object21_22_probe.elf
	m68k-elf-nm -n build/native_stage_object21_22_probe.elf > build/native_stage_object21_22_probe.sym
	m68k-elf-objcopy -O binary build/native_stage_object21_22_probe.elf build/native_stage_object21_22_probe.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_COMPILED_STAGE21_22 tools/test_arcade_stage_object21_22.c -o build/test_native_stage_object21_22
	build/test_native_stage_object21_22 "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-stage-object21_22
test-arcade-stage-object21_22: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_stage_object21_22.c -o build/test_arcade_stage_object21_22
	build/test_arcade_stage_object21_22 "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-native-stage-object0_alternate
test-native-stage-object0_alternate: build/reference_maincpu.bin
	@test -n "$(SGDK_ROOT)" || (echo "Set SGDK_ROOT to an SGDK checkout"; exit 1)
	m68k-elf-gcc -m68000 -O2 -Wall -Wextra -Werror -fno-builtin -DSGDK_GCC -I"$(SGDK_ROOT)/inc" -nostdlib -Wl,-Ttext=0x110000,-e,stageObject0Probe tools/native_stage_object0_alternate_probe.c -o build/native_stage_object0_alternate_probe.elf
	m68k-elf-nm -n build/native_stage_object0_alternate_probe.elf > build/native_stage_object0_alternate_probe.sym
	m68k-elf-objcopy -O binary build/native_stage_object0_alternate_probe.elf build/native_stage_object0_alternate_probe.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_COMPILED_STAGE0_ALTERNATE tools/test_arcade_stage_object0_alternate.c -o build/test_native_stage_object0_alternate
	build/test_native_stage_object0_alternate "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-stage-object0_alternate
test-arcade-stage-object0_alternate: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_stage_object0_alternate.c -o build/test_arcade_stage_object0_alternate
	build/test_arcade_stage_object0_alternate "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"


.PHONY: test-native-stage-object1_alternate
test-native-stage-object1_alternate: build/reference_maincpu.bin
	@test -n "$(SGDK_ROOT)" || (echo "Set SGDK_ROOT to an SGDK checkout"; exit 1)
	m68k-elf-gcc -m68000 -O2 -Wall -Wextra -Werror -fno-builtin -DSGDK_GCC -I"$(SGDK_ROOT)/inc" -nostdlib -Wl,-Ttext=0x110000,-e,stageObject1Probe tools/native_stage_object1_alternate_probe.c -o build/native_stage_object1_alternate_probe.elf
	m68k-elf-nm -n build/native_stage_object1_alternate_probe.elf > build/native_stage_object1_alternate_probe.sym
	m68k-elf-objcopy -O binary build/native_stage_object1_alternate_probe.elf build/native_stage_object1_alternate_probe.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_COMPILED_STAGE1_ALTERNATE tools/test_arcade_stage_object1_alternate.c -o build/test_native_stage_object1_alternate
	build/test_native_stage_object1_alternate "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-stage-object1_alternate
test-arcade-stage-object1_alternate: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_stage_object1_alternate.c -o build/test_arcade_stage_object1_alternate
	build/test_arcade_stage_object1_alternate "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"


.PHONY: test-arcade-stage1-population
test-arcade-stage1-population: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_stage1_population.c -o build/test_arcade_stage1_population
	build/test_arcade_stage1_population "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-native-stage-family6
test-native-stage-family6: build/reference_maincpu.bin
	@test -n "$(SGDK_ROOT)" || (echo "Set SGDK_ROOT to an SGDK checkout"; exit 1)
	m68k-elf-gcc -m68000 -O2 -Wall -Wextra -Werror -fno-builtin -DSGDK_GCC -I"$(SGDK_ROOT)/inc" -nostdlib -Wl,-Ttext=0x110000,-e,stageFamily6Probe tools/native_stage_family6_probe.c -o build/native_stage_family6_probe.elf
	m68k-elf-nm -n build/native_stage_family6_probe.elf > build/native_stage_family6_probe.sym
	m68k-elf-objcopy -O binary build/native_stage_family6_probe.elf build/native_stage_family6_probe.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_COMPILED_FAMILY6 tools/test_arcade_stage_family6.c -o build/test_native_stage_family6
	build/test_native_stage_family6 "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-stage-family6
test-arcade-stage-family6: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_stage_family6.c -o build/test_arcade_stage_family6
	build/test_arcade_stage_family6 "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"


.PHONY: test-arcade-stage2-population
test-arcade-stage2-population: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_stage2_population.c -o build/test_arcade_stage2_population
	build/test_arcade_stage2_population "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"


.PHONY: test-native-stage-object25_32
test-native-stage-object25_32: build/reference_maincpu.bin
	@test -n "$(SGDK_ROOT)" || (echo "Set SGDK_ROOT to an SGDK checkout"; exit 1)
	m68k-elf-gcc -m68000 -O2 -Wall -Wextra -Werror -fno-builtin -DSGDK_GCC -I"$(SGDK_ROOT)/inc" -nostdlib -Wl,-Ttext=0x110000,-e,stageObject25Or32Probe tools/native_stage_object25_32_probe.c -o build/native_stage_object25_32_probe.elf
	m68k-elf-nm -n build/native_stage_object25_32_probe.elf > build/native_stage_object25_32_probe.sym
	m68k-elf-objcopy -O binary build/native_stage_object25_32_probe.elf build/native_stage_object25_32_probe.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_COMPILED_STAGE25_32 tools/test_arcade_stage_object25_32.c -o build/test_native_stage_object25_32
	build/test_native_stage_object25_32 "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-stage-object25_32
test-arcade-stage-object25_32: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_stage_object25_32.c -o build/test_arcade_stage_object25_32
	build/test_arcade_stage_object25_32 "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"


.PHONY: test-native-stage-family12
test-native-stage-family12: build/reference_maincpu.bin
	@test -n "$(SGDK_ROOT)" || (echo "Set SGDK_ROOT to an SGDK checkout"; exit 1)
	m68k-elf-gcc -m68000 -O2 -Wall -Wextra -Werror -fno-builtin -DSGDK_GCC -I"$(SGDK_ROOT)/inc" -nostdlib -Wl,-Ttext=0x110000,-e,stageFamily12Probe tools/native_stage_family12_probe.c -o build/native_stage_family12_probe.elf
	m68k-elf-nm -n build/native_stage_family12_probe.elf > build/native_stage_family12_probe.sym
	m68k-elf-objcopy -O binary build/native_stage_family12_probe.elf build/native_stage_family12_probe.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_COMPILED_FAMILY12 tools/test_arcade_stage_family12.c -o build/test_native_stage_family12
	build/test_native_stage_family12 "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-stage-family12
test-arcade-stage-family12: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_stage_family12.c -o build/test_arcade_stage_family12
	build/test_arcade_stage_family12 "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"



.PHONY: test-arcade-stage3-population
test-arcade-stage3-population: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_stage3_population.c -o build/test_arcade_stage3_population
	build/test_arcade_stage3_population "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"



.PHONY: test-native-stage-object27
test-native-stage-object27: build/reference_maincpu.bin
	@test -n "$(SGDK_ROOT)" || (echo "Set SGDK_ROOT to an SGDK checkout"; exit 1)
	m68k-elf-gcc -m68000 -O2 -Wall -Wextra -Werror -fno-builtin -DSGDK_GCC -I"$(SGDK_ROOT)/inc" -nostdlib -Wl,-Ttext=0x110000,-e,stageObject27Probe tools/native_stage_object27_probe.c -o build/native_stage_object27_probe.elf
	m68k-elf-nm -n build/native_stage_object27_probe.elf > build/native_stage_object27_probe.sym
	m68k-elf-objcopy -O binary build/native_stage_object27_probe.elf build/native_stage_object27_probe.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_COMPILED_STAGE27 tools/test_arcade_stage_object27.c -o build/test_native_stage_object27
	build/test_native_stage_object27 "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-stage-object27
test-arcade-stage-object27: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_stage_object27.c -o build/test_arcade_stage_object27
	build/test_arcade_stage_object27 "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-stage5-population
test-arcade-stage5-population: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_stage5_population.c -o build/test_arcade_stage5_population
	build/test_arcade_stage5_population "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"


.PHONY: test-native-stage-object26
test-native-stage-object26: build/reference_maincpu.bin
	@test -n "$(SGDK_ROOT)" || (echo "Set SGDK_ROOT to an SGDK checkout"; exit 1)
	m68k-elf-gcc -m68000 -O2 -Wall -Wextra -Werror -fno-builtin -DSGDK_GCC -I"$(SGDK_ROOT)/inc" -nostdlib -Wl,-Ttext=0x110000,-e,stageObject26Probe tools/native_stage_object26_probe.c -o build/native_stage_object26_probe.elf
	m68k-elf-nm -n build/native_stage_object26_probe.elf > build/native_stage_object26_probe.sym
	m68k-elf-objcopy -O binary build/native_stage_object26_probe.elf build/native_stage_object26_probe.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_COMPILED_STAGE26 tools/test_arcade_stage_object26.c -o build/test_native_stage_object26
	build/test_native_stage_object26 "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-stage-object26
test-arcade-stage-object26: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_stage_object26.c -o build/test_arcade_stage_object26
	build/test_arcade_stage_object26 "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"


.PHONY: test-native-stage-object31
test-native-stage-object31: build/reference_maincpu.bin
	@test -n "$(SGDK_ROOT)" || (echo "Set SGDK_ROOT to an SGDK checkout"; exit 1)
	m68k-elf-gcc -m68000 -O2 -Wall -Wextra -Werror -fno-builtin -DSGDK_GCC -I"$(SGDK_ROOT)/inc" -nostdlib -Wl,-Ttext=0x110000,-e,stageObject31Probe tools/native_stage_object31_probe.c -o build/native_stage_object31_probe.elf
	m68k-elf-nm -n build/native_stage_object31_probe.elf > build/native_stage_object31_probe.sym
	m68k-elf-objcopy -O binary build/native_stage_object31_probe.elf build/native_stage_object31_probe.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_COMPILED_STAGE31 tools/test_arcade_stage_object31.c -o build/test_native_stage_object31
	build/test_native_stage_object31 "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-stage-object31
test-arcade-stage-object31: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_stage_object31.c -o build/test_arcade_stage_object31
	build/test_arcade_stage_object31 "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"


.PHONY: test-arcade-stage4-population
test-arcade-stage4-population: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_stage4_population.c -o build/test_arcade_stage4_population
	build/test_arcade_stage4_population "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"



.PHONY: test-native-tile-lookup-packed
test-native-tile-lookup-packed:
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_native_tile_lookup_packed.c -o build/test_native_tile_lookup_packed
	build/test_native_tile_lookup_packed

.PHONY: test-native-tile-lookup-nibble
test-native-tile-lookup-nibble:
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_native_tile_lookup_nibble.c -o build/test_native_tile_lookup_nibble
	build/test_native_tile_lookup_nibble

.PHONY: test-native-tile-mirror
test-native-tile-mirror:
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_native_tile_mirror.c -o build/test_native_tile_mirror
	build/test_native_tile_mirror

.PHONY: test-native-tile-mirror-target
test-native-tile-mirror-target:
	@test -n "$(SGDK_ROOT)" || (echo "Set SGDK_ROOT to an SGDK checkout"; exit 1)
	m68k-elf-gcc -m68000 -O2 -Wall -Wextra -Werror -DSGDK_GCC -I"$(SGDK_ROOT)/inc" -nostdlib -ffreestanding -Wl,-Ttext=0x100010,-e,tileMirrorProbe tools/tile_mirror_probe.c -o build/tile_mirror_probe.elf
	m68k-elf-objcopy -O binary build/tile_mirror_probe.elf build/tile_mirror_probe.bin
	m68k-elf-nm -n build/tile_mirror_probe.elf > build/tile_mirror_probe.sym
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_native_tile_mirror_target.c -o build/test_native_tile_mirror_target
	build/test_native_tile_mirror_target "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-native-tile-mirror-transpose
test-native-tile-mirror-transpose:
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_native_tile_mirror_transpose.c -o build/test_native_tile_mirror_transpose
	build/test_native_tile_mirror_transpose

.PHONY: test-native-attribute-mirror
test-native-attribute-mirror:
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_native_attribute_mirror.c -o build/test_native_attribute_mirror
	build/test_native_attribute_mirror

.PHONY: test-native-intro-attribute-mirror
test-native-intro-attribute-mirror:
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_native_intro_attribute_mirror.c -o build/test_native_intro_attribute_mirror
	build/test_native_intro_attribute_mirror

.PHONY: test-native-actor-selector1-target
test-native-actor-selector1-target: build/reference_maincpu.bin
	@test -n "$(SGDK_ROOT)" || (echo "Set SGDK_ROOT to an SGDK checkout"; exit 1)
	m68k-elf-gcc -m68000 -O2 -Wall -Wextra -Werror -DSGDK_GCC -I"$(SGDK_ROOT)/inc" -nostdlib -ffreestanding -Wl,-Ttext=0x100010,-e,actorSelector1Probe tools/actor_selector1_probe.c -o build/actor_selector1_probe.elf
	m68k-elf-objcopy -O binary build/actor_selector1_probe.elf build/actor_selector1_probe.bin
	m68k-elf-nm -n build/actor_selector1_probe.elf > build/actor_selector1_probe.sym
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_NATIVE_SELECTOR1 tools/test_arcade_actor_selector1.c -o build/test_native_actor_selector1_target
	build/test_native_actor_selector1_target "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-native-sat-storage
test-native-sat-storage: intro-probe
	clang -O2 tools/intro_rom_smoke.c -o build/intro_rom_smoke
	build/intro_rom_smoke "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" native/intro_probe/out/rom.bin native/intro_probe/out/symbol.txt --sat-cache-probe
	build/intro_rom_smoke "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" native/intro_probe/out/rom.bin native/intro_probe/out/symbol.txt --runtime-memory-probe

.PHONY: test-native-player-palette-owner
test-native-player-palette-owner: intro-probe
	clang -O2 tools/intro_rom_smoke.c -o build/intro_rom_smoke
	build/intro_rom_smoke "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" native/intro_probe/out/rom.bin native/intro_probe/out/symbol.txt --palette-owner-probe
	build/intro_rom_smoke "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" native/intro_probe/out/rom.bin native/intro_probe/out/symbol.txt --runtime-memory-probe

.PHONY: test-native-task-storage-lifetime
test-native-task-storage-lifetime: intro-probe
	clang -O2 tools/intro_rom_smoke.c -o build/intro_rom_smoke
	build/intro_rom_smoke "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" native/intro_probe/out/rom.bin native/intro_probe/out/symbol.txt --task-lifetime-probe
	build/intro_rom_smoke "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" native/intro_probe/out/rom.bin native/intro_probe/out/symbol.txt --all-pool-allocate-probe

native/intro_probe/src/task_startup_data.h: tools/native_task_startup_data.py build/reference_maincpu.bin
	$(PYTHON) tools/native_task_startup_data.py
intro-probe: native/intro_probe/src/task_startup_data.h

.PHONY: native-frame-history test-native-frame-history
native-frame-history:
	@test -n "$(SGDK_ROOT)" || (echo "Set SGDK_ROOT to an SGDK checkout"; exit 1)
	$(PYTHON) tools/native_frame_history.py "$(SGDK_ROOT)"
intro-probe: native-frame-history
test-native-frame-history: native-frame-history
	$(PYTHON) tools/test_native_frame_history.py "$(SGDK_ROOT)"

.PHONY: native-xgm-history test-native-xgm-history
native-xgm-history:
	@test -n "$(SGDK_ROOT)" || (echo "Set SGDK_ROOT to an SGDK checkout"; exit 1)
	$(PYTHON) tools/native_xgm_history.py "$(SGDK_ROOT)"
intro-probe: native-xgm-history
test-native-xgm-history: native-xgm-history
	$(PYTHON) tools/test_native_xgm_history.py "$(SGDK_ROOT)"

.PHONY: test-native-task-initialization
.PHONY: test-native-sound-ring
test-native-sound-ring: test-native-xgm-history test-arcade-startup-latched intro-probe
	clang -O2 tools/intro_rom_smoke.c -o build/intro_rom_smoke
	build/intro_rom_smoke "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" native/intro_probe/out/rom.bin native/intro_probe/out/symbol.txt --task-init-probe
	build/intro_rom_smoke "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" native/intro_probe/out/rom.bin native/intro_probe/out/symbol.txt --task-lifetime-probe
	build/intro_rom_smoke "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" native/intro_probe/out/rom.bin native/intro_probe/out/symbol.txt --runtime-memory-probe

.PHONY: test-native-high-score-storage
test-native-high-score-storage: test-arcade-high-score intro-probe
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_native_high_scores.c -o build/test_native_high_scores
	build/test_native_high_scores
	clang -O2 tools/intro_rom_smoke.c -o build/intro_rom_smoke
	build/intro_rom_smoke "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" native/intro_probe/out/rom.bin native/intro_probe/out/symbol.txt --task-init-probe
	build/intro_rom_smoke "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" native/intro_probe/out/rom.bin native/intro_probe/out/symbol.txt --task-lifetime-probe

.PHONY: test-native-boot-globals
test-native-boot-globals: test-native-reset-boot-chain intro-probe
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_native_boot_globals.c -o build/test_native_boot_globals
	build/test_native_boot_globals
	clang -O2 tools/intro_rom_smoke.c -o build/intro_rom_smoke
	build/intro_rom_smoke "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" native/intro_probe/out/rom.bin native/intro_probe/out/symbol.txt --boot-globals-probe
	build/intro_rom_smoke "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" native/intro_probe/out/rom.bin native/intro_probe/out/symbol.txt --task-lifetime-probe
	$(PYTHON) tools/report_reset_boot_storage.py

.PHONY: test-native-hud-cell-index
.PHONY: test-native-hud-sprite
test-native-hud-sprite: intro-probe
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_native_hud_sprite.c -o build/test_native_hud_sprite
	build/test_native_hud_sprite
	clang -O2 tools/intro_rom_smoke.c -o build/intro_rom_smoke
	build/intro_rom_smoke "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" native/intro_probe/out/rom.bin native/intro_probe/out/symbol.txt --walk-probe

test-native-hud-cell-index: intro-probe
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_native_hud_cell_index.c -o build/test_native_hud_cell_index
	build/test_native_hud_cell_index
	clang -O2 tools/intro_rom_smoke.c -o build/intro_rom_smoke
	build/intro_rom_smoke "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" native/intro_probe/out/rom.bin native/intro_probe/out/symbol.txt --walk-probe

test-native-task-initialization: test-arcade-tasks test-arcade-task-startup test-arcade-startup-latched intro-probe
	clang -O2 tools/intro_rom_smoke.c -o build/intro_rom_smoke
	build/intro_rom_smoke "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" native/intro_probe/out/rom.bin native/intro_probe/out/symbol.txt --task-init-probe

# Full latched startup through the compiled sparse native bus. Preserve the
# normal ROM on exit; this terminal experiment does not activate scheduling.
.PHONY: test-native-startup-latched
test-native-startup-latched: test-arcade-startup-latched test-arcade-palette-initialization test-arcade-palette-task test-arcade-reset-ram test-native-video-publish-target test-native-cps-color
	@test -n "$(SGDK_ROOT)" || (echo "Set SGDK_ROOT to an SGDK checkout"; exit 1)
	build/test_arcade_startup_latched "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" --native-fixtures
	@set -e; trap 'rm -f native/intro_probe/out/src/main.o; $(MAKE) intro-probe EXTRA_FLAGS="$(EXTRA_FLAGS)"' EXIT; \
	 rm -f native/intro_probe/out/src/main.o; \
	 $(MAKE) intro-probe EXTRA_FLAGS="$(EXTRA_FLAGS) -DWILLOW_STARTUP_LATCHED_PROBE -DWILLOW_VIDEO_PUBLISH_PROBE"; \
	 cp native/intro_probe/out/rom.bin build/startup-latched-experiment.bin; \
	 cp native/intro_probe/out/symbol.txt build/startup-latched-experiment.sym; \
	 clang -O2 tools/intro_rom_smoke.c -o build/intro_rom_smoke; \
	 build/intro_rom_smoke "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" build/startup-latched-experiment.bin build/startup-latched-experiment.sym --startup-latched-probe; \
	 build/intro_rom_smoke "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" build/startup-latched-experiment.bin build/startup-latched-experiment.sym --source-backdrop-hide-probe; \
	 build/intro_rom_smoke "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" build/startup-latched-experiment.bin build/startup-latched-experiment.sym --sat-publication-probe; \
	 $(PYTHON) tools/verify_source_layer_hide.py


.PHONY: test-native-word-fill-target
test-native-word-fill-target:
	@test -n "$(SGDK_ROOT)" || (echo "Set SGDK_ROOT to an SGDK checkout"; exit 1)
	m68k-elf-gcc -m68000 -O2 -Wall -Wextra -Werror -fno-builtin -DSGDK_GCC -I"$(SGDK_ROOT)/inc" -nostdlib -Wl,-Ttext=0x100010,-e,wordFillProbe tools/word_fill_probe.c -o build/word_fill_probe.elf
	m68k-elf-objcopy -O binary build/word_fill_probe.elf build/word_fill_probe.bin
	m68k-elf-nm -n build/word_fill_probe.elf > build/word_fill_probe.sym
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_word_fill_target.c -o build/test_word_fill_target
	build/test_word_fill_target "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"


.PHONY: test-cps-video-control
test-cps-video-control:
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_cps_video_control.c -o build/test_cps_video_control
	build/test_cps_video_control

.PHONY: test-arcade-video-publish
test-arcade-video-publish: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_video_publish.c -o build/test_arcade_video_publish
	build/test_arcade_video_publish "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-native-video-publish-target
test-native-video-publish-target:
	@test -n "$(SGDK_ROOT)" || (echo "Set SGDK_ROOT to an SGDK checkout"; exit 1)
	m68k-elf-gcc -m68000 -O2 -Wall -Wextra -Werror -fno-builtin -DSGDK_GCC -I"$(SGDK_ROOT)/inc" -nostdlib -Wl,-Ttext=0x100010,-e,videoPublishProbe tools/video_publish_probe.c -o build/video_publish_probe.elf
	m68k-elf-objcopy -O binary build/video_publish_probe.elf build/video_publish_probe.bin
	m68k-elf-nm -n build/video_publish_probe.elf > build/video_publish_probe.sym
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_video_publish.c -o build/test_arcade_video_publish
	build/test_arcade_video_publish "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" --compiled

.PHONY: test-native-video-publication
test-native-video-publication: test-native-video-publish-target test-arcade-reset-ram
	@test -n "$(SGDK_ROOT)" || (echo "Set SGDK_ROOT to an SGDK checkout"; exit 1)
	@set -e; trap 'rm -f native/intro_probe/out/src/main.o; $(MAKE) intro-probe EXTRA_FLAGS="$(EXTRA_FLAGS)"' EXIT; \
	 rm -f native/intro_probe/out/src/main.o; \
	 $(MAKE) intro-probe EXTRA_FLAGS="$(EXTRA_FLAGS) -DWILLOW_VIDEO_PUBLISH_PROBE"; \
	 cp native/intro_probe/out/rom.bin build/video-publish-native.bin; \
	 cp native/intro_probe/out/symbol.txt build/video-publish-native.sym; \
	 clang -O2 tools/intro_rom_smoke.c -o build/intro_rom_smoke; \
	 build/intro_rom_smoke "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" build/video-publish-native.bin build/video-publish-native.sym --video-publish-probe

.PHONY: test-native-plane-hide-feasibility
test-native-plane-hide-feasibility:
	clang -O2 tools/intro_rom_smoke.c -o build/intro_rom_smoke
	build/intro_rom_smoke "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" native/intro_probe/out/rom.bin native/intro_probe/out/symbol.txt --layer-clear-probe

.PHONY: test-native-background-hide-feasibility
test-native-background-hide-feasibility:
	clang -O2 tools/intro_rom_smoke.c -o build/intro_rom_smoke
	build/intro_rom_smoke "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" native/intro_probe/out/rom.bin native/intro_probe/out/symbol.txt --layer-window-clear-probe
	build/intro_rom_smoke "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" native/intro_probe/out/rom.bin native/intro_probe/out/symbol.txt --layer-palette-hide-probe

.PHONY: test-native-cps-color
test-native-cps-color:
	$(PYTHON) tools/native_cps_color.py --check

.PHONY: test-native-controller
test-native-controller:
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_native_controller.c -o build/test_native_controller
	build/test_native_controller

intro-probe: native/intro_probe/src/input_data.h

.PHONY: test-native-controller-history
test-native-controller-history: test-arcade-video-publish intro-probe
	build/test_arcade_video_publish "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" --controller-fixtures
	clang -O2 tools/intro_rom_smoke.c -o build/intro_rom_smoke
	build/intro_rom_smoke "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" native/intro_probe/out/rom.bin native/intro_probe/out/symbol.txt --controller-history-probe

.PHONY: test-native-boot-continuation
test-native-boot-continuation: test-native-boot-scheduler
	@test -n "$(SGDK_ROOT)" || (echo "Set SGDK_ROOT to an SGDK checkout"; exit 1)
	build/test_native_boot_scheduler "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" --native-fixtures
	@set -e; trap 'touch native/intro_probe/src/main.c; $(MAKE) intro-probe EXTRA_FLAGS="$(EXTRA_FLAGS)"' EXIT; \
	 touch native/intro_probe/src/main.c; \
	 $(MAKE) intro-probe EXTRA_FLAGS="$(EXTRA_FLAGS) -DWILLOW_STARTUP_LATCHED_PROBE -DWILLOW_VIDEO_PUBLISH_PROBE"; \
	 cp native/intro_probe/out/rom.bin build/boot-continuation-experiment.bin; \
	 cp native/intro_probe/out/symbol.txt build/boot-continuation-experiment.sym; \
	 clang -O2 tools/intro_rom_smoke.c -o build/intro_rom_smoke; \
	 build/intro_rom_smoke "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" build/boot-continuation-experiment.bin build/boot-continuation-experiment.sym --boot-continuation-probe

.PHONY: test-native-boot-fade-prefix
test-native-boot-fade-prefix: test-native-boot-clear-chain test-arcade-palette-initialization
	@test -n "$(SGDK_ROOT)" || (echo "Set SGDK_ROOT to an SGDK checkout"; exit 1)
	@set -e; trap 'touch native/intro_probe/src/main.c; $(MAKE) intro-probe EXTRA_FLAGS="$(EXTRA_FLAGS)"' EXIT; \
	 touch native/intro_probe/src/main.c; \
	 $(MAKE) intro-probe EXTRA_FLAGS="$(EXTRA_FLAGS) -DWILLOW_STARTUP_LATCHED_PROBE -DWILLOW_VIDEO_PUBLISH_PROBE"; \
	 cp native/intro_probe/out/rom.bin build/boot-fade-prefix-experiment.bin; \
	 cp native/intro_probe/out/symbol.txt build/boot-fade-prefix-experiment.sym; \
	 clang -O2 tools/intro_rom_smoke.c -o build/intro_rom_smoke; \
	 build/intro_rom_smoke "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" build/boot-fade-prefix-experiment.bin build/boot-fade-prefix-experiment.sym --boot-fade-prefix-probe

.PHONY: test-native-scene-map-fill
.PHONY: test-native-scene-boot
test-native-scene-boot: test-arcade-opening-frame test-arcade-opening-scene-run test-native-backdrop-chunks test-native-title-resources test-native-opening-resources test-native-attract-chunks test-arcade-attract-scene-init test-native-boot-clear-chain test-native-boot-text-chain test-native-boot-handoff-chain test-native-boot-attract-chain test-native-palette-attract test-arcade-palette-initialization test-native-scene-memory test-native-plane-retire
	@test -n "$(SGDK_ROOT)" || (echo "Set SGDK_ROOT to an SGDK checkout"; exit 1)
	@set -e; trap 'touch native/intro_probe/src/main.c native/intro_probe/src/boot/rom_head.c; $(MAKE) intro-probe EXTRA_FLAGS="$(EXTRA_FLAGS)"' EXIT; \
	 touch native/intro_probe/src/main.c native/intro_probe/src/boot/rom_head.c; \
	 $(MAKE) intro-probe EXTRA_FLAGS="$(EXTRA_FLAGS) -DWILLOW_OBJECT_CARTRIDGE_PROBE -DWILLOW_STARTUP_LATCHED_PROBE -DWILLOW_SCENE_BOOT_PROBE"; \
	 cp native/intro_probe/out/rom.bin build/scene-boot-experiment.bin; \
	 cp native/intro_probe/out/symbol.txt build/scene-boot-experiment.sym; \
	 clang -O2 tools/intro_rom_smoke.c -o build/intro_rom_scene_boot; \
	 build/intro_rom_scene_boot "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" build/scene-boot-experiment.bin build/scene-boot-experiment.sym --scene-boot-probe; \
	 build/intro_rom_scene_boot "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" build/scene-boot-experiment.bin build/scene-boot-experiment.sym --scene-combined-probe; \
	 build/intro_rom_scene_boot "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" build/scene-boot-experiment.bin build/scene-boot-experiment.sym --scene-text-probe; \
	 build/intro_rom_scene_boot "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" build/scene-boot-experiment.bin build/scene-boot-experiment.sym --scene-warning-probe; \
	 build/intro_rom_scene_boot "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" build/scene-boot-experiment.bin build/scene-boot-experiment.sym --scene-handoff-probe; \
	 build/intro_rom_scene_boot "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" build/scene-boot-experiment.bin build/scene-boot-experiment.sym --scene-service-handoff-probe; \
	 build/intro_rom_scene_boot "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" build/scene-boot-experiment.bin build/scene-boot-experiment.sym --scene-attract-init-probe; \
	 build/intro_rom_scene_boot "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" build/scene-boot-experiment.bin build/scene-boot-experiment.sym --scene-attract-fade-probe; \
	 build/intro_rom_scene_boot "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" build/scene-boot-experiment.bin build/scene-boot-experiment.sym --scene-pools-probe; \
	 build/intro_rom_scene_boot "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" build/scene-boot-experiment.bin build/scene-boot-experiment.sym --scene-initialize-probe; \
	 build/intro_rom_scene_boot "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" build/scene-boot-experiment.bin build/scene-boot-experiment.sym --scene-dispatch-probe; \
	 build/intro_rom_scene_boot "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" build/scene-boot-experiment.bin build/scene-boot-experiment.sym --scene-spawn-probe; \
	 build/intro_rom_scene_boot "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" build/scene-boot-experiment.bin build/scene-boot-experiment.sym --scene-countdown-probe; \
	 build/intro_rom_scene_boot "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" build/scene-boot-experiment.bin build/scene-boot-experiment.sym --boot-fade-prefix-probe

test-native-scene-map-fill: test-native-scene-clear
	@test -n "$(SGDK_ROOT)" || (echo "Set SGDK_ROOT to an SGDK checkout"; exit 1)
	@set -e; trap 'touch native/intro_probe/src/main.c; $(MAKE) intro-probe EXTRA_FLAGS="$(EXTRA_FLAGS)"' EXIT; \
	 touch native/intro_probe/src/main.c; \
	 $(MAKE) intro-probe EXTRA_FLAGS="$(EXTRA_FLAGS) -DWILLOW_SCENE_MAP_FILL_PROBE"; \
	 cp native/intro_probe/out/rom.bin build/scene-map-fill-experiment.bin; \
	 cp native/intro_probe/out/symbol.txt build/scene-map-fill-experiment.sym; \
	 clang -O2 tools/intro_rom_smoke.c -o build/intro_rom_scene_map_fill; \
	 build/intro_rom_scene_map_fill "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" build/scene-map-fill-experiment.bin build/scene-map-fill-experiment.sym --scene-map-fill-probe

.PHONY: test-native-sprite-bank
test-native-sprite-bank: test-arcade-reset-ram
	@test -n "$(SGDK_ROOT)" || (echo "Set SGDK_ROOT to an SGDK checkout"; exit 1)
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined tools/test_native_boot_globals.c -o build/test_native_boot_globals
	build/test_native_boot_globals
	clang -O2 tools/test_arcade_player_sprite_geometry.c -o build/test_arcade_player_sprite_geometry
	build/test_arcade_player_sprite_geometry "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"
	@set -e; trap 'touch native/intro_probe/src/main.c; $(MAKE) intro-probe EXTRA_FLAGS="$(EXTRA_FLAGS)"' EXIT; \
	 touch native/intro_probe/src/main.c; \
	 $(MAKE) intro-probe EXTRA_FLAGS="$(EXTRA_FLAGS) -DWILLOW_SPRITE_BANK_PROBE -DWILLOW_VIDEO_PUBLISH_PROBE"; \
	 cp native/intro_probe/out/rom.bin build/sprite-bank-experiment.bin; \
	 cp native/intro_probe/out/symbol.txt build/sprite-bank-experiment.sym; \
	 clang -O2 tools/intro_rom_smoke.c -o build/intro_rom_sprite_bank; \
	 build/intro_rom_sprite_bank "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" build/sprite-bank-experiment.bin build/sprite-bank-experiment.sym --sprite-bank-probe; \
	 build/intro_rom_sprite_bank "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" build/sprite-bank-experiment.bin build/sprite-bank-experiment.sym --video-publish-probe

.PHONY: test-native-object-banks
test-native-object-banks: build/reference_maincpu.bin
	@test -n "$(SGDK_ROOT)" || (echo "Set SGDK_ROOT to an SGDK checkout"; exit 1)
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined tools/test_native_object_banks.c -o build/test_native_object_banks
	build/test_native_object_banks
	m68k-elf-gcc -m68000 -O2 -Wall -Wextra -Werror -fno-builtin -DSGDK_GCC -I"$(SGDK_ROOT)/inc" -nostdlib -Wl,-Ttext=0x100010,-e,objectBanksProbe tools/object_banks_probe.c -o build/object_banks_probe.elf
	m68k-elf-objcopy -O binary build/object_banks_probe.elf build/object_banks_probe.bin
	m68k-elf-nm -n build/object_banks_probe.elf > build/object_banks_probe.sym
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined tools/test_object_banks_target.c -o build/test_object_banks_target
	build/test_object_banks_target "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"
	clang -O2 tools/test_arcade_player_sprite_geometry.c -o build/test_arcade_player_sprite_geometry
	build/test_arcade_player_sprite_geometry "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

# Feasibility diagnostic only; does not reserve cartridge RAM in the game.
.PHONY: test-native-object-banks-cartridge
test-native-object-banks-cartridge: test-native-object-banks
	m68k-elf-gcc -m68000 -O2 -Wall -Wextra -Werror -fno-builtin -DSGDK_GCC -DWILLOW_OBJECT_PROBE_CARTRIDGE -I"$(SGDK_ROOT)/inc" -nostdlib -Wl,-Ttext=0x100010,-e,objectBanksProbe tools/object_banks_probe.c -o build/object_banks_cartridge_probe.elf
	m68k-elf-objcopy -O binary build/object_banks_cartridge_probe.elf build/object_banks_cartridge_probe.bin
	m68k-elf-nm -n build/object_banks_cartridge_probe.elf > build/object_banks_cartridge_probe.sym
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -DWILLOW_OBJECT_PROBE_CARTRIDGE tools/test_object_banks_target.c -o build/test_object_banks_cartridge
	build/test_object_banks_cartridge "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"
	build/test_object_banks_cartridge "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" --cycles
	build/test_object_banks_target "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" --cycles

.PHONY: test-native-object-irq-prefix
test-native-object-irq-prefix: test-native-video-publish-target
	build/test_arcade_video_publish "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" --irq-prefix
	build/test_arcade_video_publish "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" --compiled-irq-prefix

.PHONY: test-native-object-resident
test-native-object-resident: test-native-object-irq-prefix test-native-object-banks-cartridge test-arcade-transition-clear
	@test -n "$(SGDK_ROOT)" || (echo "Set SGDK_ROOT to an SGDK checkout"; exit 1)
	@set -e; trap 'touch native/intro_probe/src/main.c native/intro_probe/src/boot/rom_head.c; $(MAKE) intro-probe EXTRA_FLAGS="$(EXTRA_FLAGS)"' EXIT; \
	 touch native/intro_probe/src/main.c native/intro_probe/src/boot/rom_head.c; \
	 $(MAKE) intro-probe EXTRA_FLAGS="$(EXTRA_FLAGS) -DWILLOW_OBJECT_CARTRIDGE_PROBE -DWILLOW_SPRITE_BANK_PROBE"; \
	 cp native/intro_probe/out/rom.bin build/object-resident-experiment.bin; \
	 cp native/intro_probe/out/symbol.txt build/object-resident-experiment.sym; \
	 clang -O2 tools/intro_rom_smoke.c -o build/intro_rom_object_resident; \
	 build/intro_rom_object_resident "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" build/object-resident-experiment.bin build/object-resident-experiment.sym --object-cartridge-probe

.PHONY: test-native-plane-retire
test-native-plane-retire: build/reference_maincpu.bin test-native-tile-slots
	@test -n "$(SGDK_ROOT)" || (echo "Set SGDK_ROOT to an SGDK checkout"; exit 1)
	m68k-elf-gcc -m68000 -O2 -Wall -Wextra -Werror -fno-builtin -DSGDK_GCC -I"$(SGDK_ROOT)/inc" -nostdlib -Wl,-Ttext=0x100010,-e,planeRetireProbe tools/plane_retire_probe.c -o build/plane_retire_probe.elf
	m68k-elf-objcopy -O binary build/plane_retire_probe.elf build/plane_retire_probe.bin
	m68k-elf-nm -n build/plane_retire_probe.elf > build/plane_retire_probe.sym
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_plane_retire_target.c -o build/test_plane_retire_target
	build/test_plane_retire_target "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-native-palette-attract
test-native-palette-attract: test-arcade-attract-init
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined tools/test_native_palette_attract.c -o build/test_native_palette_attract
	build/test_native_palette_attract

.PHONY: test-native-backdrop-chunks
test-native-backdrop-chunks: native/intro_probe/src/scroll3_data.h test-arcade-attract-backdrop
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined tools/test_native_backdrop_chunks.c -o build/test_native_backdrop_chunks
	build/test_native_backdrop_chunks
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -DWILLOW_TEST_BACKDROP_COPY tools/test_arcade_attract_backdrop.c -o build/test_arcade_attract_backdrop_copy
	build/test_arcade_attract_backdrop_copy "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-native-attract-chunks
test-native-attract-chunks: native/intro_probe/src/camera_data.h test-arcade-attract-scene-spawn
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined tools/test_native_attract_chunks.c -o build/test_native_attract_chunks
	build/test_native_attract_chunks
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -DWILLOW_TEST_ATTRACT_MAP_COPY tools/test_arcade_attract_scene_spawn.c -o build/test_arcade_attract_scene_spawn_copy
	build/test_arcade_attract_scene_spawn_copy "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

# Opening message data is checked against original instruction execution.
native/intro_probe/src/opening_message_data.h: tools/native_opening_message_data.py build/reference_maincpu.bin
	$(PYTHON) tools/native_opening_message_data.py
intro-probe: native/intro_probe/src/opening_message_data.h
.PHONY: test-native-opening-message-resources
test-native-opening-message-resources: native/intro_probe/src/opening_message_data.h
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_OPENING_MESSAGE_DATA tools/test_native_message_task.c -o build/test_native_opening_message_resources
	build/test_native_opening_message_resources "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"
test-native-scene-boot: test-native-opening-message-resources

# Resident scene palette dispatch supplies bank/selected-group fade callbacks.
test-native-scene-boot: test-arcade-palette-bank-task test-native-palette-bank-scheduler test-arcade-palette-selected-task test-native-palette-selected-scheduler

.PHONY: test-native-attract-pan-step
test-native-attract-pan-step: build/reference_maincpu.bin native/intro_probe/src/task_startup_data.h
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_PAN_OUTER tools/test_arcade_attract_pan_step.c -o build/test_native_attract_pan_step
	build/test_native_attract_pan_step "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"
test-native-scene-boot: test-native-attract-pan-step

.PHONY: test-native-pan-palette-live
test-native-pan-palette-live: native/intro_probe/src/palette_initial_data.h
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_NATIVE_OPENING_DATA -DWILLOW_TEST_PAN_PALETTE_LIVE tools/test_arcade_opening_actors.c -o build/test_native_pan_palette_live
	build/test_native_pan_palette_live "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"
test-native-scene-boot: test-native-pan-palette-live

.PHONY: test-native-attract-pan-wait
test-native-attract-pan-wait: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_NATIVE_PAN_WAIT tools/test_arcade_attract_pan_wait.c -o build/test_native_attract_pan_wait
	build/test_native_attract_pan_wait "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"
test-native-scene-boot: test-native-attract-pan-wait

# A fresh original-instruction trace is required by the cartridge cycle check.
.PHONY: test-arcade-pan-schedule
test-arcade-pan-schedule: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_opening_lifecycle.c -o build/test_arcade_opening_lifecycle
	build/test_arcade_opening_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" --phase-two
test-native-scene-boot: test-arcade-pan-schedule

.PHONY: test-native-attract-text-sequence
test-native-attract-text-sequence: build/reference_maincpu.bin native/intro_probe/src/task_startup_data.h
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_NATIVE_TEXT_SEQUENCE tools/test_arcade_attract_text_sequence.c -o build/test_native_attract_text_sequence
	build/test_native_attract_text_sequence "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"
test-native-scene-boot: test-native-attract-text-sequence

.PHONY: test-arcade-text-schedule
test-arcade-text-schedule: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_opening_lifecycle.c -o build/test_arcade_opening_lifecycle
	build/test_arcade_opening_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" --phase-four-text
test-native-scene-boot: test-arcade-text-schedule

native/intro_probe/src/intro_panel_data.h: tools/native_intro_panel_data.py build/reference_maincpu.bin
	python3 tools/native_intro_panel_data.py
.PHONY: test-native-attract-panel
test-native-attract-panel: native/intro_probe/src/intro_panel_data.h
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_NATIVE_PANEL tools/test_arcade_attract_panel.c -o build/test_native_attract_panel
	build/test_native_attract_panel "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"
test-native-scene-boot: test-native-attract-panel

.PHONY: test-native-attract-panel-wait
test-native-attract-panel-wait: build/reference_maincpu.bin native/intro_probe/src/task_startup_data.h
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_NATIVE_PANEL_WAIT tools/test_arcade_attract_panel_wait.c -o build/test_native_attract_panel_wait
	build/test_native_attract_panel_wait "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"
test-native-scene-boot: test-native-attract-panel-wait

.PHONY: test-arcade-panel-hold-schedule
test-arcade-panel-hold-schedule: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_opening_lifecycle.c -o build/test_arcade_opening_lifecycle
	build/test_arcade_opening_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" --phase-four-hold
test-native-scene-boot: test-arcade-panel-hold-schedule

.PHONY: test-native-attract-panel-map
test-native-attract-panel-map: build/reference_maincpu.bin native/intro_probe/src/task_startup_data.h
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_NATIVE_PANEL_MAP tools/test_arcade_attract_panel_map.c -o build/test_native_attract_panel_map
	build/test_native_attract_panel_map "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"
test-native-scene-boot: test-native-attract-panel-map

.PHONY: test-arcade-panel-map-schedule
test-arcade-panel-map-schedule: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_opening_lifecycle.c -o build/test_arcade_opening_lifecycle
	build/test_arcade_opening_lifecycle "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" --phase-four-map
test-native-scene-boot: test-arcade-panel-map-schedule

.PHONY: test-native-attract-panel-scroll
test-native-attract-panel-scroll: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_NATIVE_PANEL_SCROLL tools/test_arcade_attract_panel_scroll.c -o build/test_native_attract_panel_scroll
	build/test_native_attract_panel_scroll "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"
test-native-scene-boot: test-native-attract-panel-scroll

.PHONY: test-native-attract-panel-text
test-native-attract-panel-text: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_NATIVE_PANEL_TEXT tools/test_arcade_attract_panel_text.c -o build/test_native_attract_panel_text
	build/test_native_attract_panel_text "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"
test-native-scene-boot: test-native-attract-panel-text

.PHONY: test-native-attract-panel-replace
test-native-attract-panel-replace: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_NATIVE_PANEL_REPLACE tools/test_arcade_attract_panel_replace.c -o build/test_native_attract_panel_replace
	build/test_native_attract_panel_replace "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"
test-native-scene-boot: test-native-attract-panel-replace

.PHONY: test-native-attract-panel-exit
test-native-attract-panel-exit: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_NATIVE_PANEL_EXIT tools/test_arcade_attract_panel_exit.c -o build/test_native_attract_panel_exit
	build/test_native_attract_panel_exit "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"
test-native-scene-boot: test-native-attract-panel-exit

native/intro_probe/src/final_panel_data.h: tools/native_final_panel_data.py build/reference_maincpu.bin
	python3 tools/native_final_panel_data.py
.PHONY: test-native-attract-final-panel
test-native-attract-final-panel: build/reference_maincpu.bin native/intro_probe/src/final_panel_data.h
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_NATIVE_FINAL_PANEL tools/test_arcade_attract_final_panel.c -o build/test_native_attract_final_panel
	build/test_native_attract_final_panel "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"
test-native-scene-boot: test-native-attract-final-panel

native/intro_probe/src/demo_init_data.h: tools/native_demo_init_data.py build/reference_maincpu.bin
	python3 tools/native_demo_init_data.py
.PHONY: test-native-attract-demo-init
test-native-attract-demo-init: build/reference_maincpu.bin native/intro_probe/src/demo_init_data.h native/intro_probe/src/palette_initial_data.h
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_NATIVE_DEMO_INIT tools/test_arcade_attract_demo_init.c -o build/test_native_attract_demo_init
	build/test_native_attract_demo_init "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"
	python3 tools/report_demo_init_state.py

.PHONY: test-native-demo-state
.PHONY: test-native-attract-demo-task
.PHONY: test-native-attract-demo-start
build/demo-start-rom-access.bin: tools/test_arcade_attract_demo_start.c build/reference_maincpu.bin
	$(MAKE) test-arcade-attract-demo-start

native/intro_probe/src/demo_start_data.h: tools/native_demo_start_data.py build/demo-start-rom-access.bin build/reference_maincpu.bin
	python3 tools/native_demo_start_data.py

test-native-attract-demo-start: native/intro_probe/src/demo_start_data.h
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_NATIVE_DEMO_START tools/test_arcade_attract_demo_start.c -o build/test_native_attract_demo_start
	build/test_native_attract_demo_start "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"
	python3 tools/report_demo_start_state.py

test-native-attract-demo-task: build/reference_maincpu.bin native/intro_probe/src/demo_init_data.h native/intro_probe/src/palette_initial_data.h
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_NATIVE_DEMO_TASK tools/test_arcade_attract_demo_init.c -o build/test_native_attract_demo_task
	build/test_native_attract_demo_task "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-demo-state:
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_native_demo_state.c -o build/test_native_demo_state
	build/test_native_demo_state

.PHONY: test-native-attract-demo-start-task
test-native-attract-demo-start-task: native/intro_probe/src/demo_start_data.h
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_NATIVE_DEMO_START_TASK tools/test_arcade_attract_demo_start.c -o build/test_native_attract_demo_start_task
	build/test_native_attract_demo_start_task "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-attract-demo-update
test-arcade-attract-demo-update: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_attract_demo_update.c -o build/test_arcade_attract_demo_update
	build/test_arcade_attract_demo_update "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_DEMO_UPDATE_ROUTING tools/test_arcade_attract_demo_update.c -o build/test_arcade_attract_demo_update_routing
	build/test_arcade_attract_demo_update_routing "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-demo-first-update
test-arcade-demo-first-update: build/reference_maincpu.bin build/opening-demo-start-0-state.bin build/opening-title-context-scroll1.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_DEMO_FIRST_FRAME tools/test_arcade_demo_first_actors.c -o build/test_arcade_demo_first_frame
	build/test_arcade_demo_first_frame "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

build/demo-update-rom-access.bin: tools/test_arcade_demo_first_actors.c build/reference_maincpu.bin build/opening-demo-start-0-state.bin build/opening-title-context-scroll1.bin
	$(MAKE) test-arcade-demo-first-update

native/intro_probe/src/demo_update_data.h: tools/native_demo_update_data.py build/demo-update-rom-access.bin build/reference_maincpu.bin
	python3 tools/native_demo_update_data.py

.PHONY: test-native-demo-first-update-data
test-native-demo-first-update-data: native/intro_probe/src/demo_update_data.h
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_DEMO_FIRST_FRAME -DWILLOW_TEST_RETAINED_DEMO_UPDATE tools/test_arcade_demo_first_actors.c -o build/test_native_demo_first_frame
	build/test_native_demo_first_frame "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"
	python3 tools/report_demo_update_state.py

.PHONY: test-arcade-demo-sequence test-native-demo-sequence-data
.PHONY: test-native-demo-rom-lookup
intro-probe: native/intro_probe/src/fixed_data.h
native/intro_probe/src/fixed_data.h: tools/native_fixed_data.py src/willow_native_boot_globals.h src/willow_native_pool_auxiliary.h
	python3 tools/native_fixed_data.py

test-native-demo-rom-lookup: native/intro_probe/src/demo_update_data.h native/intro_probe/src/demo_sequence_data.h
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_native_demo_rom_lookup.c -o build/test_native_demo_rom_lookup
	build/test_native_demo_rom_lookup

test-arcade-demo-sequence: build/reference_maincpu.bin build/opening-demo-start-0-state.bin build/opening-title-context-scroll1.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_DEMO_FIRST_FRAME -DWILLOW_TEST_DEMO_UPDATE_SEQUENCE=120 tools/test_arcade_demo_first_actors.c -o build/test_arcade_demo_sequence
	build/test_arcade_demo_sequence "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

build/demo-sequence-rom-access.bin: tools/test_arcade_demo_first_actors.c build/reference_maincpu.bin build/opening-demo-start-0-state.bin build/opening-title-context-scroll1.bin
	$(MAKE) test-arcade-demo-sequence

native/intro_probe/src/demo_sequence_data.h: tools/native_demo_update_data.py build/demo-sequence-rom-access.bin build/reference_maincpu.bin
	python3 tools/native_demo_update_data.py --sequence

test-native-demo-sequence-data: native/intro_probe/src/demo_sequence_data.h
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_DEMO_FIRST_FRAME -DWILLOW_TEST_DEMO_UPDATE_SEQUENCE=120 -DWILLOW_TEST_RETAINED_DEMO_UPDATE tools/test_arcade_demo_first_actors.c -o build/test_native_demo_sequence
	build/test_native_demo_sequence "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"
	python3 tools/report_demo_update_state.py --sequence

.PHONY: test-native-record-access test-native-first-level-record-access
test-native-record-access: build/reference_maincpu.bin
	m68k-elf-gcc -m68000 -O2 -Wall -Wextra -Werror -DSGDK_GCC -DWILLOW_NATIVE_RECORD_WORDS -I"$(if $(strip $(SGDK_ROOT)),$(SGDK_ROOT),$(if $(SGDK),$(SGDK),/tmp/willow-sgdk-resume))/inc" -ffreestanding -nostdlib -Wl,-Ttext=0x100010,-e,recordAccessProbe tools/record_access_probe.c -o build/record_access_probe.elf
	m68k-elf-objcopy -O binary --only-section=.text build/record_access_probe.elf build/record_access_probe.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_native_record_access.c -o build/test_native_record_access
	build/test_native_record_access "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-first-level-record-access: test-native-record-access
	python3 tools/build_timeout_runtime_probe.py --forest --native-record-words
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_FIRST_LEVEL -DWILLOW_TEST_BOSS_INPUT tools/test_native_forest_scheduler.c -o build/first-level-runtime-investigation/normal-play-test
	WILLOW_PLAY_TICKS=4096 build/first-level-runtime-investigation/normal-play-test "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" 1

.PHONY: test-arcade-shop-entry test-native-shop-entry
test-arcade-shop-entry: build/reference_maincpu.bin
	mkdir -p build/native-shop-entry-investigation
	clang -Wall -Wextra -Werror -O1 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_shop_palette.c -o build/native-shop-entry-investigation/palette-test
	build/native-shop-entry-investigation/palette-test "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"
	clang -Wall -Wextra -Werror -O1 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_shop_entry.c -o build/native-shop-entry-investigation/entry-test
	build/native-shop-entry-investigation/entry-test "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"
	clang -Wall -Wextra -Werror -O1 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_shop_map.c -o build/native-shop-entry-investigation/map-test
	@for region in 0 1 2; do build/native-shop-entry-investigation/map-test "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" $$region || exit $$?; done

test-native-shop-entry: test-arcade-shop-entry
	python3 tools/build_timeout_runtime_probe.py --forest
	clang -Wall -Wextra -Werror -O1 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_FIRST_LEVEL -DWILLOW_TEST_BOSS_INPUT -DWILLOW_TEST_SHOP_ENTRY tools/test_native_forest_scheduler.c -o build/native-shop-entry-investigation/continuous-test
	WILLOW_PLAY_TICKS=4096 build/native-shop-entry-investigation/continuous-test "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" 1

.PHONY: test-arcade-shop-controls test-native-shop-return
test-arcade-shop-controls: build/reference_maincpu.bin
	mkdir -p build/native-shop-entry-investigation
	clang -Wall -Wextra -Werror -O1 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_shop_controls.c -o build/native-shop-entry-investigation/controls-test
	build/native-shop-entry-investigation/controls-test "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-shop-return: test-arcade-shop-controls
	python3 tools/build_timeout_runtime_probe.py --forest
	clang -Wall -Wextra -Werror -O1 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_FIRST_LEVEL -DWILLOW_TEST_BOSS_INPUT -DWILLOW_TEST_SHOP_ENTRY -DWILLOW_TEST_SHOP_EXIT -DWILLOW_TEST_SHOP_PROGRESS tools/test_native_forest_scheduler.c -o build/native-shop-entry-investigation/progress-test
	WILLOW_PLAY_TICKS=4096 build/native-shop-entry-investigation/progress-test "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" 1

.PHONY: test-arcade-stage-clear-entry test-native-results-message test-native-forest-results-entry
test-arcade-stage-clear-entry: build/reference_maincpu.bin
	mkdir -p build/native-shop-entry-investigation
	clang -Wall -Wextra -Werror -O1 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_stage_clear_entry.c -o build/native-shop-entry-investigation/stage-clear-test
	build/native-shop-entry-investigation/stage-clear-test "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-results-message: build/reference_maincpu.bin
	mkdir -p build/native-shop-entry-investigation
	clang -Wall -Wextra -Werror -O1 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_RESULTS_MESSAGE tools/test_native_message_task.c -o build/native-shop-entry-investigation/results-message-test
	build/native-shop-entry-investigation/results-message-test "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-forest-results-entry: test-arcade-stage-clear-entry test-native-results-message
	python3 tools/build_timeout_runtime_probe.py --forest
	clang -Wall -Wextra -Werror -O1 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_FIRST_LEVEL -DWILLOW_TEST_BOSS_INPUT -DWILLOW_TEST_SHOP_ENTRY -DWILLOW_TEST_SHOP_EXIT -DWILLOW_TEST_SHOP_PROGRESS -DWILLOW_TEST_STAGE_CLEAR_ENTRY tools/test_native_forest_scheduler.c -o build/native-shop-entry-investigation/results-entry-test
	WILLOW_PLAY_TICKS=8000 build/native-shop-entry-investigation/results-entry-test "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" 1

.PHONY: test-arcade-results-bonus test-native-forest-results-tally
test-arcade-results-bonus: build/reference_maincpu.bin
	mkdir -p build/native-shop-entry-investigation
	clang -Wall -Wextra -Werror -O1 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_results_bonus.c -o build/native-shop-entry-investigation/bonus-test
	build/native-shop-entry-investigation/bonus-test "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-forest-results-tally: test-arcade-results-bonus
	python3 tools/build_timeout_runtime_probe.py --forest
	clang -Wall -Wextra -Werror -O1 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_FIRST_LEVEL -DWILLOW_TEST_BOSS_INPUT -DWILLOW_TEST_SHOP_ENTRY -DWILLOW_TEST_SHOP_EXIT -DWILLOW_TEST_SHOP_PROGRESS -DWILLOW_TEST_STAGE_CLEAR_ENTRY -DWILLOW_TEST_RESULTS_TALLY tools/test_native_forest_scheduler.c -o build/native-shop-entry-investigation/tally-test
	WILLOW_PLAY_TICKS=10000 build/native-shop-entry-investigation/tally-test "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" 1

.PHONY: test-arcade-interstage-setup test-arcade-next-stage-fields test-native-next-stage-init
test-arcade-interstage-setup: build/reference_maincpu.bin
	mkdir -p build/native-shop-entry-investigation
	clang -Wall -Wextra -Werror -O1 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_interstage_map.c -o build/native-shop-entry-investigation/interstage-map-test
	build/native-shop-entry-investigation/interstage-map-test "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"
	clang -Wall -Wextra -Werror -O1 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_interstage_palette.c -o build/native-shop-entry-investigation/interstage-palette-test
	build/native-shop-entry-investigation/interstage-palette-test "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-arcade-next-stage-fields: build/reference_maincpu.bin
	mkdir -p build/native-shop-entry-investigation
	clang -Wall -Wextra -Werror -O1 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_next_stage_fields.c -o build/native-shop-entry-investigation/next-stage-fields-test
	build/native-shop-entry-investigation/next-stage-fields-test "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-next-stage-init: test-arcade-interstage-setup test-arcade-next-stage-fields
	python3 tools/build_timeout_runtime_probe.py --forest
	clang -Wall -Wextra -Werror -O1 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_FIRST_LEVEL -DWILLOW_TEST_BOSS_INPUT -DWILLOW_TEST_SHOP_ENTRY -DWILLOW_TEST_SHOP_EXIT -DWILLOW_TEST_SHOP_PROGRESS -DWILLOW_TEST_STAGE_CLEAR_ENTRY -DWILLOW_TEST_RESULTS_TALLY -DWILLOW_TEST_INTERSTAGE_MAP -DWILLOW_TEST_NEXT_STAGE tools/test_native_forest_scheduler.c -o build/native-shop-entry-investigation/next-stage-test
	WILLOW_PLAY_TICKS=10000 build/native-shop-entry-investigation/next-stage-test "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" 1

.PHONY: test-arcade-madmartigan-gate test-native-madmartigan-start
test-arcade-madmartigan-gate: build/reference_maincpu.bin
	mkdir -p build/native-shop-entry-investigation
	clang -Wall -Wextra -Werror -O1 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_madmartigan_gate.c -o build/native-shop-entry-investigation/madmartigan-gate-test
	build/native-shop-entry-investigation/madmartigan-gate-test "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-madmartigan-start: test-arcade-madmartigan-gate
	python3 tools/build_timeout_runtime_probe.py --forest
	clang -Wall -Wextra -Werror -O1 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_FIRST_LEVEL -DWILLOW_TEST_BOSS_INPUT -DWILLOW_TEST_SHOP_ENTRY -DWILLOW_TEST_SHOP_EXIT -DWILLOW_TEST_SHOP_PROGRESS -DWILLOW_TEST_STAGE_CLEAR_ENTRY -DWILLOW_TEST_RESULTS_TALLY -DWILLOW_TEST_INTERSTAGE_MAP -DWILLOW_TEST_NEXT_STAGE -DWILLOW_TEST_MADMARTIGAN_START tools/test_native_forest_scheduler.c -o build/native-shop-entry-investigation/madmartigan-start-test
	WILLOW_PLAY_TICKS=12000 build/native-shop-entry-investigation/madmartigan-start-test "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" 1

.PHONY: test-arcade-madmartigan-ground
test-arcade-madmartigan-ground: build/reference_maincpu.bin
	python3 tools/verify_stage0_collision_map.py
	mkdir -p build/native-shop-entry-investigation
	clang -Wall -Wextra -Werror -O1 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_madmartigan_ground.c -o build/native-shop-entry-investigation/madmartigan-ground-test
	build/native-shop-entry-investigation/madmartigan-ground-test "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-madmartigan-control test-native-madmartigan-play
test-arcade-madmartigan-control: test-arcade-madmartigan-ground
	clang -Wall -Wextra -Werror -O1 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_madmartigan_attack.c -o build/native-shop-entry-investigation/madmartigan-attack-test
	build/native-shop-entry-investigation/madmartigan-attack-test "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"
	clang -Wall -Wextra -Werror -O1 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_madmartigan_traversal.c -o build/native-shop-entry-investigation/madmartigan-traversal-test
	build/native-shop-entry-investigation/madmartigan-traversal-test "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-madmartigan-play: test-arcade-madmartigan-control
	python3 tools/build_timeout_runtime_probe.py --forest
	clang -Wall -Wextra -Werror -O1 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_FIRST_LEVEL -DWILLOW_TEST_BOSS_INPUT -DWILLOW_TEST_SHOP_ENTRY -DWILLOW_TEST_SHOP_EXIT -DWILLOW_TEST_SHOP_PROGRESS -DWILLOW_TEST_STAGE_CLEAR_ENTRY -DWILLOW_TEST_RESULTS_TALLY -DWILLOW_TEST_INTERSTAGE_MAP -DWILLOW_TEST_NEXT_STAGE -DWILLOW_TEST_NEXT_STAGE_PLAY tools/test_native_forest_scheduler.c -o build/native-shop-entry-investigation/madmartigan-play-test
	WILLOW_PLAY_TICKS=12000 build/native-shop-entry-investigation/madmartigan-play-test "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" 1

.PHONY: test-arcade-madmartigan-death
test-arcade-madmartigan-death: build/reference_maincpu.bin
	mkdir -p build/native-shop-entry-investigation
	clang -Wall -Wextra -Werror -O1 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_madmartigan_death.c -o build/native-shop-entry-investigation/madmartigan-death-test
	build/native-shop-entry-investigation/madmartigan-death-test "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-stage1-alias test-native-madmartigan-route
test-arcade-stage1-alias: test-arcade-stage1-population
	mkdir -p build/native-shop-entry-investigation
	clang -Wall -Wextra -Werror -O1 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_SOURCE_A5=0xff8000UL tools/test_arcade_stage1_population.c -o build/native-shop-entry-investigation/stage1-alias-test
	build/native-shop-entry-investigation/stage1-alias-test "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

test-native-madmartigan-route: test-arcade-stage1-alias
	python3 tools/build_timeout_runtime_probe.py --forest
	clang -Wall -Wextra -Werror -O1 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_FIRST_LEVEL -DWILLOW_TEST_BOSS_INPUT -DWILLOW_TEST_SHOP_ENTRY -DWILLOW_TEST_SHOP_EXIT -DWILLOW_TEST_SHOP_PROGRESS -DWILLOW_TEST_STAGE_CLEAR_ENTRY -DWILLOW_TEST_RESULTS_TALLY -DWILLOW_TEST_INTERSTAGE_MAP -DWILLOW_TEST_NEXT_STAGE -DWILLOW_TEST_NEXT_STAGE_PLAY -DWILLOW_TEST_NEXT_STAGE_PLAY_TICKS=2048 -DWILLOW_TEST_NEXT_STAGE_ROUTE tools/test_native_forest_scheduler.c -o build/native-shop-entry-investigation/madmartigan-route-test
	WILLOW_PLAY_TICKS=12000 build/native-shop-entry-investigation/madmartigan-route-test "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" 1

.PHONY: test-arcade-madmartigan-hold
test-arcade-madmartigan-hold: build/reference_maincpu.bin
	mkdir -p build/native-shop-entry-investigation
	clang -Wall -Wextra -Werror -O1 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_madmartigan_hold.c -o build/native-shop-entry-investigation/madmartigan-hold-test
	build/native-shop-entry-investigation/madmartigan-hold-test "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-madmartigan-arrival
test-arcade-madmartigan-arrival: build/reference_maincpu.bin
	python3 tools/verify_stage0_collision_map.py
	mkdir -p build/native-shop-entry-investigation
	clang -Wall -Wextra -Werror -O1 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_madmartigan_arrival.c -o build/native-shop-entry-investigation/madmartigan-arrival-test
	build/native-shop-entry-investigation/madmartigan-arrival-test "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-madmartigan-timed-walk
test-arcade-madmartigan-timed-walk: build/reference_maincpu.bin
	mkdir -p build/native-shop-entry-investigation
	clang -Wall -Wextra -Werror -O1 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_madmartigan_timed_walk.c -o build/native-shop-entry-investigation/madmartigan-timed-walk-test
	build/native-shop-entry-investigation/madmartigan-timed-walk-test "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-madmartigan-scene
test-arcade-madmartigan-scene: build/reference_maincpu.bin
	mkdir -p build/native-shop-entry-investigation
	clang -Wall -Wextra -Werror -O1 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_madmartigan_scene.c -o build/native-shop-entry-investigation/madmartigan-scene-test
	build/native-shop-entry-investigation/madmartigan-scene-test "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-madmartigan-scene-controller
test-arcade-madmartigan-scene-controller: build/reference_maincpu.bin
	mkdir -p build/native-shop-entry-investigation
	clang -Wall -Wextra -Werror -O1 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_madmartigan_scene_controller.c -o build/native-shop-entry-investigation/madmartigan-scene-controller-test
	build/native-shop-entry-investigation/madmartigan-scene-controller-test "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-native-madmartigan-scene
test-native-madmartigan-scene: build/reference_maincpu.bin
	mkdir -p build/native-shop-entry-investigation
	python3 tools/build_timeout_runtime_probe.py --forest
	clang -Wall -Wextra -Werror -O1 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_FIRST_LEVEL -DWILLOW_TEST_BOSS_INPUT -DWILLOW_TEST_SHOP_ENTRY -DWILLOW_TEST_SHOP_EXIT -DWILLOW_TEST_SHOP_PROGRESS -DWILLOW_TEST_STAGE_CLEAR_ENTRY -DWILLOW_TEST_RESULTS_TALLY -DWILLOW_TEST_INTERSTAGE_MAP -DWILLOW_TEST_NEXT_STAGE -DWILLOW_TEST_NEXT_STAGE_PLAY -DWILLOW_TEST_NEXT_STAGE_PLAY_TICKS=512 -DWILLOW_TEST_MADMARTIGAN_SCENE tools/test_native_forest_scheduler.c -o build/native-shop-entry-investigation/madmartigan-scene-scheduler-test
	WILLOW_PLAY_TICKS=12000 build/native-shop-entry-investigation/madmartigan-scene-scheduler-test "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" 1

.PHONY: test-native-madmartigan-second-roof
test-native-madmartigan-second-roof: build/reference_maincpu.bin
	mkdir -p build/native-shop-entry-investigation
	python3 tools/build_timeout_runtime_probe.py --forest
	clang -Wall -Wextra -Werror -O1 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_FIRST_LEVEL -DWILLOW_TEST_BOSS_INPUT -DWILLOW_TEST_SHOP_ENTRY -DWILLOW_TEST_SHOP_EXIT -DWILLOW_TEST_SHOP_PROGRESS -DWILLOW_TEST_STAGE_CLEAR_ENTRY -DWILLOW_TEST_RESULTS_TALLY -DWILLOW_TEST_INTERSTAGE_MAP -DWILLOW_TEST_NEXT_STAGE -DWILLOW_TEST_NEXT_STAGE_PLAY -DWILLOW_TEST_NEXT_STAGE_PLAY_TICKS=3072 -DWILLOW_TEST_NEXT_STAGE_ROUTE -DWILLOW_TEST_NEXT_STAGE_SECOND_ROOF tools/test_native_forest_scheduler.c -o build/native-shop-entry-investigation/madmartigan-second-roof-test
	WILLOW_PLAY_TICKS=12000 build/native-shop-entry-investigation/madmartigan-second-roof-test "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" 1

.PHONY: test-native-madmartigan-stage1-complete
test-native-madmartigan-stage1-complete: build/reference_maincpu.bin
	mkdir -p build/native-shop-entry-investigation
	python3 tools/build_timeout_runtime_probe.py --forest
	clang -Wall -Wextra -Werror -O1 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_FIRST_LEVEL -DWILLOW_TEST_BOSS_INPUT -DWILLOW_TEST_SHOP_ENTRY -DWILLOW_TEST_SHOP_EXIT -DWILLOW_TEST_SHOP_PROGRESS -DWILLOW_TEST_STAGE_CLEAR_ENTRY -DWILLOW_TEST_RESULTS_TALLY -DWILLOW_TEST_INTERSTAGE_MAP -DWILLOW_TEST_NEXT_STAGE -DWILLOW_TEST_NEXT_STAGE_PLAY -DWILLOW_TEST_NEXT_STAGE_ROUTE -DWILLOW_TEST_NEXT_STAGE_SECOND_ROOF -DWILLOW_TEST_STAGE1_COMPLETE tools/test_native_forest_scheduler.c -o build/native-shop-entry-investigation/madmartigan-stage1-complete-test
	WILLOW_PLAY_TICKS=26000 build/native-shop-entry-investigation/madmartigan-stage1-complete-test "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" 1

.PHONY: test-native-stage2-play
test-native-stage2-play: test-arcade-player-start test-arcade-stage-reset
	mkdir -p build/native-shop-entry-investigation
	python3 tools/build_timeout_runtime_probe.py --forest
	clang -Wall -Wextra -Werror -O1 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_FIRST_LEVEL -DWILLOW_TEST_BOSS_INPUT -DWILLOW_TEST_SHOP_ENTRY -DWILLOW_TEST_SHOP_EXIT -DWILLOW_TEST_SHOP_PROGRESS -DWILLOW_TEST_STAGE_CLEAR_ENTRY -DWILLOW_TEST_RESULTS_TALLY -DWILLOW_TEST_INTERSTAGE_MAP -DWILLOW_TEST_NEXT_STAGE -DWILLOW_TEST_NEXT_STAGE_PLAY -DWILLOW_TEST_NEXT_STAGE_ROUTE -DWILLOW_TEST_NEXT_STAGE_SECOND_ROOF -DWILLOW_TEST_STAGE1_COMPLETE -DWILLOW_TEST_STAGE2_PLAY tools/test_native_forest_scheduler.c -o build/native-shop-entry-investigation/stage2-play-test
	WILLOW_PLAY_TICKS=26000 build/native-shop-entry-investigation/stage2-play-test "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" 1

.PHONY: test-native-stage2-complete
test-native-stage2-complete: test-arcade-player-start test-arcade-stage-reset
	mkdir -p build/native-shop-entry-investigation
	python3 tools/build_timeout_runtime_probe.py --forest
	clang -Wall -Wextra -Werror -O1 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_FIRST_LEVEL -DWILLOW_TEST_BOSS_INPUT -DWILLOW_TEST_SHOP_ENTRY -DWILLOW_TEST_SHOP_EXIT -DWILLOW_TEST_SHOP_PROGRESS -DWILLOW_TEST_STAGE_CLEAR_ENTRY -DWILLOW_TEST_RESULTS_TALLY -DWILLOW_TEST_INTERSTAGE_MAP -DWILLOW_TEST_NEXT_STAGE -DWILLOW_TEST_NEXT_STAGE_PLAY -DWILLOW_TEST_NEXT_STAGE_ROUTE -DWILLOW_TEST_NEXT_STAGE_SECOND_ROOF -DWILLOW_TEST_STAGE1_COMPLETE -DWILLOW_TEST_STAGE2_PLAY -DWILLOW_TEST_STAGE2_COMPLETE tools/test_native_forest_scheduler.c -o build/native-shop-entry-investigation/stage2-complete-test
	WILLOW_PLAY_TICKS=30000 build/native-shop-entry-investigation/stage2-complete-test "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" 1

.PHONY: test-native-stage3-play
test-native-stage3-play: test-arcade-player-start test-arcade-stage-reset test-arcade-scene-manager
	mkdir -p build/native-shop-entry-investigation
	python3 tools/build_timeout_runtime_probe.py --forest
	clang -Wall -Wextra -Werror -O1 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_FIRST_LEVEL -DWILLOW_TEST_BOSS_INPUT -DWILLOW_TEST_SHOP_ENTRY -DWILLOW_TEST_SHOP_EXIT -DWILLOW_TEST_SHOP_PROGRESS -DWILLOW_TEST_STAGE_CLEAR_ENTRY -DWILLOW_TEST_RESULTS_TALLY -DWILLOW_TEST_INTERSTAGE_MAP -DWILLOW_TEST_NEXT_STAGE -DWILLOW_TEST_NEXT_STAGE_PLAY -DWILLOW_TEST_NEXT_STAGE_ROUTE -DWILLOW_TEST_NEXT_STAGE_SECOND_ROOF -DWILLOW_TEST_STAGE1_COMPLETE -DWILLOW_TEST_STAGE2_PLAY -DWILLOW_TEST_STAGE2_COMPLETE -DWILLOW_TEST_STAGE3_PLAY tools/test_native_forest_scheduler.c -o build/native-shop-entry-investigation/stage3-play-test
	WILLOW_PLAY_TICKS=40000 build/native-shop-entry-investigation/stage3-play-test "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" 1

.PHONY: test-native-stage3-complete
test-native-stage3-complete: test-arcade-player-start test-arcade-stage-reset test-arcade-scene-manager
	mkdir -p build/native-shop-entry-investigation
	python3 tools/build_timeout_runtime_probe.py --forest
	clang -Wall -Wextra -Werror -O1 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_FIRST_LEVEL -DWILLOW_TEST_BOSS_INPUT -DWILLOW_TEST_SHOP_ENTRY -DWILLOW_TEST_SHOP_EXIT -DWILLOW_TEST_SHOP_PROGRESS -DWILLOW_TEST_STAGE_CLEAR_ENTRY -DWILLOW_TEST_RESULTS_TALLY -DWILLOW_TEST_INTERSTAGE_MAP -DWILLOW_TEST_NEXT_STAGE -DWILLOW_TEST_NEXT_STAGE_PLAY -DWILLOW_TEST_NEXT_STAGE_ROUTE -DWILLOW_TEST_NEXT_STAGE_SECOND_ROOF -DWILLOW_TEST_STAGE1_COMPLETE -DWILLOW_TEST_STAGE2_PLAY -DWILLOW_TEST_STAGE2_COMPLETE -DWILLOW_TEST_STAGE3_PLAY -DWILLOW_TEST_STAGE3_COMPLETE tools/test_native_forest_scheduler.c -o build/native-shop-entry-investigation/stage3-complete-test
	WILLOW_PLAY_TICKS=64000 build/native-shop-entry-investigation/stage3-complete-test "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" 1

.PHONY: test-native-stage4-play
test-native-stage4-play: test-arcade-player-start test-arcade-stage-reset test-arcade-scene-manager test-arcade-character-select-start test-native-selection-message test-native-selection-message-scheduler
	mkdir -p build/native-shop-entry-investigation
	python3 tools/build_timeout_runtime_probe.py --forest
	clang -Wall -Wextra -Werror -O1 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_FIRST_LEVEL -DWILLOW_TEST_BOSS_INPUT -DWILLOW_TEST_SHOP_ENTRY -DWILLOW_TEST_SHOP_EXIT -DWILLOW_TEST_SHOP_PROGRESS -DWILLOW_TEST_STAGE_CLEAR_ENTRY -DWILLOW_TEST_RESULTS_TALLY -DWILLOW_TEST_INTERSTAGE_MAP -DWILLOW_TEST_NEXT_STAGE -DWILLOW_TEST_NEXT_STAGE_PLAY -DWILLOW_TEST_NEXT_STAGE_ROUTE -DWILLOW_TEST_NEXT_STAGE_SECOND_ROOF -DWILLOW_TEST_STAGE1_COMPLETE -DWILLOW_TEST_STAGE2_PLAY -DWILLOW_TEST_STAGE2_COMPLETE -DWILLOW_TEST_STAGE3_PLAY -DWILLOW_TEST_STAGE3_COMPLETE -DWILLOW_TEST_STAGE4_PLAY tools/test_native_forest_scheduler.c -o build/native-shop-entry-investigation/stage4-play-test
	WILLOW_PLAY_TICKS=70000 build/native-shop-entry-investigation/stage4-play-test "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" 1

.PHONY: test-arcade-character-select-start
test-arcade-character-select-start: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_character_select_start.c -o build/test_arcade_character_select_start
	build/test_arcade_character_select_start "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-native-selection-message
test-native-selection-message: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_native_selection_message.c -o build/test_native_selection_message
	build/test_native_selection_message "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-native-selection-message-scheduler
test-native-selection-message-scheduler: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_native_selection_message_scheduler.c -o build/test_native_selection_message_scheduler
	build/test_native_selection_message_scheduler "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-selection-number
test-arcade-selection-number: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_selection_number.c -o build/test_arcade_selection_number
	build/test_arcade_selection_number "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-final-stage-player-start
test-arcade-final-stage-player-start: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O1 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_final_stage_player_start.c -o build/test_arcade_final_stage_player_start
	build/test_arcade_final_stage_player_start "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-final-stage-reset
test-arcade-final-stage-reset: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O1 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_final_stage_reset.c -o build/test_arcade_final_stage_reset
	build/test_arcade_final_stage_reset "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

WILLOW_STAGE4_ROUTE_TICKS ?= 4096
.PHONY: test-native-stage4-route
test-native-stage4-route: build/reference_maincpu.bin
	mkdir -p build/native-shop-entry-investigation
	python3 tools/build_timeout_runtime_probe.py --forest
	clang -Wall -Wextra -Werror -O1 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_FIRST_LEVEL -DWILLOW_TEST_BOSS_INPUT -DWILLOW_TEST_SHOP_ENTRY -DWILLOW_TEST_SHOP_EXIT -DWILLOW_TEST_SHOP_PROGRESS -DWILLOW_TEST_STAGE_CLEAR_ENTRY -DWILLOW_TEST_RESULTS_TALLY -DWILLOW_TEST_INTERSTAGE_MAP -DWILLOW_TEST_NEXT_STAGE -DWILLOW_TEST_NEXT_STAGE_PLAY -DWILLOW_TEST_NEXT_STAGE_ROUTE -DWILLOW_TEST_NEXT_STAGE_SECOND_ROOF -DWILLOW_TEST_STAGE1_COMPLETE -DWILLOW_TEST_STAGE2_PLAY -DWILLOW_TEST_STAGE2_COMPLETE -DWILLOW_TEST_STAGE3_PLAY -DWILLOW_TEST_STAGE3_COMPLETE -DWILLOW_TEST_STAGE4_PLAY -DWILLOW_TEST_STAGE4_ROUTE -DWILLOW_TEST_STAGE4_PLAY_TICKS=$(WILLOW_STAGE4_ROUTE_TICKS) tools/test_native_forest_scheduler.c -o build/native-shop-entry-investigation/stage4-route-test
	WILLOW_PLAY_TICKS=90000 build/native-shop-entry-investigation/stage4-route-test "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" 1

.PHONY: test-native-stage4-complete
test-native-stage4-complete: build/reference_maincpu.bin
	mkdir -p build/native-shop-entry-investigation
	python3 tools/build_timeout_runtime_probe.py --forest
	clang -Wall -Wextra -Werror -O1 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_FIRST_LEVEL -DWILLOW_TEST_BOSS_INPUT -DWILLOW_TEST_SHOP_ENTRY -DWILLOW_TEST_SHOP_EXIT -DWILLOW_TEST_SHOP_PROGRESS -DWILLOW_TEST_STAGE_CLEAR_ENTRY -DWILLOW_TEST_RESULTS_TALLY -DWILLOW_TEST_INTERSTAGE_MAP -DWILLOW_TEST_NEXT_STAGE -DWILLOW_TEST_NEXT_STAGE_PLAY -DWILLOW_TEST_NEXT_STAGE_ROUTE -DWILLOW_TEST_NEXT_STAGE_SECOND_ROOF -DWILLOW_TEST_STAGE1_COMPLETE -DWILLOW_TEST_STAGE2_PLAY -DWILLOW_TEST_STAGE2_COMPLETE -DWILLOW_TEST_STAGE3_PLAY -DWILLOW_TEST_STAGE3_COMPLETE -DWILLOW_TEST_STAGE4_PLAY -DWILLOW_TEST_STAGE4_ROUTE -DWILLOW_TEST_STAGE4_COMPLETE -DWILLOW_TEST_STAGE4_PLAY_TICKS=$(WILLOW_STAGE4_ROUTE_TICKS) tools/test_native_forest_scheduler.c -o build/native-shop-entry-investigation/stage4-complete-test
	WILLOW_PLAY_TICKS=105000 build/native-shop-entry-investigation/stage4-complete-test "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" 1

.PHONY: test-native-stage5-play
test-native-stage5-play: build/reference_maincpu.bin
	mkdir -p build/native-shop-entry-investigation
	python3 tools/build_timeout_runtime_probe.py --forest
	clang -Wall -Wextra -Werror -O1 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_FIRST_LEVEL -DWILLOW_TEST_BOSS_INPUT -DWILLOW_TEST_SHOP_ENTRY -DWILLOW_TEST_SHOP_EXIT -DWILLOW_TEST_SHOP_PROGRESS -DWILLOW_TEST_STAGE_CLEAR_ENTRY -DWILLOW_TEST_RESULTS_TALLY -DWILLOW_TEST_INTERSTAGE_MAP -DWILLOW_TEST_NEXT_STAGE -DWILLOW_TEST_NEXT_STAGE_PLAY -DWILLOW_TEST_NEXT_STAGE_ROUTE -DWILLOW_TEST_NEXT_STAGE_SECOND_ROOF -DWILLOW_TEST_STAGE1_COMPLETE -DWILLOW_TEST_STAGE2_PLAY -DWILLOW_TEST_STAGE2_COMPLETE -DWILLOW_TEST_STAGE3_PLAY -DWILLOW_TEST_STAGE3_COMPLETE -DWILLOW_TEST_STAGE4_PLAY -DWILLOW_TEST_STAGE4_ROUTE -DWILLOW_TEST_STAGE4_COMPLETE -DWILLOW_TEST_STAGE5_PLAY -DWILLOW_TEST_STAGE4_PLAY_TICKS=$(WILLOW_STAGE4_ROUTE_TICKS) tools/test_native_forest_scheduler.c -o build/native-shop-entry-investigation/stage5-play-test
	WILLOW_PLAY_TICKS=105000 build/native-shop-entry-investigation/stage5-play-test "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" 1

.PHONY: test-arcade-final-stage-spawn
test-arcade-final-stage-spawn: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O1 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_final_stage_spawn.c -o build/test_arcade_final_stage_spawn
	build/test_arcade_final_stage_spawn "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-native-stage5-first-portal
test-native-stage5-first-portal: build/reference_maincpu.bin
	mkdir -p build/native-shop-entry-investigation
	python3 tools/build_timeout_runtime_probe.py --forest
	clang -Wall -Wextra -Werror -O1 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_FIRST_LEVEL -DWILLOW_TEST_BOSS_INPUT -DWILLOW_TEST_SHOP_ENTRY -DWILLOW_TEST_SHOP_EXIT -DWILLOW_TEST_SHOP_PROGRESS -DWILLOW_TEST_STAGE_CLEAR_ENTRY -DWILLOW_TEST_RESULTS_TALLY -DWILLOW_TEST_INTERSTAGE_MAP -DWILLOW_TEST_NEXT_STAGE -DWILLOW_TEST_NEXT_STAGE_PLAY -DWILLOW_TEST_NEXT_STAGE_ROUTE -DWILLOW_TEST_NEXT_STAGE_SECOND_ROOF -DWILLOW_TEST_STAGE1_COMPLETE -DWILLOW_TEST_STAGE2_PLAY -DWILLOW_TEST_STAGE2_COMPLETE -DWILLOW_TEST_STAGE3_PLAY -DWILLOW_TEST_STAGE3_COMPLETE -DWILLOW_TEST_STAGE4_PLAY -DWILLOW_TEST_STAGE4_ROUTE -DWILLOW_TEST_STAGE4_COMPLETE -DWILLOW_TEST_STAGE5_PLAY -DWILLOW_TEST_STAGE5_FIRST_PORTAL -DWILLOW_TEST_STAGE4_PLAY_TICKS=$(WILLOW_STAGE4_ROUTE_TICKS) tools/test_native_forest_scheduler.c -o build/native-shop-entry-investigation/stage5-first-portal-test
	WILLOW_PLAY_TICKS=105000 build/native-shop-entry-investigation/stage5-first-portal-test "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" 1


.PHONY: test-arcade-ending-credits
test-arcade-ending-credits: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O1 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_ending_credits.c -o build/test_arcade_ending_credits
	build/test_arcade_ending_credits "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-ending-map
test-arcade-ending-map: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O1 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_ending_map.c -o build/test_arcade_ending_map
	build/test_arcade_ending_map "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-ending-scene-setup
test-arcade-ending-scene-setup: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O1 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_ending_scene_setup.c -o build/test_arcade_ending_scene_setup
	build/test_arcade_ending_scene_setup "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

WILLOW_STAGE5_ROUTE_TICKS ?= 8192
.PHONY: test-native-stage5-route
test-native-stage5-route: build/reference_maincpu.bin
	mkdir -p build/native-shop-entry-investigation
	python3 tools/build_timeout_runtime_probe.py --forest
	clang -Wall -Wextra -Werror -O1 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_FIRST_LEVEL -DWILLOW_TEST_BOSS_INPUT -DWILLOW_TEST_SHOP_ENTRY -DWILLOW_TEST_SHOP_EXIT -DWILLOW_TEST_SHOP_PROGRESS -DWILLOW_TEST_STAGE_CLEAR_ENTRY -DWILLOW_TEST_RESULTS_TALLY -DWILLOW_TEST_INTERSTAGE_MAP -DWILLOW_TEST_NEXT_STAGE -DWILLOW_TEST_NEXT_STAGE_PLAY -DWILLOW_TEST_NEXT_STAGE_ROUTE -DWILLOW_TEST_NEXT_STAGE_SECOND_ROOF -DWILLOW_TEST_STAGE1_COMPLETE -DWILLOW_TEST_STAGE2_PLAY -DWILLOW_TEST_STAGE2_COMPLETE -DWILLOW_TEST_STAGE3_PLAY -DWILLOW_TEST_STAGE3_COMPLETE -DWILLOW_TEST_STAGE4_PLAY -DWILLOW_TEST_STAGE4_ROUTE -DWILLOW_TEST_STAGE4_COMPLETE -DWILLOW_TEST_STAGE5_PLAY -DWILLOW_TEST_STAGE5_ROUTE -DWILLOW_TEST_STAGE5_PLAY_TICKS=$(WILLOW_STAGE5_ROUTE_TICKS) -DWILLOW_TEST_STAGE4_PLAY_TICKS=$(WILLOW_STAGE4_ROUTE_TICKS) tools/test_native_forest_scheduler.c -o build/native-shop-entry-investigation/stage5-route-test
	WILLOW_PLAY_TICKS=145000 build/native-shop-entry-investigation/stage5-route-test "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" 1



.PHONY: test-arcade-ending-actors
test-arcade-ending-actors: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O1 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_ending_actors.c -o build/test_arcade_ending_actors
	build/test_arcade_ending_actors "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-ending-messages
test-arcade-ending-messages: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O1 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_ending_messages.c -o build/test_arcade_ending_messages
	build/test_arcade_ending_messages "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-ending-resume
test-arcade-ending-resume: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O1 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_ending_resume.c -o build/test_arcade_ending_resume
	build/test_arcade_ending_resume "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-native-stage5-tower
test-native-stage5-tower: build/reference_maincpu.bin
	mkdir -p build/native-shop-entry-investigation
	python3 tools/build_timeout_runtime_probe.py --forest
	clang -Wall -Wextra -Werror -O1 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_FIRST_LEVEL -DWILLOW_TEST_BOSS_INPUT -DWILLOW_TEST_SHOP_ENTRY -DWILLOW_TEST_SHOP_EXIT -DWILLOW_TEST_SHOP_PROGRESS -DWILLOW_TEST_STAGE_CLEAR_ENTRY -DWILLOW_TEST_RESULTS_TALLY -DWILLOW_TEST_INTERSTAGE_MAP -DWILLOW_TEST_NEXT_STAGE -DWILLOW_TEST_NEXT_STAGE_PLAY -DWILLOW_TEST_NEXT_STAGE_ROUTE -DWILLOW_TEST_NEXT_STAGE_SECOND_ROOF -DWILLOW_TEST_STAGE1_COMPLETE -DWILLOW_TEST_STAGE2_PLAY -DWILLOW_TEST_STAGE2_COMPLETE -DWILLOW_TEST_STAGE3_PLAY -DWILLOW_TEST_STAGE3_COMPLETE -DWILLOW_TEST_STAGE4_PLAY -DWILLOW_TEST_STAGE4_ROUTE -DWILLOW_TEST_STAGE4_COMPLETE -DWILLOW_TEST_STAGE5_PLAY -DWILLOW_TEST_STAGE5_ROUTE -DWILLOW_TEST_STAGE5_TOWER -DWILLOW_TEST_STAGE5_PLAY_TICKS=32768 -DWILLOW_TEST_STAGE4_PLAY_TICKS=$(WILLOW_STAGE4_ROUTE_TICKS) tools/test_native_forest_scheduler.c -o build/native-shop-entry-investigation/stage5-tower-test
	WILLOW_PLAY_TICKS=145000 build/native-shop-entry-investigation/stage5-tower-test "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" 1




.PHONY: test-arcade-ending-outer
test-arcade-ending-outer: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O1 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_ENDING_OUTER tools/test_arcade_ending_resume.c -o build/test_arcade_ending_outer
	build/test_arcade_ending_outer "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-arcade-ending-palette
test-arcade-ending-palette: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O1 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_ending_palette.c -o build/test_arcade_ending_palette
	build/test_arcade_ending_palette "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"

.PHONY: test-native-ending-fixture
test-native-ending-fixture: build/reference_maincpu.bin
	mkdir -p build/native-shop-entry-investigation
	python3 tools/build_timeout_runtime_probe.py --forest
	clang -Wall -Wextra -Werror -O1 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_FIRST_LEVEL -DWILLOW_TEST_ENDING_FIXTURE tools/test_native_forest_scheduler.c -o build/native-shop-entry-investigation/ending-fixture-test
	WILLOW_PLAY_TICKS=20000 build/native-shop-entry-investigation/ending-fixture-test "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" 1

.PHONY: test-native-ending-credits-fixture
test-native-ending-credits-fixture: build/reference_maincpu.bin
	mkdir -p build/native-shop-entry-investigation
	python3 tools/build_timeout_runtime_probe.py --forest
	clang -Wall -Wextra -Werror -O1 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_FIRST_LEVEL -DWILLOW_TEST_ENDING_FIXTURE -DWILLOW_TEST_ENDING_CREDITS_FIXTURE tools/test_native_forest_scheduler.c -o build/native-shop-entry-investigation/ending-credits-fixture-test
	WILLOW_PLAY_TICKS=20000 build/native-shop-entry-investigation/ending-credits-fixture-test "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" 1

.PHONY: test-arcade-layer-clear
test-arcade-layer-clear: build/reference_maincpu.bin
	clang -Wall -Wextra -Werror -O1 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_arcade_layer_clear.c -o build/test_arcade_layer_clear
	@for region in 0 1 2 3 4; do build/test_arcade_layer_clear "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" $$region || exit $$?; done

.PHONY: test-native-source-timing
test-native-source-timing: build/reference_maincpu.bin
	mkdir -p build/first-level-runtime-investigation
	python3 tools/build_timeout_runtime_probe.py --forest
	clang -Wall -Wextra -Werror -O1 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_FIRST_LEVEL tools/test_native_forest_scheduler.c -o build/first-level-runtime-investigation/source-timing-test
	WILLOW_SOURCE_CYCLES=1 WILLOW_PLAY_TICKS=2048 build/first-level-runtime-investigation/source-timing-test "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" 1

.PHONY: test-native-palette-add
test-native-palette-add: build/reference_maincpu.bin
	m68k-elf-gcc -m68000 -O2 -Wall -Wextra -Werror -DSGDK_GCC -I"$(if $(strip $(SGDK_ROOT)),$(SGDK_ROOT),$(if $(SGDK),$(SGDK),/tmp/willow-sgdk-resume))/inc" -ffreestanding -nostdlib -Wl,-Ttext=0x100010,-e,paletteAddProbe tools/palette_add_probe.c -o build/palette_add_probe.elf
	m68k-elf-objcopy -O binary --only-section=.text build/palette_add_probe.elf build/palette_add_probe.bin
	clang -Wall -Wextra -Werror -O1 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_native_palette_add.c -o build/test_native_palette_add
	build/test_native_palette_add "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"


.PHONY: test-native-stage5-complete
test-native-stage5-complete: build/reference_maincpu.bin
	mkdir -p build/native-shop-entry-investigation
	python3 tools/build_timeout_runtime_probe.py --forest
	clang -Wall -Wextra -Werror -O1 -fsanitize=address,undefined -fno-sanitize-recover=all -DWILLOW_TEST_FIRST_LEVEL -DWILLOW_TEST_BOSS_INPUT -DWILLOW_TEST_SHOP_ENTRY -DWILLOW_TEST_SHOP_EXIT -DWILLOW_TEST_SHOP_PROGRESS -DWILLOW_TEST_STAGE_CLEAR_ENTRY -DWILLOW_TEST_RESULTS_TALLY -DWILLOW_TEST_INTERSTAGE_MAP -DWILLOW_TEST_NEXT_STAGE -DWILLOW_TEST_NEXT_STAGE_PLAY -DWILLOW_TEST_NEXT_STAGE_ROUTE -DWILLOW_TEST_NEXT_STAGE_SECOND_ROOF -DWILLOW_TEST_STAGE1_COMPLETE -DWILLOW_TEST_STAGE2_PLAY -DWILLOW_TEST_STAGE2_COMPLETE -DWILLOW_TEST_STAGE3_PLAY -DWILLOW_TEST_STAGE3_COMPLETE -DWILLOW_TEST_STAGE4_PLAY -DWILLOW_TEST_STAGE4_ROUTE -DWILLOW_TEST_STAGE4_COMPLETE -DWILLOW_TEST_STAGE5_PLAY -DWILLOW_TEST_STAGE5_ROUTE -DWILLOW_TEST_STAGE5_TOWER -DWILLOW_TEST_STAGE5_COMPLETE -DWILLOW_TEST_STAGE5_PLAY_TICKS=32768 -DWILLOW_TEST_STAGE4_PLAY_TICKS=$(WILLOW_STAGE4_ROUTE_TICKS) tools/test_native_forest_scheduler.c -o build/native-shop-entry-investigation/stage5-complete-test
	WILLOW_PLAY_TICKS=200000 build/native-shop-entry-investigation/stage5-complete-test "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" 1

# Geometry/pixel verification only; does not launch an emulator or admit a ROM.
.PHONY: test-native-object-sat-bridge
test-native-object-sat-bridge:
	python3 tools/verify_native_object_sat_bridge.py

.PHONY: test-native-object-tiles test-native-video-relocation test-native-frame-bridge
test-native-object-tiles:
	python3 tools/verify_native_object_tiles.py

test-native-video-relocation: build/reference_maincpu.bin
	python3 tools/test_native_video_relocation.py

WILLOW_NATIVE_BRIDGE_TICKS ?= 4096
test-native-frame-bridge: build/reference_maincpu.bin
	python3 tools/build_native_frame_bridge.py
	clang -Wall -Wextra -Werror -O1 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_native_frame_bridge.c -o build/native-frame-bridge/test-bridge
	build/native-frame-bridge/test-bridge "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" $(WILLOW_NATIVE_BRIDGE_TICKS)
	python3 tools/summarize_native_bridge_timing.py --output build/native-frame-bridge/timing-summary.json

.PHONY: test-native-video-bridge
test-native-video-bridge: build/reference_maincpu.bin
	python3 tools/native_video_relocation.py --output build/native-video-relocation/program-addresses-v2.bin --manifest build/native-video-relocation/manifest-v2.json
	python3 tools/build_native_video_bridge.py
	clang -Wall -Wextra -Werror -O1 -fsanitize=address,undefined -fno-sanitize-recover=all tools/test_native_video_bridge.c -o build/native-video-relocation/test-video-bridge
	build/native-video-relocation/test-video-bridge "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" $(WILLOW_NATIVE_BRIDGE_TICKS)
	python3 tools/summarize_native_bridge_timing.py build/native-video-relocation/bridge-timing.csv --output build/native-video-relocation/timing-summary.json

.PHONY: test-native-rowscroll
test-native-rowscroll: build/reference_maincpu.bin
	python3 tools/verify_native_rowscroll.py

# Measures the existing playable opening ROM; does not build or launch a GUI.
.PHONY: measure-intro-cadence
measure-intro-cadence:
	mkdir -p build/intro-cadence
	clang -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-sanitize-recover=all tools/measure_intro_cadence.c -o build/intro-cadence/measure
	build/intro-cadence/measure "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" native/intro_probe/out/rom.bin native/intro_probe/out/symbol.txt --idle > build/intro-cadence/idle.csv 2> build/intro-cadence/idle.log
	build/intro-cadence/measure "$(HOME)/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" native/intro_probe/out/rom.bin native/intro_probe/out/symbol.txt --walk > build/intro-cadence/walk.csv 2> build/intro-cadence/walk.log

.PHONY: test-native-presentation-frame
test-native-presentation-frame:
	$(SORSHA_PYTHON) tools/verify_native_presentation_frame.py

.PHONY: test-native-object-latch-sat
test-native-object-latch-sat:
	$(SORSHA_PYTHON) tools/verify_native_object_latch_sat.py

.PHONY: test-native-object-latch-next
test-native-object-latch-next:
	$(PYTHON) tools/verify_native_object_latch_next.py

.PHONY: test-native-window-surface
test-native-window-surface:
	$(SORSHA_PYTHON) tools/verify_native_window_surface.py

.PHONY: test-native-plane-surface prepare-native-route-background
# Isolated retained-plane publication; does not claim live background cadence.
test-native-plane-surface:
	$(PYTHON) tools/verify_native_plane_surface.py --output build/native-plane-surface/current

# Uses the frozen live source context; target resources contain no captured maps.
prepare-native-route-background:
	$(SORSHA_PYTHON) tools/prepare_native_route_background.py --capture build/native-background-context/content-v2 --output build/native-route-background-audit/prepared-current
	$(SORSHA_PYTHON) tools/prepare_native_route_background_canonical.py --assets build/native-route-background-audit/prepared-current --output build/native-route-background-audit/canonical-current

.PHONY: prepare-native-background-residency test-native-background-residency
prepare-native-background-residency:
	$(SORSHA_PYTHON) tools/prepare_native_background_residency.py --checkpoint build/native-route-background-audit/checkpoint-v1 --output build/native-background-residency/prepared-current

# Cold resource integration only: no live background map or priority publication.
test-native-background-residency: prepare-native-background-residency
	mkdir -p build/native-background-residency/loader-current
	clang -std=c99 -O2 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-sanitize-recover=all -Isrc tools/test_native_background_resident.c -o build/native-background-residency/loader-current/test
	build/native-background-residency/loader-current/test build/native-background-residency/prepared-current/resident-upload.bin > build/native-background-residency/loader-current/host.json
	$(PYTHON) tools/build_native_render_cadence.py --stream-content --background-resident build/native-background-residency/prepared-current --output build/native-render-cadence/background-resident-current
	@willow_residency_run=$$(mktemp -d build/native-background-residency/run.XXXXXX); \
	$(PYTHON) tools/test_native_background_residency.py --platform build/native-render-cadence/background-resident-current --assets build/native-background-residency/prepared-current --output "$$willow_residency_run/observer" && \
	$(SORSHA_PYTHON) tools/verify_native_render_output.py --run "$$willow_residency_run/observer" --output "$$willow_residency_run/object-verification"

.PHONY: test-native-background-journal
test-native-background-journal:
	$(PYTHON) tools/verify_native_background_journal.py --output build/native-background-journal/current

.PHONY: test-native-sat-publish test-native-prepared-scene
test-native-sat-publish:
	$(PYTHON) tools/verify_native_sat_publish.py

test-native-prepared-scene:
	$(SORSHA_PYTHON) tools/verify_native_prepared_scene.py

.PHONY: test-native-prepared-scene-cells test-native-sprite-groups test-native-grouped-scene test-native-grouped-scene-commit
test-native-prepared-scene-cells:
	$(SORSHA_PYTHON) tools/verify_native_prepared_scene.py --native-cells --output build/native-prepared-scene/native-cells

test-native-sprite-groups:
	$(SORSHA_PYTHON) tools/prepare_native_sprite_groups.py

test-native-grouped-scene:
	$(SORSHA_PYTHON) tools/verify_native_grouped_scene.py

test-native-grouped-scene-commit:
	$(SORSHA_PYTHON) tools/verify_native_grouped_scene.py --native-commit --output build/native-grouped-scene/native-commit

.PHONY: test-native-grouped-scene-cells prepare-native-route-objects test-native-route-objects
test-native-grouped-scene-cells:
	$(SORSHA_PYTHON) tools/verify_native_grouped_scene.py --native-commit --native-cells --output build/native-grouped-scene/native-cells

prepare-native-route-objects:
	$(SORSHA_PYTHON) tools/prepare_native_route_objects.py

test-native-route-objects:
	$(SORSHA_PYTHON) tools/verify_native_route_objects.py

.PHONY: test-native-render-content
test-native-render-content:
	$(PYTHON) tools/build_native_render_cadence.py --stream-content --output build/native-render-cadence/content-current
	$(PYTHON) tools/test_native_render_cadence.py --platform build/native-render-cadence/content-current --output build/native-render-cadence/observer-content-current
	$(SORSHA_PYTHON) tools/verify_native_render_output.py --run build/native-render-cadence/observer-content-current --output build/native-render-cadence/verification-content-current
	$(PYTHON) tools/compare_native_render_retained.py --candidate build/native-render-cadence/observer-content-current --verification build/native-render-cadence/verification-content-current/evidence.json --output build/native-render-cadence/content-comparison-current.json

.PHONY: test-native-latch-presentation test-native-latch-presentation-records test-native-latch-presentation-full-bank
test-native-latch-presentation: test-native-latch-presentation-records test-native-latch-presentation-full-bank

test-native-latch-presentation-records:
	$(SORSHA_PYTHON) tools/verify_native_latch_presentation.py --deferred-budget --output build/native-latch-presentation/deferred

test-native-latch-presentation-full-bank:
	$(SORSHA_PYTHON) tools/verify_native_latch_presentation.py --deferred-budget --fixture build/cartridge-gap-audit/frame1200_window_v1 --asset-binding build/cartridge-gap-audit/frame1200_window_v1/asset-binding.json --output build/native-latch-presentation/frame1200-clipped
