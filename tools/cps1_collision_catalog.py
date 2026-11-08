#!/usr/bin/env python3
"""Extract Willow movement probes, terrain properties, and actor hitboxes.

This catalogs data referenced by native 68000 routines. It does not execute or
include the arcade CPU program in the Genesis build.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from cps1_program import assemble_maincpu

PROBE_TABLE = 0x3B66
PROBE_COUNT = 12
PROBE_SIZE = 8
PROPERTY_TABLE = 0x4712
PROPERTY_SIZE = 8
PROPERTY_END = 0x4882  # next 68000 routine begins here
PROPERTY_COUNT = (PROPERTY_END - PROPERTY_TABLE) // PROPERTY_SIZE
PLAYER_VELOCITY_TABLE = 0x136B0
PLAYER_VELOCITY_COUNT = 4
PLAYER_STATE_DISPATCH = 0x1273A
PLAYER_STATE_TABLE = 0x1276E
PLAYER_STATE_COUNT = 8
PLAYER_JUMP_SUBSTATE_DISPATCH = 0xC78A
PLAYER_JUMP_SUBSTATE_TABLE = 0xC7B6
PLAYER_NORMAL_UPDATE = 0xC9DA
PLAYER_CONTACT_RESPONSE = 0xC72E
CLASS10_EFFECT_PROBE = 0x3D90
CLASS10_EFFECT_CONTACT = 0x44EA
ACTOR_FRAME_BOUNDS = 0x3AC4A
ACTOR_FRAME_BOUNDS_COUNT = (0x3B1C2 - ACTOR_FRAME_BOUNDS) // 8
PLAYER_FRAME_BOUNDS = 0x3B1C2
PLAYER_FRAME_BOUNDS_COUNT = (0x3B25C - PLAYER_FRAME_BOUNDS) // 8


def be16(data: bytes, offset: int) -> int:
    return int.from_bytes(data[offset : offset + 2], "big")


def signed_frame_bounds(data: bytes, address: int, count: int) -> list[list[int]]:
    return [[int.from_bytes(data[address + index * 8 + field * 2:
                                 address + index * 8 + field * 2 + 2],
                             "big", signed=True)
             for field in range(4)] for index in range(count)]


def catalog(data: bytes) -> dict[str, object]:
    frame_bounds_checks = {
        "fixed_controller_candidate_collision_scan": (0x3A6AA, bytes.fromhex(
            "4ded18460c2e0001000066206100f412610000400c2e00010000"
            "661061000022")),
        "actor_frame_bounds_consumer": (0x3A70A, bytes.fromhex(
            "345b102a0000530066267000102a0020103b00246b1a"
            "206a001432280006e74941fa051e41f01000")),
        "player_frame_bounds_consumer": (0x39ACA, bytes.fromhex(
            "206d185a30280006e74843fa16ec43f100004e75")),
        "standard_frame_overlap_callback": (0x3A778, bytes.fromhex(
            "000a00400090009a00d0302e0006906a0006d051d058")),
    }
    for name, (address, expected) in frame_bounds_checks.items():
        if data[address:address + len(expected)] != expected:
            raise ValueError(f"Player/actor overlap source changed: {name}")
    actor_frame_bounds = signed_frame_bounds(
        data, ACTOR_FRAME_BOUNDS, ACTOR_FRAME_BOUNDS_COUNT)
    player_frame_bounds = signed_frame_bounds(
        data, PLAYER_FRAME_BOUNDS, PLAYER_FRAME_BOUNDS_COUNT)

    class10_effect_checks = {
        "profile_zero_probe": (CLASS10_EFFECT_PROBE, bytes.fromhex(
            "7000102e0013e54845fa002245f20000302e0006d05a3b408a32"
            "322e000ad25a3b418a346100007260000730")),
        "contact_dispatch": (CLASS10_EFFECT_CONTACT, bytes.fromhex(
            "422e0024700012280002024100fc6722d24143fa021443f11000"
            "10190800000366140800000066000068080000016600009c70004e75")),
        "solid_shape_sign_branch": (0x4520, bytes.fromhex(
            "10116b00003c")),
        "negative_shape_threshold": (0x4560, bytes.fromhex(
            "0240001f122d8a350241000f904165ac")),
        "positive_shape_slope": (0x4526, bytes.fromhex(
            "3200142d8a3308810006670246020240000fe8490242000fe26a"
            "d0420c40001165027010122d8a350241000f904165c672027001"
            "1d4000244e75")),
    }
    for name, (address, expected) in class10_effect_checks.items():
        if data[address:address + len(expected)] != expected:
            raise ValueError(f"Class-10 terrain-contact source changed: {name}")

    player_state_dispatch = bytes.fromhex(
        "102e0003323b002e4ebb102a"
    )
    if data[PLAYER_STATE_DISPATCH:PLAYER_STATE_DISPATCH
            + len(player_state_dispatch)] != player_state_dispatch:
        raise ValueError("Player top-level state dispatcher at 0x1273a changed")
    state_offsets = [be16(data, PLAYER_STATE_TABLE + index * 2)
                     for index in range(PLAYER_STATE_COUNT)]
    expected_state_offsets = [0x0012, 0x0032, 0x0128, 0x017E,
                              0x0358, 0x040E, 0x0488, 0x051A]
    if state_offsets != expected_state_offsets:
        raise ValueError("Player state handler table at 0x1276e changed")
    player_state_handlers = [
        {
            "state": index * 2,
            "handler": f"0x{PLAYER_STATE_TABLE + offset:06x}",
        }
        for index, offset in enumerate(state_offsets)
    ]

    player_input_checks = {
        "player_state_8_handler": (0x12AC6, bytes.fromhex(
            "102e0004323b00264ebb1022")),
        "normal_jump_state8_entry": (0xC892, bytes.fromhex(
            "422e00481d7c00080003422e00044e75")),
        "alternate_jump_state8_entry": (0xDCE0, bytes.fromhex(
            "422e00481d7c00080003422e00044e75")),
        "state8_initial_setup": (0x12AF8, bytes.fromhex(
            "542e00042d7c000136b800181d7c00400022"
            "1d7c003c0040610002321d40000f41fa063a")),
        "state8_move_and_pose_timers": (0x12B20, bytes.fromhex(
            "532e00226614086e0000000f1d7c00800022"
            "41fa06204ef81f30532e00406612542e0004"
            "1d7c0038004041fa06104ef81f304eb8208c"
            "4eb83b0a4a2e001f67c6")),
        "state8_wall_pose_repeat": (0x12B64, bytes.fromhex(
            "532e004066f6552e00041d7c003c0040"
            "41fa05de4ef81f30102e0004323b00104ebb100c")),
        "state8_exit_gate": (0x12D92, bytes.fromhex(
            "322e000a926d1850064100300c4100606216"
            "322e0006926d184c064100a00c41014062047001")),
        "state8_exit_transition": (0x12AE4, bytes.fromhex(
            "2d7c000136b000186000fd50")),
        "state4_normal_entry": (0x1283E, bytes.fromhex(
            "1d7c00040003422e00044e75")),
        "state2_no_contact_to_state4": (0x127B2, bytes.fromhex(
            "4eb8208c4eb83b0a4a2e001f6700007e")),
        "state2_timer_to_state4": (0x127D6, bytes.fromhex(
            "6100070666000062")),
        "state4_normal_handler": (0x1284A, bytes.fromhex(
            "4eb8208c4eb83b0a4a2e001f67e64eb81f4c61000510")),
        "state4_vertical_band_timer_gate": (0x12858, bytes.fromhex(
            "4eb81f4c610005100c0000046600002a6100067467000020")),
        "state4_timed_heading_gate": (0x12870, bytes.fromhex(
            "4eb820d206060008e80e4a2e000f67080c06000a67b84e75"
            "0c06000667b04e75")),
        "state8_slow_movement_vectors": (0x136B8, bytes.fromhex(
            "0100fe00ff00fe00")),
        "horizontal_input_selector": (0xC52A, bytes.fromhex(
            "102d006a080000006706422e000f4e7508000001"
            "67081d7c0001000f4e75002e0002000f4e75")),
        "down_input_test": (0xC550, bytes.fromhex(
            "102d006a080000024e75")),
        "slow_horizontal_selector": (0xC55A, bytes.fromhex(
            "102d006a0800000067081d7c0002000f4e75"
            "0800000167061d7c0003000f4e75")),
        "down_state_entry": (0xC4D6, bytes.fromhex(
            "1d7c00040004002e0002000f1d7c0002003e"
            "41fa0b9c4ef81f30")),
        "up_jump_probe": (0xC57A, bytes.fromhex(
            "102d006a0800000367044ef83a844e75")),
        "up_jump_probe_dispatch": (0x3A84, bytes.fromhex(
            "4a2e0020662e45fa0066102e000f02400001e54845f20000"
            "302e0006d05a3b408a32322e000ad25a3b418a3461000376"
            "600004ec122e000f02410001e54945fa003e45f21000"
            "302e0006d05a3b408a32322e000ad25a3b418a3461000348"
            "61000558660645fa001060a070004e75")),
        "jump_substate_dispatcher": (PLAYER_JUMP_SUBSTATE_DISPATCH,
            bytes.fromhex("102e0004323b00264ebb1022")),
        "jump_launch_substate": (0xC7BC, bytes.fromhex(
            "422e003e422e00241d7c000100483d7c04400038"
            "102d006a0800000367063d7c05800038")),
        "jump_transition_substate": (0xC7EC, bytes.fromhex(
            "302e003848c0e180d1ae000a4eb839fe660a"
            "302e0038906e00346a10542e00041d7c00020048")),
        "jump_ascent_substate": (0xC820, bytes.fromhex(
            "4eb81f4c302e003848c0e18091ae000a")),
        "stage0_jump_animation_start": (0xC7E0, bytes.fromhex(
            "542e000441fa0a944eb81f30")),
        "stage0_jump_animation_facing_reload": (0xC884,
            bytes.fromhex("1d40003041fa09f04ef81f34")),
        "jump_ascent_collision_exit": (0xC842, bytes.fromhex(
            "6100fce64a2e0024660a610000684eb838f6671e"
            "422e00481d6e000f0030422e0003422e0004303c"
            "002b4ef8786a")),
        "active_jump_ascent_collision_exit": (0xDC90, bytes.fromhex(
            "6100e8984a2e0024660a6100ec1a4eb838f6671e"
            "422e00481d6e000f0030422e0003422e0004303c"
            "002b4ef8786a6100e86a")),
        "up_probe_ascent_callsite": (0xC870, bytes.fromhex(
            "6100fcb86100fd046618102e000fb02e0030670c")),
        "normal_control_input_path": (0xC400, bytes.fromhex(
            "6100012861000310662a61000204660000a66100013c"
            "660000be102e000fb02e0030670c")),
    }
    player_input_checks_valid = {
        name: data[address:address + len(expected)] == expected
        for name, (address, expected) in player_input_checks.items()
    }
    if not all(player_input_checks_valid.values()):
        failed = ", ".join(name for name, ok
                            in player_input_checks_valid.items() if not ok)
        raise ValueError(f"Willow player input source context changed: {failed}")

    player_landing_checks = {
        "state_two_collision_then_contact_response": (
            PLAYER_NORMAL_UPDATE, bytes.fromhex(
                "102e0005323b001a4ebb10166100fd2e6600fa82"
                "6100feb24eb838f66000fd36")),
        "solid_contact_clears_jump_fields": (
            PLAYER_CONTACT_RESPONSE, bytes.fromhex(
                "4a2e00246644302e003a064000800c4007006304"
                "303c07003d40003a48c0e18091ae000a"
                "0c6e0180003a65261d7c000200031d7c00040004"
                "1d7c0002004870003d4000381d4000474e75"
                "426e003a422e00484e75")),
        "default_collision_profile_dispatch": (
            0x38F6, bytes.fromhex(
                "45fa00864a2e0020670445fa00bc7000")),
        "jump_transition_substate_table": (
            0xC906, bytes.fromhex("000a00380038004c")),
        "jump_transition_setup": (
            0xC912, bytes.fromhex(
                "1d40003c1d4000051d40003e002e0002000f"
                "542e00041d7c0006002241fa099a102e000f"
                "024000014ef81f34")),
        "jump_transition_repeat_timer": (
            0xC93E, bytes.fromhex(
                "532e0022660a542e00041d7c000600224ef81f4c")),
        "jump_transition_final_pose": (
            0xC952, bytes.fromhex(
                "532e002266f6542e00043d7c02000038"
                "1d7c000200481d7c000800226100fbba"
                "41fa095a102e000f024000014ef81f34")),
    }
    for name, (address, expected) in player_landing_checks.items():
        if data[address:address + len(expected)] != expected:
            raise ValueError(f"Willow player landing source context changed: {name}")

    jump_transition_ticks = int.from_bytes(data[0xC92A:0xC92C], "big")
    jump_repeat_ticks = int.from_bytes(data[0xC94A:0xC94C], "big")
    jump_fall_pose_ticks = int.from_bytes(data[0xC96A:0xC96C], "big")
    jump_fall_entry_delay = (jump_transition_ticks
                             + 2 * jump_repeat_ticks
                             + jump_fall_pose_ticks)
    state8_move_ticks = be16(data, 0x12B0C)
    state8_pose_ticks = be16(data, 0x12B46)
    state8_facing_initial_ticks = be16(data, 0x12B06)
    state8_facing_repeat_ticks = be16(data, 0x12B2E)

    jump_substate_offsets = [be16(data, PLAYER_JUMP_SUBSTATE_TABLE + index * 2)
                             for index in range(3)]
    expected_jump_substate_offsets = [0x0006, 0x0036, 0x006A]
    if jump_substate_offsets != expected_jump_substate_offsets:
        raise ValueError("Player jump substate table at 0xc7b6 changed")
    player_jump_substates = [
        {"state": index * 2,
         "handler": f"0x{PLAYER_JUMP_SUBSTATE_TABLE + offset:06x}"}
        for index, offset in enumerate(jump_substate_offsets)
    ]

    def read_probe_table(address: int, count: int) -> list[dict[str, object]]:
        rows = []
        for index in range(count):
            offset = address + index * PROBE_SIZE
            words = [be16(data, offset + item * 2) for item in range(4)]
            signed = [word if word < 0x8000 else word - 0x10000 for word in words]
            rows.append({
                "index": index,
                "source_offset": f"0x{offset:06x}",
                "facing_0": signed[:2],
                "facing_1": signed[2:],
                "raw_words": [f"0x{word:04x}" for word in words],
            })
        return rows

    def read_player_pairs(address: int, count: int) -> list[dict[str, object]]:
        rows = []
        for index in range(count):
            offset = address + index * 4
            words = [be16(data, offset), be16(data, offset + 2)]
            signed = [word if word < 0x8000 else word - 0x10000 for word in words]
            rows.append({
                "index": index,
                "source_offset": f"0x{offset:06x}",
                "offset": signed,
                "raw_words": [f"0x{word:04x}" for word in words],
            })
        return rows

    probes = []
    for index in range(PROBE_COUNT):
        offset = PROBE_TABLE + index * PROBE_SIZE
        words = [be16(data, offset + item * 2) for item in range(4)]
        probes.append({
            "index": index,
            "source_offset": f"0x{offset:06x}",
            "facing_0": [words[0] if words[0] < 0x8000 else words[0] - 0x10000,
                         words[1] if words[1] < 0x8000 else words[1] - 0x10000],
            "facing_1": [words[2] if words[2] < 0x8000 else words[2] - 0x10000,
                         words[3] if words[3] < 0x8000 else words[3] - 0x10000],
            "raw_words": [f"0x{word:04x}" for word in words],
        })

    properties = []
    for index in range(PROPERTY_COUNT):
        offset = PROPERTY_TABLE + index * PROPERTY_SIZE
        raw = data[offset : offset + PROPERTY_SIZE]
        properties.append({
            "attribute_class": index,
            "attribute_masked_value": f"0x{index * 4:02x}",
            "source_offset": f"0x{offset:06x}",
            "flags": f"0x{raw[0]:02x}",
            "shape": f"0x{raw[1]:02x}",
            "x_threshold_facing0": f"0x{raw[2]:02x}",
            "x_threshold_facing1": f"0x{raw[3]:02x}",
            "raw": raw.hex(),
        })

    player_velocities = []
    for index in range(PLAYER_VELOCITY_COUNT):
        offset = PLAYER_VELOCITY_TABLE + index * 4
        words = [be16(data, offset), be16(data, offset + 2)]
        player_velocities.append([
            word if word < 0x8000 else word - 0x10000 for word in words
        ])

    return {
        "actor_player_frame_bounds": {
            "actor_source_offset": f"0x{ACTOR_FRAME_BOUNDS:06x}",
            "actor_entry_count": ACTOR_FRAME_BOUNDS_COUNT,
            "player_source_offset": f"0x{PLAYER_FRAME_BOUNDS:06x}",
            "player_entry_count": PLAYER_FRAME_BOUNDS_COUNT,
            "entry_fields": ["left", "top", "right", "bottom"],
            "selected_by": "0x3a6aa indexes the actor record at 0x3ac4a using its current frame word at +6; 0x39aca indexes the fixed controller frame word at 0x3b1c2",
            "checks": {name: True for name in frame_bounds_checks},
            "actor_entries": actor_frame_bounds,
            "player_entries": player_frame_bounds,
        },
        "source": "offline Willow 68000 ROM data",
        "usage": "offline collision reference; extracted flags and shapes are emitted to a native-data header",
        "movement_probe_table": {
            "referenced_by": "0x3b0a",
            "source_offset": f"0x{PROBE_TABLE:06x}",
            "entry_count": PROBE_COUNT,
            "entry_size": PROBE_SIZE,
            "entries": probes,
        },
        "player_velocity_table": {
            "source_offset": f"0x{PLAYER_VELOCITY_TABLE:06x}",
            "selected_by": "0x208c: object field +24 table indexed by field +15",
            "format": "signed 8.8 X/Y pixels per update",
            "entries": player_velocities,
        },
        "player_input_states": {
            "source_contexts_validated": True,
            "input_byte": "a5+0x006a",
            "top_level_dispatch": {
                "dispatcher": f"0x{PLAYER_STATE_DISPATCH:06x}",
                "state_field": "object byte +3; byte value indexes the PC-relative handler table",
                "table": f"0x{PLAYER_STATE_TABLE:06x}",
                "handlers": player_state_handlers,
                "jump_substate_routine": "0x00c78a is a separate three-entry fixed-controller jump dispatcher, not an entry in Willow actor 0x125d0's top-level state table. Stage 0's fixed-record Button 2 route into it is byte-validated; movement of that fixed record and the Willow actor remain distinct records connected indirectly through the shared target",
                "state_8_handler_context": "validated dispatcher entry 0x12ac6 is a separate indexed state routine; its initial substate dispatch does not reference 0x00c78a",
            },
            "state_8": {
                "entry_sites": ["0x00c892", "0x00dce0"],
                "handler": "0x0012ac6",
                "initial_animation": "0x0013154",
                "movement_vector_table": "0x00136b8",
                "moving_substate_ticks": state8_move_ticks,
                "wall_pose_substate_ticks": state8_pose_ticks,
                "facing_timer_initial_ticks": state8_facing_initial_ticks,
                "facing_timer_repeat_ticks": state8_facing_repeat_ticks,
                "exit_gate": {
                    "routine": "0x0012d92",
                    "sample_cadence_frames": 8,
                    "player_x_relative_to_target": [-160, 159],
                    "player_y_relative_to_target": [-48, 47],
                    "exit_when_inside_target_window": True,
                    "outside_window_behavior": "continue state 8",
                    "exit_state": 4,
                    "transition": "0x12ae4 installs vector table 0x136b0 in +24 and branches to 0x1283e, which sets top-level state 4/substate 0",
                    "byte_validation": {
                        "state8_exit_transition": player_input_checks_valid[
                            "state8_exit_transition"],
                        "state4_normal_entry": player_input_checks_valid[
                            "state4_normal_entry"],
                    },
                },
                "runtime_note": "State 8 is an independent movement/contact cycle, not the 26-tick state-6 falling-pose transition at 0x00c906.",
            },
            "state_4": {
                "handler": "0x001284a",
                "entry": "0x001283e sets top-level state 4/substate 0",
                "entry_routes": {
                    "state2_side_contact": "0x00127b2 movement/probe tests byte +31 and branches to 0x001283e on contact",
                    "state2_timer": "0x00127d6 calls 0x0012ede and branches to 0x001283e when its timed mask returns nonzero",
                    "state8_target_window": "0x0012ae4 installs movement vector table 0x00136b0 then branches to 0x001283e",
                },
                "handler_behavior": "0x001284a applies 0x208c movement and 0x3b0a probe; byte +31 clear retries state 4/substate 0. On contact it advances animation, selects vertical band with 0x12d6e, and runs 0x12ede only in band 4. A set timed mask calls 0x20d2 and clears substate only for rounded heading 10 when facing right or 6 when facing left",
                "byte_validation": {
                    name: player_input_checks_valid[name] for name in (
                        "state2_no_contact_to_state4",
                        "state2_timer_to_state4",
                        "state4_normal_entry",
                        "state4_normal_handler",
                        "state4_vertical_band_timer_gate",
                        "state4_timed_heading_gate",
                        "state8_exit_transition",
                    )
                },
            },
            "jump_substate_dispatch": {
                "dispatcher": f"0x{PLAYER_JUMP_SUBSTATE_DISPATCH:06x}",
                "table": f"0x{PLAYER_JUMP_SUBSTATE_TABLE:06x}",
                "handlers": player_jump_substates,
            },
            "horizontal_selector": "0x00c52a selects rows 0/1 from direction bits; with neither held it preserves facing and selects slow rows 2/3",
            "down_selector": "0x00c550 tests input bit 2; state transition 0x00c4d6 enters substate 4 and selects animation table 0x00d086",
            "down_movement": "0x00c55a selects row 2 or 3 from horizontal direction bits while substate 4 is active",
            "stage0_jump_motion": "launch handler 0x00c7bc sets +56 and selects 0x00d27a without moving Y. Next-update substate 2 at 0x00c7ec adds +56 to 16.16 Y, runs 0x39fe, subtracts +52, and transfers to substate 4 with speed zero on contact or when the subtraction becomes negative. Ascent at 0x00c820 subtracts +56 from Y, adds +52 while the unsigned speed is below 0x0700, then checks 0x38f6 and the Up-gated 0x00c57a probe. Its common tail applies the selected X vector through 0x20b4",
            "up_jump_behavior": "the Stage 0 fixed controller reaches 0x00c78a through its computed state dispatch; launch handler 0x00c7bc selects speed 0x0440 or 0x0580 from bit 3 and animation table 0x00d27a. Ascent handler 0x00c820 reloads 0x00d27a when the facing selector changes. Probe 0x00c57a gates its 0x3a84 call on Up bit 3 at 0x00c874. Selector-1 controller 0x00d4ba has a separate Button 2 route through 0x00db30/0x00dc04 to 0x00dc0a",
            "jump_ascent_collision_exit": "the Stage 0 route exits at 0x00c856 when byte +36 is set or the 0x38f6 check reports contact; it clears jump mode and state bytes, copies +15 to +48, and queues sequenced cue 0x2b. The selector-1 route duplicates the ascent handler at 0x00dc90 and exits at 0x00dca4; its state-zero handler at 0x00daec runs on the next update and selects 0x00e194. Native Stage 0 recovery resumes its normal walk/idle pose; cue 0x2b remains unresolved for native playback",
            "stage0_jump_animation": {
                "launch_handler": "0x00c7bc selects 0x00d27a",
                "facing_change_handler": "0x00c888 reloads 0x00d27a after copying +15 to +48",
                "verified_pose_table": "player_aux_d27a",
                "other_jump_tables": "0x00d2ca/0x00d2ce are selected by internal dispatcher 0x00c8f2, with no verified link to the Stage 0 actor",
                "byte_validation": {name: player_input_checks_valid[name] for name in (
                    "stage0_jump_animation_start",
                    "stage0_jump_animation_facing_reload",
                )},
            },
        },
        "player_landing_contact": {
            "source_contexts_validated": True,
            "jump_fall_entry_timing": {
                "source_contexts_validated": True,
                "route_status": "the 0xc8f2/0xc906/0xc9da falling path is cataloged, but no verified link connects it to the Stage 0 fixed-controller route at 0xc78a or Willow actor 0x125d0",
                "state_table": "0x00c906 maps substates 2 and 4 to the six-tick transition handler and substate 6 to the eight-tick fall-pose handler",
                "timer_values": {
                    "transition_setup": jump_transition_ticks,
                    "transition_reload": jump_repeat_ticks,
                    "fall_pose": jump_fall_pose_ticks,
                },
                "total_ticks": jump_fall_entry_delay,
                "derivation": "the initial six-tick transition plus two six-tick reloads, then the eight-tick fall pose",
            },
            "normal_update": "0x00c9da runs the state-2 substate, applies movement, calls the 0x38f6 collision profile, then branches to 0x00c72e",
            "contact_response": "0x00c72e checks byte +36; on contact it clears vertical speed +58 and jump mode +72",
            "stage_zero_profile": {
                "selected_by": "0x38f6 uses the nonzero +32 profile table and indexes row +71=0; the Stage 0 player record has +32=3 and +71=0",
                "facing_0": [read_player_pairs(0x39BE, 2)[0]["offset"],
                             read_player_pairs(0x39BE, 2)[1]["offset"]],
                "facing_1": [read_player_pairs(0x39C6, 2)[0]["offset"],
                             read_player_pairs(0x39C6, 2)[1]["offset"]],
                "contact_probe": "both points call 0x3e4c; its solid correction sets byte +36",
            },
        },
        "player_probe_tables": [
            {
                "source_offsets": ["0x003a64", "0x003a74"],
                "selected_by": "0x39fe: object field +32 selects the table; field +62 selects one of four 4-byte offsets",
                "entries": [read_player_pairs(0x3A64, 4), read_player_pairs(0x3A74, 4)],
            },
            {
                "source_offsets": ["0x003af2", "0x003b02"],
                "selected_by": "0x3a84: field +32 selects the table; facing selects one of two 4-byte offsets",
                "entries": [read_player_pairs(0x3AF2, 2), read_player_pairs(0x3B02, 2)],
            },
        ],
        "player_ascent_fallback_offsets": {
            "source_offset": "0x003afa",
            "selected_by": "0x3a84 after the 0x403c signed-shape probe misses",
            "entries": read_player_pairs(0x3AFA, 2),
        },
        "terrain_property_table": {
            "referenced_by": ["0x3e4c", "0x40a0", "0x4190"],
            "source_offset": f"0x{PROPERTY_TABLE:06x}",
            "entry_count": PROPERTY_COUNT,
            "entry_size": PROPERTY_SIZE,
            "lookup": "((cell_attribute & 0xfc) << 1)",
            "entries": properties,
        },
        "class10_effect_terrain_contact": {
            "source_contexts_validated": True,
            "profile_probe": "0x3d90 selects profile-zero offset (0,0), addresses the 0x3e28 map ring, and tail-jumps to 0x44ea",
            "dispatch": "0x44ea prioritizes property bit 3, then checks bit 0 before bit 1 for directional fallback",
            "shape_rule": "bit-3 contact compares the source shape height to the Y fraction without moving the actor or performing the 0x3e4c/0x40a0 retry",
        },
        "map_address_helper": {
            "routine": "0x3e28",
            "base": "0x918000",
            "map_cell_attribute_offset": 2,
            "byte_offset_formula": "((x & 0x03f0) << 2) + (((~y) & 0x0300) << 4) + (((~y) & 0x00f0) >> 2)",
            "coordinate_units": "16-pixel CPS map cells; 64x64-cell ring with vertically inverted Y",
        },
    }


def to_header(result: dict[str, object]) -> str:
    entries = result["terrain_property_table"]["entries"]
    flags = [int(entry["flags"], 16) for entry in entries]
    shapes = [int(entry["shape"], 16) for entry in entries]
    x_thresholds_facing0 = [int(entry["x_threshold_facing0"], 16) for entry in entries]
    x_thresholds_facing1 = [int(entry["x_threshold_facing1"], 16) for entry in entries]

    def emit_array(name: str, values: list[int]) -> list[str]:
        rows = [f"static const u8 {name}[{len(values)}] = {{"]
        for start in range(0, len(values), 16):
            rows.append("    " + ", ".join(
                f"0x{value:02x}" for value in values[start:start + 16]) + ",")
        rows.append("};")
        return rows

    def emit_probe_array(name: str, table: dict[str, object],
                         count: int) -> list[str]:
        rows = [f"static const s16 {name}[2][{count}][2] = {{"]
        for profile in table["entries"]:
            rows.append("    {")
            for entry in profile:
                x, y = entry["offset"]
                rows.append(f"        {{{x}, {y}}},")
            rows.append("    },")
        rows.append("};")
        return rows

    rows = [
        "/* Generated from Willow collision property records; no program code. */",
        "#ifndef WILLOW_COLLISION_H",
        "#define WILLOW_COLLISION_H",
        "#include <genesis.h>",
        f"#define WILLOW_COLLISION_PROPERTY_COUNT {len(flags)}",
        f"#define WILLOW_JUMP_FALL_ENTRY_DELAY {result['player_landing_contact']['jump_fall_entry_timing']['total_ticks']}",
        "static const s16 willow_player_velocity_8_8[4][2] = {",
    ]
    for x, y in result["player_velocity_table"]["entries"]:
        rows.append(f"    {{{x}, {y}}},")
    rows.append("};")
    rows.extend(emit_array("willow_collision_flags", flags))
    rows.extend(emit_array("willow_collision_shapes", shapes))
    rows.extend(emit_array("willow_collision_x_threshold_facing0", x_thresholds_facing0))
    rows.extend(emit_array("willow_collision_x_threshold_facing1", x_thresholds_facing1))
    frame_bounds = result["actor_player_frame_bounds"]
    for name, key in (("willow_actor_frame_bounds", "actor_entries"),
                      ("willow_player_frame_bounds", "player_entries")):
        bounds = frame_bounds[key]
        rows.append(f"static const s16 {name}[{len(bounds)}][4] = {{")
        for left, top, right, bottom in bounds:
            rows.append(f"    {{{left}, {top}, {right}, {bottom}}},")
        rows.append("};")
    player_tables = result["player_probe_tables"]
    rows.extend(emit_probe_array("willow_player_ground_probe_offsets",
                                 player_tables[0], 4))
    rows.extend(emit_probe_array("willow_player_ascent_probe_offsets",
                                 player_tables[1], 2))
    fallback_table = result["player_ascent_fallback_offsets"]["entries"]
    rows.append("static const s16 willow_player_ascent_fallback_offsets[2][2] = {")
    for entry in fallback_table:
        x, y = entry["offset"]
        rows.append(f"    {{{x}, {y}}},")
    rows.append("};")
    rows.append(f"static const s16 willow_player_movement_probe_offsets[{PROBE_COUNT}][2][2] = {{")
    for probe in result["movement_probe_table"]["entries"]:
        rows.append("    {")
        for facing in ("facing_0", "facing_1"):
            x, y = probe[facing]
            rows.append(f"        {{{x}, {y}}},")
        rows.append("    },")
    rows.extend(["};"])
    rows.extend(["#endif", ""])
    return "\n".join(rows)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", nargs="?", type=Path, default=Path("willow.zip"))
    parser.add_argument("--output", type=Path, default=Path("build/willow_collision.json"))
    parser.add_argument("--header", type=Path, default=Path("build/willow_collision.h"))
    args = parser.parse_args()
    try:
        result = catalog(assemble_maincpu(args.archive))
    except (OSError, ValueError, KeyError) as exc:
        parser.error(str(exc))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    args.header.parent.mkdir(parents=True, exist_ok=True)
    args.header.write_text(to_header(result), encoding="ascii")
    print(f"Catalogued {PROPERTY_COUNT} terrain property records and {PROBE_COUNT} movement probes")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
