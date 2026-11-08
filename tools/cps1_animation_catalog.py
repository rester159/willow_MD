#!/usr/bin/env python3
"""Dump Willow animation sequences discovered in offline 68000 disassembly.

The record stepping below mirrors routines 0x1f30 (sequence selection) and
0x1f4c (frame advance). The catalog retains raw bytes because frame fields
still need semantic labels and mapping to CPS sprite tile codes.
"""

from __future__ import annotations

import argparse
import json
import zipfile
from pathlib import Path

from cps1_program import assemble_maincpu

TABLES = {
    "willow_main": (0xD07E, 10),
    "willow_alt": (0xD086, 6),
    # Animation tables referenced directly by Willow's native player
    # routines. Names identify table addresses, not guessed action labels.
    "player_aux_d1a2": (0xD1A2, 2),
    "player_aux_d1a6": (0xD1A6, 2),
    "player_aux_d1aa": (0xD1AA, 2),
    "player_aux_d1ae": (0xD1AE, 2),
    "player_aux_d1b2": (0xD1B2, 2),
    "player_aux_d1b6": (0xD1B6, 2),
    "player_aux_d27a": (0xD27A, 2),
    "player_aux_d2ca": (0xD2CA, 2),
    "player_aux_d2ce": (0xD2CE, 2),
    "player_aux_d3be": (0xD3BE, 2),
    "player_aux_d3c2": (0xD3C2, 2),
    # An alternate pair selected by the object state handler near 0x101e6.
    # Keep the address-based name until its exact gameplay role is decoded.
    "enemy_state_11226": (0x11226, 2),
    # State 16's facing-specific return animation after the state-20 cleanup.
    "enemy_return_1136e": (0x1136E, 2),
    # State 18's downward drift pose, selected when its side-contact probe clears.
    "enemy_state18_1155e": (0x1155E, 2),
    # State-2 mode 12 selects this two-frame pause sequence after its timer.
    "enemy_state2_pause_1133a": (0x1133A, 2),
    # State-2 attack scan follow-up poses use these direction-specific tables.
    "enemy_state2_attack_followup_114f2": (0x114F2, 2),
    # State 6's 0x1f34 selectors use the player's facing byte to choose
    # records while the separate object heading advances toward the target.
    "player_state6_turn_1316c": (0x1316C, 2),
    "player_state6_facing_1318c": (0x1318C, 2),
    # Willow's state-2 substate-zero range trigger selects this facing pair
    # before entering its eight-tick sequence.
    "player_range_entry_131b0": (0x131B0, 3),
    # Controller state 0 creates the post-phase actor chain with behavior 8;
    # its handler selects one of these ten animation sequences by actor mode.
    "stage0_post_phase_actor_15666": (0x15666, 10),
    # Queue-class-14 behavior 0 selects these relative pointers from +33 when
    # entering state 2; descriptors 2 and 5 both resolve through table 0x2d314.
    "queue14_state0_animations_2d314": (0x2D314, 18),
    # State 4 indexes this relative-pointer table by +33, adding 12 when +47
    # is set; include all 18 possible selectors for both parent-link cases.
    "queue14_state4_animations_2d32c": (0x2D32C, 18),
}
CLASS2_BEHAVIOR0_STREAM_OFFSETS = (0x0032, 0x00AE, 0x011E, 0x0182)
CLASS2_BEHAVIOR0_STREAM_POINTER_TABLE = 0xFC64
CLASS2_BEHAVIOR0_SPAWN_OFFSETS = 0xFB90
CLASS2_BEHAVIOR0_SPAWN_DELAYS = 0xFC60
CLASS2_BEHAVIOR0_STATE0 = 0xFC10
CLASS2_BEHAVIOR0_STATE0_BYTES = bytes.fromhex(
    "542e000241faff7a4a2d1884670441faffb0"
    "4eb81d8e0240000fe54841f000003018d06d184c"
    "3d4000063010d06d18503d40000a4eb81d8e02400003"
    "1d7b00120022d040323b000e41fb100a4ef81f40"
)
CLASS2_BEHAVIOR0_STATE2 = 0xFC6C
CLASS2_BEHAVIOR0_STATE2_BYTES = bytes.fromhex(
    "102e0021323b000a4ebb10064ef82244"
)
CLASS2_BEHAVIOR0_STATE2_TAIL = 0xFC84
CLASS2_BEHAVIOR0_STATE2_TAIL_BYTES = bytes.fromhex(
    "532e00226604542e00024ef81f4c4ef829ec"
)
CLASS2_BEHAVIOR0_VISIBILITY = 0x2244
CLASS2_BEHAVIOR0_VISIBILITY_BYTES = bytes.fromhex(
    "4a2e00006736422e0001302e0006906d0408"
    "064000200c4001c06220302e000a906d040c"
    "0c4001106212522e00017000102e001e303b0008"
    "4efb0004"
)
STAGE0_CLASS12_LINKED_STREAM = 0x325BC
STAGE0_CLASS12_LINKED_SETUP = 0x32592
STAGE0_CLASS12_LINKED_HANDLER = 0x3257E
STAGE0_CLASS12_LINKED_DISPATCH = 0x3258A
STAGE0_CLASS12_LINKED_DISPATCH_BYTES = bytes.fromhex(
    "0008001afcfafcfa"
)
STAGE0_CLASS12_LINKED_STATE0_BYTES = bytes.fromhex(
    "1d7c0002000e542e000241fa001e4ef81f40"
)
STAGE0_CLASS12_LINKED_STATE2 = 0x325A4
STAGE0_CLASS12_LINKED_STATE2_BYTES = bytes.fromhex(
    "102d0019d0070240000f66044eb821c64eb81f4c4ef82244"
)
STAGE0_CLASS12_LINKED_RETIREMENT_HELPER = 0x21C6
STAGE0_CLASS12_LINKED_RETIREMENT_HELPER_BYTES = bytes.fromhex(
    "302d0344d040323b00064efb1002"
    "000c0020000c000c000c000c"
    "302e0006b06d038c6c061d7c0006000270004e75"
)
STAGE0_CAMERA_UPDATE_CALLER = 0x4281E
STAGE0_CAMERA_UPDATE_CALLER_BYTES = bytes.fromhex(
    "6100054661000100610004ee610002f0"
)
STAGE0_CAMERA_STAGE_HELPER = 0x42D66
STAGE0_CAMERA_STAGE_HELPER_BYTES = bytes.fromhex(
    "302d0344d040303b00064efb0002"
)
STAGE0_CAMERA_STAGE0_BRANCH = 0x42D82
STAGE0_CAMERA_STAGE0_BRANCH_BYTES = bytes.fromhex(
    "4a2d038a66224a2d03e7671c"
)
STAGE0_CAMERA_Y_HELPER = 0x42B1C
STAGE0_CAMERA_Y_HELPER_BYTES = bytes.fromhex(
    "610003487000102e0004303b00064efb0002"
)
STAGE0_CAMERA_STAGE_TRANSITION_HELPER = 0x42D16
STAGE0_CAMERA_STAGE_TRANSITION_BYTES = bytes.fromhex(
    "0c6d000303446646302e00060c401b00651c"
)
STAGE0_CAMERA_TARGET_SETUP = 0x42602
STAGE0_CAMERA_TARGET_SETUP_BYTES = bytes.fromhex(
    "2c7c00ff84023d6d184c006a3d6d1850006c"
    "4a2d0348660c3d6d8fb0006a3d6d8fb2006c"
)
STAGE0_CAMERA_TARGET_INIT = 0xA11C
STAGE0_CAMERA_TARGET_INIT_BYTES = bytes.fromhex(
    "3b7c00c08fb03b7c02808fb2"
)
STAGE0_CAMERA_TARGET_ALT_INIT = 0xA162
STAGE0_CAMERA_TARGET_ALT_INIT_BYTES = bytes.fromhex(
    "3b7c00c08fb03b7c10508fb2"
)
STAGE0_CAMERA_TARGET_X_READ = 0x4261A
STAGE0_CAMERA_TARGET_X_READ_BYTES = bytes.fromhex(
    "3d6d8fb0006a"
)
STAGE0_CAMERA_ALT_TARGET_INIT = 0xA162
STAGE0_CAMERA_ALT_TARGET_INIT_BYTES = bytes.fromhex(
    "3b7c00c08fb03b7c10508fb2"
)
STAGE0_CAMERA_SCRIPTED_Y_STEP = 0xA2FE
STAGE0_CAMERA_SCRIPTED_Y_STEP_BYTES = bytes.fromhex(
    "556d8fb2"
)
STAGE0_CAMERA_SCRIPTED_Y_HANDLER = 0xA2F8
STAGE0_CAMERA_SCRIPTED_Y_HANDLER_BYTES = bytes.fromhex(
    "4a6d040c6710556d8fb24eb900009ef24ef90003b524"
)
STAGE0_CAMERA_TASK_TABLE = 0x9F96
STAGE0_CAMERA_TASK_TABLE_BYTES = bytes.fromhex(
    "014002a402cc0334"
)
STAGE0_CAMERA_TASK_STATE6 = 0xA0D0
STAGE0_CAMERA_TASK_STATE6_BYTES = bytes.fromhex(
    "422d799c4eb82de64eb826484eb80978"
)
STAGE0_CAMERA_TASK_STATE8 = 0xA234
STAGE0_CAMERA_TASK_STATE8_BYTES = bytes.fromhex(
    "536d000a6616546d00083b7c003f000a"
    "303c1002323c00024eb81f14"
    "4eb9000426024ef900042f48"
)
STAGE0_CAMERA_TASK_STATE10 = 0xA25C
STAGE0_CAMERA_TASK_STATE10_BYTES = bytes.fromhex(
    "536d000a67024e751b7c000102a63b7c001d000a546d0008"
)
STAGE0_CAMERA_BOUND_PUBLISHER = 0x428AE
STAGE0_CAMERA_BOUND_PUBLISHER_BYTES = bytes.fromhex(
    "3b6e0064038c"
)
STAGE0_CAMERA_TRACKER = 0x4298E
STAGE0_CAMERA_TRACKER_BYTES = bytes.fromhex(
    "302e006a906e00060c4000c06e046d284e75"
    "610004ac044000c0d16e0006302e0068b06e00066208"
    "3d6e006800064e75"
)
STAGE0_CAMERA_TRACKER_GATE = 0x42942
STAGE0_CAMERA_TRACKER_GATE_BYTES = bytes.fromhex(
    "4a6d034466164a2d038a6610"
    "302e006806400040b06d184c6232600a"
    "302e0068b06d184c62264a2d0362671a"
)
STAGE0_CAMERA_TRACKER_CLAMP = 0x429C6
STAGE0_CAMERA_TRACKER_CLAMP_BYTES = bytes.fromhex(
    "044000c0d16e0006302e0064b06e00066e08"
    "1d7c0004004c4e75"
    "3d6e006400064e75"
)
STAGE0_CAMERA_BOUND_HELPER = 0x42E4E
STAGE0_CAMERA_BOUND_HELPER_BYTES = bytes.fromhex(
    "322e0006044103006a024241b26e006463043d4100644e75"
)
STAGE0_CLASS12_LINKED_STATE6_CLEANUP = 0x32284
STAGE0_CLASS12_LINKED_STATE6_CLEANUP_BYTES = bytes.fromhex("4ef82bb8")
ENEMY_ATTACK_RECOVERY_STREAM = 0x1149E
ENEMY_ATTACK_RECOVERY_STREAM_BYTES = bytes.fromhex(
    "0010331000160004000601c40010331080160004ffff01c0"
)
ENEMY_FOLLOWUP_BELOW_RECORDS = {
    0x1144E: bytes.fromhex("00083110001700010004018e"),
    0x1145A: bytes.fromhex("00083110001a0004000401c8"),
}
ROM_LIMIT = 0x100000
MAX_RECORDS = 256
STATE6_RECOVERY_MASK_LOOKUP = 0x12A64
STATE6_RECOVERY_MASK_LOOKUP_BYTES = bytes.fromhex(
    "4eb81d8e0240000f7200122d0346e6490241001e"
    "323b101c01016600000a1d7c000100224e75"
)
STAGE0_ACTOR_DELAY_LOOKUP = 0x1168E
STAGE0_ACTOR_DELAY_LOOKUP_BYTES = bytes.fromhex(
    "4eb81d8e0240000f1d7b00060034"
)
STAGE0_ACTOR_DELAY_REUSE = 0x116BE
STAGE0_ACTOR_DELAY_REUSE_BYTES = bytes.fromhex("61ce")
STAGE0_ACTOR_DELAY_TABLE = 0x1169E
STAGE0_ACTOR_DELAY_TABLE_BYTES = bytes.fromhex(
    "7878b4f0b478f0b4f0b4b4f0b4f078b4"
)
STAGE0_ACTOR_VOICE_WAIT = 0x1197C
STAGE0_ACTOR_VOICE_WAIT_BYTES = bytes.fromhex("1d7c003c0022")
STAGE0_ACTOR_VOICE_FOLLOWUP = 0x119CA
STAGE0_ACTOR_VOICE_FOLLOWUP_BYTES = bytes.fromhex("1d7c003a0022")
STATE2_NORMAL_MASK_LOOKUP = 0x12EE8
STATE2_NORMAL_MASK_LOOKUP_BYTES = bytes.fromhex(
    "4eb81d8e0240000f72007400122d03463401e8490241000e"
    "323b101ee84a1d7b201000460101670470014e7570004e75"
)
STATE2_VERTICAL_BAND_SELECTOR = 0x12D6E
STATE2_VERTICAL_BAND_SELECTOR_BYTES = bytes.fromhex(
    "322e000a926d1850064100300c410060620470004e75"
    "044100306b0470044e7570024e75"
)
STAGE0_ACTOR_APPROACH_VECTORS = 0xED088
STAGE0_ACTOR_APPROACH_VECTOR_BYTES = bytes.fromhex(
    "03000000fd000000"
)
STAGE0_ACTOR_PATROL_VECTORS = 0xED070
STAGE0_ACTOR_PATROL_VECTOR_BYTES = bytes.fromhex("01800000fe800000")
STAGE0_ACTOR_RECOVERY_STALL = 0x11AE4
STAGE0_ACTOR_RECOVERY_STALL_BYTES = bytes.fromhex(
    "542e00041d7c0008002241fa04ec4ef81f30")
POST_PHASE_ACTOR_DELAY_TABLE = 0x15648
POST_PHASE_ACTOR_DELAY_BYTES = bytes.fromhex("3d3d3d3d3d3d3d353535")
POST_PHASE_ACTOR_VECTORS = 0x155CC
POST_PHASE_ACTOR_VECTOR_BYTES = bytes.fromhex("fa00fe0003030404050507070909")
PLAYER_HEADING_BASES = 0x2144
PLAYER_HEADING_BASE_BYTES = bytes.fromhex("008101804140c0c1")
PLAYER_RANGE_ENTRY = 0x13768
PLAYER_RANGE_ENTRY_BYTES = bytes.fromhex(
    "3c2e000a9c6d040c0c460090621e542e0003"
    "1d7c00080022610001261d40000f61000130"
)


def be16(data: bytes, offset: int) -> int:
    return int.from_bytes(data[offset : offset + 2], "big")


def be32(data: bytes, offset: int) -> int:
    return int.from_bytes(data[offset : offset + 4], "big")


def animation_record_length(data: bytes, address: int) -> int:
    """Translate 0x1f4c's next-record calculation into a byte distance."""
    control = be16(data, address)
    # 0x1f4c always doubles the cleared control word once and doubles it a
    # second time only when bit 15 was set. A nonzero byte at +2 overrides
    # the computed size with two before the final ten-byte header is added.
    length = (control & 0x7FFF) << (2 if control & 0x8000 else 1)
    if data[address + 2] != 0:
        length = 2
    return length + 10


def follow_stream(data: bytes, start: int, index: int = 0,
                  full_records: bool = False) -> dict[str, object]:
    """Walk an animation stream whose first record address is already known."""
    address = start
    seen: set[int] = set()
    records: list[dict[str, object]] = []
    termination = "record limit"

    for _ in range(MAX_RECORDS):
        if address < 0 or address + 12 > ROM_LIMIT:
            termination = "outside ROM"
            break
        if address in seen:
            termination = "loop"
            break
        seen.add(address)
        length = animation_record_length(data, address)
        record = {
            "rom_offset": f"0x{address:06x}",
            "length": length,
            "timer": be16(data, address + 8),
            "words": [f"0x{be16(data, address + i):04x}" for i in range(0, 12, 2)],
            "source_color_group": be16(data, address + 2) & 0x1F,
            "raw": data[address : address + (length if full_records
                                                else min(length, 64))].hex(),
        }
        records.append(record)

        # Avoid walking into following pointer/data blocks as visible frames
        # when the 16-bit timer is zero or implausibly long for an animation.
        # The catalog preserves that control record for later interpretation.
        timer = int(record["timer"])
        if timer == 0 or timer > 0x1000:
            termination = "long-delay/control record"
            break

        next_address = address + length
        if next_address + 6 > ROM_LIMIT:
            termination = "next record outside ROM"
            break
        # 0x1f4c tests the signed word at current_record + 4, then loads
        # the long pointer at the computed next-record address when set.
        if be16(data, address + 4) & 0x8000:
            if next_address + 4 > ROM_LIMIT:
                termination = "indirect pointer outside ROM"
                break
            next_address = be32(data, next_address)
        if next_address == address:
            termination = "self reference"
            break
        address = next_address
    else:
        termination = "record limit"

    return {
        "index": index,
        "start": f"0x{start:06x}",
        "termination": termination,
        "records": records,
    }


def follow_sequence(data: bytes, table: int, index: int) -> dict[str, object]:
    start = table + be16(data, table + index * 2)
    return follow_stream(data, start, index)


def find_selector_tables(data: bytes) -> dict[int, list[int]]:
    """Find `lea table(pc),a0; jsr $1f30` pairs in the offline ROM image."""
    references: dict[int, list[int]] = {}
    for call_site in range(0, len(data) - 8, 2):
        if (data[call_site : call_site + 2] == b"\x41\xfa"
                and data[call_site + 4 : call_site + 8] in
                (b"\x4e\xb8\x1f\x30", b"\x4e\xf8\x1f\x30")):
            displacement = int.from_bytes(data[call_site + 2 : call_site + 4], "big", signed=True)
            # 68000 PC-relative LEA uses the extension-word address as base.
            table = call_site + 2 + displacement
            if 0 <= table < ROM_LIMIT:
                references.setdefault(table, []).append(call_site)
    return references


def plausible_sequence_start(data: bytes, table: int, index: int) -> bool:
    """Reject table words that do not point at a plausible CPS sprite record."""
    pointer_at = table + index * 2
    if pointer_at + 2 > ROM_LIMIT:
        return False
    address = table + be16(data, pointer_at)
    if address < 0 or address + 12 > ROM_LIMIT:
        return False
    descriptor = be16(data, address + 2)
    timer = be16(data, address + 8)
    frame_code = be16(data, address + 10)
    width = ((descriptor >> 8) & 0x0F) + 1
    height = ((descriptor >> 12) & 0x0F) + 1
    # This is a conservative candidate filter for scanning the entire
    # program image. It is deliberately narrower than the real 15-bit CPS
    # graphics address space; emitted/validated frame records use 0x8000.
    if width > 8 or height > 8 or frame_code >= 0x5000:
        return False
    if timer == 0 or (timer > 0x1000 and timer != 0xFFFF):
        return False
    return True


def build_catalog(data: bytes) -> dict[str, object]:
    class2_behavior0_checks = (
        (CLASS2_BEHAVIOR0_STATE0, CLASS2_BEHAVIOR0_STATE0_BYTES,
         "state-0 spawn and stream selection"),
        (CLASS2_BEHAVIOR0_STATE2, CLASS2_BEHAVIOR0_STATE2_BYTES,
         "state-2 behavior dispatch and visibility"),
        (CLASS2_BEHAVIOR0_STATE2_TAIL,
         CLASS2_BEHAVIOR0_STATE2_TAIL_BYTES,
         "state-2 lifetime and animation update"),
        (CLASS2_BEHAVIOR0_VISIBILITY,
         CLASS2_BEHAVIOR0_VISIBILITY_BYTES,
         "visibility range filter"),
    )
    for address, expected, label in class2_behavior0_checks:
        if data[address:address + len(expected)] != expected:
            raise ValueError(
                f"Class-2 behavior-0 {label} at {address:#x} changed")

    animation_helpers = {
        0x1F30: bytes.fromhex(
            "102e000f4880d0003030000041f000002d4800143d680008001c"
            "4e75536e001c6634206e001430100880000f6702d040d040"
            "12280002670270020640000a3228000441f000004a416a02"
            "20502d4800143d680008001c4e75"),
        # 0x3b880 copies the low five color bits from each rendered CPS
        # sprite record. If object +18 is set, 0x3b8cc replaces those bits
        # with object +16 before the sprite attribute is emitted.
        0x3B8C6: bytes.fromhex("4a29001267080242ffe084690010"),
    }
    for address, expected in animation_helpers.items():
        if data[address:address + len(expected)] != expected:
            raise ValueError(f"Animation selector/advance helper at {address:#x} changed")

    class12_actor_checks = (
        (STAGE0_CLASS12_LINKED_DISPATCH,
         STAGE0_CLASS12_LINKED_DISPATCH_BYTES,
         "state dispatch table"),
        (STAGE0_CLASS12_LINKED_SETUP,
         STAGE0_CLASS12_LINKED_STATE0_BYTES,
         "state-0 setup"),
        (STAGE0_CLASS12_LINKED_STATE2,
         STAGE0_CLASS12_LINKED_STATE2_BYTES,
         "state-2 update"),
        (STAGE0_CLASS12_LINKED_RETIREMENT_HELPER,
         STAGE0_CLASS12_LINKED_RETIREMENT_HELPER_BYTES,
         "periodic retirement helper"),
        (STAGE0_CLASS12_LINKED_STATE6_CLEANUP,
         STAGE0_CLASS12_LINKED_STATE6_CLEANUP_BYTES,
         "state-6 cleanup route"),
    )
    for address, expected, label in class12_actor_checks:
        if data[address:address + len(expected)] != expected:
            raise ValueError(
                f"Stage 0 class-12 actor {label} at {address:#x} changed")
    stage0_camera_checks = (
        (STAGE0_CAMERA_TARGET_INIT,
         STAGE0_CAMERA_TARGET_INIT_BYTES,
         "normal Stage 0 camera target initializer"),
        (STAGE0_CAMERA_TARGET_ALT_INIT,
         STAGE0_CAMERA_TARGET_ALT_INIT_BYTES,
         "alternate camera target initializer"),
        (STAGE0_CAMERA_TARGET_X_READ,
         STAGE0_CAMERA_TARGET_X_READ_BYTES,
         "camera target X read"),
        (STAGE0_CAMERA_ALT_TARGET_INIT,
         STAGE0_CAMERA_ALT_TARGET_INIT_BYTES,
         "alternate stage camera target initializer"),
        (STAGE0_CAMERA_SCRIPTED_Y_STEP,
         STAGE0_CAMERA_SCRIPTED_Y_STEP_BYTES,
         "scripted camera-target Y step"),
        (STAGE0_CAMERA_SCRIPTED_Y_HANDLER,
         STAGE0_CAMERA_SCRIPTED_Y_HANDLER_BYTES,
         "scripted camera-target Y guard and update continuation"),
        (STAGE0_CAMERA_TASK_TABLE,
         STAGE0_CAMERA_TASK_TABLE_BYTES,
         "Stage 0 camera manager state table entries"),
        (STAGE0_CAMERA_TASK_STATE6,
         STAGE0_CAMERA_TASK_STATE6_BYTES,
         "Stage 0 normal initializer dispatch predecessor"),
        (STAGE0_CAMERA_TASK_STATE8,
         STAGE0_CAMERA_TASK_STATE8_BYTES,
         "Stage 0 camera state-8 timer and camera update"),
        (STAGE0_CAMERA_TASK_STATE10,
         STAGE0_CAMERA_TASK_STATE10_BYTES,
         "Stage 0 camera state-10 timer and state advance"),
        (STAGE0_CAMERA_TARGET_SETUP,
         STAGE0_CAMERA_TARGET_SETUP_BYTES,
         "Stage 0 player-follow target setup"),
        (STAGE0_CAMERA_UPDATE_CALLER,
         STAGE0_CAMERA_UPDATE_CALLER_BYTES,
         "camera frame update call path"),
        (STAGE0_CAMERA_STAGE_HELPER,
         STAGE0_CAMERA_STAGE_HELPER_BYTES,
         "stage-indexed camera helper dispatch"),
        (STAGE0_CAMERA_STAGE0_BRANCH,
         STAGE0_CAMERA_STAGE0_BRANCH_BYTES,
         "Stage 0 camera helper branch"),
        (STAGE0_CAMERA_Y_HELPER,
         STAGE0_CAMERA_Y_HELPER_BYTES,
         "camera Y transition helper"),
        (STAGE0_CAMERA_STAGE_TRANSITION_HELPER,
         STAGE0_CAMERA_STAGE_TRANSITION_BYTES,
         "stage-transition camera helper"),
        (STAGE0_CAMERA_BOUND_PUBLISHER,
         STAGE0_CAMERA_BOUND_PUBLISHER_BYTES,
         "published lower camera bound"),
        (STAGE0_CAMERA_TRACKER,
         STAGE0_CAMERA_TRACKER_BYTES,
         "center-follow and bound-clamp routine"),
        (STAGE0_CAMERA_TRACKER_GATE,
         STAGE0_CAMERA_TRACKER_GATE_BYTES,
         "Stage 0 camera tracker gate"),
        (STAGE0_CAMERA_TRACKER_CLAMP,
         STAGE0_CAMERA_TRACKER_CLAMP_BYTES,
         "Stage 0 camera lower-bound clamp"),
        (STAGE0_CAMERA_BOUND_HELPER,
         STAGE0_CAMERA_BOUND_HELPER_BYTES,
         "768-pixel trailing-bound update"),
    )
    for address, expected, label in stage0_camera_checks:
        if data[address:address + len(expected)] != expected:
            raise ValueError(f"Stage 0 camera {label} at {address:#x} changed")

    post_phase_table = 0x15666
    post_phase_table_bytes = bytes.fromhex(
        "0014006c00c400c4011c011c011c017401740174")
    if data[post_phase_table:post_phase_table + len(post_phase_table_bytes)] != post_phase_table_bytes:
        raise ValueError("Stage 0 post-phase actor animation table at 0x15666 changed")
    # Verify that the controller's actor creator and behavior-8 initializer
    # still connect this table to the chained transition described by the
    # native story controller.
    post_phase_actor_sites = {
        0x9034: bytes.fromhex("4eb82a1a197c00010000197c00080020197c00010021397c02d80006397c0388000a"),
        0x155A8: bytes.fromhex("542e0002422e000f2d7c000155cc0018"),
        0x155C4: bytes.fromhex("41fa00a04ef81f34"),
        0x15604: bytes.fromhex("197c00010000197c00080020197c00010021"),
    }
    for address, expected in post_phase_actor_sites.items():
        if data[address:address + len(expected)] != expected:
            raise ValueError(
                f"Stage 0 post-phase actor source at {address:#x} changed")
    if data[POST_PHASE_ACTOR_DELAY_TABLE:
            POST_PHASE_ACTOR_DELAY_TABLE + len(POST_PHASE_ACTOR_DELAY_BYTES)] != POST_PHASE_ACTOR_DELAY_BYTES:
        raise ValueError("Stage 0 post-phase actor delays at 0x15648 changed")
    if data[POST_PHASE_ACTOR_VECTORS:
            POST_PHASE_ACTOR_VECTORS + len(POST_PHASE_ACTOR_VECTOR_BYTES)] != POST_PHASE_ACTOR_VECTOR_BYTES:
        raise ValueError("Stage 0 post-phase actor vectors at 0x155cc changed")

    recovery_end = (ENEMY_ATTACK_RECOVERY_STREAM
                    + len(ENEMY_ATTACK_RECOVERY_STREAM_BYTES))
    if data[ENEMY_ATTACK_RECOVERY_STREAM:recovery_end] != ENEMY_ATTACK_RECOVERY_STREAM_BYTES:
        raise ValueError("Enemy attack recovery stream at 0x1149e changed")
    lookup_end = STATE6_RECOVERY_MASK_LOOKUP + len(STATE6_RECOVERY_MASK_LOOKUP_BYTES)
    if data[STATE6_RECOVERY_MASK_LOOKUP:lookup_end] != STATE6_RECOVERY_MASK_LOOKUP_BYTES:
        raise ValueError("Stage 0 state-6 recovery mask lookup at 0x12a64 changed")
    delay_regions = (
        (STAGE0_ACTOR_DELAY_LOOKUP, STAGE0_ACTOR_DELAY_LOOKUP_BYTES,
         "Stage 0 actor delay lookup at 0x1168e"),
        (STAGE0_ACTOR_DELAY_REUSE, STAGE0_ACTOR_DELAY_REUSE_BYTES,
         "Stage 0 actor delay-table reuse at 0x116be"),
        (STAGE0_ACTOR_DELAY_TABLE, STAGE0_ACTOR_DELAY_TABLE_BYTES,
         "Stage 0 actor delay table at 0x1169e"),
        (STAGE0_ACTOR_VOICE_WAIT, STAGE0_ACTOR_VOICE_WAIT_BYTES,
         "Stage 0 actor voice wait setup at 0x1197c"),
        (STAGE0_ACTOR_VOICE_FOLLOWUP,
         STAGE0_ACTOR_VOICE_FOLLOWUP_BYTES,
         "Stage 0 actor voice follow-up setup at 0x119ca"),
        (STAGE0_ACTOR_RECOVERY_STALL, STAGE0_ACTOR_RECOVERY_STALL_BYTES,
         "Stage 0 recovery stall animation selection at 0x11ae4"),
    )
    for offset, expected, label in delay_regions:
        if data[offset:offset + len(expected)] != expected:
            raise ValueError(f"{label} changed")
    normal_lookup_end = STATE2_NORMAL_MASK_LOOKUP + len(STATE2_NORMAL_MASK_LOOKUP_BYTES)
    if data[STATE2_NORMAL_MASK_LOOKUP:normal_lookup_end] != STATE2_NORMAL_MASK_LOOKUP_BYTES:
        raise ValueError("Willow state-2 normal mask lookup at 0x12ee8 changed")
    if data[PLAYER_HEADING_BASES:PLAYER_HEADING_BASES + 8] != PLAYER_HEADING_BASE_BYTES:
        raise ValueError("Willow state-6 heading bases at 0x2144 changed")
    if data[PLAYER_RANGE_ENTRY:PLAYER_RANGE_ENTRY + len(PLAYER_RANGE_ENTRY_BYTES)] \
            != PLAYER_RANGE_ENTRY_BYTES:
        raise ValueError("Willow state-2 range entry at 0x13768 changed")
    vertical_selector_end = (STATE2_VERTICAL_BAND_SELECTOR
                             + len(STATE2_VERTICAL_BAND_SELECTOR_BYTES))
    if data[STATE2_VERTICAL_BAND_SELECTOR:vertical_selector_end] != STATE2_VERTICAL_BAND_SELECTOR_BYTES:
        raise ValueError("Willow state-2 vertical band selector at 0x12d6e changed")
    approach_end = (STAGE0_ACTOR_APPROACH_VECTORS
                    + len(STAGE0_ACTOR_APPROACH_VECTOR_BYTES))
    if data[STAGE0_ACTOR_APPROACH_VECTORS:approach_end] != STAGE0_ACTOR_APPROACH_VECTOR_BYTES:
        raise ValueError("Stage 0 actor approach vectors at 0xed088 changed")
    approach_vectors = [
        (int.from_bytes(STAGE0_ACTOR_APPROACH_VECTOR_BYTES[index:index + 2], "big", signed=True),
         int.from_bytes(STAGE0_ACTOR_APPROACH_VECTOR_BYTES[index + 2:index + 4], "big", signed=True))
        for index in (0, 4)
    ]
    patrol_end = STAGE0_ACTOR_PATROL_VECTORS + len(STAGE0_ACTOR_PATROL_VECTOR_BYTES)
    if data[STAGE0_ACTOR_PATROL_VECTORS:patrol_end] != STAGE0_ACTOR_PATROL_VECTOR_BYTES:
        raise ValueError("Stage 0 actor patrol vectors at 0xed070 changed")
    patrol_vectors = [
        (int.from_bytes(STAGE0_ACTOR_PATROL_VECTOR_BYTES[index:index + 2], "big", signed=True),
         int.from_bytes(STAGE0_ACTOR_PATROL_VECTOR_BYTES[index + 2:index + 4], "big", signed=True))
        for index in (0, 4)
    ]

    # The two player-controller state-0 handlers initialize object field +52,
    # which the shared jump handler adds to its 8.8 vertical speed.
    jump_accel_sites = (0xBF94, 0xD514)
    jump_accels = [be16(data, site + 2) for site in jump_accel_sites]
    if any(data[site:site + 2] != b"\x3d\x7c" for site in jump_accel_sites):
        raise ValueError("Player jump-acceleration initializer opcode changed")
    if jump_accels != [0x0038, 0x0038]:
        raise ValueError("Player controller variants disagree on jump acceleration")
    if (int.from_bytes(data[0xBF3A:0xBF3E], "big") != 0x0000BF42
            or int.from_bytes(data[0xBF3E:0xBF42], "big") != 0x0000D4BA):
        raise ValueError("Player controller dispatch table no longer selects both validated handlers")

    table_specs = dict(TABLES)
    known_addresses = {address for address, _ in TABLES.values()}
    call_sites = find_selector_tables(data)
    for table in call_sites:
        if table in known_addresses:
            continue
        count = 0
        while count < 16 and plausible_sequence_start(data, table, count):
            count += 1
        if count:
            table_specs[f"source_table_{table:06x}"] = (table, count)

    tables: dict[str, object] = {}
    for name, (address, sequence_count) in table_specs.items():
        entries = []
        for index in range(sequence_count):
            entries.append(follow_sequence(data, address, index))
        if address == 0x15666:
            for sequence in entries:
                for record in sequence["records"]:
                    record_address = int(record["rom_offset"], 16)
                    descriptor = be16(data, record_address + 2)
                    origin = be16(data, record_address + 4)
                    template_index = (origin & 0x7F80) >> 5
                    template_slot = origin & 0x007F
                    if template_index not in (0, 4, 8):
                        raise ValueError("Unexpected post-phase CPS origin template")
                    table_offset = 0x3B9B2 + template_index
                    template = be32(data, table_offset)
                    displacement = int.from_bytes(
                        data[template + template_slot * 2:
                             template + template_slot * 2 + 2],
                        "big", signed=True)
                    layout = template + displacement
                    resolved = [
                        int.from_bytes(data[layout + offset:layout + offset + 2],
                                       "big", signed=True)
                        for offset in (0, 2)
                    ]
                    width = ((descriptor >> 8) & 0x0F) + 1
                    height = ((descriptor >> 12) & 0x0F) + 1
                    layout_coordinates = [
                        (int.from_bytes(data[layout + index * 4:
                                             layout + index * 4 + 2],
                                        "big", signed=True),
                         int.from_bytes(data[layout + index * 4 + 2:
                                             layout + index * 4 + 4],
                                        "big", signed=True))
                        for index in range(width * height)
                    ]
                    expected_coordinates = {
                        (resolved[0] + column * 16,
                         resolved[1] + row * 16)
                        for row in range(height) for column in range(width)
                    }
                    if set(layout_coordinates) != expected_coordinates:
                        raise ValueError(
                            "Post-phase CPS frame does not map to a regular Genesis tile block")
                    record["resolved_origin"] = resolved
                    record["resolved_layout"] = layout_coordinates
        table = {"table": f"0x{address:06x}", "sequences": entries}
        if address in call_sites:
            table["selector_call_sites"] = [f"0x{site:06x}" for site in call_sites[address]]
        tables[name] = table
    class2_pointer_bytes = b"".join(
        offset.to_bytes(2, "big")
        for offset in CLASS2_BEHAVIOR0_STREAM_OFFSETS)
    pointer_end = CLASS2_BEHAVIOR0_STREAM_POINTER_TABLE + len(class2_pointer_bytes)
    if data[CLASS2_BEHAVIOR0_STREAM_POINTER_TABLE:pointer_end] != class2_pointer_bytes:
        raise ValueError("Class-2 behavior-0 animation pointer table changed")
    class2_streams = [
        follow_stream(data, CLASS2_BEHAVIOR0_STREAM_POINTER_TABLE + offset, index)
        for index, offset in enumerate(CLASS2_BEHAVIOR0_STREAM_OFFSETS)
    ]
    for sequence in class2_streams:
        visible_records = [record for record in sequence["records"]
                           if 0 < int(record["timer"]) <= 0x1000]
        if not visible_records:
            raise ValueError("Class-2 behavior-0 animation stream has no frames")
        for record in visible_records:
            words = [int(word, 16) for word in record["words"]]
            if words[5] >= 0x8000:
                raise ValueError("Class-2 behavior-0 stream references a non-sprite code")
            descriptor = words[1]
            if ((descriptor >> 8) & 0x0F) != 0 or ((descriptor >> 12) & 0x0F) != 0:
                raise ValueError("Class-2 behavior-0 sprite block is not 16x16")
    spawn_offset_bytes = data[CLASS2_BEHAVIOR0_SPAWN_OFFSETS:
                              CLASS2_BEHAVIOR0_SPAWN_OFFSETS + 64]
    spawn_offsets = [
        [int.from_bytes(spawn_offset_bytes[index:index + 2], "big", signed=True),
         int.from_bytes(spawn_offset_bytes[index + 2:index + 4], "big", signed=True)]
        for index in range(0, 64, 4)
    ]
    spawn_delay_bytes = data[CLASS2_BEHAVIOR0_SPAWN_DELAYS:
                             CLASS2_BEHAVIOR0_SPAWN_DELAYS + 4]
    if spawn_offsets != [
            [-2, 20], [2, 18], [-4, 16], [4, 14], [-8, 12], [8, 10],
            [-10, 8], [10, 6], [-12, 4], [12, 2], [0, 0], [-10, -2],
            [10, -4], [-8, -6], [8, -8], [-5, -8]]:
        raise ValueError("Class-2 behavior-0 spawn offset table changed")
    if list(spawn_delay_bytes) != [9, 16, 7, 24]:
        raise ValueError("Class-2 behavior-0 spawn delay table changed")
    stage0_class12_linked = follow_stream(data, STAGE0_CLASS12_LINKED_STREAM)
    linked_frames = [record for record in stage0_class12_linked["records"]
                     if 0 < int(record["timer"]) <= 0x1000]
    if (len(linked_frames) != 3 or stage0_class12_linked["termination"] != "loop"
            or int(stage0_class12_linked["records"][-1]["words"][2], 16) & 0x8000 == 0):
        raise ValueError("Unexpected Stage 0 class-12 linked animation loop")
    for record in linked_frames:
        words = [int(word, 16) for word in record["words"]]
        descriptor = words[1]
        if (int(record["timer"]) != 10 or (words[2] & 0x7FFF) not in (0x001C, 0x001D)
                or words[3] != 0 or words[5] not in (0x0760, 0x0766, 0x076B)
                or ((descriptor >> 8) & 0x0F) + 1 not in (5, 6)
                or ((descriptor >> 12) & 0x0F) + 1 != 5):
            raise ValueError("Unexpected Stage 0 class-12 linked animation frame")
    # Queue-class-8 behavior 1 chooses the table at 0xefc0, then indexes its
    # facing pointer with object +15. These are the two streams reached by the
    # Stage 0 charged primary child (+32=1, +33=1).
    class8_child_sequences = [follow_stream(data, address)
                              for address in (0xF040, 0xF074)]
    class8_child_frames = []
    for sequence_index, sequence in enumerate(class8_child_sequences):
        if sequence["termination"] != "loop" or len(sequence["records"]) != 4:
            raise ValueError("Unexpected class-8 primary-child animation loop")
        frames = []
        for record in sequence["records"]:
            words = [int(word, 16) for word in record["words"]]
            descriptor = words[1]
            if (int(record["timer"]) != 3
                    or descriptor != (0x1108 | (0x0020 if sequence_index else 0))
                    or (words[2] & 0x7FFF) != 1 or words[3] != 0
                    or words[5] not in (0x0264, 0x0266, 0x0248, 0x024A)):
                raise ValueError("Unexpected class-8 primary-child animation frame")
            frames.append({
                "code": words[5], "hold": int(record["timer"]),
                "width": ((descriptor >> 8) & 0x0F) + 1,
                "height": ((descriptor >> 12) & 0x0F) + 1,
                "hflip": 1 if descriptor & 0x0020 else 0,
                "origin_x": words[2] & 0x7FFF, "origin_y": words[3],
                "source_color_group": descriptor & 0x1F,
            })
        if [frame["code"] for frame in frames] != [0x0264, 0x0266, 0x0248, 0x024A]:
            raise ValueError("Class-8 primary-child animation frame order changed")
        class8_child_frames.append(frames)
    class8_child_terminal_sequences = [follow_stream(data, address)
                                       for address in (0xF2EC, 0xF320)]
    class8_child_terminal_frames = []
    for sequence_index, sequence in enumerate(class8_child_terminal_sequences):
        if sequence["termination"] not in ("self reference", "loop") or len(sequence["records"]) != 4:
            raise ValueError("Unexpected class-8 impact animation loop")
        frames = []
        for record in sequence["records"]:
            words = [int(word, 16) for word in record["words"]]
            descriptor = words[1]
            if (int(record["timer"]) != 3
                    or descriptor != (0x1108 | (0x0020 if sequence_index else 0))
                    or (words[2] & 0x7FFF) != 1 or words[3] != 0
                    or words[5] not in (0x0268, 0x026A, 0x026C, 0x026E)):
                raise ValueError("Unexpected class-8 impact animation frame")
            frames.append({
                "code": words[5], "hold": int(record["timer"]),
                "width": ((descriptor >> 8) & 0x0F) + 1,
                "height": ((descriptor >> 12) & 0x0F) + 1,
                "hflip": 1 if descriptor & 0x0020 else 0,
                "origin_x": words[2] & 0x7FFF, "origin_y": words[3],
                "source_color_group": descriptor & 0x1F,
            })
        if [frame["code"] for frame in frames] != [0x0268, 0x026A, 0x026C, 0x026E]:
            raise ValueError("Class-8 impact animation frame order changed")
        class8_child_terminal_frames.append(frames)
    composite_resolver_offset = 0x03B99A
    composite_resolver_bytes = bytes.fromhex(
        "3a0402457f80ea4d267b500e0244007fd844d6f340004e75")
    if data[composite_resolver_offset:
            composite_resolver_offset + len(composite_resolver_bytes)] != composite_resolver_bytes:
        raise ValueError("CPS sprite coordinate resolver at 0x3b99a changed")
    composite_template_table = 0x03B9B2
    composite_template_pointers = (0x0003BB2A, 0x0003D48A, 0x0003E92A)
    if [int.from_bytes(data[composite_template_table + index * 4:
                            composite_template_table + index * 4 + 4], "big")
            for index in range(3)] != list(composite_template_pointers):
        raise ValueError("CPS sprite coordinate template table at 0x3b9b2 changed")
    composite_origin = 0x0169
    template_pointer_offset = (composite_origin & 0x7F80) >> 5
    template_index = composite_origin & 0x007F
    selected_template = int.from_bytes(
        data[composite_template_table + template_pointer_offset:
             composite_template_table + template_pointer_offset + 4], "big")
    template_displacement = int.from_bytes(
        data[selected_template + template_index * 2:
             selected_template + template_index * 2 + 2], "big", signed=True)
    composite_layout_offset = selected_template + template_displacement
    composite_layout_bytes = data[composite_layout_offset:composite_layout_offset + 52]
    composite_layout = [
        (int.from_bytes(composite_layout_bytes[index:index + 2], "big", signed=True),
         int.from_bytes(composite_layout_bytes[index + 2:index + 4], "big", signed=True))
        for index in range(0, 52, 4)
    ]
    expected_composite_layout = [
        (-24, -40), (-24, -24), (-16, -8), (-16, 8), (-8, -40),
        (-8, -24), (0, -8), (0, 8), (8, -24), (16, -8),
        (16, 8), (24, -40), (24, -24),
    ]
    if composite_layout != expected_composite_layout:
        raise ValueError("CPS sprite coordinate template at 0x3e9fe changed")
    state8_wall_origin = 0x00CD
    state8_template_pointer_offset = (state8_wall_origin & 0x7F80) >> 5
    state8_template_index = state8_wall_origin & 0x007F
    state8_template_pointer = int.from_bytes(
        data[composite_template_table + state8_template_pointer_offset:
             composite_template_table + state8_template_pointer_offset + 4],
        "big")
    state8_template_displacement = int.from_bytes(
        data[state8_template_pointer + state8_template_index * 2:
             state8_template_pointer + state8_template_index * 2 + 2],
        "big", signed=True)
    state8_wall_layout_offset = state8_template_pointer + state8_template_displacement
    state8_wall_coordinates = [
        (int.from_bytes(data[state8_wall_layout_offset + index * 4:
                             state8_wall_layout_offset + index * 4 + 2],
                          "big", signed=True),
         int.from_bytes(data[state8_wall_layout_offset + index * 4 + 2:
                             state8_wall_layout_offset + index * 4 + 4],
                          "big", signed=True))
        for index in range(11)
    ]
    state8_wall_frames: list[list[int]] = []
    state8_wall_flips: list[int] = []
    state8_wall_holds: list[int] = []
    for sequence_index, sequence in enumerate(
            tables["source_table_01315c"]["sequences"][:2]):
        records = sequence["records"]
        if len(records) != 3:
            raise ValueError("Unexpected state-8 wall animation record count")
        first_words = [int(word, 16) for word in records[0]["words"]]
        if (int(records[0]["length"]) != 12
                or int(records[0]["timer"]) != 20
                or first_words[1] != (0x3311 | (0x20 if sequence_index else 0))
                or first_words[5] != 0x0E00):
            raise ValueError("Unexpected state-8 wall entry frame")
        sequence_codes: list[int] = []
        sequence_holds: list[int] = []
        for record_index, expected_hold in ((1, 16), (2, 0xFFFF)):
            record = records[record_index]
            words = [int(word, 16) for word in record["words"]]
            raw = bytes.fromhex(record["raw"])
            expected_descriptor = 0x0011 | (0x20 if sequence_index else 0)
            if (int(record["length"]) != 32
                    or int(record["timer"]) != expected_hold
                    or words[0] != 0x000B
                    or words[1] != expected_descriptor
                    or (words[2] & 0x7FFF) != state8_wall_origin or words[3] != 0
                    or len(raw) != 32):
                raise ValueError("Unexpected state-8 composite wall frame")
            frame_codes = [int.from_bytes(raw[offset:offset + 2], "big")
                           for offset in range(10, 32, 2)]
            if len(frame_codes) != 11:
                raise ValueError("Unexpected state-8 composite wall tile count")
            sequence_codes.extend(frame_codes)
            sequence_holds.append(0 if expected_hold == 0xFFFF else expected_hold)
        state8_wall_frames.append(sequence_codes)
        state8_wall_flips.append(1 if first_words[1] & 0x20 else 0)
        state8_wall_holds = sequence_holds
    if state8_wall_frames[0] != state8_wall_frames[1]:
        raise ValueError("State-8 directional wall frames do not share tile codes")
    expected_state8_wall_coordinates = [
        (-24, -24), (-24, -8), (-24, 8), (-8, -40), (-8, -24),
        (-8, -8), (-8, 8), (8, -40), (8, -24), (8, -8), (8, 8),
    ]
    if state8_wall_coordinates != expected_state8_wall_coordinates:
        raise ValueError("State-8 wall coordinate template changed")
    setup_record = data[0x11482:0x11482 + 18]
    if setup_record != bytes.fromhex(
            "000400100010333000160000000401880010"):
        raise ValueError("Extended enemy setup frame at 0x11482 changed")
    setup_component_codes = [int.from_bytes(setup_record[offset:offset + 2], "big")
                             for offset in (10, 12, 14, 16)]
    setup_origin = 0x0010
    setup_template_pointer = int.from_bytes(
        data[composite_template_table:composite_template_table + 4], "big")
    setup_displacement = int.from_bytes(
        data[setup_template_pointer + setup_origin * 2:
             setup_template_pointer + setup_origin * 2 + 2], "big", signed=True)
    setup_layout_offset = setup_template_pointer + setup_displacement
    setup_coordinates = [
        (int.from_bytes(data[setup_layout_offset + index * 4:
                             setup_layout_offset + index * 4 + 2], "big", signed=True),
         int.from_bytes(data[setup_layout_offset + index * 4 + 2:
                             setup_layout_offset + index * 4 + 4], "big", signed=True))
        for index in range(4)
    ]
    expected_setup_coordinates = [(-64, -24), (-64, -8), (-64, 8), (-48, -24)]
    if (setup_component_codes != [0x0000, 0x0004, 0x0188, 0x0010]
            or setup_coordinates != expected_setup_coordinates):
        raise ValueError("Unexpected extended enemy setup sprite composition")
    setup_composition_frames = []
    for sequence in tables["source_table_011466"]["sequences"][:2]:
        frame_indices = [index for index, record in enumerate(sequence["records"])
                         if int(record["rom_offset"], 16) == 0x11482
                         and int(record["length"]) == 18]
        if len(frame_indices) != 1:
            raise ValueError("Expected one extended setup frame per direction")
        setup_composition_frames.append(frame_indices[0])
    if setup_composition_frames != [2, 1]:
        raise ValueError("Unexpected directional setup composition frame indices")
    special_record = data[0x11416:0x11416 + 18]
    if special_record != bytes.fromhex(
            "00040010000c321000160000000c0d40000c"):
        raise ValueError("Extended enemy special frame at 0x11416 changed")
    special_component_codes = [
        int.from_bytes(special_record[offset:offset + 2], "big")
        for offset in (10, 12, 14, 16)
    ]
    special_origin = 0x000C
    special_template_pointer = int.from_bytes(
        data[composite_template_table:composite_template_table + 4], "big")
    special_displacement = int.from_bytes(
        data[special_template_pointer + special_origin * 2:
             special_template_pointer + special_origin * 2 + 2],
        "big", signed=True)
    special_layout_offset = special_template_pointer + special_displacement
    special_coordinates = [
        (int.from_bytes(data[special_layout_offset + index * 4:
                             special_layout_offset + index * 4 + 2],
                         "big", signed=True),
         int.from_bytes(data[special_layout_offset + index * 4 + 2:
                             special_layout_offset + index * 4 + 4],
                         "big", signed=True))
        for index in range(4)
    ]
    expected_special_coordinates = [(-16, -8), (0, -8), (-32, -16), (-32, 0)]
    if (special_component_codes != [0x0000, 0x000C, 0x0D40, 0x000C]
            or special_record[6:8] != bytes.fromhex("3210")
            or special_coordinates != expected_special_coordinates):
        raise ValueError("Unexpected extended enemy special composition")
    special_composition_frames = []
    for sequence in tables["source_table_0113fa"]["sequences"][:2]:
        frame_indices = [index for index, record in enumerate(sequence["records"])
                         if int(record["rom_offset"], 16) == 0x11416
                         and int(record["length"]) == 18]
        if len(frame_indices) != 1:
            raise ValueError("Expected one extended special frame per direction")
        special_composition_frames.append(frame_indices[0])
    if special_composition_frames != [2, 1]:
        raise ValueError("Unexpected directional special composition frame indices")
    below_records = []
    for address, expected in ENEMY_FOLLOWUP_BELOW_RECORDS.items():
        raw = data[address:address + len(expected)]
        if raw != expected:
            raise ValueError(
                f"Enemy below-followup frame at {address:#x} changed")
        descriptor = be16(data, address + 2)
        below_records.append({
            "offset": f"0x{address:06x}",
            "code": be16(data, address + 10),
            "timer": be16(data, address + 8),
            "width": ((descriptor >> 8) & 0x0F) + 1,
            "height": ((descriptor >> 12) & 0x0F) + 1,
            "hflip": 1 if descriptor & 0x0020 else 0,
            "origin_x": be16(data, address + 4) & 0x7FFF,
            "origin_y": int.from_bytes(data[address + 6:address + 8],
                                        "big", signed=True),
        })
    catalog = {
        "source": "offline Willow 68000 region",
        "player_jump_acceleration": {
            "value": jump_accels[0],
            "field_offset": 52,
            "controller_handlers": ["0x00bf42", "0x00d4ba"],
            "initializer_sites": [f"0x{site:06x}" for site in jump_accel_sites],
            "shared_jump_handler": "0x00c820",
        },
        "selector_routine": "0x001f30",
        "advance_routine": "0x001f4c",
        "class8_primary_child_animation": {
            "selector_table": "0x00ecfe + 0x02c2 = 0x00efc0",
            "facing_streams": ["0x00f040", "0x00f074"],
            "frame_count": 4,
            "loop": True,
            "frames": class8_child_frames,
            "impact_streams": ["0x00f2ec", "0x00f320"],
            "impact_timer_ticks": 12,
            "impact_frames": class8_child_terminal_frames,
        },
        "range_composite_layout": {
            "source_origin": f"0x{composite_origin:04x}",
            "resolver_offset": f"0x{composite_resolver_offset:06x}",
            "resolver_bytes": composite_resolver_bytes.hex(),
            "coordinate_template_table": f"0x{composite_template_table:06x}",
            "template_displacement": f"0x{template_displacement & 0xffff:04x}",
            "coordinate_template": f"0x{composite_layout_offset:06x}",
            "coordinates": [list(pair) for pair in composite_layout],
        },
        "player_state8_wall_composition": {
            "source_origin": f"0x{state8_wall_origin:04x}",
            "template_pointer": f"0x{state8_template_pointer:06x}",
            "template_displacement": f"0x{state8_template_displacement & 0xffff:04x}",
            "template_offset": f"0x{state8_wall_layout_offset:06x}",
            "tile_count": 11,
            "direction_hflip": state8_wall_flips,
            "frame_holds": state8_wall_holds,
            "frame_codes": [state8_wall_frames[0][:11],
                            state8_wall_frames[0][11:]],
            "coordinates": [list(pair) for pair in state8_wall_coordinates],
        },
        "enemy_setup_composition": {
            "record_offset": "0x011482",
            "sprite_codes": [f"0x{code:04x}" for code in setup_component_codes],
            "descriptor": "0x3330",
            "width": 4,
            "height": 4,
            "source_origin": f"0x{setup_origin:04x}",
            "template_displacement": f"0x{setup_displacement & 0xffff:04x}",
            "template_offset": f"0x{setup_layout_offset:06x}",
            "coordinates": [list(pair) for pair in setup_coordinates],
            "frame_indices_by_sequence": setup_composition_frames,
            "used_by": "state-6 mode-16 contact/setup frame at 0x11482",
        },
        "enemy_special_composition": {
            "record_offset": "0x011416",
            "sprite_codes": [f"0x{code:04x}"
                             for code in special_component_codes],
            "descriptor": "0x3210",
            "width": 3,
            "height": 4,
            "source_origin": f"0x{special_origin:04x}",
            "template_displacement": f"0x{special_displacement & 0xffff:04x}",
            "template_offset": f"0x{special_layout_offset:06x}",
            "coordinates": [list(pair) for pair in special_coordinates],
            "frame_indices_by_sequence": special_composition_frames,
        },
        "enemy_followup_below": {
            "records": below_records,
            "used_by": "Stage 0 class-2 attack follow-up frames at 0x1144e and 0x1145a",
        },
        "state2_normal_delays": list(data[0x12F18:0x12F28]),
        "state2_normal_masks": [
            f"0x{be16(data, 0x12F20 + index * 2):04x}" for index in range(8)
        ],
        "state6_recovery_masks": [
            f"0x{be16(data, 0x12A96 + index * 2):04x}" for index in range(16)
        ],
        "state6_recovery_mask_lookup": {
            "offset": f"0x{STATE6_RECOVERY_MASK_LOOKUP:06x}",
            "instruction_bytes": STATE6_RECOVERY_MASK_LOOKUP_BYTES.hex(),
            "counter": "low byte of a5+0x0346",
            "source_index_steps": [
                "logical shift right by 3",
                "mask with 0x001e to form an even byte offset",
                "load a 16-bit word from the mask table",
            ],
            "equivalent_row": "(counter >> 4) & 15",
            "mask_table_offset": "0x0012a96",
        },
        "stage0_actor_random_delays": {
            "lookup_offset": f"0x{STAGE0_ACTOR_DELAY_LOOKUP:06x}",
            "lookup_instruction_bytes": STAGE0_ACTOR_DELAY_LOOKUP_BYTES.hex(),
            "reuse_call_offset": f"0x{STAGE0_ACTOR_DELAY_REUSE:06x}",
            "reuse_instruction_bytes": STAGE0_ACTOR_DELAY_REUSE_BYTES.hex(),
            "table_offset": f"0x{STAGE0_ACTOR_DELAY_TABLE:06x}",
            "table_bytes": STAGE0_ACTOR_DELAY_TABLE_BYTES.hex(),
            "values": list(STAGE0_ACTOR_DELAY_TABLE_BYTES),
            "index": "low nibble returned by shared LFSR 0x001d8e",
        },
        "stage0_actor_voice_timing": {
            "wait_setup_offset": f"0x{STAGE0_ACTOR_VOICE_WAIT:06x}",
            "wait_setup_bytes": STAGE0_ACTOR_VOICE_WAIT_BYTES.hex(),
            "wait_ticks": be16(data, STAGE0_ACTOR_VOICE_WAIT + 2),
            "followup_setup_offset": f"0x{STAGE0_ACTOR_VOICE_FOLLOWUP:06x}",
            "followup_setup_bytes": STAGE0_ACTOR_VOICE_FOLLOWUP_BYTES.hex(),
            "followup_ticks": be16(data, STAGE0_ACTOR_VOICE_FOLLOWUP + 2),
        },
        "stage0_actor_approach_vectors": {
            "table_offset": f"0x{STAGE0_ACTOR_APPROACH_VECTORS:06x}",
            "table_bytes": STAGE0_ACTOR_APPROACH_VECTOR_BYTES.hex(),
            "vectors_8_8": [list(vector) for vector in approach_vectors],
            "selector": "actor byte +15; each record stores signed X then signed Y",
        },
        "stage0_actor_patrol_vectors": {
            "table_offset": f"0x{STAGE0_ACTOR_PATROL_VECTORS:06x}",
            "table_bytes": STAGE0_ACTOR_PATROL_VECTOR_BYTES.hex(),
            "vectors_8_8": [list(vector) for vector in patrol_vectors],
            "selector": "actor byte +15; each record stores signed X then signed Y",
        },
        "stage0_post_phase_actor": {
            "delay_table_offset": f"0x{POST_PHASE_ACTOR_DELAY_TABLE:06x}",
            "delay_table_bytes": POST_PHASE_ACTOR_DELAY_BYTES.hex(),
            "delay_ticks": list(POST_PHASE_ACTOR_DELAY_BYTES),
            "vector_table_offset": f"0x{POST_PHASE_ACTOR_VECTORS:06x}",
            "vector_table_bytes": POST_PHASE_ACTOR_VECTOR_BYTES.hex(),
            "mode_zero_vector_8_8": [-6, -2],
            "transition_ticks": 128,
        },
        "state2_normal_mask_lookup": {
            "offset": f"0x{STATE2_NORMAL_MASK_LOOKUP:06x}",
            "instruction_bytes": STATE2_NORMAL_MASK_LOOKUP_BYTES.hex(),
            "counter": "low byte of a5+0x0346",
            "mask_row": "(counter >> 4) & 0x0e, used as an even byte offset into words",
            "delay_index": "counter >> 4",
            "random_bit": "low nibble returned by shared LFSR 0x001d8e",
            "mask_table_offset": "0x0012f20",
            "delay_table_offset": "0x0012f18",
        },
        "state2_vertical_band_selector": {
            "offset": f"0x{STATE2_VERTICAL_BAND_SELECTOR:06x}",
            "instruction_bytes": STATE2_VERTICAL_BAND_SELECTOR_BYTES.hex(),
            "compared_values": [
                "actor +10 world Y",
                "shared target Y at a5+0x1850",
            ],
            "neutral_band_inclusive": [-48, 48],
            "outside_band_substates": {
                "below_minus_48": 2,
                "above_plus_48": 4,
            },
            "method": (
                "16-bit SUB.W target Y, ADDI.W 48, unsigned compare with 96; "
                "the neutral interval includes both endpoints"
            ),
        },
        "player_heading_bases": {
            "offset": f"0x{PLAYER_HEADING_BASES:06x}",
            "bytes": PLAYER_HEADING_BASE_BYTES.hex(),
            "values": list(PLAYER_HEADING_BASE_BYTES),
            "used_by": "0x0020d2 after the 32-step minor/major division",
        },
        "class2_behavior0_streams": {
            "pointer_table": f"0x{CLASS2_BEHAVIOR0_STREAM_POINTER_TABLE:06x}",
            "selection": "random timer index from the second state-0 LFSR draw",
            "stream_offsets": [f"0x{value:04x}" for value in CLASS2_BEHAVIOR0_STREAM_OFFSETS],
            "streams": class2_streams,
            "spawn_setup": {
                "state0_handler": "0x00fc10",
                "state0_handler_bytes": CLASS2_BEHAVIOR0_STATE0_BYTES.hex(),
                "state2_handler": "0x00fc6c",
                "state2_handler_bytes": CLASS2_BEHAVIOR0_STATE2_BYTES.hex(),
                "state2_lifetime_and_animation_bytes":
                    CLASS2_BEHAVIOR0_STATE2_TAIL_BYTES.hex(),
                "visibility_filter": f"0x{CLASS2_BEHAVIOR0_VISIBILITY:06x}",
                "visibility_filter_bytes":
                    CLASS2_BEHAVIOR0_VISIBILITY_BYTES.hex(),
                "visibility_bounds_relative_to_camera": {
                    "x_min": -32,
                    "x_max": 416,
                    "y_min": 0,
                    "y_max": 272,
                },
                "verified_effects": [
                    "state 0 increments the lifecycle before either RNG draw",
                    "first LFSR draw selects the spawn offset; second selects both delay and animation stream",
                    "state 0 tail-calls the shared animation selector and returns",
                    "state 2 dispatches byte +33 to the four common visibility callbacks",
                    "state 2 visibility runs before lifetime decrement and frame advance",
                    "lifetime expiry enters state 4; cleanup runs on its next dispatch",
                ],
                "offset_table": f"0x{CLASS2_BEHAVIOR0_SPAWN_OFFSETS:06x}",
                "offset_table_bytes": spawn_offset_bytes.hex(),
                "offsets": spawn_offsets,
                "delay_table": f"0x{CLASS2_BEHAVIOR0_SPAWN_DELAYS:06x}",
                "delay_table_bytes": spawn_delay_bytes.hex(),
                "delays": list(spawn_delay_bytes),
                "lfsr_selection": "first draw & 15 selects a coordinate pair; second draw & 3 selects the lifetime",
            },
        },
        "stage0_class12_linked_actor": {
            "handler": f"0x{STAGE0_CLASS12_LINKED_HANDLER:06x}",
            "state_dispatch": {
                "offset": f"0x{STAGE0_CLASS12_LINKED_DISPATCH:06x}",
                "bytes": STAGE0_CLASS12_LINKED_DISPATCH_BYTES.hex(),
                "state0_target": f"0x{STAGE0_CLASS12_LINKED_SETUP:06x}",
                "state2_target": f"0x{STAGE0_CLASS12_LINKED_STATE2:06x}",
                "states4_and6_target": f"0x{STAGE0_CLASS12_LINKED_STATE6_CLEANUP:06x}",
            },
            "state0_setup": f"0x{STAGE0_CLASS12_LINKED_SETUP:06x}",
            "state0_setup_bytes": STAGE0_CLASS12_LINKED_STATE0_BYTES.hex(),
            "state2_update": {
                "offset": f"0x{STAGE0_CLASS12_LINKED_STATE2:06x}",
                "bytes": STAGE0_CLASS12_LINKED_STATE2_BYTES.hex(),
                "cadence_check": "(a5+0x19 + d7) & 15 == 0 before helper 0x21c6",
                "animation_advance": "calls 0x1f4c every update after the cadence check",
                "visibility": "jumps to 0x2244 after the animation advance",
            },
            "periodic_retirement_helper": {
                "offset": f"0x{STAGE0_CLASS12_LINKED_RETIREMENT_HELPER:06x}",
                "bytes": STAGE0_CLASS12_LINKED_RETIREMENT_HELPER_BYTES.hex(),
                "stage_counter": "word a5+0x0344 selects the case table at 0x21d4",
                "stage0_case": "compares actor X at +6 with a5+0x038c; signed-less sets actor state to 6",
                "native_threshold": "the native lower retirement bound starts at the Stage 0 world minimum and advances monotonically to (estimated source camera X - 768); the estimate follows Willow around source center 192 because the active writer for the source camera target is unresolved",
            },
                "stage0_camera_follow": {
                    "target_setup": "when a5+0x0348 is clear, 0x42602 loads the camera target from a5-0x8fb0/-0x8fb2; the normal Stage 0 initializer sets these fields to (192,640)",
                    "startup_camera_record": "stage setup 0x2f8a loads parameters (0x00c0,0x0048); the Stage 0 initializer 0xa0e0 calls 0x2f08, whose stage-counter-zero dispatch selects resolver 0x2fbc and camera record (192,72) with initial lower bound 0",
                    "normal_camera_target_x_operand_trace": {
                        "normal_initializer": "0x00a11c writes the constant 192 to a5-0x8fb0",
                        "alternate_initializer": "0x00a162 writes the constant 192 to a5-0x8fb0",
                        "camera_read": "0x04261a copies a5-0x8fb0 into camera object +106",
                        "other_direct_writes": [],
                        "interpretation": "the normal Stage 0 target X is fixed at 192 in the direct-operand scan; once a5+0x0348 is set, 0x42602 instead uses shared controller target a5+0x184c",
                        "validated_bytes": True,
                    },
                "target_field_accesses": {
                    "normal_stage0_initializer": "0x00a11c writes X=192,Y=640",
                    "alternate_stage_counter_initializer": "0x00a162 writes X=192,Y=4176",
                    "scripted_y_mutation": "0x00a2fe subtracts 2 from target Y while player world Y a5+0x040c is nonzero",
                    "scripted_y_state_path": "the ordinary Stage 0 path dispatches state 6 to 0x0a0d0; its normal initializer increments state to 8. State 8 at 0x0a234 runs 7 ticks then advances to state 10 with a 63-tick timer; state 10 at 0x0a25c advances to state 12. State 12 reaches 0x00a2f8 each update, which tests player world Y a5+0x040c, subtracts 2 from a5-0x8fb2 while nonzero, calls 0x9ef2, then transfers to 0x3b524",
                    "stage0_scripted_y_timing": "the normal camera-target Y decrement begins on the update after the 7-tick state-8 and 63-tick state-10 waits (70 ticks total)",
                    "direct_runtime_x_writer": "not identified in the direct A5-relative operand scan; the active camera frame path calls 0x42d66, 0x42924, 0x42d16, and 0x42b1c, whose validated Stage 0 branches do not write object +106; computed or aliased writes outside this path remain possible",
                    "bytes_validated": {
                        "normal_initializer": True,
                        "alternate_initializer": True,
                        "scripted_y_step": True,
                        "scripted_y_guard_and_continuation": True,
                        "manager_state_table": True,
                        "manager_state6": True,
                        "manager_state8": True,
                        "manager_state10": True,
                        "active_camera_helper_chain_scanned": True,
                    },
                },
                "update_path": f"0x{STAGE0_CAMERA_UPDATE_CALLER:06x} calls 0x{STAGE0_CAMERA_TRACKER:06x} each active camera update, then 0x428ae publishes object +100 to a5+0x038c",
                "tracker_center_x": 192,
                "tracker_rule": "the Stage 0 tracker compares target object +106 with camera position +6 around a 192-pixel offset, moves +6 toward target-minus-192, and clamps against camera bounds +100/+104",
                "trailing_bound_distance": 768,
                "bound_rule": "object +100 advances to max(previous lower bound, object +6 - 768), clamped at zero; the Stage 0 class-12 actor compares its X to the published bound every 16 global updates",
                "native_follow_link_status": "the active Stage 0 startup camera record is established; the runtime writer for the normal-path camera target X and its exact relation to Willow remain unresolved, so the native retirement estimate uses Willow world X",
                "bytes_validated": {
                    "target_setup": True,
                    "camera_update_call_path": True,
                    "stage_helper_dispatch": True,
                    "stage0_branch": True,
                    "y_helper": True,
                    "stage_transition_helper": True,
                    "bound_publisher": True,
                    "center_follow_and_bound_clamp": True,
                    "stage0_tracker_gate": True,
                    "stage0_tracker_lower_bound_clamp": True,
                    "trailing_bound_update": True,
                },
            },
            "state6_cleanup": {
                "offset": f"0x{STAGE0_CLASS12_LINKED_STATE6_CLEANUP:06x}",
                "bytes": STAGE0_CLASS12_LINKED_STATE6_CLEANUP_BYTES.hex(),
                "target": "0x002bb8",
                "effect": "clears actor fields and returns the record to its pool",
            },
            "animation_stream": f"0x{STAGE0_CLASS12_LINKED_STREAM:06x}",
            "sprite_color_group_semantics": "sprite descriptor low five bits select the CPS color group; renderer 0x3b8cc-0x3b8d4 replaces them with object +16 when object +18 is nonzero",
            "selector": 1,
            "frames": linked_frames,
            "loop": True,
            "frame_count": len(linked_frames),
        },
        "queue14_state4_transition": follow_stream(data, 0x2D2EC),
        "queue14_coordinate_layouts": {},
        "tables": tables,
    }

    for origin in (0, 1, 8, 12):
        pointer_offset = (origin & 0x7F80) >> 5
        template = int.from_bytes(
            data[composite_template_table + pointer_offset:
                 composite_template_table + pointer_offset + 4], "big")
        displacement = int.from_bytes(
            data[template + (origin & 0x7F) * 2:
                 template + (origin & 0x7F) * 2 + 2], "big", signed=True)
        layout_offset = template + displacement
        catalog["queue14_coordinate_layouts"][str(origin)] = [
            [int.from_bytes(data[layout_offset + index * 4:
                                 layout_offset + index * 4 + 2], "big", signed=True),
             int.from_bytes(data[layout_offset + index * 4 + 2:
                                 layout_offset + index * 4 + 4], "big", signed=True)]
            for index in range(3)
        ]
    if (catalog["queue14_coordinate_layouts"]["0"][0] != [-8, -8]
            or catalog["queue14_coordinate_layouts"]["12"][:2]
            != [[-16, -8], [0, -8]]
            or catalog["queue14_coordinate_layouts"]["8"][:3]
            != [[-24, -8], [-8, -8], [8, -8]]):
        raise ValueError("Queue-class-14 coordinate templates changed")
    return catalog


def build_walk_header(catalog: dict[str, object]) -> str:
    """Emit source-derived Willow walk and idle sequence metadata."""
    tables = catalog["tables"]
    main_sequences = tables["willow_main"]["sequences"]
    sequences = main_sequences[:2]
    frame_count = len(sequences[0]["records"])
    if frame_count != 6 or any(len(sequence["records"]) != frame_count for sequence in sequences):
        raise ValueError("Unexpected Willow walk animation frame count")
    if any(sequence["termination"] != "loop" for sequence in sequences):
        raise ValueError("Willow walk sequence no longer loops as expected")
    lines = [
        "/* Generated from Willow's offline animation tables; do not edit. */",
        f"#define WILLOW_PLAYER_JUMP_ACCELERATION 0x{int(catalog['player_jump_acceleration']['value']):04x}",
        "#define WILLOW_WALK_SEQUENCE_COUNT 2",
        f"#define WILLOW_WALK_FRAME_COUNT {frame_count}",
        "static const unsigned char willow_walk_hflip[WILLOW_WALK_SEQUENCE_COUNT] = {",
    ]
    for sequence in sequences:
        descriptors = [int(record["words"][1], 16) for record in sequence["records"]]
        if any((descriptor & 0x0020) != (descriptors[0] & 0x0020) for descriptor in descriptors):
            raise ValueError("Willow walk sequence changes horizontal flip between frames")
        lines.append("    " + str(1 if descriptors[0] & 0x0020 else 0) + ",")
    lines.extend([
        "};",
        "static const unsigned char willow_walk_width[WILLOW_WALK_SEQUENCE_COUNT] = {",
    ])
    for sequence in sequences:
        descriptors = [int(record["words"][1], 16) for record in sequence["records"]]
        widths = [((descriptor >> 8) & 0x0F) + 1 for descriptor in descriptors]
        if any(width != widths[0] for width in widths):
            raise ValueError("Willow walk sequence changes sprite block width")
        lines.append("    " + str(widths[0]) + ",")
    lines.extend([
        "};",
        "static const unsigned char willow_walk_height[WILLOW_WALK_SEQUENCE_COUNT] = {",
    ])
    for sequence in sequences:
        descriptors = [int(record["words"][1], 16) for record in sequence["records"]]
        heights = [((descriptor >> 12) & 0x0F) + 1 for descriptor in descriptors]
        if any(height != heights[0] for height in heights):
            raise ValueError("Willow walk sequence changes sprite block height")
        lines.append("    " + str(heights[0]) + ",")
    lines.extend([
        "};",
        "static const unsigned char willow_walk_holds[WILLOW_WALK_SEQUENCE_COUNT][WILLOW_WALK_FRAME_COUNT] = {",
    ])
    frame_values = []
    for sequence in sequences:
        records = sequence["records"]
        holds, codes = [], []
        for record in records:
            words = [int(word, 16) for word in record["words"]]
            if int(record["timer"]) != 6 or (words[1] & 0xFF00) != 0x2200:
                raise ValueError("Unexpected Willow walk animation record layout")
            holds.append(int(record["timer"]))
            codes.append(words[5])
        frame_values.append(codes)
        lines.append("    { " + ", ".join(str(value) for value in holds) + " },")
    lines.extend([
        "};",
        "static const unsigned char willow_walk_codes[WILLOW_WALK_SEQUENCE_COUNT][WILLOW_WALK_FRAME_COUNT] = {",
    ])
    for values in frame_values:
        lines.append("    { " + ", ".join(str(value) for value in values) + " },")
    lines.extend([
        "};",
        "#define WILLOW_PLAYER_ALT_WALK_SEQUENCE_COUNT 2",
        "#define WILLOW_PLAYER_ALT_WALK_FRAME_COUNT 6",
    ])
    alternate_sequences = tables["source_table_00e194"]["sequences"][:2]
    alternate_codes, alternate_holds = [], []
    alternate_flips, alternate_widths, alternate_heights = [], [], []
    alternate_origins_x, alternate_origins_y, alternate_colors = [], [], []
    for sequence in alternate_sequences:
        records = sequence["records"]
        if len(records) != 6 or sequence["termination"] != "loop":
            raise ValueError("Unexpected 0x00e194 alternate walk sequence")
        codes, holds, flips, widths, heights = [], [], set(), set(), set()
        origins_x, origins_y, colors = set(), set(), set()
        for index, record in enumerate(records):
            words = [int(word, 16) for word in record["words"]]
            descriptor = words[1]
            expected_descriptor = 0x3301 | (0x0020 if sequence["index"] else 0)
            if (int(record["timer"]) != 6
                    or descriptor != expected_descriptor
                    or words[2] != (0x8006 if index == 5 else 0x0006)
                    or words[3] != 9):
                raise ValueError("Unexpected 0x00e194 alternate walk record")
            codes.append(words[5])
            holds.append(int(record["timer"]))
            flips.add(1 if descriptor & 0x0020 else 0)
            widths.add(((descriptor >> 8) & 0x0F) + 1)
            heights.add(((descriptor >> 12) & 0x0F) + 1)
            origins_x.add(words[2] & 0x7FFF)
            origins_y.add(words[3] if words[3] < 0x8000 else words[3] - 0x10000)
            colors.add(descriptor & 0x001F)
        if any(len(values) != 1 for values in
               (flips, widths, heights, origins_x, origins_y, colors)):
            raise ValueError("Changing 0x00e194 alternate walk descriptor")
        alternate_codes.append(codes)
        alternate_holds.append(holds)
        alternate_flips.append(flips.pop())
        alternate_widths.append(widths.pop())
        alternate_heights.append(heights.pop())
        alternate_origins_x.append(origins_x.pop())
        alternate_origins_y.append(origins_y.pop())
        alternate_colors.append(colors.pop())
    if (alternate_codes != [[0x0400, 0x0404, 0x0440, 0x0408, 0x040C, 0x0444]] * 2
            or alternate_flips != [0, 1]
            or alternate_widths != [4, 4]
            or alternate_heights != [4, 4]
            or alternate_origins_x != [6, 6]
            or alternate_origins_y != [9, 9]
            or alternate_colors != [1, 1]):
        raise ValueError("Unexpected source 0x00e194 player alternate walk data")
    lines.extend([
        "static const unsigned short willow_player_alt_walk_codes[2][6] = {",
    ])
    for codes in alternate_codes:
        lines.append("    { " + ", ".join(f"0x{code:04x}" for code in codes) + " },")
    lines.extend([
        "};",
        "static const unsigned char willow_player_alt_walk_holds[2][6] = {",
    ])
    for holds in alternate_holds:
        lines.append("    { " + ", ".join(str(hold) for hold in holds) + " },")
    lines.extend([
        "};",
        "static const unsigned char willow_player_alt_walk_hflip[2] = { "
        + ", ".join(str(value) for value in alternate_flips) + " };",
        "static const unsigned char willow_player_alt_walk_width[2] = { "
        + ", ".join(str(value) for value in alternate_widths) + " };",
        "static const unsigned char willow_player_alt_walk_height[2] = { "
        + ", ".join(str(value) for value in alternate_heights) + " };",
        "static const signed char willow_player_alt_walk_origin_x[2] = { "
        + ", ".join(str(value) for value in alternate_origins_x) + " };",
        "static const signed char willow_player_alt_walk_origin_y[2] = { "
        + ", ".join(str(value) for value in alternate_origins_y) + " };",
        "static const unsigned char willow_player_alt_walk_color_group = 1;",
    ])
    lines.extend([
        "#define WILLOW_IDLE_SEQUENCE_COUNT 2",
        "static const unsigned char willow_idle_hflip[WILLOW_IDLE_SEQUENCE_COUNT] = {",
    ])
    idle_codes = []
    for sequence in main_sequences[2:4]:
        records = sequence["records"]
        if len(records) != 1 or int(records[0]["timer"]) != 0xFFFF:
            raise ValueError("Unexpected Willow idle animation record layout")
        words = [int(word, 16) for word in records[0]["words"]]
        if (words[1] & 0xFF00) != 0x2200:
            raise ValueError("Unexpected Willow idle descriptor")
        lines.append("    " + str(1 if words[1] & 0x0020 else 0) + ",")
        idle_codes.append(words[5])
    lines.extend([
        "};",
        "static const unsigned char willow_idle_width[WILLOW_IDLE_SEQUENCE_COUNT] = {",
    ])
    # `words[1]` is the CPS colour/size/flip descriptor. High ny/nx nibbles
    # encode the blocked sprite dimensions used by the native renderer.
    for sequence in main_sequences[2:4]:
        word = int(sequence["records"][0]["words"][1], 16)
        lines.append("    " + str(((word >> 8) & 0x0F) + 1) + ",")
    lines.extend([
        "};",
        "static const unsigned char willow_idle_height[WILLOW_IDLE_SEQUENCE_COUNT] = {",
    ])
    for sequence in main_sequences[2:4]:
        word = int(sequence["records"][0]["words"][1], 16)
        lines.append("    " + str(((word >> 12) & 0x0F) + 1) + ",")
    lines.extend([
        "};",
        "static const unsigned char willow_idle_codes[WILLOW_IDLE_SEQUENCE_COUNT] = { "
        + ", ".join(str(code) for code in idle_codes) + " };",
        "#define WILLOW_DOWN_SEQUENCE_COUNT 2",
        "static const unsigned char willow_down_hflip[WILLOW_DOWN_SEQUENCE_COUNT] = {",
    ])
    down_codes, down_widths, down_heights = [], [], []
    for sequence in tables["willow_alt"]["sequences"][2:4]:
        records = sequence["records"]
        if len(records) != 1 or int(records[0]["timer"]) != 0xFFFF:
            raise ValueError("Unexpected Willow down-state animation record")
        word = int(records[0]["words"][1], 16)
        if (word & 0xFF00) != 0x2200:
            raise ValueError("Unexpected Willow down-state descriptor")
        lines.append("    " + str(1 if word & 0x0020 else 0) + ",")
        down_codes.append(int(records[0]["words"][5], 16))
        down_widths.append(((word >> 8) & 0x0F) + 1)
        down_heights.append(((word >> 12) & 0x0F) + 1)
    lines.extend([
        "};",
        "static const unsigned char willow_down_width[WILLOW_DOWN_SEQUENCE_COUNT] = { "
        + ", ".join(str(value) for value in down_widths) + " };",
        "static const unsigned char willow_down_height[WILLOW_DOWN_SEQUENCE_COUNT] = { "
        + ", ".join(str(value) for value in down_heights) + " };",
        "static const unsigned char willow_down_codes[WILLOW_DOWN_SEQUENCE_COUNT] = { "
        + ", ".join(str(code) for code in down_codes) + " };",
        "#define WILLOW_PLAYER_WALL_STATE_COUNT 2",
        "static const unsigned short willow_player_wall_codes[2][2] = {",
    ])
    wall_tables = ("source_table_01315c", "source_table_013160")
    wall_frames = []
    for table_name in wall_tables:
        rows = []
        for sequence in tables[table_name]["sequences"][:2]:
            record = sequence["records"][0]
            words = [int(word, 16) for word in record["words"]]
            descriptor = words[1]
            if (int(record["timer"]) != (20 if table_name == wall_tables[0] else 8)
                    or (descriptor & 0xFF00) != 0x3300
                    or ((descriptor >> 8) & 0x0F) != 3
                    or ((descriptor >> 12) & 0x0F) != 3
                    or words[2] != 6 or words[3] != 0):
                raise ValueError(f"Unexpected {table_name} entry pose")
            rows.append((words[5], descriptor))
        wall_frames.append(rows)
        lines.append("    { " + ", ".join(f"0x{code:04x}" for code, _ in rows) + " },")
    lines.extend(["};", "static const unsigned char willow_player_wall_hflip[2][2] = {"])
    for rows in wall_frames:
        lines.append("    { " + ", ".join(str(1 if descriptor & 0x0020 else 0)
                                         for _, descriptor in rows) + " },")
    lines.extend([
        "};",
        "static const unsigned char willow_player_wall_width[2] = { 4, 4 };",
        "static const unsigned char willow_player_wall_height[2] = { 4, 4 };",
        "static const unsigned char willow_player_wall_x[2] = { 6, 6 };",
        "static const unsigned char willow_player_wall_y[2] = { 0, 0 };",
        "",
    ])
    state8_wall = catalog["player_state8_wall_composition"]
    state8_wall_codes = state8_wall["frame_codes"]
    state8_wall_coordinates = state8_wall["coordinates"]
    lines.extend([
        "#define WILLOW_PLAYER_STATE8_WALL_TILE_COUNT 11",
        "#define WILLOW_PLAYER_STATE8_WALL_COMPOSITE_FRAME_COUNT 2",
        "static const unsigned short willow_player_state8_wall_codes[2][11] = {",
    ])
    for codes in state8_wall_codes:
        lines.append("    { " + ", ".join(
            f"0x{int(code):04x}" for code in codes) + " },")
    lines.extend([
        "};",
        "static const short willow_player_state8_wall_x[11] = { "
        + ", ".join(str(pair[0]) for pair in state8_wall_coordinates) + " };",
        "static const short willow_player_state8_wall_y[11] = { "
        + ", ".join(str(pair[1]) for pair in state8_wall_coordinates) + " };",
        "static const unsigned char willow_player_state8_wall_hflip[2] = { "
        + ", ".join(str(value) for value in state8_wall["direction_hflip"])
        + " };",
        "static const unsigned char willow_player_state8_wall_composite_holds[2] = { "
        + ", ".join(str(value) for value in state8_wall["frame_holds"]) + " };",
        "",
    ])

    # State 6 uses 1f34 to restart this 2x2 transition frame after each
    # eight-tick heading step, then selects the indefinite facing pose.
    turn_sequences = tables["player_state6_turn_1316c"]["sequences"][:2]
    facing_sequences = tables["player_state6_facing_1318c"]["sequences"][:2]
    if len(turn_sequences) != 2 or len(facing_sequences) != 2:
        raise ValueError("Unexpected state 6 facing sequence count")
    for sequence in turn_sequences:
        records = sequence["records"]
        if len(records) != 2 or [int(record["timer"]) for record in records] != [8, 0xFFFF]:
            raise ValueError("Unexpected 0x1316c player turn sequence")
        for record in records:
            words = [int(word, 16) for word in record["words"]]
            descriptor = words[1]
            if ((descriptor & 0xFF00) != 0x3300
                    or ((descriptor >> 8) & 0x0F) != 3
                    or ((descriptor >> 12) & 0x0F) != 3
                    or words[3] != 0):
                raise ValueError("Unexpected 0x1316c player turn frame")
    if turn_sequences[0]["records"] != turn_sequences[1]["records"]:
        raise ValueError("Unexpected 0x1316c facing variants")
    turn_records = turn_sequences[0]["records"]
    turn_codes = [int(record["words"][5], 16) for record in turn_records]
    turn_origins = [int(record["words"][2], 16) & 0x7FFF for record in turn_records]
    turn_holds = [int(record["timer"]) for record in turn_records]
    turn_descriptors = [int(record["words"][1], 16) for record in turn_records]
    if turn_codes != [0x0E88, 0x0FC0] or turn_origins != [6, 6]:
        raise ValueError("Unexpected state 6 turn graphics or origins")
    if any(sequence["records"] != facing_sequences[0]["records"]
           for sequence in facing_sequences):
        raise ValueError("Unexpected 0x1318c facing variants")
    settled = facing_sequences[0]["records"]
    if (len(settled) != 1 or int(settled[0]["timer"]) != 0xFFFF
            or int(settled[0]["words"][1], 16) != 0x3311
            or int(settled[0]["words"][2], 16) != 0x8006
            or int(settled[0]["words"][5], 16) != 0x0FC0):
        raise ValueError("Unexpected 0x1318c settled pose")
    turn_width = ((turn_descriptors[0] >> 8) & 0x0F) + 1
    turn_height = ((turn_descriptors[0] >> 12) & 0x0F) + 1
    facing_code = int(settled[0]["words"][5], 16)
    facing_x = int(settled[0]["words"][2], 16) & 0x7FFF
    recovery_masks = [int(mask, 16) for mask in catalog["state6_recovery_masks"]]
    if len(recovery_masks) != 16 or recovery_masks[0] != 0x1111:
        raise ValueError("Unexpected Stage 0 state 6 recovery mask table")
    normal_delays = catalog["state2_normal_delays"]
    normal_masks = [int(mask, 16) for mask in catalog["state2_normal_masks"]]
    expected_normal_delays = [
        0x20, 0x18, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
        0x52, 0x25, 0x55, 0x55, 0x55, 0x55, 0xD5, 0x57,
    ]
    expected_normal_masks = [
        0x5225, 0x5555, 0x5555, 0xD557,
        0xD557, 0xDDED, 0xDDED, 0xFFFF,
    ]
    if (normal_delays != expected_normal_delays
            or normal_masks != expected_normal_masks):
        raise ValueError("Unexpected normal-state timer/mask tables")
    stage0_actor_delays = catalog["stage0_actor_random_delays"]["values"]
    stage0_actor_voice_timing = catalog["stage0_actor_voice_timing"]
    stage0_approach_vectors = catalog["stage0_actor_approach_vectors"]["vectors_8_8"]
    stage0_patrol_vectors = catalog["stage0_actor_patrol_vectors"]["vectors_8_8"]
    player_heading_bases = catalog["player_heading_bases"]["values"]
    expected_stage0_actor_delays = [
        120, 120, 180, 240, 180, 120, 240, 180,
        240, 180, 180, 240, 180, 240, 120, 180,
    ]
    if stage0_actor_delays != expected_stage0_actor_delays:
        raise ValueError("Unexpected Stage 0 actor random delay table")
    if (stage0_actor_voice_timing["wait_ticks"] != 60
            or stage0_actor_voice_timing["followup_ticks"] != 58):
        raise ValueError("Unexpected Stage 0 actor voice timing")
    if stage0_patrol_vectors != [[384, 0], [-384, 0]]:
        raise ValueError("Unexpected Stage 0 actor patrol vectors")
    if player_heading_bases != [0x00, 0x81, 0x01, 0x80,
                                0x41, 0x40, 0xC0, 0xC1]:
        raise ValueError("Unexpected player state-6 heading base table")
    lines.extend([
        "#define WILLOW_PLAYER_STATE6_TURN_FRAME_COUNT 2",
        "static const unsigned short willow_player_state6_turn_codes[2] = { "
        + ", ".join(f"0x{code:04x}" for code in turn_codes) + " };",
        "static const unsigned short willow_player_state6_turn_holds[2] = { "
        + ", ".join(str(hold) for hold in turn_holds) + " };",
        f"static const unsigned char willow_player_state6_turn_width = {turn_width};",
        f"static const unsigned char willow_player_state6_turn_height = {turn_height};",
        "static const unsigned char willow_player_state6_turn_x[2] = { "
        + ", ".join(str(origin) for origin in turn_origins) + " };",
        f"static const unsigned short willow_player_state6_facing_code = 0x{facing_code:04x};",
        f"static const unsigned char willow_player_state6_facing_x = {facing_x};",
        "static const unsigned short willow_player_state6_recovery_masks[16] = {",
        "    " + ", ".join(f"0x{mask:04x}" for mask in recovery_masks),
        "};",
        "static const unsigned char willow_player_state2_normal_delays[16] = { ",
        "    " + ", ".join(str(value) for value in normal_delays) + " };",
        "static const unsigned short willow_player_state2_normal_masks[8] = {",
        "    " + ", ".join(f"0x{mask:04x}" for mask in normal_masks),
        "};",
        "static const unsigned char willow_stage0_actor_random_delays[16] = {",
        "    " + ", ".join(str(value) for value in stage0_actor_delays),
        "};",
        f"static const unsigned char willow_stage0_actor_voice_wait_ticks = {stage0_actor_voice_timing['wait_ticks']};",
        f"static const unsigned char willow_stage0_actor_voice_followup_ticks = {stage0_actor_voice_timing['followup_ticks']};",
        "static const unsigned char willow_player_heading_bases[8] = {",
        "    " + ", ".join(f"0x{value:02x}" for value in player_heading_bases),
        "};",
        "static const short willow_stage0_actor_approach_x[2] = { "
        + ", ".join(str(vector[0]) for vector in stage0_approach_vectors) + " };",
        "static const short willow_stage0_actor_approach_y[2] = { "
        + ", ".join(str(vector[1]) for vector in stage0_approach_vectors) + " };",
        "static const short willow_stage0_actor_patrol_x[2] = { "
        + ", ".join(str(vector[0]) for vector in stage0_patrol_vectors) + " };",
        "",
    ])

    hit_sequences = tables["source_table_013164"]["sequences"][:2]
    hit_codes, hit_flips = [], []
    hit_holds, hit_x = [], []
    for sequence in hit_sequences:
        records = sequence["records"]
        if len(records) != 6:
            raise ValueError("Unexpected 0x13164 player recovery frame count")
        codes, holds, offsets, flips = [], [], [], set()
        for index, record in enumerate(records):
            words = [int(word, 16) for word in record["words"]]
            descriptor, timer = words[1], int(record["timer"])
            if ((descriptor & 0xFF00) != 0x2300
                    or ((descriptor >> 8) & 0x0F) != 3
                    or ((descriptor >> 12) & 0x0F) != 2
                    or timer != (8 if index < 5 else 0xFFFF)
                    or words[3] != 0):
                raise ValueError("Unexpected 0x13164 player recovery frame")
            codes.append(words[5])
            holds.append(timer)
            offsets.append(words[2] & 0x7FFF)
            flips.add(1 if descriptor & 0x0020 else 0)
        if len(flips) != 1:
            raise ValueError("Mixed facing flags in a 0x13164 player sequence")
        if offsets != [7, 7, 31, 31, 31, 31]:
            raise ValueError("Unexpected 0x13164 player animation origins")
        hit_codes.append(codes)
        hit_holds.append(holds)
        hit_x = offsets
        hit_flips.append(flips.pop())
    lines.extend([
        "#define WILLOW_PLAYER_HIT_FRAME_COUNT 6",
        "static const unsigned short willow_player_hit_codes[2][6] = {",
    ])
    for row in hit_codes:
        lines.append("    { " + ", ".join(f"0x{code:04x}" for code in row) + " },")
    lines.extend(["};", "static const unsigned short willow_player_hit_holds[6] = { "
                  + ", ".join(str(value) for value in hit_holds[0]) + " };"])
    lines.append("static const unsigned char willow_player_hit_hflip[2] = { "
                  + ", ".join(str(value) for value in hit_flips) + " };")
    lines.append("static const unsigned char willow_player_hit_x[6] = { "
                  + ", ".join(str(value) for value in hit_x) + " };")
    lines.extend([
        "static const unsigned char willow_player_hit_width = 4;",
        "static const unsigned char willow_player_hit_height = 3;",
        "",
    ])

    state4_sequences = tables["source_table_019dfe"]["sequences"][:2]
    state4_codes, state4_holds = [], []
    state4_widths, state4_heights, state4_origins_x = [], [], []
    state4_flips = []
    for sequence in state4_sequences:
        if len(sequence["records"]) != 4:
            raise ValueError("Unexpected 0x19dfe state-4 animation length")
        codes, holds, widths, heights, origins = [], [], [], [], []
        flips = set()
        for index, record in enumerate(sequence["records"]):
            words = [int(word, 16) for word in record["words"]]
            descriptor, timer = words[1], int(record["timer"])
            width = ((descriptor >> 8) & 0x0F) + 1
            height = ((descriptor >> 12) & 0x0F) + 1
            if (words[3] != 0 or words[4] != timer
                    or words[5] >= 0x8000
                    or timer != (2 if index < 2 else 16 if index == 2
                                 else 0xFFFF)
                    or (descriptor & 0x001F) != 0x11
                    or (width, height) != ((4, 4) if index == 0
                        else (4, 3) if index == 1 else (4, 2))):
                raise ValueError("Unexpected 0x19dfe state-4 frame")
            codes.append(words[5])
            holds.append(timer)
            widths.append(width)
            heights.append(height)
            origins.append(words[2] & 0x7FFF)
            flips.add(1 if descriptor & 0x0020 else 0)
        if (len(flips) != 1 or origins != [6, 7, 18, 18]
                or len(set(widths)) != 1 or len(set(heights)) != 3):
            raise ValueError("Unexpected 0x19dfe state-4 sequence layout")
        state4_codes.append(codes)
        state4_holds.append(holds)
        state4_widths.append(widths)
        state4_heights.append(heights)
        state4_origins_x.append(origins)
        state4_flips.append(flips.pop())

    state4_hold_sequences = tables["source_table_019e02"]["sequences"][:2]
    state4_hold_codes, state4_hold_widths = [], []
    state4_hold_heights, state4_hold_origins, state4_hold_flips = [], [], []
    for sequence in state4_hold_sequences:
        if len(sequence["records"]) != 1:
            raise ValueError("Unexpected 0x19e02 state-4 hold animation")
        words = [int(word, 16) for word in sequence["records"][0]["words"]]
        descriptor = words[1]
        if (int(sequence["records"][0]["timer"]) != 0xFFFF
                or words[5] != 0x0F40 or words[3] != 0
                or ((descriptor >> 8) & 0x0F) != 3
                or ((descriptor >> 12) & 0x0F) != 0
                or (descriptor & 0x001F) != 0x11):
            raise ValueError("Unexpected 0x19e02 state-4 hold frame")
        state4_hold_codes.append(words[5])
        state4_hold_widths.append(((descriptor >> 8) & 0x0F) + 1)
        state4_hold_heights.append(((descriptor >> 12) & 0x0F) + 1)
        state4_hold_origins.append(words[2] & 0x7FFF)
        state4_hold_flips.append(1 if descriptor & 0x0020 else 0)

    lines.extend([
        "#define WILLOW_PLAYER_STATE4_FRAME_COUNT 4",
        "static const unsigned short willow_player_state4_codes[2][4] = {",
    ])
    for row in state4_codes:
        lines.append("    { " + ", ".join(
            f"0x{code:04x}" for code in row) + " },")
    lines.extend(["};", "static const unsigned short willow_player_state4_holds[2][4] = {"])
    for row in state4_holds:
        lines.append("    { " + ", ".join(str(value) for value in row) + " },")
    lines.extend(["};", "static const unsigned char willow_player_state4_widths[2][4] = {"])
    for row in state4_widths:
        lines.append("    { " + ", ".join(str(value) for value in row) + " },")
    lines.extend(["};", "static const unsigned char willow_player_state4_heights[2][4] = {"])
    for row in state4_heights:
        lines.append("    { " + ", ".join(str(value) for value in row) + " },")
    lines.extend(["};", "static const unsigned char willow_player_state4_origin_x[2][4] = {"])
    for row in state4_origins_x:
        lines.append("    { " + ", ".join(str(value) for value in row) + " },")
    lines.extend(["};", "static const unsigned char willow_player_state4_hflip[2] = { "
                  + ", ".join(str(value) for value in state4_flips) + " };"])
    lines.append("static const unsigned short willow_player_state4_hold_codes[2] = { "
                  + ", ".join(f"0x{value:04x}" for value in state4_hold_codes) + " };")
    lines.append("static const unsigned char willow_player_state4_hold_widths[2] = { "
                  + ", ".join(str(value) for value in state4_hold_widths) + " };")
    lines.append("static const unsigned char willow_player_state4_hold_heights[2] = { "
                  + ", ".join(str(value) for value in state4_hold_heights) + " };")
    lines.append("static const unsigned char willow_player_state4_hold_origin_x[2] = { "
                  + ", ".join(str(value) for value in state4_hold_origins) + " };")
    lines.append("static const unsigned char willow_player_state4_hold_hflip[2] = { "
                  + ", ".join(str(value) for value in state4_hold_flips) + " };")
    lines.append("")

    # The enemy state handler at 0x1010e selects this pair of directional
    # animation loops. Export only the validated looping frame records.
    enemy_sequences = tables["source_table_01116e"]["sequences"][:2]
    enemy_codes, enemy_holds, enemy_flips = [], [], []
    enemy_origins_x, enemy_origins_y = [], []
    for sequence in enemy_sequences:
        if sequence["termination"] != "loop" or len(sequence["records"]) != 6:
            raise ValueError("Unexpected 0x1116e enemy animation sequence")
        codes, holds = [], []
        flips = set()
        origins_x, origins_y = set(), set()
        for record in sequence["records"]:
            words = [int(word, 16) for word in record["words"]]
            descriptor = words[1]
            if ((descriptor & 0xFF00) not in (0x3300, 0x3310)
                    or ((descriptor >> 8) & 0x0F) != 3
                    or ((descriptor >> 12) & 0x0F) != 3
                    or int(record["timer"]) != 6):
                raise ValueError("Unexpected 0x1116e enemy descriptor or timing")
            codes.append(words[5])
            holds.append(int(record["timer"]))
            flips.add(1 if descriptor & 0x0020 else 0)
            origins_x.add(words[2] & 0x7FFF)
            origins_y.add(words[3] - 0x10000 if words[3] & 0x8000 else words[3])
        if len(flips) != 1:
            raise ValueError("Mixed facing flags in a 0x1116e enemy loop")
        if len(origins_x) != 1 or len(origins_y) != 1:
            raise ValueError("Changing source origin in a 0x1116e enemy loop")
        enemy_codes.append(codes)
        enemy_holds.append(holds)
        enemy_flips.append(flips.pop())
        enemy_origins_x.append(origins_x.pop())
        enemy_origins_y.append(origins_y.pop())
    if enemy_origins_x != [22, 22] or enemy_origins_y != [0, 0]:
        raise ValueError("Unexpected 0x1116e enemy animation origin")
    lines.extend([
        "#define WILLOW_ENEMY_SEQUENCE_COUNT 2",
        "#define WILLOW_ENEMY_FRAME_COUNT 6",
        "static const unsigned char willow_enemy_hflip[2] = { "
        + ", ".join(str(value) for value in enemy_flips) + " };",
        "static const unsigned char willow_enemy_width[2] = { 4, 4 };",
        "static const unsigned char willow_enemy_height[2] = { 4, 4 };",
        "static const unsigned char willow_enemy_origin_x[2] = { "
        + ", ".join(str(value) for value in enemy_origins_x) + " };",
        "static const signed char willow_enemy_origin_y[2] = { "
        + ", ".join(str(value) for value in enemy_origins_y) + " };",
        "static const unsigned char willow_enemy_holds[2][6] = {",
    ])
    for holds in enemy_holds:
        lines.append("    { " + ", ".join(str(value) for value in holds) + " },")
    lines.extend(["};", "static const unsigned short willow_enemy_codes[2][6] = {"])
    for codes in enemy_codes:
        lines.append("    { " + ", ".join(f"0x{code:04x}" for code in codes) + " },")
    lines.extend(["};", ""])

    # The state handler at 0x101da changes to this timed attack sequence after
    # its movement probe reports contact. Preserve its indefinite terminal pose.
    attack_sequences = tables["enemy_state_11226"]["sequences"][:2]
    attack_codes, attack_holds, attack_widths, attack_heights = [], [], [], []
    attack_flips, attack_origins_x, attack_origins_y = [], [], []
    for sequence in attack_sequences:
        records = sequence["records"]
        if len(records) != 10 or int(records[-1]["timer"]) != 0xFFFF:
            raise ValueError("Unexpected 0x11226 enemy attack sequence")
        codes, holds, widths, heights, flips = [], [], [], [], set()
        origins_x, origins_y = set(), set()
        for record in records:
            words = [int(word, 16) for word in record["words"]]
            descriptor, timer = words[1], int(record["timer"])
            width = ((descriptor >> 8) & 0x0F) + 1
            height = ((descriptor >> 12) & 0x0F) + 1
            if ((descriptor & 0xFF00) not in (0x3300, 0x3200)
                    or width not in (3, 4) or height != 4
                    or (timer not in (8, 12, 0xFFFF))):
                raise ValueError("Unexpected 0x11226 enemy frame descriptor")
            codes.append(words[5])
            holds.append(timer)
            widths.append(width)
            heights.append(height)
            flips.add(1 if descriptor & 0x0020 else 0)
            origins_x.add(words[2] & 0x7FFF)
            origins_y.add(words[3] - 0x10000 if words[3] & 0x8000 else words[3])
        if len(flips) != 1:
            raise ValueError("Mixed facing flags in a 0x11226 enemy loop")
        if len(origins_x) != 1 or len(origins_y) != 1:
            raise ValueError("Changing source origin in a 0x11226 enemy sequence")
        attack_codes.append(codes)
        attack_holds.append(holds)
        attack_widths.append(widths)
        attack_heights.append(heights)
        attack_flips.append(flips.pop())
        attack_origins_x.append(origins_x.pop())
        attack_origins_y.append(origins_y.pop())
    if attack_origins_x != [22, 22] or attack_origins_y != [0, 0]:
        raise ValueError("Unexpected 0x11226 enemy animation origin")
    lines.extend([
        "#define WILLOW_ENEMY_ATTACK_FRAME_COUNT 10",
        "static const unsigned char willow_enemy_attack_hflip[2] = { "
        + ", ".join(str(value) for value in attack_flips) + " };",
        "static const unsigned char willow_enemy_attack_origin_x[2] = { "
        + ", ".join(str(value) for value in attack_origins_x) + " };",
        "static const signed char willow_enemy_attack_origin_y[2] = { "
        + ", ".join(str(value) for value in attack_origins_y) + " };",
        "static const unsigned short willow_enemy_attack_holds[2][10] = {",
    ])
    for holds in attack_holds:
        lines.append("    { " + ", ".join(str(value) for value in holds) + " },")
    lines.extend(["};", "static const unsigned char willow_enemy_attack_width[2][10] = {"])
    for widths in attack_widths:
        lines.append("    { " + ", ".join(str(value) for value in widths) + " },")
    lines.extend(["};", "static const unsigned char willow_enemy_attack_height[2][10] = {"])
    for heights in attack_heights:
        lines.append("    { " + ", ".join(str(value) for value in heights) + " },")
    lines.extend(["};", "static const unsigned short willow_enemy_attack_codes[2][10] = {"])
    for codes in attack_codes:
        lines.append("    { " + ", ".join(f"0x{code:04x}" for code in codes) + " },")
    lines.extend(["};", ""])

    return_sequences = tables["enemy_return_1136e"]["sequences"]
    return_codes, return_holds, return_flips = [], [], []
    return_origins_x, return_origins_y = [], []
    for sequence in return_sequences:
        records = sequence["records"]
        if (len(records) != 2
                or [int(record["timer"]) for record in records] != [3, 0xFFFF]):
            raise ValueError("Unexpected 0x1136e enemy return sequence")
        codes, flips = [], set()
        origins_x, origins_y = set(), set()
        for record in records:
            words = [int(word, 16) for word in record["words"]]
            descriptor = words[1]
            if ((descriptor & 0xFF00) != 0x3300
                    or ((descriptor >> 8) & 0x0F) != 3
                    or ((descriptor >> 12) & 0x0F) != 3):
                raise ValueError("Unexpected 0x1136e enemy return descriptor")
            codes.append(words[5])
            flips.add(1 if descriptor & 0x0020 else 0)
            origins_x.add(words[2] & 0x7FFF)
            origins_y.add(words[3] - 0x10000 if words[3] & 0x8000 else words[3])
        if len(flips) != 1:
            raise ValueError("Mixed facing flags in a 0x1136e return sequence")
        if len(origins_x) != 1 or len(origins_y) != 1:
            raise ValueError("Changing source origin in a 0x1136e return sequence")
        return_codes.append(codes)
        return_holds.append([3, 0xFFFF])
        return_flips.append(flips.pop())
        return_origins_x.append(origins_x.pop())
        return_origins_y.append(origins_y.pop())
    if return_origins_x != [22, 22] or return_origins_y != [0, 0]:
        raise ValueError("Unexpected 0x1136e enemy return origin")
    lines.extend([
        "#define WILLOW_ENEMY_RETURN_FRAME_COUNT 2",
        "static const unsigned char willow_enemy_return_hflip[2] = { "
        + ", ".join(str(value) for value in return_flips) + " };",
        "static const unsigned char willow_enemy_return_origin_x[2] = { "
        + ", ".join(str(value) for value in return_origins_x) + " };",
        "static const signed char willow_enemy_return_origin_y[2] = { "
        + ", ".join(str(value) for value in return_origins_y) + " };",
        "static const unsigned short willow_enemy_return_holds[2][2] = {",
    ])
    for holds in return_holds:
        lines.append("    { " + ", ".join(str(value) for value in holds) + " },")
    lines.extend(["};", "static const unsigned short willow_enemy_return_codes[2][2] = {"])
    for codes in return_codes:
        lines.append("    { " + ", ".join(f"0x{code:04x}" for code in codes) + " },")
    lines.extend(["};", ""])

    # State 18 selects 0x1155e when its side-contact bit is clear. The
    # sequence is directional and has an indefinitely held third frame.
    state18_sequences = tables["enemy_state18_1155e"]["sequences"]
    if len(state18_sequences) != 2:
        raise ValueError("Unexpected 0x1155e selector count")
    state18_codes, state18_holds, state18_widths = [], [], []
    state18_heights, state18_flips, state18_origins_x = [], [], []
    state18_origins_y = []
    for sequence in state18_sequences:
        records = sequence["records"]
        if len(records) != 3 or [int(record["timer"]) for record in records] != [8, 8, 0xFFFF]:
            raise ValueError("Unexpected 0x1155e sequence timing")
        codes, holds, widths, heights, flips = [], [], [], [], []
        origins_x, origins_y = [], []
        for record in records:
            words = [int(word, 16) for word in record["words"]]
            descriptor = words[1]
            if ((descriptor & 0xFF00) not in (0x2300, 0x3200)
                    or words[5] not in (0x0CC4, 0x0CC7, 0x0C9B)):
                raise ValueError("Unexpected 0x1155e frame descriptor")
            codes.append(words[5])
            holds.append(int(record["timer"]))
            widths.append(((descriptor >> 8) & 0x0F) + 1)
            heights.append(((descriptor >> 12) & 0x0F) + 1)
            flips.append(1 if descriptor & 0x0020 else 0)
            origins_x.append(words[2] & 0x7FFF)
            origins_y.append(words[3] - 0x10000 if words[3] & 0x8000 else words[3])
        state18_codes.append(codes)
        state18_holds.append(holds)
        state18_widths.append(widths)
        state18_heights.append(heights)
        state18_flips.append(flips)
        state18_origins_x.append(origins_x)
        state18_origins_y.append(origins_y)
    if (state18_codes != [[0x0CC4, 0x0CC7, 0x0C9B]] * 2
            or state18_flips != [[1, 1, 1], [0, 0, 0]]
            or state18_origins_x != [[24, 22, 7], [24, 22, 7]]
            or state18_origins_y != [[2, 0, 5], [2, 0, 5]]
            or state18_widths != [[3, 3, 4]] * 2
            or state18_heights != [[4, 4, 3]] * 2):
        raise ValueError("Unexpected 0x1155e directional frame data")
    lines.extend([
        "#define WILLOW_ENEMY_STATE18_FRAME_COUNT 3",
        "static const unsigned char willow_enemy_state18_hflip[2][3] = {",
    ])
    for values in state18_flips:
        lines.append("    { " + ", ".join(str(v) for v in values) + " },")
    lines.extend(["};", "static const unsigned char willow_enemy_state18_origin_x[2][3] = {"])
    for values in state18_origins_x:
        lines.append("    { " + ", ".join(str(v) for v in values) + " },")
    lines.extend(["};", "static const signed char willow_enemy_state18_origin_y[2][3] = {"])
    for values in state18_origins_y:
        lines.append("    { " + ", ".join(str(v) for v in values) + " },")
    lines.extend(["};", "static const unsigned short willow_enemy_state18_holds[2][3] = {"])
    for values in state18_holds:
        lines.append("    { " + ", ".join(str(v) for v in values) + " },")
    for name, values in (("width", state18_widths), ("height", state18_heights)):
        lines.extend(["};", f"static const unsigned char willow_enemy_state18_{name}[2][3] = {{"])
        for row in values:
            lines.append("    { " + ", ".join(str(v) for v in row) + " },")
    lines.extend(["};", "static const unsigned short willow_enemy_state18_codes[2][3] = {"])
    for values in state18_codes:
        lines.append("    { " + ", ".join(f"0x{v:04x}" for v in values) + " },")
    lines.extend(["};", ""])

    # Stage 0's initializer spawns a mode-0 actor that uses 0x11fcc while it
    # moves, then switches to the held terminal pose in 0x11fd4. Export the
    # record fields used by the native renderer rather than duplicating its
    # sprite descriptors and five-tick timing in C.
    stage0_move = tables["source_table_011fcc"]["sequences"][:2]
    if len(stage0_move) != 2 or any(
        sequence["termination"] != "loop" or len(sequence["records"]) != 6
        for sequence in stage0_move
    ):
        raise ValueError("Unexpected Stage 0 mode-0 movement animation")
    stage0_codes, stage0_holds, stage0_widths = [], [], []
    stage0_heights, stage0_flips = [], []
    for sequence in stage0_move:
        codes, holds, widths, heights, flips = [], [], [], [], []
        for record in sequence["records"]:
            words = [int(word, 16) for word in record["words"]]
            descriptor = words[1]
            if (int(record["timer"]) != 5
                    or (descriptor & 0xFF00) != 0x1300
                    or ((descriptor >> 8) & 0x0F) != 3
                    or ((descriptor >> 12) & 0x0F) != 1):
                raise ValueError("Unexpected Stage 0 mode-0 frame descriptor")
            codes.append(words[5])
            holds.append(int(record["timer"]))
            widths.append(((descriptor >> 8) & 0x0F) + 1)
            heights.append(((descriptor >> 12) & 0x0F) + 1)
            flips.append(1 if descriptor & 0x0020 else 0)
        if len(set(flips)) != 1:
            raise ValueError("Stage 0 mode-0 facing changes within a sequence")
        stage0_codes.append(codes)
        stage0_holds.append(holds)
        stage0_widths.append(widths)
        stage0_heights.append(heights)
        stage0_flips.append(flips[0])
    # The object +15 selector is a four-way index at the mode-0 entry point.
    # Selectors 2 and 3 are source-backed slow movement records (18-tick
    # holds), with a long terminal hold on their fourth frame. They do not
    # share the six-frame 5-tick layout used by selectors 0 and 1.
    stage0_linked = tables["source_table_011fcc"]["sequences"]
    if len(stage0_linked) != 4:
        raise ValueError("Unexpected Stage 0 linked selector count")
    linked_codes, linked_holds, linked_widths = [], [], []
    linked_heights, linked_flips, linked_counts = [], [], []
    for selector, sequence in enumerate(stage0_linked):
        records = sequence["records"]
        expected_count = 6 if selector < 2 else 4
        expected_termination = "loop" if selector < 2 else "long-delay/control record"
        if len(records) != expected_count or sequence["termination"] != expected_termination:
            raise ValueError(f"Unexpected Stage 0 linked selector {selector} records")
        codes, holds, widths, heights, flips = [], [], [], [], []
        for record in records:
            words = [int(word, 16) for word in record["words"]]
            descriptor = words[1]
            timer = int(record["timer"])
            if selector < 2:
                if (timer != 5 or (descriptor & 0xFF00) != 0x1300
                        or words[5] not in (0x0800, 0x0804, 0x0808,
                                            0x080c, 0x0820, 0x0824)):
                    raise ValueError("Unexpected Stage 0 normal movement frame")
            elif ((timer not in (18, 0x8012))
                    or words[5] != 0x0011
                    or (descriptor & 0xFF00) not in (0x2100, 0x0700, 0x1300)):
                raise ValueError(f"Unexpected Stage 0 slow selector {selector} frame")
            codes.append(words[5])
            holds.append(timer)
            widths.append(((descriptor >> 8) & 0x0F) + 1)
            heights.append(((descriptor >> 12) & 0x0F) + 1)
            flips.append(1 if descriptor & 0x0020 else 0)
        if selector >= 2 and holds != [18, 18, 18, 0x8012]:
            raise ValueError(f"Unexpected Stage 0 slow selector {selector} timing")
        linked_counts.append(expected_count)
        linked_codes.append(codes + [codes[-1]] * (6 - expected_count))
        linked_holds.append(holds + [0xFFFF] * (6 - expected_count))
        linked_widths.append(widths + [widths[-1]] * (6 - expected_count))
        linked_heights.append(heights + [heights[-1]] * (6 - expected_count))
        linked_flips.append(flips + [flips[-1]] * (6 - expected_count))
    stage0_end = tables["source_table_011fd4"]["sequences"][:2]
    if len(stage0_end) != 2 or any(
        sequence["termination"] != "long-delay/control record"
        or len(sequence["records"]) != 1
        or int(sequence["records"][0]["timer"]) != 0xFFFF
        for sequence in stage0_end
    ):
        raise ValueError("Unexpected Stage 0 mode-0 terminal pose")
    stage0_end_codes, stage0_end_widths, stage0_end_heights = [], [], []
    stage0_end_flips = []
    for sequence in stage0_end:
        words = [int(word, 16) for word in sequence["records"][0]["words"]]
        descriptor = words[1]
        if ((descriptor & 0xFF00) != 0x1300
                or ((descriptor >> 8) & 0x0F) != 3
                or ((descriptor >> 12) & 0x0F) != 1):
            raise ValueError("Unexpected Stage 0 mode-0 terminal descriptor")
        stage0_end_codes.append(words[5])
        stage0_end_widths.append(((descriptor >> 8) & 0x0F) + 1)
        stage0_end_heights.append(((descriptor >> 12) & 0x0F) + 1)
        stage0_end_flips.append(1 if descriptor & 0x0020 else 0)
    stage0_linked_end = tables["source_table_011fd4"]["sequences"][:4]
    if len(stage0_linked_end) != 4:
        raise ValueError("Unexpected Stage 0 linked terminal selector count")
    linked_end_codes, linked_end_holds = [], []
    linked_end_widths, linked_end_heights, linked_end_flips = [], [], []
    linked_end_counts = []
    for selector, sequence in enumerate(stage0_linked_end):
        records = sequence["records"]
        expected_count = 1 if selector < 2 else 2
        if len(records) != expected_count:
            raise ValueError(f"Unexpected Stage 0 linked terminal selector {selector}")
        codes, holds, widths, heights, flips = [], [], [], [], []
        for record in records:
            words = [int(word, 16) for word in record["words"]]
            descriptor, timer = words[1], int(record["timer"])
            if selector < 2:
                if timer != 0xFFFF or descriptor not in (0x1303, 0x1323) or words[5] != 0x084c:
                    raise ValueError("Unexpected Stage 0 normal terminal pose")
            elif timer not in (18, 0x8014) or words[5] != 0x0011:
                raise ValueError("Unexpected Stage 0 slow terminal animation")
            codes.append(words[5])
            holds.append(timer)
            widths.append(((descriptor >> 8) & 0x0F) + 1)
            heights.append(((descriptor >> 12) & 0x0F) + 1)
            flips.append(1 if descriptor & 0x0020 else 0)
        if selector >= 2 and holds != [18, 0x8014]:
            raise ValueError("Unexpected Stage 0 slow terminal timing")
        linked_end_counts.append(expected_count)
        linked_end_codes.append(codes + [codes[-1]] * (2 - expected_count))
        linked_end_holds.append(holds + [0xFFFF] * (2 - expected_count))
        linked_end_widths.append(widths + [widths[-1]] * (2 - expected_count))
        linked_end_heights.append(heights + [heights[-1]] * (2 - expected_count))
        linked_end_flips.append(flips[0])
    lines.extend([
        "#define WILLOW_STAGE0_ACTOR_FRAME_COUNT 6",
        "static const unsigned short willow_stage0_actor_codes[2][6] = {",
    ])
    for row in stage0_codes:
        lines.append("    { " + ", ".join(f"0x{value:04x}" for value in row) + " },")
    lines.extend(["};", "static const unsigned char willow_stage0_actor_holds[6] = { "
        + ", ".join(str(value) for value in stage0_holds[0]) + " };"])
    for name, rows in (("widths", stage0_widths), ("heights", stage0_heights),
                       ("hflip", [[value] for value in stage0_flips])):
        lines.append(f"static const unsigned char willow_stage0_actor_{name}[2][6] = {{")
        for row in rows:
            values = row * 6 if len(row) == 1 else row
            lines.append("    { " + ", ".join(str(value) for value in values) + " },")
        lines.append("};")
    for name, values in (("codes", stage0_end_codes), ("widths", stage0_end_widths),
                         ("heights", stage0_end_heights), ("hflip", stage0_end_flips)):
        ctype = "unsigned short" if name == "codes" else "unsigned char"
        formatted = [f"0x{value:04x}" if name == "codes" else str(value) for value in values]
        lines.append(f"static const {ctype} willow_stage0_actor_end_{name}[2] = {{ "
                     + ", ".join(formatted) + " };")
    # The initialized Stage 0 actor enters 0x11fe8 during its state-4
    # deceleration, then uses the first 0x11ff0 record for its six-tick tail.
    decel_sequences = tables["source_table_011fe8"]["sequences"][:2]
    tail_sequences = tables["source_table_011ff0"]["sequences"][:2]
    decel_codes, decel_widths, decel_heights, decel_flips = [], [], [], []
    decel_holds = []
    tail_codes, tail_widths, tail_heights, tail_flips, tail_holds = [], [], [], [], []
    for sequence in decel_sequences:
        records = sequence["records"]
        if len(records) != 2 or [int(record["timer"]) for record in records] != [6, 0xFFFF]:
            raise ValueError("Unexpected Stage 0 0x11fe8 deceleration animation")
        codes, widths, heights, flips = [], [], [], []
        for record in records:
            words = [int(word, 16) for word in record["words"]]
            descriptor = words[1]
            if words[5] not in (0x0860, 0x0863) or (descriptor & 0xFF00) not in (0x1200, 0x1300):
                raise ValueError("Unexpected Stage 0 0x11fe8 frame descriptor")
            codes.append(words[5])
            widths.append(((descriptor >> 8) & 0x0F) + 1)
            heights.append(((descriptor >> 12) & 0x0F) + 1)
            flips.append(1 if descriptor & 0x0020 else 0)
        decel_codes.append(codes)
        decel_widths.append(widths)
        decel_heights.append(heights)
        decel_flips.append(flips)
        decel_holds.append([6, 0xFFFF])
    for sequence in tail_sequences:
        record = sequence["records"][0]
        words = [int(word, 16) for word in record["words"]]
        descriptor = words[1]
        if (words[5] != 0x0867 or int(record["timer"]) != 10
                or (descriptor & 0xFF00) != 0x1300):
            raise ValueError("Unexpected Stage 0 0x11ff0 tail frame")
        tail_codes.append(words[5])
        tail_widths.append(((descriptor >> 8) & 0x0F) + 1)
        tail_heights.append(((descriptor >> 12) & 0x0F) + 1)
        tail_flips.append(1 if descriptor & 0x0020 else 0)
        tail_holds.append(10)
    lines.extend(["#define WILLOW_STAGE0_ACTOR_DECEL_FRAME_COUNT 2",
                  "static const unsigned short willow_stage0_actor_decel_codes[2][2] = {"])
    for row in decel_codes:
        lines.append("    { " + ", ".join(f"0x{value:04x}" for value in row) + " },")
    lines.extend(["};", "static const unsigned char willow_stage0_actor_decel_widths[2][2] = {"])
    for row in decel_widths:
        lines.append("    { " + ", ".join(str(value) for value in row) + " },")
    lines.extend(["};", "static const unsigned char willow_stage0_actor_decel_heights[2][2] = {"])
    for row in decel_heights:
        lines.append("    { " + ", ".join(str(value) for value in row) + " },")
    lines.extend(["};", "static const unsigned char willow_stage0_actor_decel_hflip[2][2] = {"])
    for row in decel_flips:
        lines.append("    { " + ", ".join(str(value) for value in row) + " },")
    lines.extend(["};", "static const unsigned short willow_stage0_actor_decel_holds[2] = { 6, 0xffff };"])
    for name, values, ctype in (("codes", tail_codes, "unsigned short"),
                                 ("widths", tail_widths, "unsigned char"),
                                 ("heights", tail_heights, "unsigned char"),
                                 ("hflip", tail_flips, "unsigned char")):
        formatted = [f"0x{value:04x}" if name == "codes" else str(value) for value in values]
        lines.append(f"static const {ctype} willow_stage0_actor_tail_{name}[2] = {{ "
                     + ", ".join(formatted) + " };")

    recovery_sequences = tables["source_table_011fec"]["sequences"][:2]
    if len(recovery_sequences) != 2:
        raise ValueError("Unexpected Stage 0 recovery facing count")
    recovery_codes, recovery_widths, recovery_heights, recovery_flips = [], [], [], []
    for sequence in recovery_sequences:
        records = sequence["records"]
        if len(records) != 1:
            raise ValueError("Unexpected Stage 0 recovery animation length")
        record = records[0]
        words = [int(word, 16) for word in record["words"]]
        descriptor = words[1]
        if (words[5] != 0x0929 or int(record["timer"]) != 0xFFFF
                or (descriptor & 0xFF00) != 0x1300
                or ((descriptor >> 8) & 0x0F) != 3
                or ((descriptor >> 12) & 0x0F) != 1):
            raise ValueError("Unexpected Stage 0 recovery terminal pose")
        recovery_codes.append(words[5])
        recovery_widths.append(((descriptor >> 8) & 0x0F) + 1)
        recovery_heights.append(((descriptor >> 12) & 0x0F) + 1)
        recovery_flips.append(1 if descriptor & 0x0020 else 0)
    for name, values, ctype in (
            ("codes", recovery_codes, "unsigned short"),
            ("widths", recovery_widths, "unsigned char"),
            ("heights", recovery_heights, "unsigned char"),
            ("hflip", recovery_flips, "unsigned char")):
        formatted = [f"0x{value:04x}" if name == "codes" else str(value)
                     for value in values]
        lines.append(f"static const {ctype} willow_stage0_actor_recovery_{name}[2] = {{ "
                     + ", ".join(formatted) + " };")

    stall_sequences = tables["source_table_011fdc"]["sequences"][:2]
    if len(stall_sequences) != 2:
        raise ValueError("Unexpected Stage 0 recovery-stall facing count")
    stall_codes, stall_widths, stall_heights, stall_flips = [], [], [], []
    for sequence in stall_sequences:
        records = sequence["records"]
        if len(records) != 1:
            raise ValueError("Unexpected Stage 0 recovery-stall sequence")
        record = records[0]
        words = [int(word, 16) for word in record["words"]]
        descriptor = words[1]
        width = ((descriptor >> 8) & 0x0F) + 1
        height = ((descriptor >> 12) & 0x0F) + 1
        if (words[5] != 0x092D or int(record["timer"]) != 0xFFFF
                or (descriptor & 0xFFDF) != 0x1203
                or width != 3 or height != 2):
            raise ValueError("Unexpected Stage 0 recovery-stall pose")
        stall_codes.append(words[5])
        stall_widths.append(width)
        stall_heights.append(height)
        stall_flips.append(1 if descriptor & 0x0020 else 0)
    for name, values, ctype in (
            ("codes", stall_codes, "unsigned short"),
            ("widths", stall_widths, "unsigned char"),
            ("heights", stall_heights, "unsigned char"),
            ("hflip", stall_flips, "unsigned char")):
        formatted = [f"0x{value:04x}" if name == "codes" else str(value)
                     for value in values]
        lines.append(f"static const {ctype} willow_stage0_actor_stall_{name}[2] = {{ "
                     + ", ".join(formatted) + " };")
    lines.extend([
        "#define WILLOW_STAGE0_LINKED_SELECTOR_COUNT 4",
        "static const unsigned char willow_stage0_linked_frame_count[4] = { "
        + ", ".join(str(value) for value in linked_counts) + " };",
        "static const unsigned short willow_stage0_linked_codes[4][6] = {",
    ])
    for row in linked_codes:
        lines.append("    { " + ", ".join(f"0x{value:04x}" for value in row) + " },")
    lines.extend(["};", "static const unsigned short willow_stage0_linked_holds[4][6] = {"])
    for row in linked_holds:
        lines.append("    { " + ", ".join(str(value) for value in row) + " },")
    lines.extend(["};", "static const unsigned char willow_stage0_linked_widths[4][6] = {"])
    for row in linked_widths:
        lines.append("    { " + ", ".join(str(value) for value in row) + " },")
    lines.extend(["};", "static const unsigned char willow_stage0_linked_heights[4][6] = {"])
    for row in linked_heights:
        lines.append("    { " + ", ".join(str(value) for value in row) + " },")
    lines.extend(["};", "static const unsigned char willow_stage0_linked_hflip[4][6] = {"])
    for row in linked_flips:
        lines.append("    { " + ", ".join(str(value) for value in row) + " },")
    lines.append("};")
    lines.extend([
        "#define WILLOW_STAGE0_LINKED_END_FRAME_MAX 2",
        "static const unsigned char willow_stage0_linked_end_frame_count[4] = { "
        + ", ".join(str(value) for value in linked_end_counts) + " };",
        "static const unsigned short willow_stage0_linked_end_codes[4][2] = {",
    ])
    for row in linked_end_codes:
        lines.append("    { " + ", ".join(f"0x{value:04x}" for value in row) + " },")
    lines.extend(["};", "static const unsigned short willow_stage0_linked_end_holds[4][2] = {"])
    for row in linked_end_holds:
        lines.append("    { " + ", ".join(str(value) for value in row) + " },")
    lines.extend(["};", "static const unsigned char willow_stage0_linked_end_widths[4][2] = {"])
    for row in linked_end_widths:
        lines.append("    { " + ", ".join(str(value) for value in row) + " },")
    lines.extend(["};", "static const unsigned char willow_stage0_linked_end_heights[4][2] = {"])
    for row in linked_end_heights:
        lines.append("    { " + ", ".join(str(value) for value in row) + " },")
    lines.extend(["};", "static const unsigned char willow_stage0_linked_end_hflip[4][2] = {"])
    for selector in range(4):
        records = stage0_linked_end[selector]["records"]
        values = [1 if int(record["words"][1], 16) & 0x0020 else 0
                  for record in records]
        values += [values[-1]] * (2 - len(values))
        lines.append("    { " + ", ".join(str(value) for value in values) + " },")
    lines.append("};")
    lines.append("")
    lines.append("")

    # Stage 0 profile-1 actor starts on 0x11fc8, then state 2 selects the
    # four-frame 0x11fd0 patrol loop. Keep both facing variants and validate
    # every CPS descriptor and eight-tick hold before generating runtime data.
    profile_specs = (
        ("source_table_011fc8", 7, "startup"),
        ("source_table_011fd0", 4, "walk"),
    )
    profile_data = {}
    for table_name, frame_count, label in profile_specs:
        sequences = tables[table_name]["sequences"][:2]
        if len(sequences) != 2:
            raise ValueError(f"Unexpected profile-1 {label} facing count")
        code_rows, hold_rows, width_rows, height_rows, flip_rows = [], [], [], [], []
        for sequence in sequences:
            if sequence["termination"] != "loop" or len(sequence["records"]) != frame_count:
                raise ValueError(f"Unexpected profile-1 {label} frame count")
            codes, holds, widths, heights, flips = [], [], [], [], []
            for record in sequence["records"]:
                words = [int(word, 16) for word in record["words"]]
                descriptor = words[1]
                width = ((descriptor >> 8) & 0x0F) + 1
                height = ((descriptor >> 12) & 0x0F) + 1
                if (int(record["timer"]) != 8
                        or (descriptor & 0xFF00) not in (0x1100, 0x1200, 0x1300)
                        or width not in (2, 3, 4) or height != 2):
                    raise ValueError(f"Unexpected profile-1 {label} frame descriptor")
                codes.append(words[5])
                holds.append(int(record["timer"]))
                widths.append(width)
                heights.append(height)
                flips.append(1 if descriptor & 0x0020 else 0)
            code_rows.append(codes)
            hold_rows.append(holds)
            width_rows.append(widths)
            height_rows.append(heights)
            flip_rows.append(flips)
        profile_data[label] = (code_rows, hold_rows, width_rows, height_rows, flip_rows)

    for label, frame_count in (("startup", 7), ("walk", 4)):
        code_rows, hold_rows, width_rows, height_rows, flip_rows = profile_data[label]
        prefix = f"willow_profile1_{label}"
        lines.append(f"#define WILLOW_PROFILE1_{label.upper()}_FRAME_COUNT {frame_count}")
        for field, rows, ctype in (
            ("codes", code_rows, "unsigned short"),
            ("holds", hold_rows, "unsigned char"),
            ("widths", width_rows, "unsigned char"),
            ("heights", height_rows, "unsigned char"),
            ("hflip", flip_rows, "unsigned char"),
        ):
            lines.append(f"static const {ctype} {prefix}_{field}[2][{frame_count}] = {{")
            for row in rows:
                values = [f"0x{value:04x}" for value in row] if field == "codes" else [str(value) for value in row]
                lines.append("    { " + ", ".join(values) + " },")
            lines.append("};")
        lines.append("")

    # Player action state at 0xcde8 selects the directional d3be animation.
    action_sequences = tables["player_aux_d3be"]["sequences"]
    action_codes, action_holds, action_flips = [], [], []
    for sequence in action_sequences:
        if len(sequence["records"]) != 3:
            raise ValueError("Unexpected Willow d3be action frame count")
        codes, holds, flips = [], [], set()
        for index, record in enumerate(sequence["records"]):
            words = [int(word, 16) for word in record["words"]]
            descriptor, timer = words[1], int(record["timer"])
            if ((descriptor & 0xFF00) != 0x1200
                    or ((descriptor >> 8) & 0x0F) != 2
                    or ((descriptor >> 12) & 0x0F) != 1
                    or timer != (6 if index < 2 else 0xFFFF)):
                raise ValueError("Unexpected Willow d3be action frame data")
            codes.append(words[5])
            holds.append(0 if timer == 0xFFFF else timer)
            flips.add(1 if descriptor & 0x0020 else 0)
        if len(flips) != 1:
            raise ValueError("Mixed facing flags in a d3be action sequence")
        action_codes.append(codes)
        action_holds.append(holds)
        action_flips.append(flips.pop())
    lines.extend([
        "#define WILLOW_ACTION_SEQUENCE_COUNT 2",
        "#define WILLOW_ACTION_FRAME_COUNT 3",
        "static const unsigned char willow_action_hflip[2] = { "
        + ", ".join(str(value) for value in action_flips) + " };",
        "static const unsigned char willow_action_width[2] = { 3, 3 };",
        "static const unsigned char willow_action_height[2] = { 2, 2 };",
        "static const unsigned char willow_action_holds[2][3] = {",
    ])
    for holds in action_holds:
        lines.append("    { " + ", ".join(str(value) for value in holds) + " },")
    lines.extend(["};", "static const unsigned short willow_action_codes[2][3] = {"])
    for codes in action_codes:
        lines.append("    { " + ", ".join(f"0x{code:04x}" for code in codes) + " },")
    recovery_sequences = tables["player_aux_d3c2"]["sequences"]
    recovery_codes, recovery_flips = [], []
    for sequence in recovery_sequences:
        if len(sequence["records"]) != 1 or int(sequence["records"][0]["timer"]) != 0xFFFF:
            raise ValueError("Unexpected Willow d3c2 recovery pose")
        record = sequence["records"][0]
        words = [int(word, 16) for word in record["words"]]
        descriptor = words[1]
        if ((descriptor & 0xFF00) != 0x1200
                or ((descriptor >> 8) & 0x0F) != 2
                or ((descriptor >> 12) & 0x0F) != 1):
            raise ValueError("Unexpected Willow d3c2 recovery descriptor")
        recovery_codes.append(words[5])
        recovery_flips.append(1 if descriptor & 0x0020 else 0)
    lines.extend([
        "};",
        "static const unsigned char willow_action_recovery_width[2] = { 3, 3 };",
        "static const unsigned char willow_action_recovery_height[2] = { 2, 2 };",
        "static const unsigned short willow_action_recovery_codes[2] = { "
        + ", ".join(f"0x{code:04x}" for code in recovery_codes) + " };",
        "static const unsigned char willow_action_recovery_hflip[2] = { "
        + ", ".join(str(value) for value in recovery_flips) + " };",
        "",
    ])

    # The charge handler at 0xca1c selects d1a2/d1aa first, then d1a6/d1ae.
    # Export the facing-specific records for both five-tick pose phases.
    charge_codes, charge_widths, charge_heights, charge_flips = [], [], [], []
    for table_name in ("player_aux_d1a2", "player_aux_d1aa",
                       "player_aux_d1a6", "player_aux_d1ae"):
        sequences = tables[table_name]["sequences"]
        if len(sequences) != 2:
            raise ValueError(f"Unexpected {table_name} charge sequence count")
        code_row, width_row, height_row, flip_row = [], [], [], []
        for sequence in sequences:
            if len(sequence["records"]) != 1:
                raise ValueError(f"Unexpected {table_name} charge pose")
            record = sequence["records"][0]
            words = [int(word, 16) for word in record["words"]]
            descriptor = words[1]
            if int(record["timer"]) != 0xFFFF or (descriptor & 0xFF00) not in (0x2200, 0x1200):
                raise ValueError(f"Unexpected {table_name} charge descriptor")
            code_row.append(words[5])
            width_row.append(((descriptor >> 8) & 0x0F) + 1)
            height_row.append(((descriptor >> 12) & 0x0F) + 1)
            flip_row.append(1 if descriptor & 0x0020 else 0)
        charge_codes.append(code_row)
        charge_widths.append(width_row)
        charge_heights.append(height_row)
        charge_flips.append(flip_row)
    lines.extend([
        "static const unsigned short willow_charge_codes[4][2] = {",
    ])
    for row in charge_codes:
        lines.append("    { " + ", ".join(f"0x{code:04x}" for code in row) + " },")
    lines.extend(["};", "static const unsigned char willow_charge_widths[4][2] = {"])
    for row in charge_widths:
        lines.append("    { " + ", ".join(str(value) for value in row) + " },")
    lines.extend(["};", "static const unsigned char willow_charge_heights[4][2] = {"])
    for row in charge_heights:
        lines.append("    { " + ", ".join(str(value) for value in row) + " },")
    lines.extend(["};", "static const unsigned char willow_charge_hflip[4][2] = {"])
    for row in charge_flips:
        lines.append("    { " + ", ".join(str(value) for value in row) + " },")
    lines.extend(["};", ""])

    jump_sequences = tables["player_aux_d2ca"]["sequences"]
    jump_codes, jump_holds, jump_widths, jump_heights = [], [], [], []
    for sequence in jump_sequences:
        if len(sequence["records"]) != 4:
            raise ValueError("Unexpected Willow d2ca jump frame count")
        codes, holds, widths, heights, descriptors = [], [], [], [], []
        for index, record in enumerate(sequence["records"]):
            words = [int(word, 16) for word in record["words"]]
            descriptor = words[1]
            timer = int(record["timer"])
            expected_timer = 6 if index < 3 else 0xFFFF
            if timer != expected_timer or (descriptor & 0xFF00) not in (0x1200, 0x2200):
                raise ValueError("Unexpected Willow d2ca jump frame data")
            descriptors.append(descriptor)
            codes.append(words[5])
            # 0xffff is the source's indefinite terminal frame; encode it as
            # zero in the byte-sized runtime table because the game loop stops
            # advancing at that frame.
            holds.append(0 if timer == 0xFFFF else timer)
            widths.append(((descriptor >> 8) & 0x0F) + 1)
            heights.append(((descriptor >> 12) & 0x0F) + 1)
        jump_codes.append(codes)
        jump_holds.append(holds)
        jump_widths.append(widths)
        jump_heights.append(heights)
    if any(jump_codes[0][i] != jump_codes[1][i] for i in range(4)):
        raise ValueError("Willow d2ca facing variants no longer share tile codes")
    lines.extend([
        "#define WILLOW_JUMP_SEQUENCE_COUNT 2",
        "#define WILLOW_JUMP_FRAME_COUNT 4",
        "static const unsigned char willow_jump_hflip[2] = { 0, 1 };",
        "static const unsigned char willow_jump_holds[2][4] = {",
    ])
    for holds in jump_holds:
        lines.append("    { " + ", ".join(str(value) for value in holds) + " },")
    lines.extend(["};", "static const unsigned char willow_jump_widths[2][4] = {"])
    for widths in jump_widths:
        lines.append("    { " + ", ".join(str(value) for value in widths) + " },")
    lines.extend(["};", "static const unsigned char willow_jump_heights[2][4] = {"])
    for heights in jump_heights:
        lines.append("    { " + ", ".join(str(value) for value in heights) + " },")
    lines.extend(["};", "static const unsigned short willow_jump_codes[4] = {"])
    lines.append("    " + ", ".join(f"0x{code:04x}" for code in jump_codes[0]))
    lines.extend(["};"])

    launch_sequences = tables["player_aux_d27a"]["sequences"][:2]
    launch_codes, launch_flips = [], []
    for sequence in launch_sequences:
        records = sequence["records"]
        if len(records) != 1 or int(records[0]["timer"]) != 0xFFFF:
            raise ValueError("Unexpected Willow d27a launch pose")
        words = [int(word, 16) for word in records[0]["words"]]
        descriptor = words[1]
        if ((descriptor & 0xFF00) != 0x1200
                or ((descriptor >> 8) & 0x0F) != 2
                or ((descriptor >> 12) & 0x0F) != 1):
            raise ValueError("Unexpected Willow d27a descriptor")
        launch_codes.append(words[5])
        launch_flips.append(1 if descriptor & 0x0020 else 0)
    lines.extend([
        "static const unsigned short willow_jump_launch_codes[2] = { "
        + ", ".join(f"0x{code:04x}" for code in launch_codes) + " };",
        "static const unsigned char willow_jump_launch_width = 3;",
        "static const unsigned char willow_jump_launch_height = 2;",
        "static const unsigned char willow_jump_launch_hflip[2] = { "
        + ", ".join(str(value) for value in launch_flips) + " };",
    ])

    fall_sequences = tables["player_aux_d2ce"]["sequences"][:2]
    fall_codes, fall_flips = [], []
    for sequence in fall_sequences:
        records = sequence["records"]
        if len(records) != 1 or int(records[0]["timer"]) != 0xFFFF:
            raise ValueError("Unexpected Willow d2ce falling pose")
        words = [int(word, 16) for word in records[0]["words"]]
        descriptor = words[1]
        if ((descriptor & 0xFF00) != 0x2200
                or ((descriptor >> 8) & 0x0F) != 2
                or ((descriptor >> 12) & 0x0F) != 2):
            raise ValueError("Unexpected Willow d2ce descriptor")
        fall_codes.append(words[5])
        fall_flips.append(1 if descriptor & 0x0020 else 0)
    lines.extend([
        "static const unsigned short willow_jump_fall_codes[2] = { "
        + ", ".join(f"0x{code:04x}" for code in fall_codes) + " };",
        "static const unsigned char willow_jump_fall_width = 3;",
        "static const unsigned char willow_jump_fall_height = 3;",
        "static const unsigned char willow_jump_fall_hflip[2] = { "
        + ", ".join(str(value) for value in fall_flips) + " };",
        "",
    ])

    # The 0x10c9c recovery branches select these two facing-indexed animation
    # tables. Keep the source records before each table's long-delay/control
    # tail; the 0x113fa branch has a 22-tick blank frame before that tail.
    special_tables = ["source_table_0113fa", "source_table_011416"]
    special_counts = []
    special_codes, special_holds = [], []
    special_widths, special_heights, special_flips = [], [], []
    special_origins_x, special_origins_y = [], []
    for table_name in special_tables:
        counts = []
        code_rows, hold_rows = [], []
        width_rows, height_rows, flip_rows = [], [], []
        origin_x_rows, origin_y_rows = [], []
        for sequence_index in range(2):
            records = tables[table_name]["sequences"][sequence_index]["records"]
            codes, holds, widths, heights, flips = [], [], [], [], []
            origins_x, origins_y = [], []
            for record in records:
                words = [int(word, 16) for word in record["words"]]
                descriptor, code = words[1], words[5]
                timer = int(record["timer"])
                if timer < 1 or timer > 255 or code >= 0x8000:
                    break
                extended_frame = int(record["length"]) > 12
                if extended_frame:
                    if (table_name not in ("source_table_0113fa",
                                           "source_table_011416")
                            or int(record["rom_offset"], 16) != 0x11416
                            or int(record["length"]) != 18
                            or words[1] != 0x0010 or words[3] != 0x3210):
                        raise ValueError(f"Unexpected extended {table_name} frame")
                    descriptor = words[3]
                width = ((descriptor >> 8) & 0x0F) + 1
                height = ((descriptor >> 12) & 0x0F) + 1
                # Stop at the first control/tail entry that cannot be drawn as
                # one of the small actor frames represented by this runtime.
                if (timer < 1 or timer > 255 or width > 4 or height > 4
                        or code >= 0x8000):
                    break
                codes.append(code)
                holds.append(timer)
                widths.append(width)
                heights.append(height)
                flips.append(1 if descriptor & 0x0020 else 0)
                if extended_frame:
                    # Coordinates and component codes for this shared record
                    # are exported as enemy_special_composition below.
                    origins_x.append(0)
                    origins_y.append(0)
                else:
                    origins_x.append(words[2] & 0x7FFF)
                    origins_y.append(words[3] - 0x10000 if words[3] & 0x8000 else words[3])
            if not codes:
                raise ValueError(f"No drawable {table_name} special frames")
            if len(codes) > 4:
                raise ValueError(f"Too many {table_name} special frames")
            counts.append(len(codes))
            while len(codes) < 4:
                codes.append(0)
                holds.append(0)
                widths.append(1)
                heights.append(1)
                flips.append(0)
                origins_x.append(0)
                origins_y.append(0)
            code_rows.append(codes)
            hold_rows.append(holds)
            width_rows.append(widths)
            height_rows.append(heights)
            flip_rows.append(flips)
            origin_x_rows.append(origins_x)
            origin_y_rows.append(origins_y)
        special_codes.append(code_rows)
        special_holds.append(hold_rows)
        special_widths.append(width_rows)
        special_heights.append(height_rows)
        special_flips.append(flip_rows)
        special_origins_x.append(origin_x_rows)
        special_origins_y.append(origin_y_rows)
        special_counts.append(counts)

    lines.extend([
        "#define WILLOW_ENEMY_SPECIAL_KIND_COUNT 2",
        "#define WILLOW_ENEMY_SPECIAL_FRAME_MAX 4",
        "static const unsigned char willow_enemy_special_frame_count[2][2] = {",
        "    { " + ", ".join(str(value) for value in special_counts[0]) + " },",
        "    { " + ", ".join(str(value) for value in special_counts[1]) + " },",
        "};",
    ])
    for symbol, rows in (
            ("codes", special_codes), ("holds", special_holds),
            ("widths", special_widths), ("heights", special_heights),
            ("hflip", special_flips), ("origin_x", special_origins_x),
            ("origin_y", special_origins_y)):
        ctype = "unsigned short" if symbol == "codes" else (
            "signed char" if symbol == "origin_y" else "unsigned char")
        lines.append(f"static const {ctype} willow_enemy_special_{symbol}[2][2][4] = {{")
        for table_rows in rows:
            lines.append("    {")
            for row in table_rows:
                values = [f"0x{v:04x}" for v in row] if symbol == "codes" else [str(v) for v in row]
                lines.append("        { " + ", ".join(values) + " },")
            lines.append("    },")
        lines.extend(["};", ""])

    # State-2 substate 2 loads 0x114f2 after its first movement step. Preserve
    # both source directions, including the terminal held record.
    follow_sequences = tables[
        "enemy_state2_attack_followup_114f2"]["sequences"][:2]
    follow_codes, follow_holds = [], []
    follow_widths, follow_heights, follow_flips = [], [], []
    follow_origins_x, follow_origins_y = [], []
    for sequence in follow_sequences:
        codes, holds, widths, heights, flips = [], [], [], [], []
        origins_x, origins_y = [], []
        for record in sequence["records"]:
            words = [int(word, 16) for word in record["words"]]
            descriptor, code = words[1], words[5]
            timer = int(record["timer"])
            width = ((descriptor >> 8) & 0x0F) + 1
            height = ((descriptor >> 12) & 0x0F) + 1
            if (width > 4 or height > 4 or code >= 0x8000
                    or timer > 0xFFFF):
                break
            codes.append(code)
            holds.append(0 if timer == 0xFFFF else timer)
            widths.append(width)
            heights.append(height)
            flips.append(1 if descriptor & 0x0020 else 0)
            origins_x.append(words[2] & 0x7FFF)
            origins_y.append(words[3] - 0x10000 if words[3] & 0x8000 else words[3])
        if len(codes) != 4 or len(set(flips)) != 1:
            raise ValueError("Unexpected 0x114f2 enemy follow-up sequence")
        if origins_x != [7, 22, 22, 24] or origins_y != [5, 0, 0, 2]:
            raise ValueError("Unexpected 0x114f2 enemy follow-up origins")
        follow_codes.append(codes)
        follow_holds.append(holds)
        follow_widths.append(widths)
        follow_heights.append(heights)
        follow_flips.append(flips[0])
        follow_origins_x.append(origins_x)
        follow_origins_y.append(origins_y)
    lines.extend([
        "#define WILLOW_ENEMY_FOLLOWUP_FRAME_COUNT 4",
        "static const unsigned short willow_enemy_followup_codes[2][4] = {",
    ])
    for row in follow_codes:
        lines.append("    { " + ", ".join(f"0x{v:04x}" for v in row) + " },")
    lines.extend(["};", "static const unsigned short willow_enemy_followup_holds[2][4] = {"])
    for row in follow_holds:
        lines.append("    { " + ", ".join(str(v) for v in row) + " },")
    lines.extend(["};", "static const unsigned char willow_enemy_followup_widths[2][4] = {"])
    for row in follow_widths:
        lines.append("    { " + ", ".join(str(v) for v in row) + " },")
    lines.extend(["};", "static const unsigned char willow_enemy_followup_heights[2][4] = {"])
    for row in follow_heights:
        lines.append("    { " + ", ".join(str(v) for v in row) + " },")
    lines.extend(["};", "static const unsigned char willow_enemy_followup_hflip[2] = { "
                  + ", ".join(str(v) for v in follow_flips) + " };", ""])
    for suffix, rows, ctype in (("origin_x", follow_origins_x, "unsigned char"),
                                ("origin_y", follow_origins_y, "signed char")):
        lines.append(f"static const {ctype} willow_enemy_followup_{suffix}[2][4] = {{")
        for row in rows:
            lines.append("    { " + ", ".join(str(v) for v in row) + " },")
        lines.extend(["};", ""])

    # State-6 mode 4 loads the direct 0x1144e frame stream with 1f40. The
    # two drawable records precede its non-frame control record at 0x11466.
    below_records = catalog["enemy_followup_below"]["records"]
    if ([record["code"] for record in below_records] != [0x018E, 0x01C8]
            or [record["timer"] for record in below_records] != [4, 4]
            or [record["origin_x"] for record in below_records] != [23, 26]
            or [record["origin_y"] for record in below_records] != [1, 4]
            or any((record["width"], record["height"], record["hflip"])
                   != (2, 4, 0) for record in below_records)):
        raise ValueError("Unexpected 0x1144e below-player frames")
    lines.extend([
        "#define WILLOW_ENEMY_FOLLOWUP_BELOW_FRAME_COUNT 2",
        "static const unsigned short willow_enemy_followup_below_codes[2] = { 0x018e, 0x01c8 };",
        "static const unsigned char willow_enemy_followup_below_holds[2] = { 4, 4 };",
        "static const unsigned char willow_enemy_followup_below_width = 2;",
        "static const unsigned char willow_enemy_followup_below_height = 4;",
        "static const unsigned char willow_enemy_followup_below_hflip = 0;",
        "static const unsigned char willow_enemy_followup_below_origin_x[2] = { "
        + ", ".join(str(record["origin_x"]) for record in below_records) + " };",
        "static const signed char willow_enemy_followup_below_origin_y[2] = { "
        + ", ".join(str(record["origin_y"]) for record in below_records) + " };",
        "",
    ])

    # State-6 mode 10 loads the two-record recovery stream at 0x1149e.
    lines.extend([
        "#define WILLOW_ENEMY_ATTACK_RECOVERY_FRAME_COUNT 2",
        "static const unsigned short willow_enemy_attack_recovery_codes[2] = { 0x01c4, 0x01c0 };",
        "static const unsigned char willow_enemy_attack_recovery_holds[2] = { 6, 0 };",
        "static const unsigned char willow_enemy_attack_recovery_width = 4;",
        "static const unsigned char willow_enemy_attack_recovery_height = 4;",
        "static const unsigned char willow_enemy_attack_recovery_hflip = 0;",
        "",
    ])

    # The +3 == 2 above-player response uses 0x114ba for its 32-tick entry,
    # then 0x114d6 during the vertical rise. Both are directional selector
    # tables with an extended frame whose descriptor sits at record word 3.
    for table_name, symbol, extended_address, extended_descriptor, expected in (
            ("source_table_0114ba", "willow_enemy_attack_above_entry",
             0x114D6, 0x3310,
             ([[0x0C94, 0x0C94, 0x0003, 0x0004],
               [0x0C94, 0x0003, 0x0004]],
              [[8, 8, 22, 3200], [8, 22, 3200]])),
            ("source_table_0114d6", "willow_enemy_attack_above_rise",
             0x114F2, 0x2310,
             ([[0x0C80, 0x0C80, 0x0005, 0x0010],
               [0x0C80, 0x0005, 0x0010]],
              [[8, 8, 7, 3203], [8, 7, 3203]]))):
        sequences = tables[table_name]["sequences"][:2]
        code_rows, hold_rows = [], []
        width_rows, height_rows, flip_rows = [], [], []
        origin_x_rows, origin_y_rows, counts = [], [], []
        for sequence in sequences:
            codes, holds, widths, heights, flips = [], [], [], [], []
            origins_x, origins_y = [], []
            for record in sequence["records"]:
                words = [int(word, 16) for word in record["words"]]
                address = int(record["rom_offset"], 16)
                timer, code = int(record["timer"]), words[5]
                if timer > 0xFFFF or code >= 0x8000:
                    break
                descriptor = words[1]
                extended = address == extended_address
                if extended:
                    expected_control = 0x0010 if extended_address == 0x114D6 else 0x0038
                    if (int(record["length"]) != 18
                            or words[1] != expected_control
                            or words[3] != extended_descriptor):
                        raise ValueError(f"Unexpected extended record in {table_name}")
                    descriptor = words[3]
                width = ((descriptor >> 8) & 0x0F) + 1
                height = ((descriptor >> 12) & 0x0F) + 1
                if width > 4 or height > 4:
                    raise ValueError(f"Unexpected dimensions in {table_name}")
                codes.append(code)
                holds.append(timer)
                widths.append(width)
                heights.append(height)
                flips.append(1 if descriptor & 0x0020 else 0)
                if extended:
                    origins_x.append(0)
                    origins_y.append(0)
                else:
                    origins_x.append(words[2] & 0x7FFF)
                    origins_y.append(words[3] - 0x10000 if words[3] & 0x8000 else words[3])
                if timer > 0xFF:
                    break
            if not codes or len(codes) > 4:
                raise ValueError(f"Unexpected frame count in {table_name}")
            counts.append(len(codes))
            while len(codes) < 4:
                codes.append(0)
                holds.append(0)
                widths.append(1)
                heights.append(1)
                flips.append(0)
                origins_x.append(0)
                origins_y.append(0)
            code_rows.append(codes)
            hold_rows.append(holds)
            width_rows.append(widths)
            height_rows.append(heights)
            flip_rows.append(flips)
            origin_x_rows.append(origins_x)
            origin_y_rows.append(origins_y)
        expected_codes, expected_holds = expected
        if ([row[:count] for row, count in zip(code_rows, counts)] != expected_codes
                or [row[:count] for row, count in zip(hold_rows, counts)] != expected_holds):
            raise ValueError(f"Unexpected source frames in {table_name}")
        lines.extend([
            f"static const unsigned char {symbol}_frame_count[2] = {{ "
            + ", ".join(str(count) for count in counts) + " };",
        ])
        for suffix, rows, ctype in (
                ("codes", code_rows, "unsigned short"),
                ("holds", hold_rows, "unsigned short"),
                ("widths", width_rows, "unsigned char"),
                ("heights", height_rows, "unsigned char"),
                ("hflip", flip_rows, "unsigned char"),
                ("origin_x", origin_x_rows, "unsigned char"),
                ("origin_y", origin_y_rows, "signed char")):
            lines.append(f"static const {ctype} {symbol}_{suffix}[2][4] = {{")
            for row in rows:
                values = [f"0x{value:04x}" for value in row] if suffix == "codes" else [str(value) for value in row]
                lines.append("    { " + ", ".join(values) + " },")
            lines.extend(["};", ""])

    # State-6 mode 12 selects the 0x11466 directional stream and mode 16
    # switches to 0x11482. Keep the source record holds and dimensions.
    for table_name, symbol, expected_starts in (
            ("source_table_011466", "willow_enemy_attack_recovery_setup",
             (0x1146A, 0x11476)),
            ("source_table_011482", "willow_enemy_attack_recovery_contact",
             (0x11486, 0x11492))):
        source_sequences = tables[table_name]["sequences"][:2]
        if tuple(int(sequence["start"], 16) for sequence in source_sequences) != expected_starts:
            raise ValueError(f"Unexpected {table_name} selector targets")
        code_rows, hold_rows = [], []
        width_rows, height_rows, flip_rows, counts = [], [], [], []
        origin_x_rows, origin_y_rows = [], []
        for sequence in source_sequences:
            codes, holds, widths, heights, flips = [], [], [], [], []
            origins_x, origins_y = [], []
            for record in sequence["records"]:
                words = [int(word, 16) for word in record["words"]]
                descriptor, code = words[1], words[5]
                # This 18-byte setup record has a compact control word at +2;
                # its sprite descriptor remains in the usual +6 descriptor
                # slot. The four component codes and origin-template layout
                # are validated above in enemy_setup_composition.
                if (table_name == "source_table_011466"
                        and int(record["rom_offset"], 16) == 0x11482):
                    if (int(record["length"]) != 18 or words[1] != 0x0010
                            or words[3] != 0x3330):
                        raise ValueError("Unexpected extended 0x11482 setup record")
                    descriptor = words[3]
                timer = int(record["timer"])
                if (timer > 0x1000 and timer != 0xFFFF) or code >= 0x8000:
                    break
                width = ((descriptor >> 8) & 0x0F) + 1
                height = ((descriptor >> 12) & 0x0F) + 1
                if width > 4 or height > 4:
                    raise ValueError(f"Unexpected frame dimensions in {table_name}")
                codes.append(code)
                holds.append(0 if timer == 0xFFFF else timer)
                widths.append(width)
                heights.append(height)
                flips.append(1 if descriptor & 0x0020 else 0)
                if (table_name == "source_table_011466"
                        and int(record["rom_offset"], 16) == 0x11482):
                    # The extended record's +6 word is its descriptor, so +6
                    # cannot be treated as a Y origin. Its appended components
                    # are rendered from enemy_setup_composition instead.
                    origins_x.append(0)
                    origins_y.append(0)
                else:
                    origins_x.append(words[2] & 0x7FFF)
                    origins_y.append(words[3] - 0x10000 if words[3] & 0x8000 else words[3])
            if not codes or len(codes) > 4:
                raise ValueError(f"Unexpected frame count in {table_name}")
            counts.append(len(codes))
            while len(codes) < 4:
                codes.append(0)
                holds.append(0)
                widths.append(1)
                heights.append(1)
                flips.append(0)
                origins_x.append(0)
                origins_y.append(0)
            code_rows.append(codes)
            hold_rows.append(holds)
            width_rows.append(widths)
            height_rows.append(heights)
            flip_rows.append(flips)
            origin_x_rows.append(origins_x)
            origin_y_rows.append(origins_y)
        expected_streams = {
            "source_table_011466": (
                [[0x018C, 0x018C, 0x0000, 0x0010],
                 [0x018C, 0x0000, 0x0010]],
                [[4, 4, 22, 392], [4, 22, 392]],
            ),
            "source_table_011482": (
                [[0x0188, 0x0188, 0x01C4, 0x01C0],
                 [0x0188, 0x01C4, 0x01C0]],
                [[4, 4, 6, 0], [4, 6, 0]],
            ),
        }
        if table_name in expected_streams:
            expected_codes, expected_holds = expected_streams[table_name]
            observed_codes = [row[:count] for row, count in zip(code_rows, counts)]
            observed_holds = [row[:count] for row, count in zip(hold_rows, counts)]
            if observed_codes != expected_codes or observed_holds != expected_holds:
                raise ValueError(f"Unexpected source records in {table_name}")
        lines.extend([
            f"#define {symbol.upper()}_FRAME_MAX 4",
            f"static const unsigned char {symbol}_frame_count[2] = {{ "
            + ", ".join(str(count) for count in counts) + " };",
        ])
        for suffix, rows, ctype in (
                ("codes", code_rows, "unsigned short"),
                ("holds", hold_rows, "unsigned short"),
                ("widths", width_rows, "unsigned char"),
                ("heights", height_rows, "unsigned char"),
                ("hflip", flip_rows, "unsigned char"),
                ("origin_x", origin_x_rows, "unsigned char"),
                ("origin_y", origin_y_rows, "signed char")):
            lines.append(f"static const {ctype} {symbol}_{suffix}[2][4] = {{")
            for row in rows:
                values = [f"0x{v:04x}" for v in row] if suffix == "codes" else [str(v) for v in row]
                lines.append("    { " + ", ".join(values) + " },")
            lines.extend(["};", ""])

    setup_composition = catalog["enemy_setup_composition"]
    lines.extend([
        "#define WILLOW_ENEMY_SETUP_COMPONENT_COUNT 4",
        "static const unsigned short willow_enemy_setup_component_codes[4] = { "
        + ", ".join(setup_composition["sprite_codes"]) + " };",
        "static const short willow_enemy_setup_component_x[4] = { "
        + ", ".join(str(pair[0]) for pair in setup_composition["coordinates"])
        + " };",
        "static const short willow_enemy_setup_component_y[4] = { "
        + ", ".join(str(pair[1]) for pair in setup_composition["coordinates"])
        + " };",
        "static const unsigned char willow_enemy_setup_composition_frame[2] = { "
        + ", ".join(str(value) for value in
                     setup_composition["frame_indices_by_sequence"]) + " };",
        "",
    ])
    special_composition = catalog["enemy_special_composition"]
    lines.extend([
        "#define WILLOW_ENEMY_SPECIAL_COMPONENT_COUNT 4",
        "#define WILLOW_ENEMY_SPECIAL_COMPONENT_WIDTH 3",
        "#define WILLOW_ENEMY_SPECIAL_COMPONENT_HEIGHT 4",
        "static const unsigned short willow_enemy_special_component_codes[4] = { "
        + ", ".join(special_composition["sprite_codes"]) + " };",
        "static const short willow_enemy_special_component_x[4] = { "
        + ", ".join(str(pair[0]) for pair in special_composition["coordinates"])
        + " };",
        "static const short willow_enemy_special_component_y[4] = { "
        + ", ".join(str(pair[1]) for pair in special_composition["coordinates"])
        + " };",
        "static const unsigned char willow_enemy_special_composition_frame[2] = { "
        + ", ".join(str(value) for value in
                     special_composition["frame_indices_by_sequence"]) + " };",
        "",
    ])

    # The Stage 0 player handler at 0x125d0 starts animation table 0x13154.
    # Preserve its six-frame, 8-tick directional loop for the native idle pose.
    initial_idle = tables["source_table_013154"]["sequences"][:2]
    if any(sequence["termination"] != "loop" or len(sequence["records"]) != 6
           for sequence in initial_idle):
        raise ValueError("Unexpected Stage 0 player idle sequence")
    idle_codes, idle_holds, idle_flips, idle_x = [], [], [], []
    for sequence in initial_idle:
        codes, holds = [], []
        descriptors, offsets_x, offsets_y = [], [], []
        for record in sequence["records"]:
            words = [int(word, 16) for word in record["words"]]
            descriptor = words[1]
            if (int(record["timer"]) != 8
                    or ((descriptor >> 8) & 0x0F) != 3
                    or ((descriptor >> 12) & 0x0F) != 3
                    or (descriptor & 0xFF00) not in (0x3300,)
                    or (words[2] & 0x7FFF) != 6 or words[3] != 0
                    or words[0] != 0x0010):
                raise ValueError("Unexpected 0x13154 player idle record")
            descriptors.append(descriptor)
            offsets_x.append(words[2] & 0x7FFF)
            offsets_y.append(words[3])
            codes.append(words[5])
            holds.append(int(record["timer"]))
        if (len({descriptor & 0x0020 for descriptor in descriptors}) != 1
                or len(set(offsets_x)) != 1 or len(set(offsets_y)) != 1):
            raise ValueError("0x13154 player idle changes flip or origin per frame")
        idle_codes.append(codes)
        idle_holds.append(holds)
        idle_flips.append(1 if descriptors[0] & 0x0020 else 0)
        idle_x.append(offsets_x[0])
    if idle_codes[0] != idle_codes[1] or idle_x != [6, 6]:
        raise ValueError("Unexpected directional 0x13154 player idle frames")
    lines.extend([
        "#define WILLOW_PLAYER_IDLE_FRAME_COUNT 6",
        "static const unsigned short willow_player_idle_codes[2][WILLOW_PLAYER_IDLE_FRAME_COUNT] = {",
    ])
    for row in idle_codes:
        lines.append("    { " + ", ".join(f"0x{v:04x}" for v in row) + " },")
    lines.extend([
        "};",
        "static const unsigned char willow_player_idle_holds[2][WILLOW_PLAYER_IDLE_FRAME_COUNT] = {",
    ])
    for row in idle_holds:
        lines.append("    { " + ", ".join(str(value) for value in row) + " },")
    lines.extend([
        "};",
        "static const unsigned char willow_player_idle_hflip[2] = { "
        + ", ".join(str(value) for value in idle_flips) + " };",
        "static const unsigned char willow_player_idle_width = 4;",
        "static const unsigned char willow_player_idle_height = 4;",
        "static const unsigned char willow_player_idle_x = 6;",
        "",
    ])

    pause_sequences = tables["enemy_state2_pause_1133a"]["sequences"][:2]
    pause_codes, pause_holds, pause_flips = [], [], []
    pause_origins_x, pause_origins_y = [], []
    for sequence in pause_sequences:
        if len(sequence["records"]) != 2:
            raise ValueError("Unexpected 0x1133a pause animation frame count")
        codes, holds, flips = [], [], set()
        origins_x, origins_y = set(), set()
        for index, record in enumerate(sequence["records"]):
            words = [int(word, 16) for word in record["words"]]
            descriptor = words[1]
            if (words[0] != 0x0010 or words[2] != 0x0016
                    or words[3] != 0
                    or descriptor not in (0x3310, 0x3330)
                    or int(record["timer"]) != (8 if index == 0 else 0xFFFF)
                    or words[5] != (0x0188 if index == 0 else 0x0D00)):
                raise ValueError("Unexpected 0x1133a pause animation frame")
            codes.append(words[5])
            holds.append(int(record["timer"]))
            flips.add(1 if descriptor & 0x0020 else 0)
            origins_x.add(words[2] & 0x7FFF)
            origins_y.add(words[3] - 0x10000 if words[3] & 0x8000 else words[3])
        if len(flips) != 1:
            raise ValueError("Mixed facing flags in a 0x1133a pause sequence")
        if len(origins_x) != 1 or len(origins_y) != 1:
            raise ValueError("Changing source origin in a 0x1133a pause sequence")
        pause_codes.append(codes)
        pause_holds.append(holds)
        pause_flips.append(flips.pop())
        pause_origins_x.append(origins_x.pop())
        pause_origins_y.append(origins_y.pop())
    if pause_origins_x != [22, 22] or pause_origins_y != [0, 0]:
        raise ValueError("Unexpected 0x1133a enemy pause origin")
    lines.extend([
        "#define WILLOW_ENEMY_PAUSE_FRAME_COUNT 2",
        "static const unsigned short willow_enemy_pause_codes[2][2] = {",
    ])
    for row in pause_codes:
        lines.append("    { " + ", ".join(f"0x{value:04x}" for value in row) + " },")
    lines.extend(["};", "static const unsigned short willow_enemy_pause_holds[2][2] = {"])
    for row in pause_holds:
        lines.append("    { " + ", ".join(str(value) for value in row) + " },")
    lines.extend([
        "};",
        "static const unsigned char willow_enemy_pause_hflip[2] = { "
        + ", ".join(str(value) for value in pause_flips) + " };",
        "static const unsigned char willow_enemy_pause_origin_x[2] = { "
        + ", ".join(str(value) for value in pause_origins_x) + " };",
        "static const signed char willow_enemy_pause_origin_y[2] = { "
        + ", ".join(str(value) for value in pause_origins_y) + " };",
        "static const unsigned char willow_enemy_pause_width = 4;",
        "static const unsigned char willow_enemy_pause_height = 4;",
        "",
    ])

    # State-2 substate zero at 0x13768 selects the facing-specific entry
    # frame before the eight-tick range sequence. Keep the record bytes and
    # call path tied to the verified Stage 0 player handler.
    range_sequences = tables["player_range_entry_131b0"]["sequences"][:2]
    range_codes, range_origins_x, range_flips = [], [], []
    for index, sequence in enumerate(range_sequences):
        records = sequence["records"]
        if len(records) != 1:
            raise ValueError("Unexpected 0x131b0 player entry record count")
        words = [int(word, 16) for word in records[0]["words"]]
        descriptor = words[1]
        if (int(records[0]["timer"]) != 0xFFFF
                or descriptor not in (0x3311, 0x3331)
                or words[2] != 0x801E or words[3] != 0
                or words[5] != 0x0F84
                or int(records[0]["length"]) != 12):
            raise ValueError("Unexpected 0x131b0 player entry frame")
        expected_descriptor = (0x3311, 0x3331)[index]
        if descriptor != expected_descriptor:
            raise ValueError("Unexpected 0x131b0 facing sequence order")
        range_codes.append(words[5])
        range_origins_x.append(words[2] & 0x7FFF)
        range_flips.append(1 if descriptor & 0x20 else 0)
    lines.extend([
        "#define WILLOW_PLAYER_RANGE_ENTRY_SEQUENCE_COUNT 2",
        "static const unsigned short willow_player_range_entry_codes[2] = { "
        + ", ".join(f"0x{code:04x}" for code in range_codes) + " };",
        "static const unsigned char willow_player_range_entry_hflip[2] = { "
        + ", ".join(str(flip) for flip in range_flips) + " };",
        "static const unsigned char willow_player_range_entry_origin_x[2] = { "
        + ", ".join(str(origin) for origin in range_origins_x) + " };",
        "static const unsigned char willow_player_range_entry_width = 4;",
        "static const unsigned char willow_player_range_entry_height = 4;",
        "",
    ])

    # State-2 substate 8 uses the 0x13160 seven-record tail while its
    # enclosing state timer runs for 56 ticks. Six records advance at eight
    # ticks; the final FFFF record remains selected until the outer timer exits.
    tail_sequences = tables["source_table_013160"]["sequences"][:2]
    tail_codes, tail_widths, tail_heights, tail_origins, tail_flips = [], [], [], [], []
    for facing, sequence in enumerate(tail_sequences):
        records = sequence["records"]
        if len(records) != 7:
            raise ValueError("Unexpected 0x13160 player tail record count")
        expected_flip = facing == 1
        codes, widths, heights, origins, flips = [], [], [], [], []
        for frame, record in enumerate(records):
            words = [int(word, 16) for word in record["words"]]
            descriptor = words[1]
            expected_descriptor = (0x3311 if frame == 0 else 0x2311)
            if expected_flip:
                expected_descriptor |= 0x20
            expected_timer = 8 if frame < 6 else 0xFFFF
            if (descriptor != expected_descriptor
                    or int(record["timer"]) != expected_timer
                    or words[3] != 0
                    or int(record["length"]) != 12):
                raise ValueError("Unexpected 0x13160 player tail frame")
            codes.append(words[5])
            widths.append(((descriptor >> 8) & 0x0F) + 1)
            heights.append(((descriptor >> 12) & 0x0F) + 1)
            origins.append(words[2] & 0x7FFF)
            flips.append(1 if descriptor & 0x20 else 0)
        if facing == 0:
            tail_codes, tail_widths, tail_heights, tail_origins = (
                codes, widths, heights, origins)
        else:
            if (codes != tail_codes or widths != tail_widths
                    or heights != tail_heights or origins != tail_origins):
                raise ValueError("Unexpected 0x13160 facing graphics")
        tail_flips.append(flips)
    lines.extend([
        "#define WILLOW_PLAYER_RANGE_TAIL_FRAME_COUNT 7",
        "static const unsigned short willow_player_range_tail_codes[7] = { "
        + ", ".join(f"0x{code:04x}" for code in tail_codes) + " };",
        "static const unsigned char willow_player_range_tail_widths[7] = { "
        + ", ".join(str(value) for value in tail_widths) + " };",
        "static const unsigned char willow_player_range_tail_heights[7] = { "
        + ", ".join(str(value) for value in tail_heights) + " };",
        "static const unsigned char willow_player_range_tail_origin_x[7] = { "
        + ", ".join(str(value) for value in tail_origins) + " };",
        "static const unsigned char willow_player_range_tail_hflip[2] = { "
        + ", ".join(str(flips[0]) for flips in tail_flips) + " };",
        "",
    ])

    # The 0x13b3a frame stores thirteen independent sprite codes. Its origin
    # indexes the CPS sprite-coordinate template table rather than a block size.
    composite_sequences = tables["source_table_013b3a"]["sequences"][:2]
    composite_codes, composite_flips = [], []
    for facing, sequence in enumerate(composite_sequences):
        records = sequence["records"]
        if len(records) != 1:
            raise ValueError("Unexpected 0x13b3a composite record count")
        record = records[0]
        raw = bytes.fromhex(record["raw"])
        words = [int(word, 16) for word in record["words"]]
        expected_descriptor = 0x0011 | (0x20 if facing else 0)
        if (int(record["length"]) != 36
                or int(record["timer"]) != 0xFFFF
                or words[0] != 13
                or words[1] != expected_descriptor
                or words[2] != 0x0169 or words[3] != 0
                or len(raw) != 36):
            raise ValueError("Unexpected 0x13b3a composite frame header")
        codes = [int.from_bytes(raw[offset:offset + 2], "big")
                 for offset in range(10, 36, 2)]
        if facing == 0:
            composite_codes = codes
        elif codes != composite_codes:
            raise ValueError("Unexpected 0x13b3a facing code list")
        composite_flips.append(1 if words[1] & 0x20 else 0)
    composite_xy = [tuple(pair) for pair in catalog["range_composite_layout"]["coordinates"]]
    if len(composite_codes) != 13:
        raise ValueError("Unexpected 0x13b3a composite tile count")
    lines.extend([
        "#define WILLOW_PLAYER_RANGE_COMPOSITE_TILE_COUNT 13",
        "static const unsigned short willow_player_range_composite_codes[13] = {",
        "    " + ", ".join(f"0x{code:04x}" for code in composite_codes),
        "};",
        "static const unsigned char willow_player_range_composite_hflip[2] = { "
        + ", ".join(str(flip) for flip in composite_flips) + " };",
        "static const short willow_player_range_composite_x[13] = { "
        + ", ".join(str(x) for x, _ in composite_xy) + " };",
        "static const short willow_player_range_composite_y[13] = { "
        + ", ".join(str(y) for _, y in composite_xy) + " };",
        "static const unsigned short willow_player_range_composite_source_origin = 0x0169;",
        "",
    ])
    # Behavior 8, mode 1 uses actor byte +5 to select one of these ten
    # uncompressed CPS frame sequences. Preserve the raw descriptor/origin
    # words in the JSON and resolve each frame's first placement through the
    # validated CPS coordinate templates for the native Genesis renderer.
    phase_sequences = tables["stage0_post_phase_actor_15666"]["sequences"]
    if len(phase_sequences) != 10:
        raise ValueError("Unexpected Stage 0 post-phase actor mode count")
    phase_codes, phase_descriptors, phase_origins_x = [], [], []
    phase_origins_y, phase_holds, phase_color_groups = [], [], []
    for sequence in phase_sequences:
        records = sequence["records"]
        if sequence["termination"] != "loop" or len(records) != 7:
            raise ValueError("Unexpected Stage 0 post-phase actor animation length")
        fields = [[], [], [], [], [], []]
        for record in records:
            words = [int(word, 16) for word in record["words"]]
            if (int(record["length"]) != 12
                    or int(record["timer"]) not in (8, 16)):
                raise ValueError("Unexpected Stage 0 post-phase actor frame record")
            descriptor = words[1]
            width = ((descriptor >> 8) & 0x0F) + 1
            height = ((descriptor >> 12) & 0x0F) + 1
            if width > 4 or height > 4 or words[5] >= 0x8000:
                raise ValueError("Invalid Stage 0 post-phase actor frame bounds")
            fields[0].append(words[5])
            fields[1].append(descriptor)
            origin = record.get("resolved_origin")
            if origin is None or len(origin) != 2:
                raise ValueError("Missing resolved post-phase actor origin")
            if any(value < -128 or value > 127 for value in origin):
                raise ValueError("Post-phase actor origin exceeds native range")
            fields[2].append(origin[0])
            fields[3].append(origin[1])
            fields[4].append(int(record["timer"]))
            fields[5].append(int(record["source_color_group"]))
        phase_codes.append(fields[0])
        phase_descriptors.append(fields[1])
        phase_origins_x.append(fields[2])
        phase_origins_y.append(fields[3])
        phase_holds.append(fields[4])
        phase_color_groups.append(fields[5])
    if any(group != 8 for row in phase_color_groups for group in row):
        raise ValueError("Unexpected Stage 0 post-phase actor color group")
    lines.extend([
        "#define WILLOW_STAGE0_POST_PHASE_MODE_COUNT 10",
        "#define WILLOW_STAGE0_POST_PHASE_FRAME_COUNT 7",
        "#define WILLOW_STAGE0_POST_PHASE_COLOR_GROUP 8",
    ])
    for suffix, rows, ctype in (
            ("codes", phase_codes, "unsigned short"),
            ("descriptors", phase_descriptors, "unsigned short"),
            ("origin_x", phase_origins_x, "signed char"),
            ("origin_y", phase_origins_y, "signed char"),
            ("holds", phase_holds, "unsigned char")):
        lines.append(
            f"static const {ctype} willow_stage0_post_phase_{suffix}"
            "[WILLOW_STAGE0_POST_PHASE_MODE_COUNT]"
            "[WILLOW_STAGE0_POST_PHASE_FRAME_COUNT] = {")
        for row in rows:
            values = [f"0x{value:04x}" for value in row] if ctype == "unsigned short" else [str(value) for value in row]
            lines.append("    { " + ", ".join(values) + " },")
        lines.extend(["};", ""])
    actor_timing = catalog["stage0_post_phase_actor"]
    delays = actor_timing["delay_ticks"]
    if len(delays) != 10:
        raise ValueError("Unexpected post-phase actor delay table length")
    lines.extend([
        "static const unsigned char willow_stage0_post_phase_delays[10] = { "
        + ", ".join(str(value) for value in delays) + " };",
        "#define WILLOW_STAGE0_POST_PHASE_ACTOR_VELOCITY_X_8_8 -1536",
        "#define WILLOW_STAGE0_POST_PHASE_ACTOR_VELOCITY_Y_8_8 -512",
        "",
    ])
    class2_streams = catalog["class2_behavior0_streams"]["streams"]
    class2_frames = []
    for sequence in class2_streams:
        class2_frames.append([
            record for record in sequence["records"]
            if 0 < int(record["timer"]) <= 0x1000
        ])
    class2_max_frames = max(len(frames) for frames in class2_frames)
    if [len(frames) for frames in class2_frames] != [9, 8, 7, 7]:
        raise ValueError("Class-2 behavior-0 source animation frame counts changed")
    if any(int(record["source_color_group"]) != 8
            for frames in class2_frames for record in frames):
        raise ValueError("Unexpected class-2 behavior-0 source color group")
    class2_spawn = catalog["class2_behavior0_streams"]["spawn_setup"]
    spawn_offsets = class2_spawn["offsets"]
    spawn_delays = class2_spawn["delays"]
    if len(spawn_offsets) != 16 or len(spawn_delays) != 4:
        raise ValueError("Unexpected class-2 behavior-0 spawn setup size")
    lines.extend([
        "#define WILLOW_CLASS2_BEHAVIOR0_SEQUENCE_COUNT 4",
        f"#define WILLOW_CLASS2_BEHAVIOR0_FRAME_MAX {class2_max_frames}",
        "#define WILLOW_CLASS2_BEHAVIOR0_COLOR_GROUP 8",
        "static const signed char willow_class2_behavior0_spawn_offsets[16][2] = {",
    ])
    for x, y in spawn_offsets:
        lines.append(f"    {{ {x}, {y} }},")
    lines.extend([
        "};",
        "static const unsigned char willow_class2_behavior0_spawn_delays[4] = { "
        + ", ".join(str(value) for value in spawn_delays) + " };",
        "static const unsigned char willow_class2_behavior0_frame_count[4] = { "
        + ", ".join(str(len(frames)) for frames in class2_frames) + " };",
        "static const unsigned short willow_class2_behavior0_codes[4][WILLOW_CLASS2_BEHAVIOR0_FRAME_MAX] = {",
    ])
    for frames in class2_frames:
        codes = [int(record["words"][5], 16) for record in frames]
        codes.extend([0] * (class2_max_frames - len(codes)))
        lines.append("    { " + ", ".join(f"0x{value:04x}" for value in codes) + " },")
    lines.extend([
        "};",
        "static const unsigned char willow_class2_behavior0_holds[4][WILLOW_CLASS2_BEHAVIOR0_FRAME_MAX] = {",
    ])
    for frames in class2_frames:
        holds = [int(record["timer"]) for record in frames]
        holds.extend([0] * (class2_max_frames - len(holds)))
        lines.append("    { " + ", ".join(str(value) for value in holds) + " },")
    lines.extend(["};", ""])
    linked_actor = catalog["stage0_class12_linked_actor"]
    linked_frames = linked_actor["frames"]
    if (linked_actor["handler"] != "0x03257e"
            or linked_actor["state0_setup"] != "0x032592"
            or linked_actor["frame_count"] != 3):
        raise ValueError("Unexpected Stage 0 class-12 linked actor dispatch")
    linked_codes = [int(record["words"][5], 16) for record in linked_frames]
    linked_color_groups = [int(record["source_color_group"])
                           for record in linked_frames]
    linked_widths = [((int(record["words"][1], 16) >> 8) & 0x0F) + 1
                     for record in linked_frames]
    linked_heights = [((int(record["words"][1], 16) >> 12) & 0x0F) + 1
                      for record in linked_frames]
    linked_origins_x = [int(record["words"][2], 16) & 0x7FFF
                        for record in linked_frames]
    linked_origins_y = [int.from_bytes(bytes.fromhex(record["raw"])[6:8],
                                      "big", signed=True)
                        for record in linked_frames]
    linked_max_tiles = max(width * height for width, height
                           in zip(linked_widths, linked_heights))
    lines.extend([
        "#define WILLOW_STAGE0_CLASS12_LINKED_FRAME_COUNT 3",
        f"#define WILLOW_STAGE0_CLASS12_LINKED_FRAME_MAX_TILES {linked_max_tiles}",
        "static const unsigned short willow_stage0_class12_linked_codes[3] = { "
        + ", ".join(f"0x{value:04x}" for value in linked_codes) + " };",
        "static const unsigned char willow_stage0_class12_linked_color_groups[3] = { "
        + ", ".join(str(value) for value in linked_color_groups) + " };",
        "static const unsigned char willow_stage0_class12_linked_holds[3] = { "
        + ", ".join(str(int(record["timer"])) for record in linked_frames) + " };",
        "static const unsigned char willow_stage0_class12_linked_widths[3] = { "
        + ", ".join(str(value) for value in linked_widths) + " };",
        "static const unsigned char willow_stage0_class12_linked_heights[3] = { "
        + ", ".join(str(value) for value in linked_heights) + " };",
        "static const unsigned char willow_stage0_class12_linked_origin_x[3] = { "
        + ", ".join(str(value) for value in linked_origins_x) + " };",
        "static const signed char willow_stage0_class12_linked_origin_y[3] = { "
        + ", ".join(str(value) for value in linked_origins_y) + " };",
        "",
    ])
    class8_child = catalog["class8_primary_child_animation"]
    lines.extend([
        "#define WILLOW_CLASS8_CHILD_SEQUENCE_COUNT 2",
        f"#define WILLOW_CLASS8_CHILD_FRAME_COUNT {class8_child['frame_count']}",
    ])
    for field, ctype in (("code", "unsigned short"), ("hold", "unsigned char"),
                         ("width", "unsigned char"), ("height", "unsigned char"),
                         ("hflip", "unsigned char"), ("origin_x", "signed char"),
                         ("origin_y", "signed char")):
        lines.append(
            f"static const {ctype} willow_class8_child_{field}[2][WILLOW_CLASS8_CHILD_FRAME_COUNT] = {{"
        )
        for frames in class8_child["frames"]:
            values = [frame[field] for frame in frames]
            rendered = [f"0x{value:04x}" for value in values] if field == "code" else [str(value) for value in values]
            lines.append("    { " + ", ".join(rendered) + " },")
        lines.append("};")
    lines.append("")
    for field, ctype in (("code", "unsigned short"), ("hold", "unsigned char"),
                         ("width", "unsigned char"), ("height", "unsigned char"),
                         ("hflip", "unsigned char"), ("origin_x", "signed char"),
                         ("origin_y", "signed char")):
        lines.append(
            f"static const {ctype} willow_class8_child_impact_{field}[2][WILLOW_CLASS8_CHILD_FRAME_COUNT] = {{"
        )
        for frames in class8_child["impact_frames"]:
            values = [frame[field] for frame in frames]
            rendered = [f"0x{value:04x}" for value in values] if field == "code" else [str(value) for value in values]
            lines.append("    { " + ", ".join(rendered) + " },")
        lines.append("};")
    lines.append("")

    # Exact streams selected by the Stage 0 recovery-created class-14 actor.
    # +33=2/5 picks state-0 loops; state 4 adds 12 to that index when +47=1.
    queue14 = tables["queue14_state0_animations_2d314"]["sequences"]
    state0_frames = []
    state0_properties = []
    for index in (2, 5):
        sequence = queue14[index]
        if sequence["termination"] != "loop" or len(sequence["records"]) != 4:
            raise ValueError("Unexpected queue-class-14 state-0 animation loop")
        frames = []
        properties = []
        for record in sequence["records"]:
            words = [int(word, 16) for word in record["words"]]
            if (int(record["timer"]) != 4 or words[1] != 2
                    or words[3] != 0 or words[5] == 0):
                raise ValueError("Unexpected queue-class-14 state-0 frame")
            frames.append((words[5], int(record["timer"])))
            layout = catalog["queue14_coordinate_layouts"][
                str(words[2] & 0x7FFF)]
            properties.append((1, 1, 0, layout[0][0], layout[0][1]))
        state0_frames.append(frames)
        state0_properties.append(properties)
    state4_sequences = tables["queue14_state4_animations_2d32c"]["sequences"]
    state4_codes = []
    state4_properties = []
    state4_components = []
    for index in (2, 5, 14, 17):
        sequence = state4_sequences[index]
        if len(sequence["records"]) != 1:
            raise ValueError("Unexpected queue-class-14 state-4 pose sequence")
        record = sequence["records"][0]
        words = [int(word, 16) for word in record["words"]]
        if int(record["timer"]) != 0xFFFF or words[1] != 2:
            raise ValueError("Unexpected queue-class-14 state-4 pose record")
        state4_codes.append(words[5])
        layout = catalog["queue14_coordinate_layouts"][
            str(words[2] & 0x7FFF)]
        if words[0] > len(layout):
            raise ValueError("Queue-class-14 pose exceeds coordinate template")
        state4_properties.append((1, 1, 0, layout[0][0], layout[0][1]))
        component_count = words[0] - 1
        raw = bytes.fromhex(record["raw"])
        if component_count < 0 or len(raw) != 12 + component_count * 2:
            raise ValueError("Unexpected queue-class-14 state-4 component list")
        state4_components.append([
            int.from_bytes(raw[12 + component * 2:14 + component * 2], "big")
            for component in range(component_count)
        ])
    state4_component_positions = [
        catalog["queue14_coordinate_layouts"][
            str(int(state4_sequences[index]["records"][0]["words"][2], 16)
                & 0x7FFF)][:len(state4_components[position]) + 1][1:]
        for position, index in enumerate((2, 5, 14, 17))
    ]
    transition = catalog["queue14_state4_transition"]
    if (transition["termination"] != "self reference"
            or len(transition["records"]) != 3):
        raise ValueError("Unexpected queue-class-14 transition animation")
    transition_codes, transition_holds = [], []
    transition_descriptors = []
    transition_origins_x, transition_origins_y = [], []
    for index, record in enumerate(transition["records"]):
        words = [int(word, 16) for word in record["words"]]
        expected_descriptor = 0x0102 if index == 0 else 0x1102
        if int(record["timer"]) != 4 or words[1] != expected_descriptor:
            raise ValueError("Unexpected queue-class-14 transition frame")
        transition_codes.append(words[5])
        transition_holds.append(int(record["timer"]))
        transition_descriptors.append(words[1])
        layout = catalog["queue14_coordinate_layouts"][str(words[2] & 0x7FFF)]
        transition_origins_x.append(layout[0][0])
        transition_origins_y.append(layout[0][1])
    lines.extend([
        "#define WILLOW_CLASS14_STATE0_FRAME_COUNT 4",
        "static const unsigned short willow_class14_state0_codes[2][WILLOW_CLASS14_STATE0_FRAME_COUNT] = {",
    ])
    for frames in state0_frames:
        lines.append("    { " + ", ".join(f"0x{code:04x}" for code, _ in frames) + " },")
    lines.extend([
        "};",
        "static const unsigned char willow_class14_state0_holds[2][WILLOW_CLASS14_STATE0_FRAME_COUNT] = {",
    ])
    for frames in state0_frames:
        lines.append("    { " + ", ".join(str(hold) for _, hold in frames) + " },")
    lines.extend([
        "};",
        "static const unsigned char willow_class14_state0_widths[2][WILLOW_CLASS14_STATE0_FRAME_COUNT] = {",
    ])
    for properties in state0_properties:
        lines.append("    { " + ", ".join(str(item[0]) for item in properties) + " },")
    lines.extend([
        "};",
        "static const unsigned char willow_class14_state0_heights[2][WILLOW_CLASS14_STATE0_FRAME_COUNT] = {",
    ])
    for properties in state0_properties:
        lines.append("    { " + ", ".join(str(item[1]) for item in properties) + " },")
    lines.extend([
        "};",
        "static const signed short willow_class14_state0_origin_x[2][WILLOW_CLASS14_STATE0_FRAME_COUNT] = {",
    ])
    for properties in state0_properties:
        lines.append("    { " + ", ".join(str(item[3]) for item in properties) + " },")
    lines.extend([
        "};",
        "static const signed short willow_class14_state0_origin_y[2][WILLOW_CLASS14_STATE0_FRAME_COUNT] = {",
    ])
    for properties in state0_properties:
        lines.append("    { " + ", ".join(str(item[4]) for item in properties) + " },")
    lines.extend([
        "};",
        "static const unsigned short willow_class14_state4_codes[2][2] = {",
        "    { " + ", ".join(f"0x{code:04x}" for code in state4_codes[:2]) + " },",
        "    { " + ", ".join(f"0x{code:04x}" for code in state4_codes[2:]) + " },",
        "};",
        "static const unsigned char willow_class14_state4_component_counts[2][2] = {",
        "    { " + ", ".join(str(len(state4_components[index])) for index in (0, 1)) + " },",
        "    { " + ", ".join(str(len(state4_components[index])) for index in (2, 3)) + " },",
        "};",
        "static const unsigned short willow_class14_state4_component_codes[2][2][2] = {",
    ])
    for components in (state4_components[:2], state4_components[2:]):
        lines.append("    {")
        for codes in components:
            lines.append("        { " + ", ".join(
                f"0x{code:04x}" for code in (codes + [0] * (2 - len(codes)))
            ) + " },")
        lines.append("    },")
    lines.extend([
        "};",
        "static const signed short willow_class14_state4_component_x[2][2][2] = {",
    ])
    for positions in (state4_component_positions[:2], state4_component_positions[2:]):
        lines.append("    {")
        for points in positions:
            lines.append("        { " + ", ".join(str(point[0]) for point in
                (points + [[0, 0]] * (2 - len(points)))) + " },")
        lines.append("    },")
    lines.extend([
        "};",
        "static const signed short willow_class14_state4_component_y[2][2][2] = {",
    ])
    for positions in (state4_component_positions[:2], state4_component_positions[2:]):
        lines.append("    {")
        for points in positions:
            lines.append("        { " + ", ".join(str(point[1]) for point in
                (points + [[0, 0]] * (2 - len(points)))) + " },")
        lines.append("    },")
    lines.extend([
        "};",
        "static const signed short willow_class14_state4_origin_x[2][2] = {",
        "    { " + ", ".join(str(item[3]) for item in state4_properties[:2]) + " },",
        "    { " + ", ".join(str(item[3]) for item in state4_properties[2:]) + " },",
        "};",
        "static const signed short willow_class14_state4_origin_y[2][2] = {",
        "    { " + ", ".join(str(item[4]) for item in state4_properties[:2]) + " },",
        "    { " + ", ".join(str(item[4]) for item in state4_properties[2:]) + " },",
        "};",
        "static const unsigned short willow_class14_transition_codes[3] = { "
        + ", ".join(f"0x{code:04x}" for code in transition_codes) + " };",
        "static const unsigned char willow_class14_transition_holds[3] = { "
        + ", ".join(str(hold) for hold in transition_holds) + " };",
        "static const unsigned char willow_class14_transition_widths[3] = { "
        + ", ".join(str(((value >> 8) & 15) + 1)
                     for value in transition_descriptors) + " };",
        "static const unsigned char willow_class14_transition_heights[3] = { "
        + ", ".join(str(((value >> 12) & 15) + 1)
                     for value in transition_descriptors) + " };",
        "static const signed short willow_class14_transition_origin_x[3] = { "
        + ", ".join(str(value) for value in transition_origins_x) + " };",
        "static const signed short willow_class14_transition_origin_y[3] = { "
        + ", ".join(str(value) for value in transition_origins_y) + " };",
        "",
    ])
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", nargs="?", type=Path, default=Path("willow.zip"))
    parser.add_argument("--output", type=Path, default=Path("build/willow_animations.json"))
    parser.add_argument("--header", type=Path)
    args = parser.parse_args()
    try:
        data = assemble_maincpu(args.archive)
    except (OSError, zipfile.BadZipFile, KeyError, ValueError) as exc:
        parser.error(str(exc))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    catalog = build_catalog(data)
    args.output.write_text(json.dumps(catalog, indent=2) + "\n", encoding="utf-8")
    if args.header:
        args.header.parent.mkdir(parents=True, exist_ok=True)
        args.header.write_text(build_walk_header(catalog), encoding="utf-8")
    print(f"Wrote raw animation sequence catalog to {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
