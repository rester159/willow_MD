#!/usr/bin/env python3
"""Catalog immediate object allocations in Willow's offline 68000 image.

This is a static ROM-data aid for identifying stage spawn scripts. It does not
execute or include arcade program code in the Genesis build.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from cps1_program import assemble_maincpu

ALLOC = b"\x4e\xb8\x2b\x9c"  # jsr $2b9c: pop one reusable object
SCAN_LIMIT = 96
ROM_LIMIT = 0x100000
CLUSTER_GAP = 16
OBJECT_DISPATCH = 0xFFA2
HANDLER_TABLE = 0xFFC8
OBJECT_RECORD_COUNT = 50  # d7 starts at 49 in the dispatcher loop.
HANDLER_COUNT = 35  # The table ends immediately before code at 0x10054.
HANDLER_TABLE_END = 0x10054
CLASS2_AI_DISPATCH = 0xFB48
CLASS2_AI_TABLE = 0xFB70
CLASS2_AI_RECORD_COUNT = 20  # d7 starts at 19 in the dispatcher loop.
STAGE0_HIGH_SCORE_DEFAULT = 0x183C
STAGE0_HIGH_SCORE_DEFAULT_BYTES = bytes.fromhex("00055000")
SOURCE_SCORE_AWARD_TABLE = 0x4E74
SOURCE_SCORE_AWARD_TABLE_BYTES = bytes.fromhex(
    "000000050000001000000020000000500000010000000200"
    "0000050000001000000020000000500000010000"
)


def signed16(value: int) -> int:
    return value - 0x10000 if value & 0x8000 else value


def stage0_high_score_header(data: bytes) -> str:
    """Export source-validated Stage 0 score and charge timing constants."""
    rng_seed = int(stage_rng_initialization(data)["stage0_lfsr_seed"], 16)
    raw = data[STAGE0_HIGH_SCORE_DEFAULT:
               STAGE0_HIGH_SCORE_DEFAULT + len(STAGE0_HIGH_SCORE_DEFAULT_BYTES)]
    if raw != STAGE0_HIGH_SCORE_DEFAULT_BYTES:
        raise ValueError("Stage 0 high-score default at 0x183c changed")
    value = 0
    for byte in raw:
        for digit in (byte >> 4, byte & 0x0F):
            if digit > 9:
                raise ValueError("Stage 0 high-score default is not packed BCD")
            value = value * 10 + digit
    score_table = data[SOURCE_SCORE_AWARD_TABLE:
                       SOURCE_SCORE_AWARD_TABLE + len(SOURCE_SCORE_AWARD_TABLE_BYTES)]
    if score_table != SOURCE_SCORE_AWARD_TABLE_BYTES:
        raise ValueError("Shared score-award BCD table at 0x4e74 changed")
    score_awards = []
    for offset in range(0, len(score_table), 4):
        award = 0
        for byte in score_table[offset:offset + 4]:
            for digit in (byte >> 4, byte & 0x0F):
                if digit > 9:
                    raise ValueError("Score-award table is not packed BCD")
                award = award * 10 + digit
        score_awards.append(award)
    charge_entry = data[0xC610:0xC626]
    expected_charge_entry = bytes.fromhex(
        "102d8e9208000004671c1d7c0001003f1d7c000a0044"
    )
    if charge_entry != expected_charge_entry:
        raise ValueError("Stage 0 charge entry timer instruction at 0xc610 changed")
    charge_timer = int.from_bytes(data[0xC622:0xC624], "big")
    charge_counter_code = data[0xC658:0xC66C]
    expected_charge_counter_code = bytes.fromhex(
        "0c2e007f00316700004c522e00310c2e00100031"
    )
    if charge_counter_code != expected_charge_counter_code:
        raise ValueError("Stage 0 charge counter limits at 0xc658 changed")
    charge_limit = int.from_bytes(data[0xC65A:0xC65C], "big")
    charge_threshold = int.from_bytes(data[0xC668:0xC66A], "big")
    charge_reload = data[0xC706:0xC70E]
    expected_charge_reload = bytes.fromhex("0a06050403020201")
    if charge_reload != expected_charge_reload:
        raise ValueError("Stage 0 charge spawn reload table at 0xc706 changed")
    charge_reload_values = list(charge_reload)
    burst_entry = data[0xCE02:0xCE08]
    expected_burst_entry = bytes.fromhex("1d7c00030022")
    if burst_entry != expected_burst_entry:
        raise ValueError("Stage 0 burst startup timer instruction at 0xce02 changed")
    burst_countdown = data[0xCE18:0xCE32]
    expected_burst_countdown = bytes.fromhex(
        "532e00226610022e0001000f3d7c0700004a542e00044ef81f4c"
    )
    if burst_countdown != expected_burst_countdown:
        raise ValueError("Stage 0 burst startup countdown path at 0xce18 changed")
    burst_startup = int.from_bytes(data[0xCE04:0xCE06], "big")
    state4_selectors = data[0x13120:0x13130]
    expected_state4_selectors = bytes.fromhex(
        "01020101010101010101010101020101"
    )
    if state4_selectors != expected_state4_selectors:
        raise ValueError("State-4 child selector table at 0x13120 changed")
    return (
        "/* Generated from Willow's source score, charge, burst, and RNG data. */\n"
        "#ifndef WILLOW_SCORE_H\n#define WILLOW_SCORE_H\n"
        f"#define WILLOW_STAGE0_HIGH_SCORE_INITIAL {value}UL\n"
        f"#define WILLOW_STAGE0_LFSR_SEED 0x{rng_seed:04x}\n"
        f"#define WILLOW_STAGE0_CHARGE_SPAWN_INITIAL_TIMER {charge_timer}\n"
        f"#define WILLOW_STAGE0_CHARGE_COUNTER_LIMIT {charge_limit}\n"
        f"#define WILLOW_STAGE0_CHARGE_ATTACK_THRESHOLD {charge_threshold}\n"
        f"#define WILLOW_STAGE0_BURST_STARTUP_TICKS {burst_startup}\n"
        "#define WILLOW_SOURCE_SCORE_MAX 99999999UL\n"
        "static const unsigned long willow_source_score_awards[11] = { "
        + ", ".join(f"{item}UL" for item in score_awards)
        + " };\n"
        "static const unsigned char willow_stage0_charge_spawn_reload[8] = { "
        + ", ".join(str(item) for item in charge_reload_values)
        + " };\n"
        "static const unsigned char willow_state4_child_descriptor_indices[16] = { "
        + ", ".join(str(item) for item in state4_selectors)
        + " };\n"
        "#endif\n"
    )


def immediate_write(data: bytes, offset: int) -> tuple[str, int, int] | None:
    """Decode the small immediate writes used by observed object setup code."""
    if offset + 6 <= len(data) and data[offset:offset + 2] == b"\x19\x7c":
        value = int.from_bytes(data[offset + 2:offset + 4], "big")
        target = int.from_bytes(data[offset + 4:offset + 6], "big")
        names = {
            0x0000: "active_type",
            0x0002: "state",
            0x0003: "substate",
            0x0004: "mode",
            0x001E: "queue_class",
            0x0020: "field_32",
            0x0021: "byte_33",
            0x0022: "timer",
            0x0024: "interaction_flags",
            0x0028: "mode_flags",
            0x0029: "aux_flags",
            0x002C: "velocity_x",
            0x002E: "velocity_y",
            0x0030: "parameters",
            0x0031: "parameters_1",
            0x0032: "parameters_2",
        }
        if target in names:
            return names[target], value & 0xFF, 6
    if offset + 6 <= len(data) and data[offset:offset + 2] == b"\x39\x7c":
        value = int.from_bytes(data[offset + 2:offset + 4], "big")
        target = int.from_bytes(data[offset + 4:offset + 6], "big")
        names = {0x0006: "x", 0x000A: "y"}
        if target in names:
            return names[target], signed16(value), 6
    return None


def charge_release_trace(data: bytes) -> dict[str, object]:
    """Validate the Stage 0 charged-release child and its opening states."""
    animation_table_sites = {
        "startup": (0x116E8, bytes.fromhex("41fa08de4ef81f30"), "0x0011fc8"),
        "patrol": (0x117E0, bytes.fromhex(
            "542e000341fa07ea4eb81f3061000094"), "0x0011fd0"),
        "state6_wait": (0x11986, bytes.fromhex("41fa064c4ef81f30"), "0x0011fd4"),
        "voice_followup": (0x119D4, bytes.fromhex("41fa060e4ef81f30"), "0x0011fe4"),
        "approach": (0x11A26, bytes.fromhex(
            "2d7c000ed0880018542e000441fa05984ef81f30"), "0x0011fcc"),
    }
    for name, (address, expected, _) in animation_table_sites.items():
        if data[address:address + len(expected)] != expected:
            raise ValueError(
                f"profile-1 {name} animation-table transition changed"
            )
    animation_table_transitions = [
        {
            "table": table,
            "install_address": f"0x{address:06x}",
            "validated_bytes": expected.hex(),
            "phase": name,
        }
        for name, (address, expected, table) in animation_table_sites.items()
    ]
    checks = {
        "stage_mode_setup": (0x8D74, bytes.fromhex(
            "70011b40038a1b40038b1b400366")),
        "charge_behavior_dispatch": (0xCB24, bytes.fromhex(
            "7000102d8e945200e648123b00d8")),
        "primary_child_allocation": (0xCBA6, bytes.fromhex(
            "4eb82b04673c522d799d197c00010000"
            "196e000f000f022c0001000f396e00060006"
            "302e000a4a2e003e6604064000083940000a"
            "61000066196d8e93002160000042")),
        "linked_child_allocation": (0xCC28, bytes.fromhex(
            "4eb829d06712197c00010000197c00010020"
            "196e000f000f4e75")),
        "linked_child_class2_behavior_dispatch": (0xFB48, bytes.fromhex(
            "4ded18c63e3c00134a2e0000670000107000"
            "102e0020e548207b000e4e904dee004051cfffe4"
            "4e750000fb7c0000fe3e0000ff14")),
        "linked_child_behavior1_state0": (0xFE3E, bytes.fromhex(
            "102e0002323b00064efb100200080022fe48fe48"
            "1d7c00080022542e000241fa004a102e000f"
            "024000014ef81f34")),
        "linked_child_behavior1_state2": (0xFE6C, bytes.fromhex(
            "4eb81f4c532e00226604542e0002302d184c"
            "06400010082e0000000f6704044000203d400006"
            "302d18504a2d18846604064000083d40000a"
            "4ef82244")),
        "linked_child_behavior1_cleanup": (0xFC92,
                                           bytes.fromhex("4ef829ec")),
        "linked_child_behavior1_animation_streams": (0xFEA8,
            bytes.fromhex(
                "00040038"
                "000411080001000000040280000411080001000000040282"
                "0004110800010000000402840004110880010000ffff0286"
                "0000feac"
                "000411280001000000040280000411280001000000040282"
                "0004112800010000000402840004112880010000ffff0286"
                "0000fee0")),
        "linked_class2_pool_init": (0x293E, bytes.fromhex(
            "41ed112a4ded18c61b7c001417fe303c00137200224e3e3c000f"
            "22c151cffffc1d7c0002001e310e4dee004051c8ffe63b481816"
            "41f8937e3b48181841f893a63b48181a1b4117ff1b4118004e75")),
        "linked_class2_pool_alloc": (0x29D0, bytes.fromhex(
            "4a2d17fe6712306d181638583b481816532d17fe70014e75")),
        "stage0_linked_actor_spawn": (0xA28A, bytes.fromhex(
            "4eb82b9c197c00020000197c00010020"
            "397c00da0006397c0088000a4eb82b9c"
            "197c00010000197c00030020397c00a00006"
            "397c0050000a4e75")),
        "stage0_actor_score_work_event_handler": (0x6274, bytes.fromhex(
            "122d029366000084303b00d041fb00cc43ed034e"
            "222d034a023c00efc308c308c308c30864000006")),
        "stage0_actor_score_parameter_two": (0x6250, bytes.fromhex(
            "0102")),
        "stage0_actor_score_increment_100_bcd": (0x634C, bytes.fromhex(
            "00000100")),
        "stage0_actor_score_display_positions": (0x62A0, bytes.fromhex(
            "207c009102084a2d02926706207c0091138845ed034a7400")),
        "stage0_actor_high_score_display_position": (0x62CA, bytes.fromhex(
            "207c00910a8845ed703e7400101a")),
        "stage0_actor_high_score_default": (0x183C, bytes.fromhex(
            "00055000")),
        "stage0_actor_work_event_enqueue": (0x1F14, bytes.fromhex(
            "49ed0072342d001a3980200039812002"
            "5842024200fc3b42001a4e75")),
        "stage0_actor_work_event_dispatch": (0x1E24, bytes.fromhex(
            "41ed0072342d001c203020006a8460a8")),
        "stage0_actor_work_event_class18_target": (0x1E4C, bytes.fromhex(
            "0000626a")),
        "stage0_actor_work_event_1802_handler": (0x626A, bytes.fromhex(
            "3028001061044ef8165a")),
        "stage0_actor_state4_dispatch": (0x11CA6, bytes.fromhex(
            "102e0003323b000a4ebb10064ef82244")),
        "stage0_actor_state4_substate_table": (0x11CB6, bytes.fromhex(
            "000c0026007c00a000ea0108")),
        "stage0_actor_substate6_entry": (0x11D1E, bytes.fromhex(
            "1d7c00060003426e002c4e75")),
        "stage0_actor_substate6_recovery": (0x11D56, bytes.fromhex(
            "302e00366100fd6e0c6e0700002c")),
        "stage0_actor_recovery_cull_gate": (0x11D8C, bytes.fromhex(
            "102d0019d00754000200000366044ef821624e75")),
        "stage0_actor_cull_window": (0x2162, bytes.fromhex(
            "302e0006906d0408064000300c4001e06216"
            "302e000a906d040c064000200c4001406204"
            "70004e751d7c0006000270004e75")),
        "stage0_actor_substate8_setup": (0x11DA0, bytes.fromhex(
            "303c00334eb8786a4eb84908542e0003"
            "1d7c0008002241fa02344ef81f30")),
        "stage0_actor_substate10_exit": (0x11DBE, bytes.fromhex(
            "532e0022661c303c18024eb81f14542e0002"
            "4eb81d8e0240000f103b00084ef825084e75")),
        "stage0_object_pool_class4": (0x2660, bytes.fromhex(
            "1b7c00321801303c00317200224e3e3c001f"
            "22c151cffffc1d7c0004001e310e4dee0080")),
        "stage0_object_pool_zero_initialize": (0x266C, bytes.fromhex(
            "224e3e3c001f22c151cffffc1d7c0004001e"
            "310e4dee008051c8ffe6")),
        "behavior_mode_dispatch": (0x115EE, bytes.fromhex(
            "102e0021d040323b00064efb1002")),
        "behavior_mode_table": (0x115FC, bytes.fromhex(
            "0008001c00300044")),
        "stage0_actor_state_dispatch": (0x11604, bytes.fromhex(
            "102e0002323b00064efb10020044012806960926")),
        "stage0_object_dispatch_slot_order": (0xFFA2, bytes.fromhex(
            "4ded1dc63e3c00314a2e0000670e7000102e0020e548"
            "207b000e4e904dee008051cfffe64e75")),
        "stage0_actor_state0_initialize": (0x11654, bytes.fromhex(
            "3d7c0202002a1d7c0002000e1d7c00040013422e0038"
            "302e0006064000803d400030302e00060640ff803d400032"
            "2d7c000ed0700018542e00024eb81d8e0240000f"
            "1d7b000600344e75")),
        "stage0_actor_lfsr_delay_table": (0x1169E,
            bytes.fromhex("7878b4f0b478f0b4f0b4b4f0b4f078b4")),
        "stage0_actor_state2_startup": (0x116BE, bytes.fromhex(
            "61ce3d7c0202002a1d7c0002000e1d7c00040013"
            "1d7c00180022422e00382d7c000ed0700018542e0003"
            "41fa08de4ef81f30")),
        "stage0_actor_state2_startup_timer": (0x116F0, bytes.fromhex(
            "4eb81f4c4eb82244532e00226614516e0006542e0002"
            "1d7c000200031d7c000100004e75")),
        "stage0_actor_state4_delay": (0x11D32, bytes.fromhex(
            "532e0022661c1d7c000100002d7c000ed0700018"
            "1d7c00020002422e0003422e00044e75")),
        "stage0_actor_state4_tail_setup": (0x11D0C, bytes.fromhex(
            "542e00031d7c0006002241fa02d84ef81f30")),
        "stage0_player_world_position": (0xA128, bytes.fromhex(
            "3b7c00c004083b7c0200040c3b7c00c0"
            "04883b7c0200048c")),
        "player_class10_effect_spawn_a": (0x12DFA, bytes.fromhex(
            "303c00344eb8786c4eb82b4e673c197c00010000"
            "197c00000020196e0030000f396e00060006396e000a000a")),
        "player_class10_effect_trigger_a": (0x12CB0, bytes.fromhex(
            "102e0050d040323b00064efb100200040010206e0026"
            "4a28003c66024e75542e00031d7c0030002261000120")),
        "player_class10_effect_spawn_b": (0x138BA, bytes.fromhex(
            "303c00344eb8786c4eb82b4e674e197c00010000"
            "197c00000020102e000fe70858001940000f"
            "396e00060006396e000a000a")),
        "player_class10_effect_trigger_b": (0x1383C, bytes.fromhex(
            "532e002266f8542e00031d7c001000226100006c"
            "41fa02e84ef81f30")),
        "player_state6_class10_effect_spawn": (0x129FC, bytes.fromhex(
            "0c2e00060021671a542e00041d7c00100022"
            "610003ea102e003041fa07744ef81f34")),
        "player_class10_effect_range_trigger": (0x13762, bytes.fromhex(
            "0006005000703c2e000a9c6d040c0c460090621e"
            "542e00031d7c00080022610001261d40000f"
            "6100013041fafa22")),
        "stage_counter_three_actor_branch": (0xA0F4, bytes.fromhex(
            "0c6d0003034466084a2d03a0670000484a2d034866000116"
            "4a6d03446600010e4eb82dfe546d00083b7c00c08fb03b7c"
            "02808fb23b7c00c004083b7c0200040c3b7c00c004883b7c"
            "0200048c3b7c0007000a4ef878b81b7c000103a0422d0348"
            "3b7c001a0008426d000c4eb82dfe3b7c00c08fb03b7c1050"
            "8fb23b7c00c004083b7c1000040c3b7c00c004883b7c1000"
            "048c3b7c01c0184c1b7c001218483b7c0007000a4eb82b9c"
            "197c00020000197c00070020397cffa00006397c1068000a"
            "6000ad8e")),
        "stage_counter_four_allocations": (0xA0EA, bytes.fromhex(
            "0c6d00040344670000c60c6d0003034466084a2d03a067000048")),
        "stage_counter_four_spawn_sequence": (0xA1B8, bytes.fromhex(
            "1b7c000103a3422d799c3b7c0007000a3b7c00200008426d000c"
            "422d8e6e4eb82b9c197c00020000197c002000204eb82b9c"
            "197c00020000197c00220020397c00700006397c0048000a"
            "4eb82b9c197c00020000197c00210020397c01100006"
            "397c0050000a4e75")),
        "shared_object_scheduler_window": (0x2244, bytes.fromhex(
            "4a2e00006736422e0001302e0006906d0408"
            "064000200c4001c06220302e000a906d040c"
            "0c4001106212")),
        "profile1_patrol_bound_initialization": (0x11660, bytes.fromhex(
            "1d7c00040013422e0038302e000606400080"
            "3d400030302e00060640ff803d400032")),
        "mode1_initial_state": (0x115FC, bytes.fromhex(
            "0008001c00300044")),
        "state0_entry_dispatch": (0x116AE, bytes.fromhex(
            "102e0003323b00064efb100200040036")),
        "startup_transition": (0x116BE, bytes.fromhex(
            "61ce3d7c0202002a1d7c0002000e"
            "1d7c000400131d7c00180022422e0038"
            "2d7c000ed0700018542e000341fa08de4ef81f30")),
        "startup_complete": (0x116F0, bytes.fromhex(
            "4eb81f4c4eb82244532e00226614516e0006"
            "542e00021d7c000200031d7c000100004e75")),
        "primary_child_pool_init": (0x26F2, bytes.fromhex(
            "41ed12164ded42c61b7c00141807303c0013"
            "224e3e3c000f22c151cffffc1d7c0008001e"
            "310e4dee004051c8ffe63b481828")),
        "primary_child_allocator": (0x2B04, bytes.fromhex(
            "4a2d18076712306d182838583b481828532d1807"
            "70014e7570004e75")),
        "primary_child_class8_dispatch": (0xE6F4, bytes.fromhex(
            "4ded42c63e3c00134a2e0000670000107000"
            "102e0020e548207b000e4e904dee004051cfffe4")),
        "primary_child_class8_entry": (0xEC84, bytes.fromhex(
            "102e0002323b00064efb1002000800ec0118"
            "01621d7c00400022542e0002303c00204eb8786a")),
        "primary_child_class8_vector_table": (0xED0C, bytes.fromhex(
            "04000000fc00000004400000fbc0000004800000"
            "fb80000004c00000fb40000005000000fb000000"
            "05400000fac0000005800000fa800000")),
        "primary_child_class8_state2": (0xED7C, bytes.fromhex(
            "532e002266061d7c00060002102d0019d0070200"
            "000366044eb822124eb8208c4eb81f4c4eb83744"
            "4ef82244")),
        "primary_child_class8_state6": (0xEDF2, bytes.fromhex(
            "532d799d4ef82b20")),
        "primary_child_class8_collision": (0x3744, bytes.fromhex(
            "302e00063b408a32322e000a3b418a34610006d261000ba0"
            "67121d7c000200001d7c000400021d7c000200034e75"
            "45fa01024a2e0020670445fa01387000102e0047e948"
            "45f20000302e0006d0523b408a32322e000ad26a0002"
            "3b418a346100068461000b5e6666302e0006d06a0004"
            "3b408a32322e000ad26a00063b418a346100066261000b3c"
            "664445ea0008302e0006d0523b408a32322e000ad26a0002"
            "3b418a346100063e61000bce6620302e0006d06a0004"
            "3b408a32322e000ad26a00063b418a346100061c60000bac")),
        "primary_child_class8_collision_offsets": (0x38B6,
            bytes.fromhex("0010fff000100000fff0fff0fff00000")),
        "primary_child_class8_impact_state": (0xEDD2, bytes.fromhex(
            "1d7c000c0022542e000341fa01da4ef81f30")),
        "primary_child_class8_impact_wait": (0xEDE4, bytes.fromhex(
            "532e00226604542e00024ef81f4c")),
        "primary_child_free": (0x2B20, bytes.fromhex(
            "70002d4000003d4000042d40000e3d400012"
            "3d4000201d40001f1d400024306d1828")),
        "walk_state_dispatch": (0x117E0, bytes.fromhex(
            "542e000341fa07ea4eb81f3061000094663e4eb820b4"
            "4eb81f4c102d0019d0070240000366146100004a670e"
            "1d7c00040003422e0035422e0004532e0034660e6100"
            "fe6c1d7c00060003422e00044e75")),
        "patrol_bounds": (0x11852, bytes.fromhex(
            "302e00064a2e000f660ab06e00306f0c70014e75"
            "b06e00326d00fff670004e75")),
        "proximity_window": (0x11882, bytes.fromhex(
            "102d0019d007024000036626302e000a906d1850"
            "064000280c4000306214302d184cb06e00306e0a"
            "b06e00326d0470014e7570004e75")),
        "proximity_transition": (0x11830, bytes.fromhex(
            "422e0004613cb02e000f670e1d7c00080035"
            "1d7c000400034e751d7c000800034e75")),
        "patrol_edge_return": (0x1180C, bytes.fromhex(
            "1d7c00040003422e0035422e0004")),
        "proximity_direction_target": (0x11872, bytes.fromhex(
            "7001322e0006b26d184c6e0270004e75")),
        "approach_followup_proximity": (0x118B8, bytes.fromhex(
            "302e000a906d1850064000480c40005062ea"
            "0c40003062064a2d186a66de302d184cb06e0030"
            "6ed4b06e00326dce70014e75")),
        "approach_contact_target_gate": (0x11A78, bytes.fromhex(
            "302e0006906d184c064000080c400010620a"
            "1d7c000a0003422e00044e75")),
        "approach_bound_return": (0x11A56, bytes.fromhex(
            "1d7c000400031d7c00080035422e00044e75")),
        "approach_vectors": (0xED088, bytes.fromhex(
            "03000000fd000000")),
        "player_heading_classifier": (0x20D2, bytes.fromhex(
            "302d184c322d185060181c3c0080906e0006e2164e75e51e"
            "1c3b60044e752060e0a0740076007c003601966e000a67dae216"
            "6a0244433400946e000667d6e2166a0244423203924267cc6302"
            "c543e216484361000026eb8b48430243001fe71e1c3b60100886"
            "00006604dc034e759c034e75")),
        "player_heading_bases": (0x2144, bytes.fromhex(
            "008101804140c0c1")),
        "player_heading_divider": (0x214C, bytes.fromhex(
            "7000781fd683d180b08265049082528351ccfff24e75")),
        "turn_entry_and_deceleration": (0x118FC, bytes.fromhex(
            "542e00043d7c0300003641fa06d0226e0018"
            "0c510300670e1d7c00100022542e000441fa06c0"
            "4ef81f30")),
        "turn_motion_and_return": (0x11926, bytes.fromhex(
            "4eb81f4c302e0036044000186b243d40003648c0"
            "e1804a2e000f6606d1ae00064e7591ae00064e75")),
        "contact_lunge_and_recovery": (0x11ABA, bytes.fromhex(
            "6100fd966624302e0036044000106b1a3d400036"
            "48c0e1804a2e000f6606d1ae00064e7591ae00064e75")),
        "linked_decel_terrain_exit": (0x11CDC, bytes.fromhex(
            "302e0036044000186b1a6100fde2102d0019d007"
            "0200000366064eb83b0a67224ef81f4c")),
        "linked_mode_zero_dispatch": (0x11604, bytes.fromhex(
            "102e0002323b00064efb1002")),
        "linked_mode_zero_state_table": (0x1164C, bytes.fromhex(
            "0902092c097408ea")),
        "linked_mode_zero_initialize": (0x11F4E, bytes.fromhex(
            "4a2d038b666c1d7c00020000422d8e96542e0002"
            "1d7c004800222d7c000ed078001841fa005a4ef81f30")),
        "linked_mode_zero_visibility_gate": (0x11F8E, bytes.fromhex(
            "4a2e00016704542e00034e75")),
        "linked_mode_zero_timed_state": (0x11F78, bytes.fromhex(
            "102e0003323b000a4ebb10064ef822440006001200"
            "2c4a2e00016704542e00034e75532e0022660c542e"
            "000341fa002e4ef81f304eb8208c4ef81f4c4a2d"
            "8e966704542e00024e75")),
        "linked_mode_zero_exit": (0x11FC0, bytes.fromhex(
            "1d7c000600024e75")),
        "linked_mode_one_global_dispatch_lock": (0x11738, bytes.fromhex(
            "4a6d03d2660c102e0003323b00204ebb101c61000024102d0019d007"
            "53000240003f66044eb821c6")),
        "global_display_transition_lock_lifecycle": (0x690C, bytes.fromhex(
            "4a2d03d667346b403b7c00ff1896546d00420c6d010000426512"
            "426d0042426d03d2422d03d63b7c00011896")),
        "stage_rng_seed_selection": (0x8CEC, bytes.fromhex(
            "302d8e5ae74843fb00d63b5900183b5902842051"
            "2b48006c546d0008")),
        "stage_rng_seed_records": (0x8CCA, bytes.fromhex(
            "7938f6970004800e4912a6490004820e")),
        "stage_rng_seed_selector_toggle": (0x8D54, bytes.fromhex(
            "526d8e5a026d00018e5a302d0344d040323b0006")),
        "linked_mode_zero_state_six": (0x11F36, bytes.fromhex(
            "4ef82a36")),
        "linked_mode_zero_vector_table": (0xED078, bytes.fromhex(
            "02000000fe00000002800000fd800000")),
        "linked_cleanup_flag_writer": (0x26F24, bytes.fromhex(
            "542e00041d7c003c0022522d8e9641fa03e4"
            "4eb81f404ef90001663a")),
        # The global cleanup byte is written by queue-class-18 selector 0.
        # Preserve the manager pool, dispatcher, attached child creation, and
        # sole observed type-0 allocator as one source-grounded trace.
        "linked_cleanup_class18_pool": (0x2690, bytes.fromhex(
            "4ded74901b7c0008748c303c00077200224e3e3c001f"
            "22c151cffffc1d7c0012001e")),
        "linked_cleanup_class18_dispatch": (0x26DAE, bytes.fromhex(
            "4a2e 0000 670e 7000 102e 0020 e548 207b 000e 4e90 "
            "4dee 0080 51cf ffe6 4e75")),
        "linked_cleanup_class18_selector0_entry": (0x26DCC,
            bytes.fromhex("00026de8")),
        "linked_cleanup_class18_actor_states": (0x26DF4,
            bytes.fromhex("0008006802b8041a")),
        "linked_cleanup_class18_writer_context": (0x26E5C, bytes.fromhex(
            "102e0003323b000a4ebb10064ef82244")),
        "linked_cleanup_class18_state4_dispatch": (0x270AC, bytes.fromhex(
            "102e0003323b000a4ebb10064ef82244"
            "00080022004600b0")),
        "linked_cleanup_class18_state4_gate": (0x27048, bytes.fromhex(
            "4a2e003467241d6e0002003a1d6e0003003b"
            "1d6e0004003c1d7c00040002422e0003422e0004"
            "70014e7570004e75")),
        "generic_class4_pool_init": (0x2660, bytes.fromhex(
            "1b7c00321801303c00317200224e3e3c001f"
            "22c151cffffc1d7c0004001e310e4dee0080"
            "51c8ffe63b48181c")),
        "generic_class4_pool_alloc": (0x2A1A, bytes.fromhex(
            "4a2d18016712306d181c38583b48181c"
            "532d180170014e7570004e75")),
        "class8_collision_pool_init": (0x26F2, bytes.fromhex(
            "41ed12164ded42c61b7c00141807303c0013"
            "224e3e3c000f22c151cffffc1d7c0008001e"
            "310e4dee004051c8ffe63b481828")),
        "class4_collision_active_scan": (0x3A42C, bytes.fromhex(
            "7e001e2d180867f65347386d182a7c00"
            "1c2d748d67e853463b468faa3b6d74888fac")),
        "class4_collision_outer_type_map": (0x3A4D6,
            bytes.fromhex("0004080c1014181c20")),
        "class4_collision_inner_type_map": (0x3A4E6,
            bytes.fromhex("00ff0202040202ffffffffff")),
        "class4_collision_inner_type_dispatch": (0x3A4A4,
            bytes.fromhex("70003200102a0020123b00386b1a")),
        "shared_actor_collision_scan": (0x37B5E, bytes.fromhex(
            "41ed42c63c3c00131028000053006658"
            "0c28000400206608")),
        "shared_collision_byte42_response": (0x37C42, bytes.fromhex(
            "0c2e0002000067201d7c000200007c00"
            "1028002a902e002a67206a144228002a"
            "44001d40002a4e75")),
        "linked_cleanup_class18_spawn_state4": (0x27102,
            bytes.fromhex("532e00226626")),
        "linked_cleanup_class18_type10_child_spawn": (0x26EC0,
            bytes.fromhex(
                "4eb82a1a672c197c00010000197c000a0020"
                "396e00060006066cffe00006396e000a000a"
                "066c0050000a294e00262d4c0046")),
        "linked_cleanup_type10_mode_init": (0x163E6,
            bytes.fromhex(
                "542e0002422e00301d7c0020002a1d7c0020002b"
                "1b7c002003cc4eb84ac6")),
        "linked_cleanup_type10_parent_transition": (0x165D2,
            bytes.fromhex(
                "102e002a6724b02e002b671c1d40002b1b4003cc"
                "4eb84ac6303c00324eb8786a1d7c001100446040"
                "4e751b4003cc4eb84ac6303c008e4eb8786a"
                "1d7c00020000286e0026197c00040002197c00020003"
                "206c0040117c00020000")),
        "linked_cleanup_type11_parent_transition": (0x16706,
            bytes.fromhex(
                "4a2e002b662e1b7c000103e9542e0003303c008e"
                "4eb8786a286e0026197c00040002197c00020003"
                "206c0040117c000200004e75")),
        "linked_cleanup_children_shutdown": (0x27102,
            bytes.fromhex(
                "532e00226626206e0046117c00060002206e0040"
                "117c00060002542e00031d7c00c0002241fa05ee"
                "4ef81f40")),
        "linked_cleanup_class18_type0_creator": (0x14A20, bytes.fromhex(
            "4eb82a686722197c00000020197c00010000397c0e100006"
            "302d040c064000383940000a522d03ce4e75")),
        "linked_cleanup_later_stage_handler": (0x14384, bytes.fromhex(
            "4e751d7c0006000260000692")),
        "linked_cleanup_later_stage_dispatch_entry": (0xFFE4,
            bytes.fromhex("0001432c")),
        "wait_voice_followup": (0x1196A, bytes.fromhex(
            "102e0004323b00064efb10020006001800661d7c"
            "003c0022542e000441fa064c4ef81f30532e0022"
            "672e4eb81f4c6100fee86722422e00046100fece"
            "b02e000f670e1d7c000800351d7c000400034e75"
            "1d7c000800034e75303c008a4eb8786c1d7c003a"
            "0022542e000441fa060e4ef81f30532e00226704"
            "4ef81f4c422e0003422e00044e75")),
        "wait_voice_state6_reentry": (0x119DC, bytes.fromhex(
            "532e002267044ef81f4c422e0003422e00044e75")),
        "secondary_pool_init_call": (0x2648, bytes.fromhex(
            "6100034261000356610002d661000284")),
        "secondary_pool_init": (0x293E, bytes.fromhex(
            "41ed112a4ded18c61b7c001417fe303c0013"
            "7200224e3e3c000f22c151cffffc1d7c0002001e"
            "310e4dee004051c8ffe63b48181641f8937e"
            "3b48181841f893a63b48181a1b4117ff1b411800")),
        "secondary_pool_free": (0x29EC, bytes.fromhex(
            "70002d4000003d4000042d40000e3d400012"
            "3d4000201d40001f1d400024306d1816310e"
            "3b481816522d17fe4e75")),
        "charge_actor_state0_vertical_gate": (0x10080, bytes.fromhex(
            "302e000a906d040c0c40014062000094")),
        "charge_actor_out_of_window_state6": (0x10122, bytes.fromhex(
            "1d7c000600024e75")),
        "charge_actor_state6_free": (0x1115A, bytes.fromhex(
            "4a2e0021660a532d8f406a04422d8f404ef82a36")),
        "charge_actor_free_destination": (0x2A36, bytes.fromhex(
            "70002d4000003d4000042d40000e3d400012"
            "3d4000201d40001f1d4000243d40002e306d"
            "181c310e3b48181c522d18014e75")),
        "player_charge_counter_and_cue": (0xC610, bytes.fromhex(
            "102d8e9208000004671c1d7c0001003f1d7c000a0044"
            "422e0031422d8e9470011d40003c4e75102d006a08000004"
            "673e4a2e003f6612522e003f1d7c000a0044422e0031422d"
            "8e940c2e007f00316700004c522e00310c2e001000316608"
            "303c00314eb8786a6100004870004e75")),
        "player_charge_release_gate": (0xC67E, bytes.fromhex(
            "0c2e00100031641070001d4000311b408e941d40003f4e75"
            "422e003f1b6d03768e94422e003170011d40003c4e75")),
        "player_charge_periodic_actor_spawn": (0xC6C0, bytes.fromhex(
            "4a2e003f672e532e00446628610000280c2d000117fe631c"
            "4eb829d06716197c00010000422c00204eb81d8e02400006"
            "194000214e75")),
        "player_charge_spawn_delay_table": (0xC6F6, bytes.fromhex(
            "7000102e0031e8481d7b000600444e75")),
        "player_charge_spawn_reload_values": (0xC706,
            bytes.fromhex("0a06050403020201")),
        "player_burst_startup_timer_store": (0xCE02,
            bytes.fromhex("1d7c00030022")),
        "player_burst_startup_countdown": (0xCE18, bytes.fromhex(
            "532e00226610022e0001000f3d7c0700004a542e00044ef81f4c")),
    }


    for name, (address, expected) in checks.items():
        if data[address:address + len(expected)] != expected:
            raise ValueError(f"Stage 0 charge child source context changed: {name}")

    bucket = (16 + 1) >> 3
    behavior_selector = data[0xCB08 + bucket]
    mode = 1  # Stage 0 setup writes 1 to a5+0x366 at 0x8d7e.
    map_index = mode * 8 + behavior_selector
    behavior_index = data[0xCB4C + map_index]
    class8_behavior_index = data[0xCC50 + behavior_index]
    class8_handler_address = int.from_bytes(
        data[0xE71C + class8_behavior_index * 4:
             0xE720 + class8_behavior_index * 4], "big")
    return {
        "source_contexts_validated": True,
        "player_handler_class10_effect_routes": [
            {
                "source_site": "0x0012dfa",
                "source_handler": "0x00125d0",
                "sound_command": "0x34 via 0x786c",
                "allocator": "0x002b4e",
                "pool": "queue class 10 at 0x0047c6",
                "selector": 0,
                "dispatch_handler": "0x002359c",
                "trigger": "player byte +80 selects the state; parent pointer at +38 must have byte +60 nonzero; then a 48-tick timer begins",
                "position": "player X/Y plus the vector selected by player byte +48 low nibble",
                "field_15": "player byte +48",
            },
            {
                "source_site": "0x00138ba",
                "source_handler": "0x00125d0",
                "sound_command": "0x34 via 0x786c",
                "allocator": "0x002b4e",
                "pool": "queue class 10 at 0x0047c6",
                "selector": 0,
                "dispatch_handler": "0x002359c",
                "trigger": "either the player state at 0x13762 passes its <144-unit range gate and calls 0x138ba, or the 16-tick substate timer at 0x1383c expires",
                "position": "player X/Y plus the vector selected from facing; subtracts 16 Y when byte +46 is clear",
                "field_15": "player byte +15 shifted left three bits, then +4",
            },
        ],
        "queue_class_10_selector_0_handler": {
            "address": "0x002359c",
            "dispatch_field": "+33 selects a secondary handler table; new records start at 0",
            "initial_state": "sets state 2, timer +34=1, clears +48/+46, installs vector 0x0ed524 and animation table 0x0023856",
            "opening_animation": "the 0x23856 facing entries used by the Stage 0 state-6 spawn are 1x1 CPS tile 0x1362, held indefinitely; the right-facing entry uses horizontal flip",
            "next_state_animation": "state 2 counts down 64 updates and tests 0x3d90 on every fourth global frame; timeout or contact sets +46=2 and state 4, whose substate 0 selects terminal table 0x23b06",
            "terminal_animation": "table 0x23b06 selects 0x13ae, 0x13af, and 0x13ef on four-tick holds; substate 2 runs a twelve-tick timer then state 6 dispatches to cleanup 0x2b6a",
            "stage0_runtime_port": "native selector-0 records can occupy 20 modeled slots with the source LIFO allocation and cleanup cadence, 64-update state 2, 0x3d90 probe cadence, terminal 0x23b06 frames, and next-update cleanup; two visible records are drawn through the reserved Genesis sprite slots 78/79. Other class-10 behavior selectors are not integrated into the modeled pool, and generic object dispatch remains incomplete",
        },
        "player_state6_class10_effect": {
            "player_handler": "0x00128ec",
            "spawn_site": "0x0012a0e",
            "gate": "state-6 substate-4 timer expires and player byte +33 is not 6",
            "stage0_gate_value": 0,
            "allocator": "0x002b4e; queue class 10",
            "dispatch_selector": 0,
            "dispatch_handler": "0x002359c",
            "position_offset_by_facing": {"left_of_target": [10, 4], "right_of_target": [-10, 4]},
            "animation_table": "0x0023856",
            "sound_command": "0x34 (sequenced cue; native sequencer not implemented)",
            "runtime_port": "native selector-0 spawns can occupy 20 modeled LIFO slots with 64-update stationary state, four-global-frame solid probe, terminal three-frame animation, and source cleanup; only two visible records are currently rendered through Genesis sprite slots 78/79. Other class-10 selectors are not integrated into this pool model, and sequenced cue playback remains incomplete",
        },
        "stage_mode": mode,
        "stage_counter_three_branch": {
            "gate": "a5+0x0344 equals 3 and a5+0x03a0 is zero",
            "spawn": "0x00a198",
            "spawn_position": [-96, 4200],
            "field_32": 7,
            "relevance_to_initial_stage0_spawn": "separate from the normal 0x00a114-0x00a146 initializer path that writes Willow world position (192, 512)",
        },
        "stage_counter_four_branch": {
            "gate": "a5+0x0344 equals 4",
            "allocation_sites": ["0x00a1d6", "0x00a1e6", "0x00a202"],
            "field_32_values": [32, 34, 33],
            "relevance_to_stage0": "separate stage-counter branch; not evidence for an initial Stage 0 enemy spawn",
            "out_of_table_behavior_values": "remain unresolved until a dispatch mechanism other than the verified 35-entry table is demonstrated",
        },
        "example_charge_ticks": 16,
        "charge_bucket_index": bucket,
        "charge_bucket_value": behavior_selector,
        "behavior_map_index": map_index,
        "behavior_index": behavior_index,
        "class8_behavior_index": class8_behavior_index,
        "class8_handler_address": f"0x{class8_handler_address:06x}",
        "primary_child": {
            "allocator": "0x002b04",
            "pool": "20 records of 64 bytes; queue class 8",
            "active_type": 1,
            "field_32_from_cc50": class8_behavior_index,
            "field_33_from_charge_behavior": behavior_index,
            "field_15": "player object +15 masked to 0/1; selects the +4/-4 X vector",
            "x": "copied from player object +6",
            "y": "player object +10; add 8 unless down flag +62 is set",
            "class8_dispatcher": "0x00e6f4 routes +32=1 to handler 0x00ec84",
            "opening_handler": "0x00ec84 sets +34=64 and state +2=2, then selects motion/animation from +15/+33",
            "state2_handler": "0x00ed7c checks the 64-tick expiry, performs periodic camera culling, then moves, advances animation, and runs 0x3744 collision before visibility; later same-tick collision state changes can supersede state 6",
            "cleanup": "0x00edf2 calls 0x002b20, which clears source control fields and pushes the record onto the class-8 LIFO free list",
            "native_status": "64-tick lifetime, horizontal vector cadence, class-8 animation streams 0xf040/0xf074, directional collision probes, impact animation/timer, and LIFO allocation/cleanup are modeled; exact palette validation remains incomplete",
        },
        "unrelated_profile1_class4_route": {
            "handler_entry": "0x0115ee mode +33 = 1",
            "startup_table": "0x0011fc8",
            "timer_ticks": 24,
            "validated_tick_order": "0x116f0 calls animation helper 0x1f4c and visibility helper 0x2244 before decrementing byte +34; on timer expiry it shifts X by -8, advances state, sets substate 2 and active type 1",
            "animation_frame_order": "the helper selects the next 8-tick frame before the 24-tick timer expiry branch on the same update",
            "completion_x_delta": -8,
            "next_state": 1,
            "next_substate": 2,
            "relationship_to_primary_child": "separate class-4 dispatch route; not used by the queue-class-8 primary charge child",
        },
        "primary_child_pool": {
            "initialized_at": "0x0026f2",
            "allocator": "0x002b04",
            "records": 20,
            "record_bytes": 64,
            "queue_class": 8,
            "first_free_record_index": 19,
            "second_free_record_index": 18,
            "third_free_record_index": 17,
            "fourth_free_record_index": 16,
            "allocation_order_after_reset": [19, 18, 17, 16],
            "cadence_slot": 19,
        },
        "periodic_charge_actor": {
            "handler": "0x000c610",
            "trigger": "first held update sets active flag +63, clears charge counter +49, arms the 10-tick timer, and returns; later held updates increment +49 and call the timer helper",
            "charge_16_event": "sends sequenced sound command 0x31 through 0x786a when player byte +49 first reaches 16",
            "saturated_charge_event": "sends sequenced sound command 0x44 each time the global frame low nibble is zero while charge is saturated at 127",
            "spawn_timer_reload_table": list(data[0xC706:0xC70E]),
            "spawn_condition": "timer +68 reaches zero and class-2 free count at a5+0x17fe is greater than one; the 0xc6d0 compare/branch preserves one record",
            "reserved_class2_records": 1,
            "allocator": "0x0029d0",
            "pool_init": "0x00293e via reset at 0x002648",
            "pool_records": 20,
            "pool_record_bytes": 64,
            "initial_queue_class": 2,
            "active_type": 1,
            "field_32": 0,
            "field_33": "shared LFSR output AND 6 (0, 2, 4, or 6)",
            "coordinates_written_in_charge_context": False,
            "coordinate_semantics": "fresh pool entries are zero-initialized; free routine 0x0029ec clears selected control fields but not +6/+10, so reused entries retain their last coordinates",
            "fresh_stage0_outcome": {
                "initial_coordinates": [192, 72],
                "player_world_coordinates": [192, 512],
                "work_area_coordinate_writes": {
                    "stage0_setup": {"address": "0x000a128", "position": [192, 512]},
                    "stage_counter_three_reset": {"address": "0x000a16e", "position": [192, 4096]},
                    "runtime_direct_writer": "no additional direct writes found in the main-CPU cross-reference",
                },
                "state0_vertical_gate": "0x0010080 compares actor Y against a5+0x040c and branches to state 6 when unsigned difference exceeds 320",
                "initial_stage0_result": "at the initialized player Y=512, the fresh actor's unsigned Y difference is 0xfe00, so it enters state 6 before the visible state-2 movement path",
                "later_stage0_condition": "with actor Y still zero, the gate can pass only when a5+0x040c is zero or in 0xfec0..0xffff; whether normal Stage 0 movement reaches such a value remains unverified",
                "state6_handler": "0x001115a skips the a5-0x8f40 decrement when byte +33 is nonzero; a zero mode decrements the counter, clamps underflow to zero, then deactivates through 0x002a36",
                "free_destination": "0x002a36 inserts the record into the list headed at a5+0x181c and increments a5+0x1801",
                "visual_result": "no on-screen actor at initial Stage 0 position; a later world-Y-zero entry remains possible but unverified",
            },
            "native_runtime": "charge timer/reload, mode LFSR draw, class-2 free-count drain, and release-child pool gate are implemented; the generic handler, state-6 transfer to class 4, and sprite rendering are not",
        },
        "verified_walk_state": {
            "animation_table_transitions": animation_table_transitions,
            "initial_patrol_bounds": "entry X plus or minus 128 pixels",
            "walk_velocity": "signed 8.8 vector at 0x000ed070, plus or minus 1.5 pixels per update",
            "edge_check": "0x11852 on (global frame + scheduler slot) modulo 4; enters state 4 and clears return byte +53, so the deceleration returns to patrol substate 0",
            "walk_wait_timer_order": "0x1181a decrements +52 after the patrol-edge check; expiration reloads the random wait and enters state 6 even if the edge just entered a turn. A successful proximity transition returns before this decrement",
            "proximity_check": "0x11882 on the same cadence; shared target X at a5+0x184c within actor +48/+50 bounds, actorY-targetY from -40 through +8",
            "proximity_transition": "0x11830 compares the actor's +15 direction with the predicate from 0x11872; that predicate is actor X > shared target X at a5+0x184c, not actor X > Willow X",
            "proximity_direction_target": {
                "address": "0x0011872",
                "instruction_bytes": data[0x11872:0x11882].hex(),
                "predicate": "returns 1 when actor word +6 is greater than shared target X a5+0x184c; otherwise returns 0",
            },
            "approach_followup_proximity": {
                "address": "0x00118b8",
                "instruction_bytes": data[0x118B8:0x118EA].hex(),
                "vertical_delta": "actorY-targetY using shared Y at a5+0x1850, in [-72,+8]",
                "horizontal_bounds": "shared target X at a5+0x184c within actor +48/+50",
                "global_flag_gate": "a5+0x186a rejects [-23,+8] when nonzero; reset clears RAM and no source write is present",
            },
            "approach_contact_target_gate": {
                "address": "0x0011a78",
                "instruction_bytes": data[0x11A78:0x11A94].hex(),
                "predicate": "actor X minus shared target X at a5+0x184c plus 8 is below 16; enters state-10 substate 0 and clears substate +4",
            },
            "approach_vector_table": {
                "address": "0x000ed088",
                "bytes": data[0xED088:0xED090].hex(),
                "signed_8_8_xy": [[3, 0], [-3, 0]],
            },
            "heading_classifier": {
                "routine": "0x0020d2",
                "routine_bytes": data[0x20D2:0x2144].hex(),
                "octant_bases_address": "0x002144",
                "octant_bases": data[0x2144:0x214C].hex(),
                "divider_address": "0x00214c",
                "divider_bytes": data[0x214C:0x2162].hex(),
                "native_equivalent": "absolute minor/major ratio in 32 steps; add or subtract from the selected 8-bit octant base, add 8, then use bits 4-7",
            },
            "turn_state": "state 4 initializes 0x0300, subtracts 0x18 per update, applies the resulting signed X step, and reverses when the subtraction turns negative",
            "turn_return_paths": {
                "patrol_edge": "0x1180c selects substate 4 and 0x11812 clears +53; 0x11958 therefore resumes patrol substate 0 after deceleration",
                "proximity_or_approach_edge": "0x1183c and 0x11a56 set +53=8; the same deceleration returns to approach substate 8",
                "native_model": "turnReturnPhase preserves the traced destination: patrol edges return to phase 0, while proximity and approach turns return to phase 4 (approach setup)",
            },
            "contact_state": "state 10 checks 0x11852 before each 0x10 deceleration step; patrol exit or negative speed enters an eight-tick pose then reverses",
            "wait_voice_sequence": "state 6 waits 60 ticks, sends command 0x8a, then advances the 58-tick 0x11fe4 sequence",
            "after_followup": "0x00119e6 clears +3/+4 while leaving +2 at state 6; next update restarts the 60-tick wait at 0x001197a unless proximity branches to state 4 or 8",
        },
        "linked_allocation": {
            "spawn_site": "0x00cc28, reached after the charged-child allocation succeeds at 0x00cba6",
            "condition": "free-list count at a5+0x17fe is nonzero",
            "allocator": "0x0029d0",
            "pool_init": "0x00293e via reset at 0x002648",
            "pool_records": 20,
            "pool_record_bytes": 64,
            "initial_queue_class": 2,
            "first_free_record_index": 19,
            "allocation_order_after_reset": list(range(19, -1, -1)),
            "allocator_slot_rule": "the allocator pops the free-list head; with no frees, record index is free_count - 1 before decrement",
            "fresh_pool_coordinates": "(0, 0) from zero-initialized record",
            "active_type": 1,
            "field_32": 1,
            "facing_copied": True,
            "movement_selector_copied_from_player_byte_15": True,
            "coordinates_written_in_charge_context": False,
            "coordinates_may_change_after_dispatch": True,
            "behavior1_lifecycle": {
                "pool_dispatcher": "0x00fb48 dispatches class-2 field +32=1 to 0x00fe3e",
                "state0": "0x00fe52 sets active type 2, state 2, and an eight-tick +34 timer; animation selector uses object +15 bit 0 and table 0x00fea8",
                "animation": {
                    "table": "0x00fea8",
                    "stream_addresses": ["0x00feac", "0x00fee0"],
                    "frame_codes": ["0x0280", "0x0282", "0x0284", "0x0286"],
                    "first_three_holds": 4,
                    "last_hold": 65535,
                    "left_stream_hflip": True,
                },
                "state2": "0x00fe6c advances the animation, decrements +34, sets X from a5+0x184c (+16 or -16 by facing), sets Y from a5+0x1850 (+8 when a5+0x1884 is clear), then calls 0x2244",
                "state4": "computed state dispatch reaches 0x00fc92, which jumps to class-2 free routine 0x0029ec on the update after the timer expires",
            },
            "native_runtime": "the native charge-release path creates this record only after the primary queue-class-8 allocation at 0x00cba6 succeeds; it consumes the matching class-2 free-list slot and follows the verified behavior-1 setup, eight-tick update, and state-4 pool return",
        },
        "stage0_linked_actor_spawn": {
            "initializer": "0x00a28a",
            "active_type": 2,
            "field_32": 1,
            "field_33": 0,
            "position": [218, 136],
            "pool_record_index": 59,
            "dispatcher_countdown_d7": 0,
            "pool": {"initializer": "0x002756", "records": 60,
                     "record_bytes": 64, "queue_class": 12,
                     "first_free_record_index": 59},
            "behavior_mode_handler": "0x003257e",
            "behavior_route": "main object dispatcher 0xffa2 routes field +32=1 through class-12 dispatcher 0x2f39e",
            "state_dispatch": {
                "table": "0x003258a",
                "state0": "0x0032592 initializes state 2 and the stream at 0x00325bc",
                "state2": "0x00325a4 advances the three-frame animation at 10-tick holds, checks retirement every 16 global updates, and calls visibility helper 0x2244",
                "states4_and6": "route to class-12 pool cleanup at 0x002bb8",
            },
            "animation_codes": ["0x0760", "0x0766", "0x076b"],
            "native_status": "implemented by the native stage-linked actor state and renderer",
            "source_bytes_validated_by": "tools/cps1_animation_catalog.py",
            "player_world_position": [192, 512],
            "initial_camera_origin": [192, 512],
            "render_window": {
                "x_relative_to_camera": [-32, 416],
                "y_relative_to_camera": [0, 272],
            },
            "initially_in_render_window": False,
            "enters_render_window_when_camera_reaches_actor": True,
        },
        "class4_patrol_behavior_without_stage0_spawn_link": {
            "pool": {"initializer": "0x00266c", "records": 50,
                     "record_bytes": 128, "queue_class": 4,
                     "first_free_record_index": 49},
            "behavior_mode_handler": "0x00115ee",
            "mode_0_entry": "0x0011604",
            "state0_entry": "0x0011654 initializes state 2, movement table 0xed070, bounds, and an LFSR delay",
            "state2_substate0": "0x00116be selects animation 0x11fc8 and a 24-tick timer",
            "state4_substate10_score_event": "0x0011dbe queues 0x1802; handler 0x626a adds BCD 0x00000100 to score",
            "stage0_spawn_link": "not established; the actor at 0xa28a is class 12 and uses handler 0x3257e",
            "native_status": "behavior is not implemented as a Stage 0 actor",
        },
    }


def scripted_actor_spawn_candidate(data: bytes) -> dict[str, object]:
    """Validate a timed controller handler with additional actor allocations.

    The handler is a useful lead for scripted spawns, but its association with
    the normal Stage 0 script has not been established.
    """
    checks = {
        "controller_state_dispatch": (0xB11A, bytes.fromhex(
            "422d799c302d000c303b00064efb0002")),
        "controller_state_table": (0xB12A, bytes.fromhex(
            "0028004a008e00e6011e013e03640388043c04da04de0566"
            "05b205e60634065c068406b206fe0772")),
        "state12_timer_exit": (0xB48E, bytes.fromhex(
            "536d000e671604ad000040008e8c3b6d8e8c0030026d01ff0030"
            "4e75546d000c4ef81e88")),
        "state14_entry": (0xB4B2, bytes.fromhex(
            "422d03434eb80ac44eb82648546d000c"
            "3b7c001f000e207c000d4000")),
        "timed_handler": (0xB566, bytes.fromhex(
            "536d000e66000096546d000c3b7c005a000e422d0411"
            "1b7c000304243b7c013f000e4eb82b9c"
            "197c00010000197c00260020397c0100000a4eb82c86"
        )),
        "conditional_followup_allocations": (0xB5A4, bytes.fromhex(
            "197c00010000197c0001003c297c000e7c000034"
            "197c00110038197c000c003b197c0018003a"
            "197c000100204eb82c86662c197c00010000"
            "197c0003003c297c000ec0e00034197c000c0038"
            "197c000c003b197c0018003a197c00010020"
        )),
        "next_timed_state": (0xB604, bytes.fromhex(
            "546d000c536d000e662e303c00864eb8786a"
            "546d000c1b7c00018e583b7c00c8000e"
        )),
    }
    for name, (offset, expected) in checks.items():
        if data[offset:offset + len(expected)] != expected:
            raise ValueError(f"scripted actor candidate bytes changed at {name}")
    return {
        "handler": "0x00b566",
        "timing": "decrements controller timer at a5+0x0e; on expiry advances controller state at a5+0x0c and loads 90 ticks",
        "first_allocation": {
            "site": "0x00b588",
            "active_type": 1,
            "field_32": 38,
            "y": 256,
            "position": "no X write occurs before helper 0x2c86; that helper only checks for a free class-4 record, so X and remaining fields are inherited or unresolved",
        },
        "conditional_followup_allocations": [
            {"site": "0x00b5a4", "active_type": 1, "field_32": 1,
             "animation_pointer": "0x000e7c00", "x_y": "helper/context derived"},
            {"site": "0x00b5d6", "active_type": 1, "field_32": 1,
             "animation_pointer": "0x000ec0e0", "x_y": "helper/context derived"},
        ],
        "later_timed_state": "0x00b604 advances controller state and initializes a 200-tick stage event",
        "controller_chain": {
            "dispatcher_offset": "0x00b11a",
            "state_table_offset": "0x00b12a",
            "state12_entry": "0x00b48e",
            "state14_entry": "0x00b4b2",
            "state16_entry": "0x00b566",
            "verified_local_flow": (
                "the indexed dispatcher selects state handlers by a5+0x000c; "
                "state 12 advances to 14 when its timer expires, state 14 "
                "performs a 31-tick setup then advances to 16, and state 16 "
                "performs the timed allocations"
            ),
            "normal_stage0_link": (
                "not established; normal Stage 0 enters the separate "
                "controller path at 0x00a0d0/0x00a114, so this chain remains "
                "disabled in native gameplay"
            ),
        },
        "stage0_link": "unverified; keep disabled in native gameplay until the stage-indexed script route and actor handlers are traced",
        "byte_validation": [
            {"address": f"0x{offset:06x}", "length": len(expected)}
            for offset, expected in checks.values()
        ],
    }


def camera_triggered_actor_batch_trace(data: bytes) -> dict[str, object]:
    """Extract a camera-gated fixed actor batch from the main-CPU ROM."""
    loader_address = 0x2D550
    table_address = 0x2D586
    table_end = 0x2D78C
    handler_address = 0x30ED0
    trigger_address = 0x30F3C
    loader_bytes = bytes.fromhex(
        "3b7cffff8f0443fa002e3c196b1c4eb82bea6718"
        "197c0001000039590020395900063959000a51ceffe64e75"
        "3b468f042b498f00"
    )
    trigger_bytes = bytes.fromhex(
        "302d184cb06e00066538542e0003426d8f06422d1847"
        "1b7c000a184870001b4018493b40184a1d7c00200022"
        "41fa03544eb81f40303c00084eb8786a4ef90002d550"
    )
    if data[loader_address:loader_address + len(loader_bytes)] != loader_bytes:
        raise ValueError("camera-triggered actor batch loader at 0x2d550 changed")
    if data[trigger_address:trigger_address + len(trigger_bytes)] != trigger_bytes:
        raise ValueError("camera-triggered actor batch trigger at 0x30f3c changed")

    # The ordinary active-record dispatcher at 0xffa2 indexes this table by
    # actor byte +32. Its 35 verified entries end immediately before code at
    # 0x10054; the camera batch manager is not one of those normal handlers.
    # Keeping this distinction in the catalog prevents its camera gate from
    # being treated as a Stage 0 main-pool enemy spawn.
    main_handler_table = 0x00FFC8
    main_handler_count = 35
    main_handlers = [
        int.from_bytes(data[main_handler_table + index * 4:
                            main_handler_table + index * 4 + 4], "big")
        for index in range(main_handler_count)
    ]
    expected_first_handler = 0x00010054
    expected_last_handler = 0x00022F38
    if (main_handlers[0] != expected_first_handler
            or main_handlers[-1] != expected_last_handler
            or main_handler_table + main_handler_count * 4 != 0x10054):
        raise ValueError("normal active-record handler table at 0xffc8 changed")
    if handler_address in main_handlers:
        raise ValueError("camera batch manager unexpectedly entered main handler table")

    def absolute_transfer_sites(target: int) -> list[int]:
        # 68000 JSR abs.l and JMP abs.l opcodes. This checks only direct
        # absolute transfers; PC-relative and computed dispatch are separate.
        sites = []
        for opcode in (b"\x4e\xb9", b"\x4e\xf9"):
            pattern = opcode + target.to_bytes(4, "big")
            offset = 0
            while (offset := data.find(pattern, offset)) >= 0:
                sites.append(offset)
                offset += 1
        return sorted(sites)

    loader_absolute_transfers = absolute_transfer_sites(loader_address)
    manager_absolute_transfers = absolute_transfer_sites(handler_address)
    if loader_absolute_transfers != [0x30F78]:
        raise ValueError("camera batch loader absolute-transfer sites changed")
    if manager_absolute_transfers:
        raise ValueError("camera batch manager gained a direct absolute transfer")

    header = int.from_bytes(data[table_address:table_address + 2], "big")
    # MOVE.W (A1)+,D6 followed by DBF creates header+1 records.
    record_count = header + 1
    records_start = table_address + 2
    records_stop = records_start + record_count * 6
    if header != 0x55 or record_count != 86 or records_stop != table_end:
        raise ValueError("camera-triggered actor batch table bounds changed")
    sentinel = int.from_bytes(data[records_stop:records_stop + 2], "big")
    if sentinel != 0xFFFF:
        raise ValueError("camera-triggered actor batch sentinel changed")

    actors = []
    selectors: dict[str, int] = {}
    minimum_x = 0xFFFF
    maximum_x = 0
    for index in range(record_count):
        offset = records_start + index * 6
        active_and_selector = int.from_bytes(data[offset:offset + 2], "big")
        world_x = int.from_bytes(data[offset + 2:offset + 4], "big")
        world_y = int.from_bytes(data[offset + 4:offset + 6], "big")
        selector = active_and_selector >> 8
        behavior = active_and_selector & 0xFF
        selectors[f"0x{selector:02x}/0x{behavior:02x}"] = (
            selectors.get(f"0x{selector:02x}/0x{behavior:02x}", 0) + 1)
        minimum_x = min(minimum_x, world_x)
        maximum_x = max(maximum_x, world_x)
        actors.append({
            "index": index,
            "source_address": f"0x{offset:06x}",
            "behavior_selector": selector,
            "initial_behavior": behavior,
            "world_xy": [world_x, world_y],
        })

    if set(selectors) != {"0x01/0x06", "0x01/0x07"}:
        raise ValueError("camera-triggered actor batch selector values changed")
    return {
        "trigger": {
            "handler": "0x0030ed0",
            "gate": "when current camera X a5+0x184c reaches or passes current actor X, its state handler tail-jumps to 0x2d550",
            "loader": f"0x{loader_address:06x}",
            "owner_activation": "not connected to normal Stage 0 or a specific stage-entry route",
        },
        "main_pool_dispatch_check": {
            "table_address": f"0x{main_handler_table:06x}",
            "entry_count": main_handler_count,
            "handler_range": [
                f"0x{min(main_handlers):08x}",
                f"0x{max(main_handlers):08x}",
            ],
            "contains_camera_batch_handler": False,
            "interpretation": "the batch manager is not a normal active-record handler selected by actor byte +32 at 0xffa2; its owning scheduler or stage-entry path remains unresolved",
        },
        "absolute_transfer_scan": {
            "opcodes": ["JSR abs.l", "JMP abs.l"],
            "loader_sites": [f"0x{site:06x}"
                             for site in loader_absolute_transfers],
            "manager_sites": [f"0x{site:06x}"
                              for site in manager_absolute_transfers],
            "scope": "direct absolute transfers only; does not cover PC-relative branches, tables, or computed dispatch",
        },
        "allocator": "0x002bea (60-record queue-class-14 pool)",
        "table": {
            "address": f"0x{table_address:06x}",
            "count_word": f"0x{header:04x}",
            "dbf_iterations": record_count,
            "record_bytes": 6,
            "sentinel_address": f"0x{records_stop:06x}",
            "sentinel": f"0x{sentinel:04x}",
            "selector_and_behavior_counts": selectors,
            "world_x_bounds": [minimum_x, maximum_x],
            "stage0_reachability": "all source X coordinates begin beyond the initial 4096-pixel Stage 0 map window; manager activation is unresolved",
            "records": actors,
        },
        "byte_validation": {
            "batch_loader": True,
            "camera_gate_and_loader_call": True,
            "count_and_sentinel": True,
        },
    }


def player_state4_queue14_spawn_trace(data: bytes) -> dict[str, object]:
    """Trace the queue-class-14 actor emitted by the Stage 0 player state path."""
    state4_table_address = 0x12FC0
    state4_table_bytes = bytes.fromhex(
        "008200ce00fc01200172"
    )
    state4_substate = 6
    state4_dispatch_target = state4_table_address + 0x0120
    if state4_table_bytes[state4_substate:state4_substate + 2] != b"\x01\x20":
        raise ValueError("player state-4 substate-6 dispatch changed")
    spawn_path_address = state4_dispatch_target
    spawn_path_bytes = bytes.fromhex(
        "0002303c18004eb81f144eb81d8e0240000f103b002c"
        "4eb825080c2e00060021661a206e00264a2e0050670a"
        "117c0001003c4ef81f4c117c0001003d4ef81f4c"
    )
    recovery_timer_address = 0x130BA
    recovery_timer_bytes = bytes.fromhex(
        "532e00226616542e00031d7c00080022303c0033"
        "4eb8786a4eb8492c4ef81f4c"
    )
    state4_entry = (0x12FE0, bytes.fromhex(
        "542e00031d7c000100133d7c040000422d7c00019e8a0018"
        "322e0006422e000fb26d184c62061d7c0001000f422e0048"
        "41fa6dec4ef81f301d7c"))
    state4_ballistic_step = (0x13050, bytes.fromhex(
        "0c6ef88000426d08066effc0004260063d7cf880004230"
        "2e004248c0e180d1ae000a4eb81f4c"))
    state4_contact_transition = (0x1308C, bytes.fromhex(
        "102d0019d0070240000f66044eb821624a2e002466064e"
        "b83b0a67a8542e00031d7c000a0022"))
    selector_table_address = 0x13120
    selector_table = bytes.fromhex(
        "01020101010101010101010101020101"
    )
    allocator_address = 0x2508
    allocator_bytes = bytes.fromhex(
        "d040323b003847fb10343c1b4eb82bea6728"
        "197c00010000422c0020195b00211946002e"
        "196e002f002f396e00060006396e000a000a51ceffd44e75"
    )
    descriptor_address = 0x25A0
    descriptors = bytes.fromhex("000002ff000005ff")
    first_main_handler = (0xFFC8, bytes.fromhex("00010054"))
    generic_child_dispatch = (0x10054, bytes.fromhex(
        "102e0002323b00064efb1002000802600e7a10fa"
        "102e0021323b00064efb1002000c00b600f6018201be000c"))
    selector_2_path = (0x1016A, bytes.fromhex(
        "102e0003323b00064efb100200040042"
        "102d0019d007024000036630"))
    selector_5_path = (0x10080, bytes.fromhex(
        "302e000a906d040c0c400140620000943d6d04080006"
        "046e002000064a2e000f6706066e01c000060c6d00010344"))
    selector_2_state_dispatch = (0x10864, bytes.fromhex(
        "102e0004323b00064efb1002001000f2015a017c"))
    selector_2_ground_delay = (0x10962, bytes.fromhex(
        "4a2e00246618046e0007000a4eb83b0a660c1d7c00120003"
        "422e00044e754eb81f4c532e002266f4"))
    selector_2_vector_step = (0x109CA, bytes.fromhex(
        "532e00226610542e00041d7c000100221d7c003c003c"
        "4eb820b44eb83b0a4ef81f4c"))
    state2_substate18 = (0x10B92, bytes.fromhex(
        "102e0004323b00064efb10020004002a4a2e00246616"
        "542e00041d7c00020000426e003441fa09a64ef81f30"
        "422e0003422e00044e75"))
    state2_substate18_fall = (0x10BC8, bytes.fromhex(
        "6100fc60102d0019d0070240000f66044eb821624e75"))
    class4_state6_release = (0x1115A, bytes.fromhex(
        "4a2e0021660a532d8f406a04422d8f404ef82a36"))
    class4_state4_gate = (0x10C32, bytes.fromhex(
        "4a2d03d06624102e002ab02e002b67181d40002b"
        "1d7c000200001d7c00040002422e0003422e00044e75"))
    class4_state4_flag_init = (0x102A8, bytes.fromhex(
        "1d7c0001002a1d7c0001002b"))
    class4_state4_dispatch = (0x10EDA, bytes.fromhex(
        "102e0003323b000a4ebb10064ef82244"))
    class4_state4_entry = (0x10EF8, bytes.fromhex(
        "303c00324eb8786a4a2e002b663e542e0003422e0043"
        "1d7c00010013"))
    class4_main_dispatcher = (0xFFA2, bytes.fromhex(
        "4ded1dc63e3c00314a2e0000670e7000102e0020e548"
        "207b000e4e904dee008051cfffe64e75"))
    class4_handler_slot0 = (0xFFC8, bytes.fromhex("00010054"))
    class4_handler_slot1 = (0xFFCC, bytes.fromhex("000115EE"))
    class4_slot1_flag_writer = (0x116C0, bytes.fromhex(
        "3d7c0202002a1d7c0002000e1d7c00040013"))
    class14_pool_init = (0x28DA, bytes.fromhex(
        "41ed132e4ded5bc61b7c003c1810303c003b7200224e3e3c000f"
        "22c151cffffc1d7c000e001e310e4dee004051c8ffe63b48183a"
        "41f897363b48183c41f897ae3b48183e"))
    class14_scheduler = (0x2CF8E, bytes.fromhex(
        "00004ded5bc63e3c003b4a2e0000670e7000102e0020e548"
        "207b000e4e904dee004051cfffe64e75"))
    class14_behavior0 = (0x2CFC2, bytes.fromhex(
        "102e0002323b00064efb1002001c00dc025e031a"))
    class14_horizontal_step = (0x2D0F0, bytes.fromhex(
        "302e0030673248c0e180d1ae00064a2d03cf6724"
        "102d0019d0070200000366184a6e00306a14302d0408"
        "06400008b06e00066d04446e00304e75302d04080640"
        "0178b06e00066ef0446e00304e75"))
    class14_reflection_flag_writer = (0x27E96, bytes.fromhex(
        "4a2e0031670c532e003166061d7c00010000"
        "102e002a675ab02e002b67521b4003cc1d40002b"
        "1d7c000200001d7c002800314eb84ac6303c0032"
        "4eb8786a4eb82c86662c197c00010000297c000e6b200034"
        "197c001a0038197c000d003b197c0003003a197c00010020"
        "197c0000003c4e751b7c000103e91b7c000103cf"
        "1d7c00020000542e0002422e0003422e0004422d03cc"
        "4eb82ab66712197c00010000197c00060020"
        "197c000200214ef84ac6"))
    class14_transition_entry = (0x2D1F0, bytes.fromhex(
        "0c6e012c003465201d7c000200001d7c00040002"
        "1d7c000400031d7c000c002241fa00da4ef81f40"))
    class14_state4_sound_entry = (0x2D22C, bytes.fromhex(
        "102e0003323b000a4ebb10064ef822440006006e009e"
        "303c002d4eb8786a542e0003"))
    class14_state4_score_award = (0x2D28C, bytes.fromhex(
        "4a2e002f670c7000102e0021e5484eb84e12"
        "7000102e0021e5484ef84e12"))
    class14_phase_gate = (0x2D19C, bytes.fromhex(
        "102d0019d007520002000001660c4eb86e444a2e0024"
        "660a4e754eb83c48660a4e75422e0036"))
    class14_state2_substate_dispatch = (0x2D0AA, bytes.fromhex(
        "102e0003323b003a4ebb10364eb8488a"))
    class14_state2_substate_table = (0x2D0EA, bytes.fromhex(
        "0054007e0130"))
    class14_state2_substate_writers = (
        (0x2D156, bytes.fromhex("542e0003")),
        (0x2D1CC, bytes.fromhex("e0ee0030542e0003")),
        (0x2D1E6, bytes.fromhex("422e0003")),
    )
    class14_overlap_dispatch = (0x2D0AA, bytes.fromhex(
        "102e0003323b003a4ebb10364eb8488a526e0034"))
    class14_overlap_helper = (0x488A, bytes.fromhex(
        "102d0019d007020000076654206d185a30280006e748"
        "207c0003b1c241f00000"))
    class14_overlap_target_selector = (0xD14A, bytes.fromhex(
        "00092200800b0000ffff00330000d14a"))
    class14_overlap_bounds_row0 = (0x3B1C2, bytes.fromhex(
        "000c001800120022"))
    class14_visibility_callback = (0x2244, bytes.fromhex(
        "4a2e00006736422e0001302e0006906d040806400020"
        "0c4001c06220302e000a906d040c0c4001106212522e"
        "00017000102e001e303b00084efb00044e75"))
    class14_visibility_type_entry = (0x2290, bytes.fromhex("01f0"))
    class14_visibility_list_dispatch = (0x2472, bytes.fromhex(
        "7000102e000e303b00064efb000200040014"
        "306d183c310e3b48183c522d18114e75"
        "306d183e310e3b48183e522d18124e75"))
    checks = {
        "state4_substate_table": (
            data[state4_table_address:state4_table_address
                 + len(state4_table_bytes)] == state4_table_bytes
        ),
        "state4_child_creation_path": (
            data[spawn_path_address:spawn_path_address + len(spawn_path_bytes)]
            == spawn_path_bytes
        ),
        "state4_recovery_to_child_order": (
            data[recovery_timer_address:recovery_timer_address
                 + len(recovery_timer_bytes)] == recovery_timer_bytes
        ),
        "state4_entry_motion_setup": (
            data[state4_entry[0]:state4_entry[0] + len(state4_entry[1])]
            == state4_entry[1]
        ),
        "state4_ballistic_motion_step": (
            data[state4_ballistic_step[0]:state4_ballistic_step[0]
                 + len(state4_ballistic_step[1])] == state4_ballistic_step[1]
        ),
        "state4_contact_transition": (
            data[state4_contact_transition[0]:state4_contact_transition[0]
                 + len(state4_contact_transition[1])]
            == state4_contact_transition[1]
        ),
        "random_selector_table": (
            data[selector_table_address:selector_table_address
                 + len(selector_table)] == selector_table
        ),
        "class4_allocator": (
            data[allocator_address:allocator_address + len(allocator_bytes)]
            == allocator_bytes
        ),
        "child_descriptors": (
            data[descriptor_address:descriptor_address + len(descriptors)]
            == descriptors
        ),
        "class4_child_main_dispatch": (
            data[first_main_handler[0]:first_main_handler[0]
                 + len(first_main_handler[1])] == first_main_handler[1]
        ),
        "generic_child_behavior_dispatch": (
            data[generic_child_dispatch[0]:generic_child_dispatch[0]
                 + len(generic_child_dispatch[1])]
            == generic_child_dispatch[1]
        ),
        "selector2_initial_handler": (
            data[selector_2_path[0]:selector_2_path[0]
                 + len(selector_2_path[1])] == selector_2_path[1]
        ),
        "selector5_initial_handler": (
            data[selector_5_path[0]:selector_5_path[0]
                 + len(selector_5_path[1])] == selector_5_path[1]
        ),
        "selector2_state2_dispatch": (
            data[selector_2_state_dispatch[0]:selector_2_state_dispatch[0]
                 + len(selector_2_state_dispatch[1])]
            == selector_2_state_dispatch[1]
        ),
        "selector2_ground_delay": (
            data[selector_2_ground_delay[0]:selector_2_ground_delay[0]
                 + len(selector_2_ground_delay[1])]
            == selector_2_ground_delay[1]
        ),
        "selector2_vector_step": (
            data[selector_2_vector_step[0]:selector_2_vector_step[0]
                 + len(selector_2_vector_step[1])]
            == selector_2_vector_step[1]
        ),
        "state2_substate18_dispatch_and_entry": (
            data[state2_substate18[0]:state2_substate18[0]
                 + len(state2_substate18[1])] == state2_substate18[1]
        ),
        "state2_substate18_fall_and_cull_cadence": (
            data[state2_substate18_fall[0]:state2_substate18_fall[0]
                 + len(state2_substate18_fall[1])]
            == state2_substate18_fall[1]
        ),
        "class4_state6_release": (
            data[class4_state6_release[0]:class4_state6_release[0]
                 + len(class4_state6_release[1])]
            == class4_state6_release[1]
        ),
        "class4_state4_transition_gate": (
            data[class4_state4_gate[0]:class4_state4_gate[0]
                 + len(class4_state4_gate[1])]
            == class4_state4_gate[1]
        ),
        "class4_state4_flag_initialization": (
            data[class4_state4_flag_init[0]:class4_state4_flag_init[0]
                 + len(class4_state4_flag_init[1])]
            == class4_state4_flag_init[1]
        ),
        "class4_state4_substate_dispatch": (
            data[class4_state4_dispatch[0]:class4_state4_dispatch[0]
                 + len(class4_state4_dispatch[1])]
            == class4_state4_dispatch[1]
        ),
        "class4_state4_initial_entry": (
            data[class4_state4_entry[0]:class4_state4_entry[0]
                 + len(class4_state4_entry[1])]
            == class4_state4_entry[1]
        ),
        "class4_main_dispatcher_ascending_slots": (
            data[class4_main_dispatcher[0]:class4_main_dispatcher[0]
                 + len(class4_main_dispatcher[1])]
            == class4_main_dispatcher[1]
        ),
        "class4_handler_selector_slot0": (
            data[class4_handler_slot0[0]:class4_handler_slot0[0]
                 + len(class4_handler_slot0[1])]
            == class4_handler_slot0[1]
        ),
        "class4_handler_selector_slot1": (
            data[class4_handler_slot1[0]:class4_handler_slot1[0]
                 + len(class4_handler_slot1[1])]
            == class4_handler_slot1[1]
        ),
        "class4_slot1_flag_writer": (
            data[class4_slot1_flag_writer[0]:class4_slot1_flag_writer[0]
                 + len(class4_slot1_flag_writer[1])]
            == class4_slot1_flag_writer[1]
        ),
        "queue_class14_pool_capacity_60": (
            data[class14_pool_init[0]:class14_pool_init[0]
                 + len(class14_pool_init[1])] == class14_pool_init[1]
        ),
        "queue_class14_ascending_scheduler": (
            data[class14_scheduler[0]:class14_scheduler[0]
                 + len(class14_scheduler[1])] == class14_scheduler[1]
        ),
        "queue_class14_behavior0_state_dispatch": (
            data[class14_behavior0[0]:class14_behavior0[0]
                 + len(class14_behavior0[1])] == class14_behavior0[1]
        ),
        "queue_class14_horizontal_step": (
            data[class14_horizontal_step[0]:class14_horizontal_step[0]
                 + len(class14_horizontal_step[1])]
            == class14_horizontal_step[1]
        ),
        "queue_class14_reflection_flag_writer": (
            data[class14_reflection_flag_writer[0]:
                 class14_reflection_flag_writer[0]
                 + len(class14_reflection_flag_writer[1])]
            == class14_reflection_flag_writer[1]
        ),
        "queue_class14_300_update_transition": (
            data[class14_transition_entry[0]:class14_transition_entry[0]
                 + len(class14_transition_entry[1])]
            == class14_transition_entry[1]
        ),
        "queue_class14_state4_sound_entry": (
            data[class14_state4_sound_entry[0]:class14_state4_sound_entry[0]
                 + len(class14_state4_sound_entry[1])]
            == class14_state4_sound_entry[1]
        ),
        "queue_class14_state4_score_award": (
            data[class14_state4_score_award[0]:
                 class14_state4_score_award[0]
                 + len(class14_state4_score_award[1])]
            == class14_state4_score_award[1]
        ),
        "queue_class14_alternating_probe_gate": (
            data[class14_phase_gate[0]:class14_phase_gate[0]
                 + len(class14_phase_gate[1])]
            == class14_phase_gate[1]
        ),
        "queue_class14_state2_substate_dispatch": (
            data[class14_state2_substate_dispatch[0]:
                 class14_state2_substate_dispatch[0]
                 + len(class14_state2_substate_dispatch[1])]
            == class14_state2_substate_dispatch[1]
        ),
        "queue_class14_state2_substate_table": (
            data[class14_state2_substate_table[0]:
                 class14_state2_substate_table[0]
                 + len(class14_state2_substate_table[1])]
            == class14_state2_substate_table[1]
        ),
        "queue_class14_state2_substate_writers": all(
            data[address:address + len(instructions)] == instructions
            for address, instructions in class14_state2_substate_writers
        ),
        "queue_class14_overlap_dispatch": (
            data[class14_overlap_dispatch[0]:class14_overlap_dispatch[0]
                 + len(class14_overlap_dispatch[1])]
            == class14_overlap_dispatch[1]
        ),
        "queue_class14_overlap_helper": (
            data[class14_overlap_helper[0]:class14_overlap_helper[0]
                 + len(class14_overlap_helper[1])]
            == class14_overlap_helper[1]
        ),
        "queue_class14_overlap_target_selector": (
            data[class14_overlap_target_selector[0]:
                 class14_overlap_target_selector[0]
                 + len(class14_overlap_target_selector[1])]
            == class14_overlap_target_selector[1]
        ),
        "queue_class14_overlap_bounds_row0": (
            data[class14_overlap_bounds_row0[0]:
                 class14_overlap_bounds_row0[0]
                 + len(class14_overlap_bounds_row0[1])]
            == class14_overlap_bounds_row0[1]
        ),
        "queue_class14_visibility_callback": (
            data[class14_visibility_callback[0]:class14_visibility_callback[0]
                 + len(class14_visibility_callback[1])]
            == class14_visibility_callback[1]
        ),
        "queue_class14_visibility_type_dispatch": (
            data[class14_visibility_type_entry[0]:class14_visibility_type_entry[0]
                 + len(class14_visibility_type_entry[1])]
            == class14_visibility_type_entry[1]
        ),
        "queue_class14_visibility_list_dispatch": (
            data[class14_visibility_list_dispatch[0]:
                 class14_visibility_list_dispatch[0]
                 + len(class14_visibility_list_dispatch[1])]
            == class14_visibility_list_dispatch[1]
        ),
    }
    if not all(checks.values()):
        failed = ", ".join(name for name, valid in checks.items() if not valid)
        raise ValueError(f"player state-4 child spawn bytes changed: {failed}")

    selector_counts = {
        f"0x{selector:02x}": selector_table.count(selector)
        for selector in sorted(set(selector_table))
    }
    return {
        "owner_path": {
            "handler": "0x00125d0",
            "state": 4,
            "substate_dispatch_table": f"0x{state4_table_address:06x}",
            "substate": state4_substate,
            "substate_target": f"0x{state4_dispatch_target:06x}",
            "entry_effects": "queues event 0x1800, advances the shared LFSR, and selects a queue-class-14 actor descriptor",
            "spawn_order": "state-4 substate 4 counts down +34=10 at 0x130ba; expiry enters substate 6, loads +34=8, sends sound command 0x33, and attempts a queue-class-14 allocation through 0x2508 on each handler call. The 0x130e0 handler leaves parent +3 unchanged and calls 0x1f4c, which advances parent animation fields, so allocation repeats on subsequent calls until an external state change or actor retirement. A separate class-12 actor is attempted at timer expiry.",
            "native_entry_mismatch": "The parent emits queue-class-14 actors from the verified selector table and allocation path. The child model covers state-0 initialization, state-2 vertical phases, the every-eight-frame 0x488a overlap against bounds row 0, state-4 follow/timer behavior, extracted animations, and source coordinate templates for multi-part held poses. The recovery timer also creates the separate class-12 child and models its 32-tick state-2 timer and state-6 pool release. The 0x6e44 candidate-list scan, active target-list handoff, and sequenced cue playback remain incomplete. State-2 substate 6 is a handler-table overread with no verified normal Stage 0 writer.",
            "stage0_reachability": "the normal Stage 0 Willow actor uses handler 0x125d0; side contact and the timed state-2 transition both enter state 4",
        },
        "random_child_selector_table": {
            "address": f"0x{selector_table_address:06x}",
            "draw_index": "shared 0x1d8e LFSR low nibble",
            "values": list(selector_table),
            "value_counts": selector_counts,
            "descriptor_indices_to_behavior_selectors": {
                "1": 2,
                "2": 5,
            },
        },
        "queue_class14_creation": {
            "allocator": f"0x{allocator_address:06x}",
            "pool": "60 records of 64 bytes; queue class 14, stepped by 0x2cf8e",
            "pool_capacity_source": "0x28da initializes count +0x1810 to 60 and the 60-slot free list at +0x183a; each record is 64 bytes with +30=14",
            "active": 1,
            "main_handler_selector_plus_32": 0,
            "handler_table": ["0x2cfc2", "0x2cfd6", "0x2d78e"],
            "handler": "queue-class-14 scheduler 0x2cf8e dispatches +32=0 through 0x2cfc2; separate from main class-4 table at 0xffc8 and handler 0x10054",
            "scheduler_phase": "physical slots update ascending; D7 starts at 59 and decrements, so source expression (frame + D7 + 1) & 1 is equivalent to (frame + physical_slot) & 1",
            "state2_substate_dispatch": {
                "dispatcher": "0x2d0aa indexes its relative table at 0x2d0ea directly with byte +3",
                "table_words": ["0x0054", "0x007e", "0x0130"],
                "valid_substates": [0, 2, 4],
                "verified_writers": ["0x2d156 adds 2", "0x2d1cc adds 2", "0x2d1e6 clears to 0"],
                "substate_6": "out-of-range table read into instruction bytes; no verified normal Stage 0 transition writes 6",
            },
            "visibility_callback": "0x2244 applies screen bounds then dispatches +30=14 through 0x2290 to 0x2472; +14=0 pushes the actor at head a5+0x183c. The native renderer scans visible physical slots downward, matching this reverse insertion order for the zero-initialized +14 group.",
            "camera_edge_reflection_gate": {
                "reader": "0x2d0f0 tests global byte a5+0x03cf before applying the camera-edge reflection to +48",
                "writer": "0x27f08 sets a5+0x03cf only on the +42=0 branch of the surrounding actor state path at 0x27e96",
                "writer_effects": "also sets global a5+0x03e9, enters actor state 2/substate 0, clears +3/+4 and a5+0x03cc, then attempts a queue-class-6 behavior-6 allocation",
                "stage0_link": "writer bytes are validated; its owning actor creation path is not connected to normal Stage 0 setup, so native reflection remains disabled unless a verified Stage 0 route is found",
                "bytes_validated": class14_reflection_flag_writer[1].hex(),
            },
            "source_child_selector_plus_33": {
                "random_value_1": 2,
                "random_value_2": 5,
            },
            "descriptor_address": f"0x{descriptor_address:06x}",
            "created_per_call": 1,
            "coordinates": "copies parent +6/+10",
            "selector_handlers": {
                "selector_2": "+33 selects the class-14 state-0 animation and state-4 pose; it is not a generic class-4 handler selector",
                "selector_5": "+33 selects the alternate class-14 state-0 animation and state-4 pose; it is not a generic class-4 handler selector",
            },
            "native_status": "class-14 state-0 initialization, state-2 substates 0/2/4, the every-eight-frame 0x488a overlap, state-4 follow phases, state-6 release, extracted state-0/state-4 streams, and source coordinate templates for multi-part held poses are modeled. The parent recovery route creates a class-12 child with selector-5's 32-tick state-2 timer and next-update state-6 release. The 0x2d0aa state-2 substate table contains only handlers for substates 0/2/4; normal transitions write those same values. Substate 6 would index past the table into unrelated code, and no verified Stage 0 writer reaches it. The alternate 0x6e44 candidate effects, source-gated 0x2d0f0 reflection, target-list handoff, and sequenced cue playback remain incomplete",
            "parent_link": "copies parent +47 to child +47; child +46 is zero",
            "native_mismatch": "native now uses queue-class-14 sprite codes and implements state-0 initialization, state-2 vertical phases with source scheduler parity for the 0x3c48 terrain gate, 3/4 velocity decay, the every-eight-frame 0x488a overlap against the selected target rectangle, state-4 follow/retirement timers, the 0x2d0f0 +48 horizontal step, and the 0x2d2ec transition animation. Its visible-pool scan matches the 0x2244 bounds and reverse insertion order for the +14=0 list. Base pose and appended held-pose sprites use the source coordinate templates at 0x3b99a. The parent recovery path also models the class-12 selector-5 child timer and release. The gated camera-edge reflection, 0x6e44 candidate effects, target-list handoff, and sequenced sound playback remain incomplete. The state-2 substate-6 table overread is not modeled because no normal Stage 0 transition reaches it.",
        },
            "native_status": "state-4 movement, animation, probe, and substate-4 timer are modeled; substate-6 selector RNG/allocation, selector-2 behavior through shared substate-18 recovery and below-target attack recovery, and selector-5 state-0 placement, state-2 vector/attack/turn/shape/contact behavior, shared state-2 substate-8 modes 2/4 and substate-18 recovery, plus shared attack follow-ups for substates 2/6 and substate-12/14 recovery/drift are modeled for selectors 2/5, while other later selector-2/selector-5 recovery, exact visibility behavior, and class-12 cue/allocation remain incomplete",
        "unrelated_class4_state4_followup": {
            "dispatcher": "0x10054 state +2=4 routes through the +3 table at 0x10eea; common update returns through 0x2244",
            "transition_gate": "0x10c32 enters state +2=4/substate 0 only when global lock a5+0x03d0 is clear and actor bytes +42 and +43 differ; it copies +42 to +43, sets +0=2, and clears +3/+4",
            "local_flag_writers": "within the validated 0x10054-0x1116a selector-0 handler block, the only direct +42 writer is initialization at 0x102a8, which sets +42 and +43 to 1; +43 is also written back from +42 at 0x10c42, but no local instruction changes +42 after initialization",
            "other_handler_writer": "the apparent +42/+43 writer at 0x116c0 belongs to selector-1 handler 0x115ee, selected by class-4 dispatcher slot 1 at 0xffcc; the modeled Stage 0 children are allocated with selector 0 and dispatched through slot 0 at 0xffc8, so this is not a writer for their records",
            "entry_effect": "substate 0 at 0x10ef8 sends sequenced cue 0x32; if +43 is nonzero it clears +3/+4, otherwise it advances into the +0x0400 vector and recovery path",
            "native_status": "this is a separate class-4 path; the class-14 Stage 0 children use their own 0x2cf8e scheduler and 0x2cfc2 behavior-0 dispatcher, so this gate does not describe their state transitions",
        },
        "byte_validation": checks,
    }


def player_state4_class12_spawn_trace(data: bytes) -> dict[str, object]:
    """Trace the separate queue-class-12 child created at 0x130ba."""
    caller = (0x130BA, bytes.fromhex(
        "532e00226616542e00031d7c00080022303c00334eb8786a"
        "4eb8492c4ef81f4c"))
    allocator = (0x492C, bytes.fromhex(
        "4eb82b9c67f8197c00010000422c0020197c00050021"
        "396e00060006396e000a000a4e75"))
    pool_alloc = (0x2B9C, bytes.fromhex(
        "4a2d180d6712306d183438583b481834532d180d70014e7570004e75"))
    class12_selector0 = (0x2F3C4, bytes.fromhex("0003222e"))
    stage0_phase_record = (0x3078, bytes.fromhex("00c0004803000000"))
    handler_entry = (0x322E, bytes.fromhex(
        "00067000102d0370e748122d0372040100106a02720002410007"
        "d041123b0010b22d039463041b4103946000"))
    selector5_timer_byte = 0x32265
    selector0_state2_expiry = (0x32270, bytes.fromhex(
        "4eb81f4c532e002266061d7c000600024ef82244"))
    state6_release = (0x32284, bytes.fromhex("4ef82bb8"))
    scripted_target_writer = (0x3807C, bytes.fromhex(
        "4eb85630610000664a2d036267124a2d186a670c"
        "3b6d184c03703b6d18500372"))
    scripted_target_callers = [
        (0x08DF0, bytes.fromhex(
            "522d00194eb81f884eb84ea24eb900009ef2"
            "4eb900039a524eb90003807c4ef90003b524")),
        (0x0948A, bytes.fromhex(
            "4eb90003807c4eb900009ef24ef90003b524")),
        (0x09562, bytes.fromhex(
            "4eb90003807c4ef90003b524")),
    ]
    event_descriptor_writer = (0x2D89A, bytes.fromhex(
        "542e0003303c10fb4eb8786a4eb81e887000102e0021d040"
        "41fa00c03b7000000374d04041fa00d441f000003b50184c"
        "3b5003703b5804083b5018503b5003723b50040c"))
    phase_recovery_tail = (0x2FF4, bytes.fromhex(
        "302d0344d040303b005041fb004c7200122d0394e749"
        "41f0100030186b16"))
    phase_record_pair = (0x36F8, bytes.fromhex("3b408a32"))
    checks = {
        "state4_recovery_timer_callsite":
            data[caller[0]:caller[0] + len(caller[1])] == caller[1],
        "class12_actor_initializer":
            data[allocator[0]:allocator[0] + len(allocator[1])] == allocator[1],
        "class12_pool_allocator":
            data[pool_alloc[0]:pool_alloc[0] + len(pool_alloc[1])]
                == pool_alloc[1],
        "class12_selector0_dispatch_target":
            data[class12_selector0[0]:class12_selector0[0]
                 + len(class12_selector0[1])] == class12_selector0[1],
        "stage0_phase_record":
            data[stage0_phase_record[0]:stage0_phase_record[0]
                 + len(stage0_phase_record[1])] == stage0_phase_record[1],
        "class12_selector0_handler_entry":
            data[handler_entry[0]:handler_entry[0] + len(handler_entry[1])]
                == handler_entry[1],
        "class12_selector5_timer_32":
            data[selector5_timer_byte] == 0x20,
        "class12_state2_timer_expiry": (
            data[selector0_state2_expiry[0]:selector0_state2_expiry[0]
                 + len(selector0_state2_expiry[1])]
            == selector0_state2_expiry[1]
        ),
        "class12_state6_pool_release": (
            data[state6_release[0]:state6_release[0]
                 + len(state6_release[1])] == state6_release[1]
        ),
        "stage0_phase_overread_byte": data[0x385C] == 0xD0,
        "phase_recovery_tail":
            data[phase_recovery_tail[0]:phase_recovery_tail[0]
                 + len(phase_recovery_tail[1])] == phase_recovery_tail[1],
        "stage0_phase_selected_pair":
            data[phase_record_pair[0]:phase_record_pair[0]
                 + len(phase_record_pair[1])] == phase_record_pair[1],
        "scripted_target_phase_writer":
            data[scripted_target_writer[0]:scripted_target_writer[0]
                 + len(scripted_target_writer[1])] == scripted_target_writer[1],
        "scripted_target_writer_callers": all(
            data[address:address + len(instructions)] == instructions
            for address, instructions in scripted_target_callers
        ),
        "event_descriptor_phase_writer":
            data[event_descriptor_writer[0]:event_descriptor_writer[0]
                 + len(event_descriptor_writer[1])]
                == event_descriptor_writer[1],
    }
    if not all(checks.values()):
        failed = ", ".join(name for name, valid in checks.items() if not valid)
        raise ValueError(f"player state-4 class-12 spawn bytes changed: {failed}")

    return {
        "caller": {
            "address": "0x00130ba",
            "trigger": "state-4 recovery timer +34 expires",
            "calls": ["0x786a with command 0x33", "0x492c", "0x1f4c"],
        },
        "allocation": {
            "helper": "0x00492c",
            "pool_allocator": "0x002b9c",
            "pool": "queue class 12; 60 records of 64 bytes; free count a5+0x180d",
            "fields": {"active +0": 1, "dispatcher +32": 0,
                       "behavior +33": 5},
            "coordinates": "copies parent +6/+10",
        },
        "dispatch": {
            "queue_class12_dispatcher": "0x002f39e",
            "selector_table": "0x002f3c4",
            "field_plus32_value": 0,
            "handler": "0x003222e",
            "handler_summary": "state 0 sets +2=2, loads +34 from the byte table at 0x32260 indexed by +33, and installs its pose. Selector 5 reads +0x32265=0x20 (32 ticks). State 2 advances its animation, decrements +34, then sets +2=6 at expiry; state 6 calls pool release 0x2bb8. It also computes an index from a5+0x0370/+0x0372, reads at 0x325c, max-updates camera phase a5+0x0394, then branches to 0x2ff4; the valid input range is not established",
        },
        "stage0_handler_input_check": {
            "initial_phase_record": "0x003078",
            "initial_0370_0372": [192, 72],
            "handler_table_base": "0x00325c",
            "computed_lookup_index": "0x0600",
            "computed_lookup_address": "0x00385c",
            "lookup_note": "0x385c lies in executable bytes and contains 0xd0. If this handler is reached with the reset Stage 0 values, it raises camera phase byte a5+0x0394 to 0xd0 (208). The following 0x2ff4 lookup uses stage counter 0 and selects the word pair at 0x36f8, also executable bytes: (0x3b40,0x8a32). The helper writes that pair to the actor coordinates and sets a5+0x038c to 0x3940. This proves the computed overread result, but not that the recovery path reaches the handler during normal Stage 0 play.",
            "computed_stage0_result": {
                "lookup_byte": "0xd0",
                "phase_after_max_update": "0xd0",
                "stage_counter": 0,
                "phase_pair_address": "0x0036f8",
                "actor_coordinates": [15168, 35378],
                "camera_retirement_bound": 14656,
                "interpretation": "out-of-table ROM-byte behavior, not a verified enemy or intended gameplay effect",
            },
            "additional_validated_writers": {
                "scripted_target": {
                    "address": "0x003807c",
                    "writes_at": "0x0038090",
                    "gate": "a5+0x0362 and a5+0x186a must both be nonzero",
                    "values": "copies shared target X/Y from a5+0x184c/+0x1850",
                    "callers": [
                        {"address": f"0x{address:06x}",
                         "instruction_bytes": instructions.hex()}
                        for address, instructions in scripted_target_callers
                    ],
                    "stage0_flag_evidence": "a5+0x186a is cleared by the startup work-RAM reset; decoded direct accesses are reads at 0x118d0, 0x37084, and this writer gate at 0x3808a, with no direct setter identified",
                },
                "event_descriptor": {
                    "address": "0x002d89a",
                    "values": "selector-indexed event descriptor supplies a5+0x0370/+0x0372",
                    "stage0_reachability": "not established for the recovery timer path; the recovery-created queue-class-14 actor has +32=0 and dispatches through scheduler 0x2cf8e to 0x2cfc2, so it does not use this handler",
                },
            },
        },
        "native_status": "not implemented; this record is not part of the 20-slot class-2 pool",
        "byte_validation": checks,
    }


def generic_enemy_spawn_trace(data: bytes) -> dict[str, object]:
    """Validate spawn candidates with pool-specific selector dispatch."""
    stage_selector_table = bytes.fromhex(
        "0000000004000000000000000400000000000000")
    if data[0x2F72:0x2F72 + len(stage_selector_table)] != stage_selector_table:
        raise ValueError("Stage-indexed +0x386 selector table changed")
    wave_selector_records = bytes.fromhex(
        "fff000000005000800080004ffe8001000040010"
        "fff80005ffe8fff000040010002800040000ffe8"
        "0004fff000200004001800180004")
    if data[0x14264:0x14264 + len(wave_selector_records)] != wave_selector_records:
        raise ValueError("Timed wave selector table at 0x14264 changed")
    random_manager_selectors = bytes.fromhex(
        "02010203020102020202020102010203")
    if data[0x14842:0x14842 + len(random_manager_selectors)] != random_manager_selectors:
        raise ValueError("Random queue-class-10 selector table changed")
    stage_selector_values = [stage_selector_table[i] for i in range(0, 20, 4)]
    wave_selector_values = [wave_selector_records[i * 6 + 4]
                            for i in range(9)]
    if 20 in stage_selector_values + wave_selector_values + list(random_manager_selectors):
        raise ValueError("A validated non-behavior-20 selector source changed")
    global_attack_gate_reads = [
        (0x0E81E, "4a2d188e"), (0x1046C, "4a6d188e"),
        (0x10C9C, "4a6d188e"), (0x10E7E, "4a6d188e"),
        (0x14568, "4a2d188e"), (0x148BA, "4a2d188e"),
        (0x14ADC, "4a2d188e"), (0x14B10, "4a2d188e"),
        (0x1C02C, "4a2d188e"), (0x1C0AA, "4a2d188e"),
        (0x385A4, "4a6d188e"), (0x42BAC, "4a2d188e"),
    ]
    for address, expected_hex in global_attack_gate_reads:
        expected = bytes.fromhex(expected_hex)
        if data[address:address + len(expected)] != expected:
            raise ValueError(
                f"Global RAM gate +0x188e read changed at 0x{address:06x}")
    checks = {
        "startup_work_ram_clear_call": (0x5AA, bytes.fromhex(
            "60000146")),
        "startup_clear_work_ram": (0x6F2, bytes.fromhex(
            "41f900ff0000383c1fff700020c020c051ccfffa4ed4")),
        "reset_entry_selected_work_ram_initializers": (0x1400, bytes.fromhex(
            "4bf8800070003b4000203b40001e3b40001c3b40001a"
            "41ed0072303c001f72ff6100000c41ed0172303c001f720020c1")),
        "generic_actor_counter_reset": (0x2EDE, bytes.fromhex(
            "41ed8f403c3c000f700020c051cefffc")),
        "generic_actor_counter_increment_a": (0x384CA, bytes.fromhex(
            "522d8f40")),
        "generic_actor_counter_increment_b": (0x38682, bytes.fromhex(
            "522d8f40")),
        "fresh_actor_state0_vertical_gate": (0x10080, bytes.fromhex(
            "302e000a906d040c0c40014062000094")),
        "fresh_actor_state0_player_relative_position": (0x10090, bytes.fromhex(
            "3d6d04080006046e002000064a2e000f6706066e01c00006")),
        "fresh_actor_state0_contact_and_state2_path": (0x100E6, bytes.fromhex(
            "1d7c000000135f6e000a4eb83b0a66024e75302e0006322e000a"
            "4eb83734661c422e0044542e000241fa105e4eb81f302d7c0001"
            "15e6001860000188")),
        "generic_enemy_state2_dispatch": (0x102C0, bytes.fromhex(
            "4a2e004466124a6d03d2660c102e0003323b0022"
                "4ebb101e102d0019d0070240000f66044eb821c6")),
        "class2_pool_ai_dispatcher": (CLASS2_AI_DISPATCH, bytes.fromhex(
            "4ded18c63e3c00134a2e0000670000107000102e0020e548"
            "207b000e4e904dee004051cfffe44e75")),
        "class2_pool_ai_handler_table": (CLASS2_AI_TABLE, bytes.fromhex(
            "0000fb7c0000fe3e0000ff14")),
        "class2_pool_ai_state_table": (0xFB88, bytes.fromhex(
            "008800e4010a010afffe00140002")),
        "class2_behavior0_state0_initializer": (0xFC10, bytes.fromhex(
            "542e000241faff7a4a2d1884670441faffb04eb81d8e0240000f"
            "e54841f000003018d06d184c3d4000063010d06d18503d40000a"
            "4eb81d8e024000031d7b00120022d040323b000e41fb100a"
            "4ef81f40")),
        "class2_behavior0_state2_timer": (0xFC6C, bytes.fromhex(
            "102e0021323b000a4ebb10064ef822440008000800080008"
            "532e00226604542e00024ef81f4c")),
        "class2_behavior0_offsets_a": (0xFB90, bytes.fromhex(
            "fffe001400020012fffc00100004000efff8000c0008000a"
            "fff60008000a0006fff40004000c000200000000fff6fffe"
            "000afffcfff8fffa0008fff8fffbfff8")),
        "class2_behavior0_offsets_b": (0xFBD0, bytes.fromhex(
            "fffe000800020006fffc000400040002fff800000008fffe"
            "fff6fffc000afffafff4fff8000cfff60000fff4fff6fff2"
            "000afff0fff8ffee0008ffecfffbffec")),
        "class2_behavior0_timer_values": (0xFC60, bytes.fromhex("09100718")),
        "stage0_class2_shared_target_x": (0xA186, bytes.fromhex(
            "3b7c01c0184c")),
        "stage_shared_target_later_x_write": (0xA452, bytes.fromhex(
            "422d799c1b7c000103483b7c0100000a"
            "546d00083b7cffe0184c4e75")),
        "stage_shared_target_later_y_write": (0xA542, bytes.fromhex(
            "422d799c4a2d8e6e660c536d000a663e"
            "700061000080227c009102004a2d18666606"
            "227c00910c00303c001a4eb87dae1b7c0001"
            "8e6e4a2d18666706066d000818503b7c003f000a")),
        "generic_enemy_state2_behavior_dispatch_table": (0x1030A,
            bytes.fromhex(
                "102e0004323b00064efb10020010003200f2011201340144019a01b6")),
        "generic_enemy_state2_mode0_init": (0x10326, bytes.fromhex(
            "542e00041d7c003c002261000a824eb8208c4eb83b0a"
            "41fa0e304eb81f304ef81f4c")),
        "generic_enemy_state2_mode2_no_side_contact_transition": (0x103A6,
            bytes.fromhex(
                "102e000fd040303b0056d06e0006322e000a04410021"
                "4eb8373c67124a016a0e422e001f1d7c00140003"
                "4ef83b0a1d7c00080003422e00044ef83b0a")),
        "generic_enemy_state2_mode4_selector_vectors": (0x10404,
            bytes.fromhex("0010fff0")),
        "generic_enemy_state2_mode4_timer_select": (0x10408,
            bytes.fromhex(
                "4eb81d8e02400007103b000e1d40003c542e0004"
                "6000ff44")),
        "generic_enemy_state2_mode4_timer_values": (0x10420,
            bytes.fromhex("1018202028303840")),
        "generic_enemy_state2_mode6_timer_expiry": (0x10428,
            bytes.fromhex(
                "532e003c6600ff34542e00041d7c0004003c"
                "2d7c000115ce0018")),
        "generic_enemy_state2_mode6_facing_helper": (0x10442,
            bytes.fromhex("610009486000ff1a")),
        "generic_enemy_state2_mode8_hold": (0x1044A,
            bytes.fromhex("532e003c6600ff16542e00046000ff0e")),
        "generic_enemy_facing_update": (0x10D8C,
            bytes.fromhex(
                "302e0006906d184c650e4a2e000f6616"
                "1d7c0001000f600a4a2e000f6708422e000f"
                "6000fe6a4e75")),
        "generic_enemy_facing_change_saved_state": (0x10C1A,
            bytes.fromhex(
                "1d6e000300401d6e000400411d7c00100003"
                "422e00044e75")),
        "generic_enemy_state2_mode8_range_test": (0x1045A,
            bytes.fromhex("322e000a926d1850064100190c4100326520")),
        "generic_enemy_state2_mode8_mode2_fallback": (0x1048C,
            bytes.fromhex(
                "1d7c000200041d7c00010022610008f2"
                "4eb83b0a41fa0ccc4ef81f30")),
        "generic_enemy_state2_mode8_random_gate": (0x1046C,
            bytes.fromhex(
                "4a6d188e661a4eb81d8e0240000f103b002c670c"
                "542e00041d7c0020003c4e75")),
        "generic_enemy_state2_mode8_random_byte_table": (0x104A8,
            bytes.fromhex("0001010000010000532e003c6600feac")),
        "generic_enemy_state2_mode12_hold": (0x104B0,
            bytes.fromhex("532e003c6600feac542e000406000010"
                          "1d40003c41fa0e74")),
        "generic_enemy_state2_mode14_hold": (0x104CC,
            bytes.fromhex("532e003c6600fe941d7c000a0004610008d8"
                          "41fa0c8e4ef81f30")),
        "generic_enemy_state2_mode0_motion_vectors": (0x115E6,
            bytes.fromhex("0280fd00fe00fd00")),
        "generic_enemy_shared_3b0a_selector_probe": (0x3B0A,
            bytes.fromhex(
                "7000102e0013e74845fa005245f20000422e001f"
                "4a2e000f670445ea0004302e0006d05a")),
        "generic_enemy_shared_3b0a_profile0_probe_offsets": (0x3B66,
            bytes.fromhex("0008ffe7fff8ffe7")),
        "generic_enemy_state2_mode4_any_bit3_shape_probe": (0x4634,
            bytes.fromhex(
                "700012280002024100fc671ed24143fa00ce43f11000"
                "101908000003670c72004a116b0272ff70014e757000"
                "72004e75")),
        "generic_enemy_state2_mode2_proximity": (0x10348, bytes.fromhex(
            "102d0019d0070240000f660e6100092c67000008"
            "1d7c000400044eb8208c4eb81f4c")),
        "generic_enemy_state8_mode2_cull_cadence": (0x109F0,
            bytes.fromhex(
                "66000010610002a666061d7c000f00224e75"
                "4a2e00216610102d0019d0070240000f6604"
                "4eb821624a2e001f6606422e00044e75")),
        "generic_enemy_class2_queue_d7_countdown": (0x3B25C,
            bytes.fromhex(
                "7e001e2d180267f65347386d181e4a2d1806"
                "67ea3c5c4a2e00006746")),
        "generic_enemy_class2_queue_head_insertion": (0x2368,
            bytes.fromhex("306d181e310e3b48181e522d1802")),
        "generic_enemy_state2_contact_timer_order": (0x1036A, bytes.fromhex(
            "4a2e001f67364eb83b0a532e00226668"
            "1d7c003c0022322e000a926d1850064100190c4100326202")),
        "generic_enemy_state8_special_scan_vertical_gate": (0x1098A,
            bytes.fromhex(
                "302e000a906d18500640001e0c40003c6508"
                "610002fe67024e75")),
        "generic_enemy_attack_entry_gates": (0x10C9C, bytes.fromhex(
            "4a6d188e660000ba0c6d00030344670000b0"
            "302e000a906d18500640001e0c40003c6500009c")),
        "generic_enemy_attack_four_point_terrain_scan": (0x10D60,
            bytes.fromhex(
                "302e000ad16e003a7c036000000a302e0036"
                "d16e003a302e0006322e003a4eb83c406606"
                "51ceffe870004e75")),
        "generic_enemy_attack_above_player_setup": (0x10CCC, bytes.fromhex(
            "3d6e00060038610000fe6708046e002800386006"
            "066e002800383d7c0027003a3d7c00100036"
            "6100006c67641d7c00020003422e0004"
            "1d7c00100022610000aa41fa06ec4ef81f30")),
        "generic_enemy_attack_below_player_setup": (0x10D14, bytes.fromhex(
            "3d7c ffb0 003a 0c2e000400216612"
            "302e0006322e000a044100204eb83c406728"
            "3d7cfff0003661000024671c1d7c00060003"
            "422e00041d7c001000226100006241fa06c0".replace(" ", ""))),
        "generic_enemy_attack_substate_dispatch": (0x102F4,
            bytes.fromhex("001601f202ea038c0570")),
        "generic_enemy_attack_above_followup": (0x105F4,
            bytes.fromhex("610e41fa0efa4ef81f30")),
        "generic_enemy_attack_above_followup_motion": (0x10604,
            bytes.fromhex(
                "7000102e0004e548122e000f02410001e549d041"
                "41fb001c3018d16e00063010d16e000a1d7c0008"
                "0022542e00044ef81f4c")),
        "generic_enemy_attack_above_followup_vectors": (0x10636,
            bytes.fromhex(
                "0020000affe0000a00000020000000200000000a"
                "0000000afff8000000080000")),
        "generic_enemy_attack_below_followup": (0x106A8,
            bytes.fromhex("542e00041d7c0008002241fa0d7e4ef81f40")),
        "generic_enemy_attack_below_recovery_mode8": (0x10702,
            bytes.fromhex(
                "610001266100056867081d7c000c00044e754a2e0024"
                "660e4eb836f46b0008ce4eb83b0a6712542e0004"
                "1d7c0012002241fa0d6a4ef81f40")),
        "generic_enemy_attack_vertical_velocity": (0x1082A,
            bytes.fromhex(
                "0c6ef88000346e08066efff9000a4e75066effd8"
                "0034302e003448c0e180d1ae000a4e75")),
        "generic_enemy_attack_recovery_init": (0x1076C,
            bytes.fromhex(
                "542e00043d7c03800034102d185502000001"
                "1d40000f2d7c000115d600181d7c00230022"
                "422e004341fa0cd04ef81f30")),
        "generic_enemy_attack_recovery_hold": (0x1079C,
            bytes.fromhex("532e0022660a542e0004422e00244e75")),
        "generic_enemy_attack_recovery_pointer_contact": (0x107AC,
            bytes.fromhex(
                "6100007c4a2e0043660e4eb820b44eb83bc6"
                "6704522e00434e75")),
        "generic_enemy_attack_recovery_contact_handoff": (0x107C6,
            bytes.fromhex(
                "61e44a2e0024660e4eb836f46b00081a4eb83b0a"
                "6712542e00041d7c0008002241fa0c9a"
                "4ef81f30")),
        "generic_enemy_attack_recovery_terminal": (0x10800,
            bytes.fromhex(
                "532e002266222d7c000115ce00184a2e001f"
                "660c1d7c00080003422e00044e75422e0003"
                "422e00044e75")),
        "generic_enemy_attack_pointer_x_motion": (0x20B4,
            bytes.fromhex(
                "102e000f206e00184880d000d00041f00000"
                "301048c0e180d1ae00064e75")),
        "generic_enemy_attack_pointer_x_vectors": (0x115D6,
            bytes.fromhex("0240fe80fdc0fe800180fe80fe80fe80")),
        "generic_enemy_attack_recovery_contact": (0x3BC6,
            bytes.fromhex(
                "7000102e002041fa003a103000006b30122e"
                "000f02410001e549d04145fa004c45f20000"
                "302e0006d05a3b408a32322e000ad2523b41"
                "8a3461000228600006f64e75")),
        "generic_enemy_attack_recovery_probe_offsets": (0x3C08,
            bytes.fromhex(
                "00ffff00ffffffffffffffffffffffffffffffffffffffffffff08"
                "ffffffffffffffffffffff")),
        "generic_enemy_attack_recovery_points": (0x3C30,
            bytes.fromhex("0002ffe7fffeffe7")),
        "generic_enemy_state2_proximity_test": (0x10C70, bytes.fromhex(
            "302e000a906d1850064000220c4000446416302e0006"
            "906d184c0640001c0c400038640470014e7570004e75")),
        "fresh_actor_state6_exit": (0x10122, bytes.fromhex(
            "1d7c000600024e75")),
        "fresh_actor_state6_mode_counter": (0x1115A, bytes.fromhex(
            "4a2e0021660a532d8f406a04422d8f404ef82a36")),
        "randomized_child_spawn": (0x27196, bytes.fromhex(
            "303c00334eb8786a4eb82b9c6748197c00000020"
            "196e00210021197c00010000396e00060006396e000a000a"
            "4eb81d8e0240000fd040303b0020d16c00064eb81d8e"
            "0240000fd040303b000ed16c000a066c0030000a4e75")),
        "timed_child_spawn": (0x33B4C, bytes.fromhex(
            "532e00306614532e0022670a1d7c0002003060000008"
            "582e00024e75")),
        "timed_child_creation": (0x33B68, bytes.fromhex(
            "4eb82b9c6742197c00000020197c00020021197c00010000"
            "396e00060006396e000a000a4eb81d8e0240000fd040"
            "303b001ad16c00064eb81d8e0240000fd040303b0008"
            "d16c000a")),
    }
    for name, (address, expected) in checks.items():
        if data[address:address + len(expected)] != expected:
            raise ValueError(f"Generic enemy spawn source context changed: {name}")
    randomized_parent = bytes.fromhex(
        "532e004466261d7c00060044532e00456736"
        "4eb81d8e0240000f103b00121d400021"
        "610000444ef90001663a"
    )
    if data[0x2712E:0x2712E + len(randomized_parent)] != randomized_parent:
        raise ValueError("Randomized generic-child timer caller changed")
    behavior9_checks = {
        "parent_0x15832": (0x15B82, bytes.fromhex(
            "4eb82a1a6728197c00010000197c00090020"
            "197c00010021196e000f000f396e00060006"
            "396e000a000a")),
        "parent_0x18bd8": (0x18E82, bytes.fromhex(
            "4eb82b4e6728197c00010000197c00090020"
            "396e00060006396e000a000a046c00100006"
            "046c0018000a")),
        "parent_0x1995e": (0x194F0, bytes.fromhex(
            "70004eb82b4e6742197c00010000197c00090020"
            "396e00060006396e000a000a046c00100006")),
        "parent_0x1a4e2": (0x1A646, bytes.fromhex(
            "4eb8786a4eb82b9c6718197c00010000"
            "197c00090020396e00060006396e000a000a")),
    }
    for name, (address, expected) in behavior9_checks.items():
        if data[address:address + len(expected)] != expected:
            raise ValueError(f"Behavior-9 spawn source context changed: {name}")
    parent_actor_checks = {
        "behavior_15_parent": (0x49E6, bytes.fromhex(
            "4eb82b9c671c197c00010000197c000f0020"
            "396e00060006396e000a000a")),
        "behavior_15_facing_parent": (0x4A0A, bytes.fromhex(
            "4eb82b9c67f8197c00010000197c000f0020"
            "197c00020021396e00060006396e000a")),
        "behavior_16_parent": (0x255AC, bytes.fromhex(
            "4eb82b9c6718396e00060006396e000a000a"
            "197c00010000197c001000204e75")),
    }
    for name, (address, expected) in parent_actor_checks.items():
        if data[address:address + len(expected)] != expected:
            raise ValueError(f"Parent actor creation source context changed: {name}")
    behavior20_spawner_chain = {
        "class6_scheduler": (0x2F34C, bytes.fromhex(
            "4ded36c63e3c002f4a2e0000670e7000"
            "102e0020e548207b000e4e904dee004051cfffe64e75")),
        "class6_behavior2_handler_entry": (0x2F37A,
            bytes.fromhex("0002f740")),
        "class6_behavior2_state_dispatch": (0x2F740, bytes.fromhex(
            "102e0002323b00064efb1002")),
        "class6_behavior2_state_table": (0x2F74C,
            bytes.fromhex("0008011804960496")),
        "behavior20_parent_substate_dispatch": (0x2F870, bytes.fromhex(
            "102e0003323b00324ebb102e")),
        "behavior20_parent_substate_table": (0x2F8A8,
            bytes.fromhex("0008001800460228")),
        "behavior20_parent_dispatch": (0x2FAD0, bytes.fromhex(
            "102e0004323b001c4efb1018")),
        "behavior20_parent_state_table": (0x2FAF2, bytes.fromhex(
            "000e00340056006e00c000dc00ee")),
        "behavior20_collision_gate": (0x2FB60, bytes.fromhex(
            "302e003848c0e18091ae000a302e0038064000300c400600"
            "6504303c06003d4000384eb83c4867281b7c00018f0a"
            "542e00044eb82b9c")),
        "behavior20_creator": (0x2FB92, bytes.fromhex(
            "4eb82b9c6718197c00010000197c00140020"
            "396e00060006396e000a000a4e75")),
        "behavior20_dispatch": (0x1B268, bytes.fromhex(
            "102e0021d040323b00064efb1002")),
        "behavior20_state_dispatch": (0x1B27A, bytes.fromhex(
            "102e0002323b00064efb1002")),
        "behavior20_child_dispatch": (0x1B28E, bytes.fromhex(
            "102e0003323b00064efb1002")),
        "behavior20_child_route_a": (0x1B29E, bytes.fromhex(
            "542e00034ef849e6")),
        "behavior20_child_route_b": (0x1B65C, bytes.fromhex(
            "542e00034ef849e6")),
    }
    for name, (address, expected) in behavior20_spawner_chain.items():
        if data[address:address + len(expected)] != expected:
            raise ValueError(f"Behavior-20 enemy-spawner parent path changed: {name}")
    behavior14_checks = {
        "manager_pool": (0x1F5D6, bytes.fromhex(
            "4eb82b4e6728197c00010000197c000e0020"
            "102e0033550019400021196e000f000f"
            "396e00060006396e000a000a4e75")),
        "queue_class_4": (0x216B0, bytes.fromhex(
            "4eb82a1a6740197c00010000197c000e0020"
            "197c000a0021102e00210c00000466027000"
            "d12c0021196e000f000f396e00060006")),
        "queue_class_12_offset_spawn": (0x2F7C8, bytes.fromhex(
            "4eb82b9c6756197c00010000197c000e0020"
            "302e00060640007039400006302e000a"
            "0640000b3940000a394e0026")),
        "queue_class_12_pair": (0x34BF6, bytes.fromhex(
            "4eb82b9c674a197c00010000197c000e0020"
            "197c00020021396e00060006396e000a000a"
            "394e00264eb82b9c6722197c00010000"
            "197c000d0020197c00020021")),
    }
    for name, (address, expected) in behavior14_checks.items():
        if data[address:address + len(expected)] != expected:
            raise ValueError(f"Behavior-14 creator source context changed: {name}")
    manager_pool_setup = bytes.fromhex(
        "41ed123e4ded47c61b7c0014180a303c0013"
        "224e3e3c000f22c151cffffc1d7c000a001e"
    )
    if data[0x2724:0x2724 + len(manager_pool_setup)] != manager_pool_setup:
        raise ValueError("Queue-class-10 manager pool setup changed")
    manager_dispatch = bytes.fromhex(
        "4ded47c63e3c00134a2e0000670e7000102e0020"
        "e548207b000e4e904dee004051cfffe6"
    )
    if data[0x23516:0x23516 + len(manager_dispatch)] != manager_dispatch:
        raise ValueError("Manager pool dispatcher at 0x23516 changed")
    if data[0x23560:0x23564] != bytes.fromhex("00024a04"):
        raise ValueError("Manager selector 9 no longer targets 0x27f40")
    if data[0x2353C:0x23540] != bytes.fromhex("0002359c"):
        raise ValueError("Queue-class-10 selector 0 no longer targets 0x2359c")
    class10_selector0_entry = bytes.fromhex(
        "102e0021d040323b00064efb1002000400be"
        "102e0002323b00064efb10020008002e021c0298"
    )
    if data[0x2359C:0x2359C + len(class10_selector0_entry)] != class10_selector0_entry:
        raise ValueError("Queue-class-10 selector-0 handler at 0x2359c changed")
    class10_selector0_init = bytes.fromhex(
        "542e00021d7c00010022422e0030422e002e"
        "2d7c000ed524001841fa02784eb81f304ef82162"
    )
    if data[0x235C2:0x235C2 + len(class10_selector0_init)] != class10_selector0_init:
        raise ValueError("Queue-class-10 selector-0 initial state changed")
    class10_selector0_timed_state = bytes.fromhex(
        "532e0022660a1d7c00400022542e00034e75"
        "532e00226714102d0019d007024000036624"
        "4eb83d9066084e751d7c00ff00301d7c0002002e"
        "1d7c000200001d7c00040002422e00034e75"
    )
    if data[0x2361E:0x2361E + len(class10_selector0_timed_state)] != class10_selector0_timed_state:
        raise ValueError("Queue-class-10 selector-0 state-2 timer/collision path changed")
    class10_selector0_terminal = bytes.fromhex(
        "542e00031d7c000c002241fa02c84ef81f30"
        "532e00226604542e00024ef81f4c"
    )
    if data[0x23832:0x23832 + len(class10_selector0_terminal)] != class10_selector0_terminal:
        raise ValueError("Queue-class-10 selector-0 terminal state/cleanup path changed")
    manager_spawner = bytes.fromhex(
        "536e00226756532e0037664e1d7c00100037"
        "4eb82b9c6742197c00010000422c0020197c0004"
        "0021"
    )
    if data[0x27FAA:0x27FAA + len(manager_spawner)] != manager_spawner:
        raise ValueError("Manager selector-9 recurring spawner changed")
    manager_behavior16_gate = bytes.fromhex(
        "102d0019d00712000200000367064eb836f46b0e"
        "0201000767044eb821624ef82244"
    )
    if data[0x25586:0x25586 + len(manager_behavior16_gate)] != manager_behavior16_gate:
        raise ValueError("Behavior-16 manager spawn gate at 0x25586 changed")
    manager_index11_parent = bytes.fromhex(
        "4eb82b4e671c197c00010000197c000b0020"
        "396e00060006396e000a000a1946000f"
    )
    if data[0x27D2E:0x27D2E + len(manager_index11_parent)] != manager_index11_parent:
        raise ValueError("Queue-class-10 selector-11 parent spawn at 0x27d2e changed")
    behavior18_actor_setup = bytes.fromhex(
        "4eb82b9c671c197c00010000197c00120020"
        "396e00060006396e000a000a294e0026"
    )
    if data[0x27CDA:0x27CDA + len(behavior18_actor_setup)] != behavior18_actor_setup:
        raise ValueError("Behavior-18 actor setup at 0x27cda changed")
    manager_index7_parent = bytes.fromhex(
        "4eb82b4e674a197c00010000197c00070020"
        "7000102e000f1940000f396e00060006"
        "d040303b0040d16c0006396e000a000a"
        "066c0008000a422c003a422c004d294e0026"
    )
    if data[0x17B3A:0x17B3A + len(manager_index7_parent)] != manager_index7_parent:
        raise ValueError("Queue-class-10 selector-7 parent spawn at 0x17b3a changed")
    class12_dispatcher = bytes.fromhex(
        "4ded4cc63e3c003b4a2e0000670e7000102e0020"
        "e548207b000e4e904dee004051cfffe64e75"
    )
    if data[0x2F39E:0x2F39E + len(class12_dispatcher)] != class12_dispatcher:
        raise ValueError("Queue-class-12 dispatcher at 0x2f39e changed")
    class12_targets = {
        9: 0x0001A6D4,
        13: 0x0002FDC8,
        14: 0x0002FF06,
        15: 0x00030116,
        16: 0x000368EA,
        18: 0x000369E4,
    }
    for selector, target in class12_targets.items():
        offset = 0x2F3C4 + selector * 4
        if int.from_bytes(data[offset:offset + 4], "big") != target:
            raise ValueError(f"Queue-class-12 selector {selector} dispatch changed")
    queue10_behavior20_checks = {
        "queue_class10_dispatcher": (0x23516, bytes.fromhex(
            "4ded47c63e3c00134a2e0000670e7000102e0020"
            "e548207b000e4e904dee004051cfffe64e75")),
        "selector20_dispatch": (0x2358C, bytes.fromhex("00026a1c")),
        "wave_creator": (0x26A64, bytes.fromhex(
            "4eb82b4e197c00140020197c0002000022cc51ceffec")),
        "timed_creator": (0x2C150, bytes.fromhex(
            "4eb82b4e673c197c00020000197c00140020422c000e"
            "0c2e000200216706197c0002000e196e000f000f"
            "1942002e")),
    }
    for name, (address, expected) in queue10_behavior20_checks.items():
        if data[address:address + len(expected)] != expected:
            raise ValueError(f"Queue-class-10 behavior-20 context changed: {name}")
    class12_selector9_states = {
        "entry": (0x1A6D4, bytes.fromhex(
            "102e0002323b00064efb10020008001e003e0044")),
        "initialize": (0x1A6E8, bytes.fromhex(
            "542e00021d7c00180022426e003041fa00ee4ef81f40")),
        "timed_motion": (0x1A6FE, bytes.fromhex(
            "532e00226604542e0002066effe00030302e0030"
            "48c0e180d1ae000a4ef82244")),
        "finish": (0x1A71E, bytes.fromhex("542e00024e75")),
        "deactivate": (0x1A724, bytes.fromhex("4ef82bb8")),
    }
    for name, (address, expected) in class12_selector9_states.items():
        if data[address:address + len(expected)] != expected:
            raise ValueError(f"Queue-class-12 selector-9 lifecycle changed: {name}")
    class12_handler_entries = {
        15: (0x30116, bytes.fromhex(
            "102e0002323b00064efb10020008002efc54fc54"
            "2d7c000ed07800181d7c0004000e1d7c00010012"
            "3d7c001d0010542e0002")),
        16: (0x368EA, bytes.fromhex(
            "102e0002323b00064efb10020008001a00480048"
            "542e00021b7c000103e3")),
        18: (0x369E4, bytes.fromhex(
            "102e0002323b00064efb100200080020ff4eff4e"
            "542e0002286e0026102c00301d400030")),
    }
    for selector, (address, expected) in class12_handler_entries.items():
        if data[address:address + len(expected)] != expected:
            raise ValueError(f"Queue-class-12 selector {selector} handler entry changed")
    selector16_flag_trace = {
        "writer": (0x36902, bytes.fromhex("1b7c000103e3")),
        "generic_enemy_flag_branch": (0x1136, bytes.fromhex(
            "4a2d03e36700fb1a542e0003")),
        "generic_enemy_wait": (0x11E4, bytes.fromhex(
            "4a2d03e36604552e00034e75")),
        "generic_enemy_clear": (0x1210, bytes.fromhex(
            "422d03e34e75")),
        "generic_enemy_recheck": (0x1216, bytes.fromhex(
            "4a2d03e366061d7c000400024e75")),
    }
    for name, (address, expected) in selector16_flag_trace.items():
        if data[address:address + len(expected)] != expected:
            raise ValueError(f"Class-12 selector-16 flag path changed: {name}")
    return {
        "source_contexts_validated": True,
        "generic_attack_global_gate": {
            "work_ram_offset": "a5+0x188e (word at 0x10c9c)",
            "direct_read_sites": [f"0x{address:06x}"
                                  for address, _ in global_attack_gate_reads],
            "direct_write_sites_found": [],
            "startup_initialization": "startup calls the clear helper at 0x6f2 from 0x5aa; it clears 8192 longwords from 0xff0000 through 0xffffff. With A5=0xffff8000, a5+0x188e resolves to 0xff988e and is zero before gameplay. The game reset entry 0x1400 then initializes selected fields and pools. No direct writer to a5+0x188e is present among the traced references; indirect runtime writes remain unverified",
            "known_read_contexts": {
                "0x00e81e": "selects one of three animation tables for an actor",
                "0x01046c": "gates a timed behavior transition",
                "0x010c9c": "blocks the generic enemy attack scan when nonzero",
                "0x010e7e": "gates a separate proximity/attack transition",
                "0x014568": "gates an actor behavior branch",
                "0x0148ba": "blocks a class-2 actor allocation when nonzero",
                "0x014adc": "when clear, applies the Willow Y delta to a tracked actor; when set, skips that vertical follow update",
                "0x014b10": "blocks an eight-frame position follow update when nonzero",
                "0x01c02c": "gates an actor state branch",
                "0x01c0aa": "gates an actor state branch",
                "0x0385a4": "selects whether a created actor's Y comes from Willow or the stage work area",
                "0x042bac": "gates an actor state branch",
            },
            "value_status": "verified zero after startup RAM clear at 0x6f2, called from reset path 0x5aa; direct reads affect actor creation, vertical follow, and behavior; no direct writer is present in the traced code, but indirect runtime writes remain unverified",
        },
        "dispatcher_behavior_index": 0,
        "handler_address": "0x010054",
        "class2_cadence_dispatch": {
            "spawn_pool": {
                "initializer": "0x00293e",
                "base": "a5+0x18c6",
                "records": 20,
                "record_bytes": 64,
                "allocation": "0x0029d0",
                "first_free_record": 19,
            },
            "nearby_queue_walker": {
                "address": "0x003b25c",
                "purpose": "pairwise contact scan over the class-2 active list",
                "d7": "outer-list count minus one; decremented while walking that list",
                "ai_dispatch_link": "not established; this routine dispatches contact callbacks, not 0x10348",
            },
            "separate_fixed_object_dispatcher": {
                "address": "0x00ffa2",
                "base": "a5+0x1dc6",
                "records": 50,
                "record_bytes": 128,
                "d7": "starts at 49 and decrements across the fixed pool",
                "table": "0x00ffc8",
                "handler_0": "0x010054",
                "class2_pool_link": "not established; its d7 must not be substituted for the class-2 queue cadence",
            },
            "active_pool_ai_dispatcher": {
                "address": "0x00fb48",
                "record_base": "a5+0x18c6",
                "record_bytes": 64,
                "record_count": CLASS2_AI_RECORD_COUNT,
                "iteration": "d7 starts at 19, advances by 64 bytes, and decrements through 0",
                "active_check": "word at record +0 must be nonzero",
                "behavior_field": "byte at record +32; indexed longword table at 0x00fb70",
                "behavior_0_handler": "0x00fb7c",
                "behavior_1_handler": "0x00fe3e",
                "behavior_2_handler": "0x00ff14",
                "handler_0_dispatch": "state word +2 indexes the handler table embedded in 0x00fb7c; substate word +6 selects entries",
                "d7_semantics": "19 minus zero-based record index, passed to the selected behavior handler",
                "native_d7_mapping": "native slot 19-i corresponds to source record address base+64*i and receives d7=19-i; the source scheduler bytes and LIFO free-list initialization are validated",
                "byte_evidence": "4d ed 18 c6 3e 3c 00 13 ... 51 cf ff e4 4e 75; table at 0x00fb70 is 0000fb7c 0000fe3e 0000ff14",
                "stage0_shared_target": {
                    "normal_stage0_initial_value": [0, 0],
                    "normal_stage0_basis": "the normal initializer at 0x00a11c writes player/camera fields but does not write a5+0x184c/+0x1850; the active fixed controller setup at 0x002f08 sets record byte +0 only, leaving target coordinates at the reset value",
                    "x_write": "0x00a186 writes 448 to a5+0x184c only in the stage-counter-3 branch",
                    "y_value": "a5+0x1850 remains zero after the startup RAM clear on this Stage 0 path",
                    "later_writes": [
                        {
                            "offset": "0x00a466",
                            "field": "a5+0x184c",
                            "value": -32,
                            "context": "state handler at 0x00a452 also sets a 256-tick timer and advances controller word a5+8",
                            "stage0_reachability": "not established",
                        },
                        {
                            "offset": "0x00a57e",
                            "field": "a5+0x1850",
                            "effect": "adds 8 when byte a5+0x1866 is nonzero",
                            "context": "state handler at 0x00a542 advances controller word a5+8 and loads a 63-tick timer",
                            "stage0_reachability": "not established",
                        },
                    ],
                    "offset_selector": "a5+0x1884 is zero after reset; traced reads are at 0x00e828, 0x00fc18, and 0x00fe9c, with no direct write found",
                    "active_offset_table": "0x00fb90",
                },
                "link_to_charge_spawner": "the spawner allocates from this same pool and writes behavior byte +32 = 0; its records therefore enter behavior-0 handler 0x00fb7c",
                "behavior_0_handler_state0": {
                    "state_table": "0x00fb88; word offsets 0x0088, 0x00e4, 0x010a, 0x010a, ...",
                    "state_0_target": "0x00fc10",
                    "state_0_effects": "increments record state word +2; selects one of two 16-pair coordinate-offset tables using global byte a5+0x1884; uses shared LFSR masked to 15 for a pair; adds that pair to global words a5+0x184c/+0x1850 and writes record +6/+10; draws LFSR again masked to 3, loads +34 from 9,16,7,24; selects an animation stream from record byte +33 and jumps to animation setup helper 0x1f40",
                    "state_2_target": "0x00fc6c",
                    "state_2_effects": "dispatches by record byte +33; the common path decrements byte +34, increments state word +2 by two at expiry, tail-calls animation advance helper 0x1f4c every update, then returns through visibility helper 0x2244",
                    "state_4_target": "0x00fc92",
                    "state_4_effects": "deactivates/transfers record through helper 0x29ec",
                },
            },
            "native_cadence_status": "native class-2 dispatch follows physical pool slots 19 through 0; each slot supplies the matching source d7 value for staggered checks such as (global frame + d7) modulo 16; animation-helper internal cadence remains under trace",
        },
        "charge_spawner_first_allocation": {
            "allocation_site": "0x00c6d8",
            "pool": "queue class 2, 20 records of 64 bytes",
            "writes": "active type 1, behavior byte +32 = 0, randomized mode byte +33 = LFSR & 6; no X/Y writes",
            "fresh_record_coordinates": [0, 0],
            "first_state_gate": "the charge allocation uses the 20-record class-2 scheduler at 0x00fb48, so behavior 0 dispatches to 0x00fb7c; handler 0x010054 belongs to the separate 50-record fixed-object pool",
            "stage0_outcome": "the active class-2 scheduler enters 0x00fc10; normal Stage 0's shared target begins at (0,0), so the handler writes one of the 0x00fb90 offsets and a random lifetime without the 0x010054 vertical gate",
            "state0_followup": "0x00fc10 advances state to 2, chooses the 0x00fb90/0x00fbd0 offset table from a5+0x1884, draws an offset and a timer, then initializes an animation stream; state 2 at 0x00fc6c counts down and state 4 at 0x00fc92 frees via 0x0029ec",
            "state2_mode0": "the 0x10326 mode-0 initializer belongs to handler 0x010054 in the separate fixed-object pool and is not attributed to these class-2 allocations",
            "state2_mode2_no_side_contact": "after mode-2 motion/animation, a clear previous side-contact flag falls through to 0x103a6; it probes X +/-16 and Y-33 with 0x373c/0x4634, stays in mode 2 at substate 20 when D1 is negative, or sets substate 8 and clears behavior mode otherwise; both branches run 0x3b0a",
            "state2_mode4": "dispatch mode 4 at 0x10408 selects a 16/24/32/32/40/48/56/64 tick hold from 0x10420, increments behavior mode to 6, then continues the shared 0x10362 movement/animation path that update",
            "state2_mode6": "dispatch mode 6 at 0x10428 decrements +60; at zero it increments behavior mode to 8, reloads +60=4, updates facing through 0x10d8c, and continues the shared movement/animation path; if facing changes, 0x10c1a saves +3/+4 to +64/+65 and enters substate 16/behavior mode 0",
            "state2_mode8": "at 0x1044a, decrements +60 and advances to behavior mode 10 when it reaches zero",
            "state2_mode10": "at 0x1045a, unsigned actorY - sharedTargetY (a5+0x1850) +25 below 50 falls back to behavior mode 2, timer +34=1, facing update, 0x3b0a, and 0x1116e animation; otherwise a5+0x188e gates an LFSR lookup at 0x104a8, where a nonzero byte advances to mode 12 and loads +60=32",
            "state2_mode12": "at 0x104b0, decrements +60 and on expiry advances to mode 14, loads +60=16, and selects animation 0x1133a",
            "state2_mode14": "at 0x104cc, decrements +60 and on expiry returns to mode 10, updates facing, and selects animation 0x1116e",
            "shared_terrain_probe_0x3b0a": "selects profile from +19 and its side point from movement selector +15; the center 0x40a0 probe uses the same profile Y and writes its shape correction into actor Y",
            "state2_mode2": "mode +4 = 2 at 0x10348 checks 0x10c82 when (global frame + d7) modulo 16 is zero, then applies 0x208c motion and animation; the class-2 scheduler's physical pool slot supplies d7 as 19 minus its zero-based slot index; a passing proximity test selects mode 4",
            "state2_mode2_contact_response": "after motion and animation, 0x1036a checks the previous side-contact flag; when set it runs 0x3b0a, decrements byte +34, reloads 60 at zero, and compares unsigned actorY - sharedTargetY (a5+0x1850) + 25 against 50; outside that window it consumes one RNG value and indexes the 16-byte table at 0x103e4",
            "state8_special_scan_vertical_gate": "0x1098a computes unsigned actorY - sharedTargetY (a5+0x1850) + 30; carry from compare with 60 skips the attack scan, while values >=60 call 0x10c9c. The catalog validates these source bytes; native gating uses the same >=60 condition",
            "attack_entry_0x10c9c": "the RNG table's nonzero entries enter 0x10c9c, which requires global a5+0x188e=0, stage a5+0x0344!=3, and unsigned actorY - sharedTargetY (a5+0x1850) + 30 >= 60; above-target and below-target paths scan four terrain points via 0x3c40 and can change this enemy's substate, timer, and animation; this path does not call the player hurt/knockback handler, so its attack pose alone is not a verified player hit",
            "attack_scan_four_point_loop": "byte-validated at 0x10d60: initializes the first sample at actorY + object offset +10, then checks three further Y samples using +54; each iteration calls 0x3c40 with actor X. It returns zero after four clear terrain checks. This routine establishes a terrain test, not a Willow hitbox test",
            "attack_followup_substates": "a successful above-target scan sets substate +3=2, +4=0, +34=16, updates facing, and selects 0x113fa; the dispatcher maps +3=2 to 0x104e6, which runs the 0x114ba entry stream and then the 0x114d6 rising stream. Substate +3=4 dispatches to 0x105de and runs the distinct 0x10604 vector phases using 0x114f2. A successful below-target scan sets substate +3=6, +4=0, +34=16, updates facing, and selects 0x11416; its next mode-0 entry at 0x106a8 advances +4 to 2, sets +34=8, and selects 0x11432. The native runtime distinguishes the +3=2 entry/rise sequence from the +3=4 vector phases; the below-target 16-tick hold, 8-tick pose, signed velocity ramp, 24-tick 0x1145a hold, and mode-8 velocity/proximity/contact path from 0x10702 use the 0x1149e recovery frames; the source mode-12 setup at 0x1076c loads +52=896, selects the 0x115d6 pointer from the global frame bit, and waits 35 ticks; helper 0x20b4 adds signed +0x0240/-0x0240 X motion in 8.8 units; 0x3bc6 probes actor X +/-2 and Y-25 using collision class +32. The 0x11482 contact/setup frame renders four components from descriptor 0x3330 and origin template 0x0010; the 0x2162 cull bounds and 0x115ce vector motion for mode 6, substate 18, substate 20, and state-8 mode 6 have been added to the native class-2 model; scheduler d7 is the physical pool slot value (19 minus zero-based slot index). The selectors and initializers for other +24 vectors remain under trace",
            "state2_proximity_test": "0x10c82 returns true when unsigned actorY - sharedTargetY (a5+0x1850) + 34 is below 68 and unsigned actorX - sharedTargetX (a5+0x184c) + 28 is below 56",
            "state6_mode_counter": "the byte at a5-0x8f40 is initialized to zero by 0x2ede; state 6 decrements it only when actor byte +33 is zero, clamps negative underflow to zero, then deactivates through 0x002a36",
            "state6_pool_transfer": {
                "free_helper": "0x002a36",
                "destination_free_count": "a5+0x1801",
                "destination_list_head": "a5+0x181c",
                "spawn_pool_free_count": "a5+0x17fe",
                "spawn_pool_list_head": "a5+0x1816",
                "returns_to_spawn_pool": False,
                "coordinates_preserved": ["+6", "+10"],
            },
            "other_counter_writers": "source handlers at 0x384ca and 0x38682 increment the same byte; their parent activation routes are not yet connected to the normal Stage 0 script",
            "deactivation": "state 6 at 0x01115a deactivates the object through 0x002a36 before it can appear on screen",
            "native_runtime_effect": "charge-created behavior-0 records dispatch through the source-backed 0x00fb7c lifecycle in native code; the separate generic-enemy fallback remains unconnected to a verified Stage 0 spawn and must not be treated as recurring Stage 0 enemy behavior",
        },
        "behavior20_spawner_parent_chain": {
            "pool_handler": "queue-class-6 scheduler at 0x2f34c dispatches selector 2 through 0x2f372 to handler 0x2f740",
            "parent_state_dispatch": "handler 0x2f740 indexes table 0x2f74c with actor byte +2; state 2 reaches 0x2f864, which dispatches byte +3 through table 0x2f8a8",
            "parent_substate_dispatch": "routine 0x2f870 indexes table 0x2f8a8 with actor byte +3; entries for states 0/2/4/6 target 0x2f8b0/0x2f8c0/0x2f8ee/0x2fad0, so state 6 reaches the behavior-20 parent dispatcher",
            "candidate_parent_dispatch": "routine 0x2fad0 indexes table 0x2faf2 with byte +4; state 6 dispatches to the collision-and-spawn handler at 0x2fb60, which can allocate behavior 20 at 0x2fb92. The containing actor's creation and owner pool remain unresolved",
            "candidate_selector2_creation": "class-6 allocation at 0x398b4 copies parent byte +16 into the new record's selector +32; it could create selector 2 only when parent +16 equals 2, but the parent owner and value are unresolved",
            "parent_creator": "0x002fb92 allocates an active main-pool behavior-20 actor at the caller's X/Y",
            "creation_gate": "the path moves the current actor vertically, calls collision helper 0x003c48, and only allocates behavior 20 when the helper returns nonzero; it sets global byte a5+0x8f0a first",
            "parent_handler": "main behavior table selector 20 resolves to 0x001b268",
            "child_routes": ["0x001b2a2", "0x001b660"],
            "child_creator": "both routes jump to 0x0049e6, which allocates main behavior 15 at the parent position and stores the parent pointer",
            "child_handler": "main behavior table selector 15 resolves to 0x0018bd8",
            "recurring_spawner": "behavior 15 creates queue-class-10 selector 9 at 0x0018e82; that manager dispatches to 0x0027f40 and attempts generic enemy creation every 16 updates",
            "stage0_reachability": "the nested queue-class-6 selector-2 state path through 0x2f740, 0x2f870, 0x2fad0, and 0x2fb60 to the behavior-20 creator at 0x2fb92 is byte-validated; 0x398b4 is a candidate selector-2 creation site when its parent +16 equals 2, but that parent's owner, value, and Stage 0 activation remain unestablished, so keep the chain disabled in the native runtime",
        },
        "behavior20_dynamic_selector_sources": {
            "stage_indexed_selector_table": {
                "address": "0x002f72",
                "values": stage_selector_values,
                "writer": "0x002f08 copies the selected stage row into a5+0x0386; handlers 0x002f46/0x002f5e copy that selector to new main-pool actors",
            },
            "timed_wave_table": {
                "address": "0x0014264",
                "selector_values": wave_selector_values,
                "writer": "0x0014212 copies a table selector to each allocated main-pool actor",
            },
            "random_queue_class10_table": {
                "address": "0x0014842",
                "selector_values": list(random_manager_selectors),
                "writer": "0x001486e and 0x00148c6 copy the selected value to a queue-class-10 record",
            },
            "result": "these validated dynamic selector sources produce only 0 through 4; they do not seed behavior 20. This does not rule out other computed writes or reused records",
        },
        "queue_class10_behavior20_creation_sites": {
            "pool": "queue class 10 at 0x0047c6; 20 records of 64 bytes",
            "dispatcher": "0x0023516 scans all 20 records and dispatches selector +32 through table 0x002353c",
            "selector20_handler": "0x0026a1c",
            "wave_creator": {
                "site": "0x0026a64",
                "allocator": "0x002b4e",
                "record_setup": "sets selector +32 = 20 and active type 2; reserves a linked set of records, then assigns directional positions from table 0x0026b0c",
                "parent_context": "handler's current record +46 selects the wave size; +15 selects the directional offset table",
            },
            "timed_creator": {
                "site": "0x002c150",
                "allocator": "0x002b4e",
                "record_setup": "sets active type 2 and selector +32 = 20, copies directional and subtype fields, then applies a position offset from table 0x002c198",
                "parent_context": "state 2 substate 2 increments parent +46 on a ten-tick timer and attempts allocation after the count reaches two",
            },
            "stage0_reachability": "both queue-class-10 creation sites are source-byte validated; their parent activation paths are not linked to the initial Stage 0 setup, so neither is enabled natively",
        },
        "randomized_child": {
            "spawn_helper": "0x0027196",
            "known_timer_caller": {
                "routine": "0x002712e",
                "timer": "decrements parent +68; reloads 6 and decrements spawn count +69 at expiry",
                "subtype_selection": "LFSR low nibble indexes the table at 0x2715c and writes child byte +33",
                "allocator_call": "0x0027150",
                "post_spawn_transfer": "jumps to 0x001663a",
                "parent_route": "queue-class-18 selector 0 dispatches to 0x26de8; state +2 = 4 selects 0x270ac and substate +3 = 4 selects 0x27102, whose active timer path reaches this spawner",
                "state4_gate": "helper 0x27048 is one state-4 path that requires parent byte +52 nonzero; the attached type-10 child also writes parent state +2=4/+3=2 directly when its own byte +42 is zero",
                "attached_type10_signal": "manager creates behavior-10 child at 0x26ec0, stores its pointer at parent +70, and sets its +38 back-pointer; handler 0x163d2 initializes child +42/+43=32, then state subroutine 0x165d2 branches on +42 and at zero sets the manager to state 4/substate 2 and activates its +64 child",
                "sibling_and_shutdown_writes": "the behavior-11 child zero branch at 0x16706 sets the manager to state 4/substate 2 and activates the +64 child; manager shutdown at 0x27102 loads the +70 and +64 children and writes state +2=6 to each. These validated linked-child paths do not write the behavior-10 child's +42; no direct writer has yet been identified.",
                "type10_zero_writer": "the state-zero condition on child byte +42 is source-validated; the exact writer and call path remain unresolved",
                "shared_collision_writer_candidate": "not the behavior-10 child's trigger: the 20-record class-8 scan at 0x37b5e is a separate pool. A second collision scan at 0x3a42c traverses the class-4 active list and dispatches candidate inner records through the byte map at 0x3a4e6; it rejects negative entries before callback. Entry +10 (behavior type 10) is 0xff and is skipped, so the attached type-10 child does not reach these collision callbacks. Other class-4 actor types can reach callbacks that write +42, but the child's exact +42 writer and call path remain unresolved.",
                "later_stage_creator_route": "Stage-counter-3 branch at 0x00a198 creates behavior 7; handler 0x001432c can reach type-0 creator 0x0014a20, which allocates queue-class-18 selector 0 at world X 3600 and player world Y +56",
                "stage0_activation": "not linked to the normal Stage 0 initializer; later-stage creation and the type-10 state-4 signal are verified, but the +42 zeroing condition remains unresolved",
            },
            "subtype_from_parent": True,
            "subtype_candidates": [4, 5],
            "position": "parent X/Y plus independent signed offsets from the 16-entry table at 0x00271ee; then +48 Y",
            "parent_activation": "not yet linked to Stage 0 initialization",
        },
        "timed_child": {
            "spawn_site": "0x0033b68",
            "timer_path": "state at 0x0033b4c decrements +34 once every two updates and attempts a child spawn on each nonzero count; state entry sets +34 to 40",
            "subtype": 2,
            "position": "parent X/Y plus independent signed offsets from the 16-entry table at 0x0033bb2",
            "parent_activation": "not yet linked to Stage 0 initialization",
        },
        "selector_9_allocations_by_pool": [
            {
                "parent_behavior_index": 9,
                "parent_handler": "0x0015832",
                "spawn_site": "0x0015b82",
                "allocator": "0x002a1a",
                "pool": "0x001dc6, 50 records of 128 bytes, queue class 4",
                "dispatch": "behavior table at 0x00ffc8; selector 9 resolves to 0x0015832",
                "byte_33": 1,
                "position": "copies parent X/Y",
                "stage0_link": "unverified",
            },
            {
                "parent_behavior_index": 15,
                "parent_handler": "0x0018bd8",
                "spawn_site": "0x0018e82",
                "allocator": "0x002b4e",
                "pool": "0x0047c6, 20 records of 64 bytes, queue class 10",
                "dispatch": "manager table at 0x002353c; selector 9 resolves to 0x0027f40",
                "child_handler": "recurring manager spawner at 0x0027f40",
                "byte_33": "preserved from reused record",
                "position": "parent X - 16, Y - 24",
                "stage0_link": "unverified",
            },
            {
                "parent_behavior_index": 16,
                "parent_handler": "0x001995e",
                "spawn_site": "0x00194f0",
                "allocator": "0x002b4e",
                "pool": "0x0047c6, 20 records of 64 bytes, queue class 10",
                "dispatch": "manager table at 0x002353c; selector 9 resolves to 0x0027f40",
                "child_handler": "recurring manager spawner at 0x0027f40",
                "byte_33": "preserved from reused record",
                "position": "parent X - 16, optionally +32 when facing flag is set; Y - 24",
                "stage0_link": "unverified",
            },
            {
                "parent_behavior_index": 18,
                "parent_handler": "0x001a4e2",
                "spawn_site": "0x001a64a",
                "allocator": "0x002b9c",
                "pool": "0x004cc6, 60 records of 64 bytes, queue class 12",
                "dispatch": "queue-class-12 table at 0x2f3c4; selector 9 resolves to 0x001a6d4",
                "byte_33": "preserved from reused record",
                "position": "copies parent X/Y",
                "child_lifecycle": "class-12 selector-9 handler at 0x001a6d4 sets a 24-tick timer, applies its timed Y motion, then deactivates via 0x2bb8",
                "preceding_sound_command": "0x32",
                "stage0_link": "unverified",
            },
        ],
        "parent_actor_creation_routes": [
            {
                "spawn_site": "0x0049e6",
                "pool_selector": 15,
                "pool_handler": "0x0030116",
                "pool": "0x004cc6, 60 records of 64 bytes, queue class 12",
                "fields": {"active_type": 1, "byte_32": 15},
                "position": "copies parent X/Y",
                "stage0_link": "unverified",
            },
            {
                "spawn_site": "0x004a0a",
                "pool_selector": 15,
                "pool_handler": "0x0030116",
                "pool": "0x004cc6, 60 records of 64 bytes, queue class 12",
                "fields": {"active_type": 1, "byte_32": 15, "byte_33": 2},
                "position": "copies parent X/Y",
                "stage0_link": "unverified",
            },
            {
                "spawn_site": "0x00255ac",
                "pool_selector": 16,
                "pool_handler": "0x00368ea",
                "pool": "0x004cc6, 60 records of 64 bytes, queue class 12",
                "fields": {"active_type": 1, "byte_32": 16},
                "position": "copies parent X/Y",
                "activation_gate": "handler state case at 0x2557e reaches this allocation when ((a5+0x19 + d7) & 3) is nonzero and the subsequent 0x36f4 call returns negative",
                "stage0_link": "unverified",
            },
            {
                "spawn_site": "0x0027cda",
                "pool_selector": 18,
                "pool_handler": "0x00369e4",
                "allocator": "0x002b9c",
                "pool": "0x004cc6, 60 records of 64 bytes, queue class 12",
                "fields": {"active_type": 1, "byte_32": 18},
                "position": "copies parent X/Y",
                "parent_pointer_field": "+38",
                "stage0_link": "unverified",
            },
        ],
        "manager_selector_11_parent_routes": [
            {
                "spawn_site": "0x0027d2e",
                "allocator": "0x002b4e",
                "pool": "0x0047c6, 20 records of 64 bytes, queue class 10",
                "selector": 11,
                "dispatch_handler": "0x0025552",
                "timer": "parent field +34 counts down from 20 before each spawn attempt",
                "position": "copies parent X/Y",
                "field_15": "computed direction value in d6",
                "same_parent_flow_actor": {
                    "spawn_site": "0x0027cda",
                    "allocator": "0x002b9c",
                    "pool": "0x004cc6, 60 records of 64 bytes, queue class 12",
                    "behavior_index": 18,
                "handler": "0x00369e4",
                    "position": "copies parent X/Y",
                    "parent_pointer_field": "+38",
                },
                "stage0_parent_link": "unverified",
            },
        ],
        "manager_selector_7_parent_routes": [
            {
                "spawn_site": "0x0017b3a",
                "allocator": "0x002b4e",
                "pool": "0x0047c6, 20 records of 64 bytes, queue class 10",
                "selector": 7,
                "dispatch_handler": "0x0027bd8",
                "state_path": "state table at 0x17aec reaches this route after state 2 starts a 42-tick timer and state 4 counts it down",
                "position": "parent X/Y plus an X offset from the direction table at 0x17ba0 and +8 Y",
                "parent_pointer_field": "+38",
                "stage0_parent_link": "unverified",
            },
        ],
        "queue_class_12_handler_notes": {
            "selector_15": "0x0030116 initializes the 0xed078 vector and parent-following behavior",
            "selector_16": "0x00368ea sets a5+0x03e3 and starts its timed sequence",
            "selector_18": "0x00369e4 copies byte +48 from its parent and follows parent coordinates",
        },
        "queue_class_12_selector_16_flag": {
            "writer": "0x0036902 writes 1 to a5+0x03e3",
            "generic_enemy_handler": "0x0010054",
            "uses": [
                "0x001136 branches on the flag and advances a substate when set",
                "0x0011e4 holds the substate while the flag is set",
                "0x001210 clears the flag on a state-4 path",
                "0x001216 rechecks the flag and returns to state 4 while clear",
            ],
            "stage0_relationship": "not established",
        },
        "behavior_14_creation_routes": [
            {
                "spawn_site": "0x001f5d6",
                "allocator": "0x002b4e",
                "pool": "queue class 10 manager pool at 0x0047c6",
                "selector": 14,
                "dispatch_handler": "0x0028c7c",
                "byte_33": "parent byte +51 minus 2",
                "position": "copies parent X/Y and facing",
                "stage0_link": "unverified",
            },
            {
                "spawn_site": "0x00216b0",
                "allocator": "0x002a1a",
                "pool": "queue class 4 at 0x001dc6",
                "selector": 14,
                "dispatch_handler": "0x002ff06",
                "byte_33": "10 plus parent byte +33, except value 4 maps to 0 first",
                "position": "copies parent X/Y and facing",
                "stage0_link": "unverified",
            },
            {
                "spawn_site": "0x002f7c8",
                "allocator": "0x002b9c",
                "pool": "queue class 12 at 0x004cc6",
                "selector": 14,
                "dispatch_handler": "0x002ff06",
                "position": "parent X +112, Y +11; stores parent pointer at +38",
                "stage0_link": "unverified",
            },
            {
                "spawn_site": "0x0034bf6",
                "allocator": "0x002b9c",
                "pool": "queue class 12 at 0x004cc6",
                "selector": 14,
                "dispatch_handler": "0x002ff06",
                "byte_33": 2,
                "position": "copies parent X/Y; stores parent pointer at +38",
                "followup_spawn": {"site": "0x0034c1e", "selector": 13,
                                   "handler": "0x002fdc8", "byte_33": 2},
                "stage0_link": "unverified",
            },
        ],
    }


def class6_contact_pool_trace(data: bytes) -> dict[str, object]:
    """Validate the class-6 pool consumed by the object-overlap scanner."""
    checks = {
        "pool_initializer": (0x26C0, bytes.fromhex(
            "41ed11ee4ded36c61b7c00301804303c002f"
            "224e3e3c000f22c151cffffc1d7c0006001e"
            "310e4dee004051c8ffe63b481822")),
        "allocator": (0x2AB6, bytes.fromhex(
            "4a2d18046712306d182238583b481822532d1804"
            "70014e7570004e75")),
        "contact_list_selector": (0x2398, bytes.fromhex(
            "7000102e000e303b00064efb000200040014")),
        "contact_list_insert_left": (0x23AA, bytes.fromhex(
            "306d1824310e3b481824522d18054e75")),
        "contact_list_insert_right": (0x23BA, bytes.fromhex(
            "306d1826310e3b481826522d18064e75")),
        "overlap_scanner": (0x6E44, bytes.fromhex(
            "7c001c2d180567105346366d1824610000164a2e002466e6"
            "7c001c2d180667de5346366d1826345b102a00005300661c"
            "7000102a002041fa001a103000006b0c41fa002832300000"
            "4eb0100051ceffd84e75")),
        "fixed_coordinate_pair": (0x14ECA, bytes.fromhex(
            "4eb82ab6673e197c00010000197c00010020422c0021"
            "397c08000006397c0050000a4eb82ab6671c197c00010000"
            "197c00010020422c0021397c08900006397c0050000a")),
        "followup_actor": (0x1ADE4, bytes.fromhex(
            "4eb82ab6671e197c00010000197c00010020197c00010021"
            "397c09200006397c0100000a4e75")),
        "attached_actor": (0x21550, bytes.fromhex(
            "4eb82ab6674e197c00010000197c00080020"
            "196e000f000f396e00060006396e000a000a"
            "294e00262d4c0038")),
        "state_triggered_actor": (0x27F2A, bytes.fromhex(
            "4eb82ab66712197c00010000197c00060020"
            "197c000200214ef84ac6")),
        "camera_relative_actor": (0x398B4, bytes.fromhex(
            "4eb82ab66742522c0000196e00100020396e0008000a"
            "4a2d8f8a6718197c0001000f532d8f8a302d0408"
            "064001b8394000064e75422c000f532d8f89"
            "302d040804400038394000064e75")),
        "camera_parent_pool_init": (0x29A4, bytes.fromhex(
            "41ed05027000323c00bf20c020c020c020c051c9fff6")),
        "camera_parent_pool_scheduler": (0x3809C, bytes.fromhex(
            "4ded05023e3c002f4a2e0000670e7000102e000ed040"
            "207b000e4e904dee004051cfffe64e75")),
        "camera_parent_type3_dispatch": (0x380CE,
            bytes.fromhex("00039814")),
        "behavior19_class6_child_gate": (0x1ADCA, bytes.fromhex(
            "4a2e002166126112422d03e61b7c000103e7"
            "1b7c0001038a4e75")),
        "class6_behavior20_handler_entry": (0x2F414,
            bytes.fromhex("00033a56")),
        "behavior19_child_selector_table": (0x33B18, bytes.fromhex(
            "0002040002040002")),
        "behavior19_spawn_context": (0x33AD4, bytes.fromhex(
            "4eb82b9c673c197c00010000197c00130020"
            "396e00060006396e000a000a4eb81d8e02400007"
            "103b001c19400021323b001c")),
        "stage3_behavior7_creation": (0xA198, bytes.fromhex(
            "4eb82b9c197c00020000197c00070020"
            "397cffa00006397c1068000a")),
        "behavior7_dispatch_entry": (HANDLER_TABLE + 7 * 4,
                                     bytes.fromhex("0001432c")),
    }
    validity = {
        name: data[address:address + len(expected)] == expected
        for name, (address, expected) in checks.items()
    }
    if not all(validity.values()):
        failed = ", ".join(name for name, ok in validity.items() if not ok)
        raise ValueError(f"Class-6 contact-pool source changed: {failed}")

    spawn_sites = [
        {"address": "0x0014eca", "behavior": 1, "position": [2048, 80],
         "owner_handler": "0x001432c (behavior 7)",
         "route": "stage-counter-3 setup at 0x00a198 creates behavior 7"},
        {"address": "0x0014eec", "behavior": 1, "position": [2192, 80],
         "owner_handler": "0x001432c (behavior 7)",
         "route": "stage-counter-3 setup at 0x00a198 creates behavior 7"},
        {"address": "0x001ade4", "behavior": 1, "position": [2336, 256]},
        {"address": "0x0021550", "behavior": 8, "position": "parent + (0,0)",
         "parent_pointer_field": "+38"},
        {"address": "0x0027f2a", "behavior": 6, "mode": 2,
         "position": "not written at this allocation"},
        {"address": "0x00398b4", "behavior": "parent +16",
         "position": "camera X ±56/440, parent Y +8"},
    ]
    return {
        "pool": {
            "initializer": "0x0026c0",
            "records": 48,
            "record_bytes": 64,
            "queue_class": 6,
            "free_count": "a5+0x1804",
            "free_list_head": "a5+0x1822",
            "allocator": "0x002ab6",
            "visibility_callback": "0x002398",
            "contact_lists": ["a5+0x1805", "a5+0x1806"],
            "scanner": "0x006e44",
        },
        "spawn_sites": spawn_sites,
        "later_stage_routes": {
            "fixed_position_pair": "both sites are within behavior-7 handler 0x001432c; the stage-counter-3 setup at 0x00a198 creates behavior 7",
            "behavior19_followup": "class-6 behavior selector 20 dispatches to 0x033a56, whose routine at 0x033ad4 creates main-pool behavior-19 actors. Their +33 selector comes from 0x033b18; when it is zero, main-pool handler 0x01a7f2 spawns behavior-1 class-6 candidates at (2336,256). The route that activates a class-6 behavior-20 actor during normal Stage 0 remains unresolved",
            "camera_relative_parent_pool": "the separate 48-record pool at a5+0x0502 is cleared by 0x29a4 and scheduled by 0x3809c using byte +14. Type 3 dispatches to 0x39814; its timed branch at 0x398b4 creates class-6 candidates using parent byte +16 as child byte +32. The creator and selector value of a normal Stage 0 type-3 parent remain unresolved",
            "other_sites": "owning behavior routes remain unresolved",
        },
        "stage0_reachability": "none of the six allocation contexts has been connected to the normal Stage 0 initializer",
        "source_checks": validity,
    }


def scripted_actor_message_sequence(data: bytes) -> dict[str, object]:
    """Validate the timer/message/actor sequence around the 0xa28a allocation."""
    checks = {
        "story_controller_dispatch": (0x9F84, bytes.fromhex(
            "302d0008303b00064efb0002")),
        "story_controller_state_table": (0x9F90, bytes.fromhex(
            "00280106012a014002a402cc03340380039203d0")),
        "normal_stage0_gate": (0xA104, bytes.fromhex(
            "4a2d0348660001164a6d03446600010e4eb82dfe546d0008")),
        "normal_stage0_init": (0xA114, bytes.fromhex(
            "4eb82dfe546d00083b7c00c08fb03b7c02808fb2"
            "3b7c00c004083b7c0200040c3b7c00c004883b7c"
            "0200048c3b7c0007000a4ef878b8")),
        "startup_work_ram_clear": (0x6F2, bytes.fromhex(
            "41f900ff0000383c1fff700020c020c051ccfffa4ed4")),
        "normal_stage0_reset_helper": (0x2DFE, bytes.fromhex(
            "41ed03c23c3c000f700020c051cefffc")),
        "state4_delay": (0xA234, bytes.fromhex(
            "536d000a6616546d00083b7c003f000a")),
        "timer_to_message": (0xA25C, bytes.fromhex(
            "536d000a67024e751b7c000102a63b7c001d000a546d0008"
            "323c002e343c1302303c00c0207c000078f04eb815cc")),
        "type2_actor": (0xA28A, bytes.fromhex(
            "4eb82b9c197c00020000197c00010020"
            "397c00da0006397c0088000a")),
        "willow_actor": (0xA2A6, bytes.fromhex(
            "4eb82b9c197c00010000197c00030020"
            "397c00a00006397c0050000a4e75")),
    }
    for name, (address, expected) in checks.items():
        if data[address:address + len(expected)] != expected:
            raise ValueError(f"Scripted actor/message source context changed: {name}")

    from cps1_text_task import decode
    message_id = 0x2E
    message = decode(data, message_id, 0x1302)['stream'][:-1].decode('ascii')
    if message != 'ONE YOUNG MAN STARTS ON HIS JOURNEY':
        raise ValueError('Opening message source changed')
    return {
        "sequence_entry": "0x00a25c",
        "stage0_call_chain": (
            "normal stage counter zero passes the gate at 0xa104; state-3 "
            "setup at 0xa114 advances the 0x9f90 controller from state 3 "
            "to state 4; state 4 at 0xa234 waits seven ticks then advances "
            "to state 5"
        ),
        "entry_condition": "state 5 timer at 0xa25c expires after the seven-tick state-4 delay and 63-tick state-5 wait",
        "message_task": {
            "launch_site": "0x00a274",
            "message_id": f"0x{message_id:02x}",
            "task_parameter": "0x1302",
            "wait_ticks": 29,
            "text": message,
        },
        "following_allocations": [
            {"site": "0x00a28a", "active_type": 2, "behavior": 1,
             "position": [218, 136]},
            {"site": "0x00a2a6", "active_type": 1, "behavior": 3,
             "position": [160, 80], "candidate_identity": "scripted character; sprite stream 0x32aa2, distinct from the blue-clad player"},
        ],
        "normal_stage0_shared_target": {
            "initial_coordinates": [192, 72],
            "basis": "0xa0e0 calls 0x2f08; stage-zero dispatch at 0x2f66 selects 0x2fba, whose checkpoint-zero position table at 0x3078 supplies (192,72) through 0x3012/0x3016 before the active-byte write. The old zero-position claim omitted this initializer call.",
            "alternate_write": "0xa186 writes X=448 only in the stage-counter-3 branch",
        },
        "normal_stage0_initializer_relationship": "verified through the controller state table and the stage-counter-zero branch",
    }


def stage_rng_initialization(data: bytes) -> dict[str, object]:
    """Prove the Stage 0 RNG selector starts cleared by the reset path."""
    lfsr_helper = bytes.fromhex(
        "302d0284024002023200e0481b400286b101023c00ee6704"
        "003c0011e4ed02847000102d02854e75"
    )
    checks = {
        "reset_vector": (0x000004, bytes.fromhex("00000404")),
        "work_ram_clear_call": (0x0005A6,
            bytes.fromhex("49fa000660000146")),
        "work_ram_clear_loop": (0x0006F2,
            bytes.fromhex("41f900ff0000383c1fff700020c020c051ccfffa4ed4")),
        "global_base_setup": (0x0005B6, bytes.fromhex("4bf88000")),
        "stage_seed_selector_read": (0x008CEC,
            bytes.fromhex("302d8e5ae74843fb00d63b5900183b5902842051")),
        "stage_seed_selector_toggle": (0x008D54,
            bytes.fromhex("526d8e5a026d00018e5a")),
        "shared_lfsr_helper": (0x001D8E, lfsr_helper),
    }
    validated = {
        name: data[address:address + len(expected)] == expected
        for name, (address, expected) in checks.items()
    }
    if not all(validated.values()):
        failed = ", ".join(name for name, ok in validated.items() if not ok)
        raise ValueError(f"Stage RNG initialization changed: {failed}")

    # Brief An displacement 0x8e5a is signed: it is -0x71a6, so the
    # startup A5 base 0xffff8000 resolves to 0xffff0e5a, or 0xff0e5a on
    # the 24-bit main bus. The reset loop clears this address before setup.
    selector_displacement = int.from_bytes(
        data[0x008CEE:0x008CF0], "big", signed=True)
    selector_address = (0xFFFF8000 + selector_displacement) & 0xFFFFFF
    if selector_displacement != -0x71A6 or selector_address != 0xFF0E5A:
        raise ValueError(
            f"Unexpected Stage RNG selector address: 0x{selector_address:06x}")
    if not 0xFF0000 <= selector_address <= 0xFFFFFF:
        raise ValueError("Stage RNG selector falls outside cleared work RAM")

    seed_records = (data[0x8CCA:0x8CD2], data[0x8CD2:0x8CDA])
    seeds = tuple(int.from_bytes(record[2:4], "big")
                  for record in seed_records)
    if seeds != (0xF697, 0xA649):
        raise ValueError(f"Unexpected Stage RNG seed words: {seeds!r}")
    return {
        "source_contexts_validated": validated,
        "reset_vector_pc": "0x000404",
        "clear_call": "0x0005a6 -> 0x0006f2",
        "work_ram_clear_range": ["0xff0000", "0xffffff"],
        "work_ram_clear_bytes": 0x10000,
        "startup_global_base": "0xffff8000",
        "startup_global_base_set_at": "0x0005b6",
        "selector_displacement_signed": selector_displacement,
        "selector_effective_address": f"0x{selector_address:06x}",
        "selector_initial_value": 0,
        "selector_initialization": "reset clears the selector at 0xff0e5a before Stage 0 setup",
        "seed_records": [record.hex() for record in seed_records],
        "stage0_lfsr_seed": f"0x{seeds[0]:04x}",
        "next_stage_lfsr_seed": f"0x{seeds[1]:04x}",
        "shared_lfsr_helper": {
            "address": "0x001d8e",
            "bytes": lfsr_helper.hex(),
            "state_word_offset": "a5+0x0284",
            "returned_byte_offset": "a5+0x0285",
            "feedback_taps": [9, 1],
            "recurrence": "shift state right one and insert bit9 XOR bit1 into bit15",
        },
        "selector_toggle": "increments and masks selector to one bit at 0x8d54 after copying the selected seed into Stage 0 state",
    }


def player_jump_entry_trace(data: bytes, handlers: list[int]) -> dict[str, object]:
    """Record the verified normal-player and internal jump dispatch bytes."""
    checks = {
        "player_state_dispatch": (0xC3B6, bytes.fromhex(
            "102e000441fa00264a2d03dc660441fa0016323000004eb01000"
            "610003b04eb838f660000354")),
        "stage0_actor_object_handler_dispatch": (0x125D0,
            bytes.fromhex("102e0021323b00064efb1002")),
        "stage0_actor_initial_state_dispatch": (0x125EA,
            bytes.fromhex("102e0002323b00064efb1002000c013809ae0b52")),
        "stage0_actor_state0_dispatch": (0x12602,
            bytes.fromhex("102e0021323b00064efb1002000e0042007400da")),
        "stage0_actor_normal_state_dispatch": (0x1272E,
            bytes.fromhex("4a2e004a66124a6d03d2660c102e0003323b002e"
                          "4ebb102a")),
        "stage0_actor_normal_state_table": (0x1276E,
            bytes.fromhex("001200320128017e0358")),
        "stage0_actor_first_handler": (0x1261C,
            bytes.fromhex("1d7c0001002241fa0b304eb81f30422e004a")),
        "stage0_actor_vector_table_init": (0x12640,
            bytes.fromhex("2d7c000136b00018")),
        "stage0_actor_normal_state_entry": (0x12780,
            bytes.fromhex("542e00031d7c00400046610005b81d40000f"
                          "41fa09c04ef81f30")),
        "stage0_actor_vector_move_and_ground_probe": (0x127B2,
            bytes.fromhex("4eb8208c4eb83b0a4a2e001f6700007e")),
        "stage0_actor_target_direction_timer": (0x12EDE,
            bytes.fromhex("532e004666306100004a4eb81d8e0240000f")),
        "stage0_actor_target_facing_refresh": (0x12F30,
            bytes.fromhex("322e0006b26d184c6d144a2e000f6620"
                          "1d7c0001000f41fa020c4ef81f30")),
        "normal_state_table": (0xC3DC, bytes.fromhex(
            "000c00240114")),
        "normal_state2_handler": (0xC400, bytes.fromhex(
            "6100012861000310662a61000204660000a6"
            "6100013c660000be102e000fb02e0030670c")),
        "internal_jump_animation_state_dispatch": (0xC8F2,
            bytes.fromhex("102e0004323b000e4ebb100a6100fdc06000fe82"
                          "000a00380038004c")),
        "internal_jump_animation_state_zero": (0xC910,
            bytes.fromhex("70001d40003c1d4000051d40003e002e0002000f"
                          "542e00041d7c0006002241fa099a102e000f"
                          "024000014ef81f34")),
        "internal_jump_animation_timing": (0xC93E,
            bytes.fromhex("532e0022660a542e00041d7c000600224ef81f4c"
                          "532e002266f6542e00043d7c020000381d7c0002"
                          "00481d7c000800226100fbba41fa095a102e000f"
                          "024000014ef81f34532e002266101d7c00020003"
                          "1d7c00040004422e0024")),
        "internal_jump_animation_button_probe_tail": (0xC9DA,
            bytes.fromhex("102e0005323b001a4ebb10166100fd2e6600fa82"
                          "6100feb24eb838f66000fd36")),
        "button2_edge_gate": (0xC716, bytes.fromhex(
            "102d8e920200002067084a2e0024660270014e75")),
        "button2_edge_followup": (0xC434, bytes.fromhex(
            "4a2d03dc666e422e00041d7c00020003102d006a"
            "08000002671c1d7c000a00034a2e00466a08"
            "1d7c000600034e75")),
        "internal_jump_dispatch": (0xC78A, bytes.fromhex(
            "102e0004323b00264ebb102261000284102e0003")),
        "internal_jump_table_and_launch": (0xC7B6, bytes.fromhex(
            "00060036006a422e003e422e00241d7c00010048"
            "3d7c04400038102d006a0800000367063d7c0580"
            "0038542e000441fa0a944eb81f30")),
        "alternate_player_controller_dispatch": (0xBF1E,
            bytes.fromhex(
                "102d006b4600c02d006a1b408e927000102e0020"
                "207b00064ed04e750000bf420000d4ba")),
        "alternate_player_state_dispatch": (0xD4BA,
            bytes.fromhex("102e0002323b000a4ebb10064ef84b0e001805c0")),
        "alternate_jump_outer_state": (0xDA8A,
            bytes.fromhex("4a2d03ea6600e690102e0003323b001a4ebb1016")),
        "alternate_jump_state_table": (0xDAB2,
            bytes.fromhex("001a012402f00252048e0578")),
        "alternate_player_normal_input": (0xDB00,
            bytes.fromhex("6100ea286100ec1066266100eb0466546100ea3e")),
        "alternate_state_zero_reset_entry": (0xDAEC,
            bytes.fromhex("422e003e422e0047542e000441fa069a4eb81f30")),
        "alternate_state_zero_common_tail": (0xDACC,
            bytes.fromhex("7000102e0004323b0012")),
        "alternate_button2_to_jump_state": (0xDB30,
            bytes.fromhex("422e00041d7c00020003102d006a08000002671c")),
        "alternate_jump_substate_dispatch": (0xDBD6,
            bytes.fromhex("102e0004323b00284ebb102461000202")),
        "alternate_jump_substate_table_and_launch": (0xDC04,
            bytes.fromhex("00060036006a422e003e422e00241d7c00010048"
                          "3d7c04400038102d006a0800000367063d7c0580"
                          "0038542e000441fa07f44eb81f30")),
        "debug_dip_jump_flag_sampler": (0x1A8E,
            bytes.fromhex("102d006802400010e7481b400295422d799e"
                          "102d006802400080ee481b40028f671061000060"
                          "082d000300686704522d799e4e75")),
        "debug_jump_flag_dip_source": (0x1A80,
            bytes.fromhex("10390080001e46001b4000684e75")),
        "normal_controller_debug_jump_transition": (0xC33E,
            bytes.fromhex("302e000ab06d045c6e484a2d799e6736"
                          "1d7c000200041d7c000200031d7c00020002"
                          "2d7c0000c01c0018422e003e422e0024")),
        "debug_jump_shared_state_caller": (0xC03C,
            bytes.fromhex("102e0003323b00124ebb100e61000126610002f0"
                          "60000340")),
    }
    validated = {
        name: data[address:address + len(expected)] == expected
        for name, (address, expected) in checks.items()
    }
    if not all(validated.values()):
        failed = ", ".join(name for name, ok in validated.items() if not ok)
        raise ValueError(f"player jump entry bytes changed: {failed}")

    stage0_fixed_controller_checks = {
        "stage0_live_frame_call_order": (0x8DE4,
            bytes.fromhex(
                "536d000a672a4a2d03c26624522d00194eb81f884eb84ea2"
                "4eb900009ef24eb900039a524eb90003807c4ef90003b524")),
        "stage0_object_controller_class2_dispatch_order": (0x9EF2,
            bytes.fromhex(
                "4eb900000bc44eb900026da64eb90002f34c4eb90002f39e"
                "4eb900036c7e4eb90002cf904eb90000ffa24eb900023516"
                "4eb90000bef84eb90000e6f44ef90000fb48")),
        "stage_initializer_sets_active": (0x2F08,
            bytes.fromhex("4ded1846302d0344d040323b00524ebb104e1d7c00010000")),
        "stage0_selector_word": (0x2F72, bytes.fromhex("0000")),
        "controller_dispatch": (0xBF1E,
            bytes.fromhex("102d006b4600c02d006a1b408e927000102e0020"
                          "207b00064ed04e75")),
        "controller_handler_table": (0xBF3A,
            bytes.fromhex("0000bf420000d4ba")),
        "stage0_major_state_dispatch": (0xBF42,
            bytes.fromhex("102e0002323b000a4ebb100660008bbe")),
        "stage0_substate_dispatch": (0xBF62,
            bytes.fromhex("102e0003323b00064efb1002")),
        "stage0_initial_substate_entry": (0xBF74,
            bytes.fromhex("542e00031b6d036203651b6d036403896100003c")),
        "stage0_wait_for_intro_gate": (0xBFE8,
            bytes.fromhex("4a2d0348671c3d7c00100022542e00037002"
                          "1d40000f1d40003041fa107a4ef81f344e75")),
        "stage0_intro_timer_transition": (0xC00C,
            bytes.fromhex("536e00226608542e0002422e00034e75")),
        "stage0_major_state2_table_entry": (0xBF54,
            bytes.fromhex("01ec")),
        "stage0_gameplay_state2_dispatch": (0xC13E,
            bytes.fromhex("4a2d03ea66dc102e0003323b001a4ebb1016"
                          "4eb837724eb839fe61000016610001e060000230")),
        "stage0_controller_state0_table_entry": (0xC164,
            bytes.fromhex("0252")),
        "stage0_button2_jump_table_entry": (0xC166,
            bytes.fromhex("0626")),
        "stage0_button2_to_substate2": (0xC434,
            bytes.fromhex("4a2d03dc666e422e00041d7c00020003"
                          "102d006a08000002671c1d7c000a0003")),
        "stage0_input_controller_state0": (0xC3E8,
            bytes.fromhex("422e0048422e003e422e0047542e0004"
                          "41fa0c844eb81f30")),
        "stage0_input_controller_state_table": (0xC3DC,
            bytes.fromhex("000c0024")),
        "stage0_normal_input_handler": (0xC400,
            bytes.fromhex("6100012861000310662a61000204660000a6")),
        "stage0_controller_down_state_entry": (0xC4D6,
            bytes.fromhex("1d7c00040004002e0002000f1d7c0002003e"
                          "41fa0b9c4ef81f30")),
        "stage0_controller_state4_release": (0xC604,
            bytes.fromhex("422e00041d6e000f00304e75")),
        "stage0_controller_vector_table": (0xC01C,
            bytes.fromhex("0200fe00fe00fe000000fe000000fe00"
                          "0100fe00ff00fe000000fe000000fe00")),
        "stage0_horizontal_input_decode": (0xC52A,
            bytes.fromhex("102d006a080000006706422e000f4e75"
                          "0800000167081d7c0001000f4e75"
                          "002e0002000f4e75")),
        "stage0_down_input_decode": (0xC550,
            bytes.fromhex("102d006a080000024e75")),
        "stage0_controller_motion_integrator": (0xC782,
            bytes.fromhex("4ef8208c")),
        "stage0_fixed_record_vector_integrator": (0x208C,
            bytes.fromhex("206e00187000102e000fd040d04041f00000"
                          "301848c0e180d1ae0006301048c0e180d1ae000a4e75")),
        "stage0_actor_target_facing_compare": (0x12D44,
            bytes.fromhex("7000322e0006b26d184c6d0270014e75")),
        "stage0_internal_jump_dispatch": (0xC78A,
            bytes.fromhex("102e0004323b00264ebb102261000284"
                          "102e0003802e000466104a2e003c670a")),
        "stage0_internal_jump_launch_entry": (0xC7B6,
            bytes.fromhex("00060036006a422e003e422e0024")),
        "stage0_jump_horizontal_tail": (0xC7B4,
            bytes.fromhex("60d0")),
        "stage0_jump_horizontal_vector_dispatch": (0xC786,
            bytes.fromhex("4ef820b4")),
        "stage0_jump_horizontal_vector_helper": (0x20B4,
            bytes.fromhex("102e000f206e00184880d000d00041f00000"
                          "301048c0e180d1ae00064e75")),
        "stage0_jump_ascent_input_vector_refresh": (0xC842,
            bytes.fromhex("6100fce64a2e0024660a")),
        "stage0_jump_fall_pose_vector_refresh": (0xC96E,
            bytes.fromhex("6100fbba41fa095a")),
        "stage0_jump_fall_physics_vector_refresh": (0xC9B6,
            bytes.fromhex("6100fb72102e000fb02e00306714")),
    }
    stage0_fixed_controller_valid = {
        name: data[address:address + len(expected)] == expected
        for name, (address, expected) in stage0_fixed_controller_checks.items()
    }
    if not all(stage0_fixed_controller_valid.values()):
        failed = ", ".join(name for name, ok in
                            stage0_fixed_controller_valid.items() if not ok)
        raise ValueError(f"Stage 0 fixed controller bytes changed: {failed}")

    # Direct BRA.W callers of the horizontal knockback integrator. Each
    # context ends in JSR $38f6 followed by BRA.W $c72e; including the call
    # prelude distinguishes these references from matching bytes in data.
    knockback_callers = {
        "0x00c3d8": bytes.fromhex("4eb838f660000354"),
        "0x00c9f6": bytes.fromhex("4eb838f66000fd36"),
        "0x00cdde": bytes.fromhex("4eb838f66000f94e"),
        "0x00dae2": bytes.fromhex("4eb838f66000ec4a"),
        "0x00ddbe": bytes.fromhex("4eb838f66000e96e"),
        "0x00e046": bytes.fromhex("4eb838f66000e6e6"),
    }
    knockback_callers_valid = {
        site: data[int(site, 16) - 4:int(site, 16) - 4 + len(context)]
            == context
        for site, context in knockback_callers.items()
    }
    if not all(knockback_callers_valid.values()):
        failed = ", ".join(site for site, ok in knockback_callers_valid.items()
                            if not ok)
        raise ValueError(f"player knockback caller bytes changed: {failed}")

    contact_checks = {
        "separate_controller_state_dispatch": (0xC3B6,
            bytes.fromhex("102e000441fa00264a2d03dc660441fa0016"
                          "323000004eb01000610003b04eb838f6")),
        "separate_controller_state2_button2_branch": (0xC400,
            bytes.fromhex("6100012861000310662a61000204660000a6"
                          "6100013c660000be102e000fb02e0030670c")),
        "separate_controller_button2_followup": (0xC434,
            bytes.fromhex("4a2d03dc666e422e00041d7c00020003"
                          "102d006a08000002671c1d7c000a0003")),
        "separate_controller_charge_helper": (0xC610,
            bytes.fromhex("102d8e9208000004671c1d7c0001003f"
                          "1d7c000a0044422e0031422d8e947001")),
        "interaction_record_stage0_init_call": (0xA0E0,
            bytes.fromhex("4eb82f084a2d03a3")),
        "interaction_record_stage_init_dispatch": (0x2F08,
            bytes.fromhex("4ded1846302d0344d040323b00524ebb104e")),
        "interaction_record_stage_init_sets_active": (0x2F1A,
            bytes.fromhex("1d7c00010000")),
        "interaction_record_state2_writer_3a66c": (0x3A66C,
            bytes.fromhex(
                "1b7c000218464a2d0364672a532d03644a2d035c6706"
                "532d035c60164a2d035a6706532d035a600a4a2d035b"
                "6704532d035b70014e75532d036270014e75")),
        "interaction_record_terrain_collision_caller": (0xC2DC,
            bytes.fromhex(
                "4a2e003e6616302e0006322e000a064100104eb837245341"
                "672a4e75302e0006322e000a4eb83724534167184e75"
                "302e0006322e000a0641fff84eb83724534167024e75"
                "4ef90003a66c")),
        "interaction_record_object_overlap_caller": (0x29000,
            bytes.fromhex(
                "4a2d799e660000840c2d000218466700007a4eb900039aca"
                "7000102e007fe54849fb006a7400322e000614144882"
                "4a2e000f67024442d242302d184c90417400142c0002"
                "d042d05132290002d442d242b041620000367400322e000a"
                "142c00014882d242302d185090417400142c0003d042d069"
                "000432290006d442d242b041620000084ef90003a66c4e")),
        "contact_interaction_scan_39ade": (0x39ADE, bytes.fromhex(
            "4ded18464a2e000067307e001e2d180567285307366d1824345b"
            "082a0000000067147000102a0020103b00126b08323b0024"
            "4ebb102051cfffe0")),
        "contact_interaction_type_map_39b1a": (0x39B1A,
            bytes.fromhex("00020406ffff080a0cff0eff00000000")),
        "contact_interaction_callback_offsets_39b32": (0x39B32,
            bytes.fromhex("002a008800d4016001a601ec02740342")),
        "contact_interaction_type0_response_39bba": (0x39BBA,
            bytes.fromhex(
                "302e000a906a000a044000180c4000086234"
                "302e0006906a00060640004c0c4000986222"
                "3d6a000a000a066e0020000a426e000c426e003a"
                "1d7c00010024157c000100244e75422a0024")),
        "actor_contact_scan_743a": (0x743A, bytes.fromhex(
            "7c001c2d180567305346366d1824345b102a00005300661c"
            "7000102a002041fa001a103000006b0c41fa002832300000"
            "4eb0100051ceffd84e75")),
        "actor_contact_type_map_7474": (0x7474,
            bytes.fromhex("ffff0000ffffffffffffffffffffffffffffffffffffffff")),
        "actor_contact_callback_offsets_748c": (0x748C,
            bytes.fromhex("0002")),
        "actor_contact_type2_response_748e": (0x748E, bytes.fromhex(
            "7000102e0020e74841fa005641f00000302e000a906a000a"
            "906800040c400008623c302e0006906a0006d05006400040"
            "323c0080d2680002b0416222302a000ad06800063d40000a"
            "426e000c1d7c00010024302a002cd16e00064e75")),
        "actor_contact_caller_11c2e": (0x11C2E, bytes.fromhex(
            "4eb8743a4a2e0024660c422e00384eb83b0a6622")),
        "actor_contact_caller_11c98": (0x11C98, bytes.fromhex(
            "4eb8743a4a2e002467de4ef81f4c")),
        "actor_contact_caller_11eb2": (0x11EB2, bytes.fromhex(
            "4eb8743a4a2e002466084eb83b0a6628")),
        "actor_contact_caller_19344": (0x1933E, bytes.fromhex(
            "4a2e002166124eb8743a4a2e002467081d7c000800034e75")),
        "actor_contact_caller_1c16e": (0x1C16E, bytes.fromhex(
            "4eb8743a4a2e002467161d7c00080022")),
        "actor_contact_caller_1c2e6": (0x1C2E6, bytes.fromhex(
            "4eb8743a4a2e002467201d7c00080022")),
        "actor_contact_caller_1c63a": (0x1C63A, bytes.fromhex(
            "4eb8743a4a2e00246744542e0002042d")),
        "contact_queue_scan": (0x6E44, bytes.fromhex(
            "7c001c2d180567105346366d1824610000164a2e002466e6"
            "7c001c2d180667de5346366d1826345b102a00005300661c"
            "7000102a002041fa001a103000006b0c41fa002832300000"
            "4eb0100051ceffd84e75")),
        "contact_overlap_sets_player_flag": (0x6EB8, bytes.fromhex(
            "102a002102000003663c302e000a906a000a044000040c400008"
            "622a302e0006906a00060640000e0c40004c62183d6a000a000a"
            "066e000c000a426e000c1d7c00010024")),
        "contact_candidate_type_map": (0x6E96,
            bytes.fromhex("000204ff")),
        "player_terrain_contact_writer": (0x396E,
            bytes.fromhex("802d8a3a1d4000244e75")),
        "player_terrain_probe_calls": (0x3930,
            bytes.fromhex("610004f6610005161b408a3a1b6e00468a3b")),
        "stage0_contact_scan_caller": (0x2D19C, bytes.fromhex(
            "102d0019d007520002000001660c4eb86e444a2e0024660a"
            "4e754eb83c48660a4e75")),
        "stage0_player_normal_queue_selector": (0x1270C,
            bytes.fromhex("1d7c0002000e")),
        "behavior4_handler_render_queue_selector": (0x13C16,
            bytes.fromhex("1d7c0002000e")),
        "behavior4_actor_allocation_loop": (0xCC90,
            bytes.fromhex("3c3c00074eb82b9c672c197c000100001946000f"
                          "396e00060006302e000a4a2e003e670404400008"
                          "3940000a197c0004002051ceffd04eb82b046722"
                          "197c00010000396e00060006302e000a4a2e003e"
                          "6704044000083940000a6000ff56")),
        "stage0_player_visibility_queue_call": (0x12766,
            bytes.fromhex("610005924ef82244")),
        "class2_pairwise_contact_scan_lists": (0x3B25C, bytes.fromhex(
            "7e001e2d180267f65347386d181e4a2d180667ea3c5c")),
        "class8_pairwise_collision_scan": (0x37B5E, bytes.fromhex(
            "41ed42c63c3c001310280000530066580c280004002066084a2e0001663a6048"
            "30280006906e0006d0680030d06e003432280032d26e0036b041622c3028000a"
            "906e000ad0680034d06e003832280036d26e003ab0416210700010280020d040"
            "303b00104ebb000c41e8004051ceff9a4e75")),
        "class8_pairwise_collision_callback_table": (0x37BD0,
            bytes.fromhex("001200340034007200b600d40030007200d4")),
        "class8_pairwise_collision_callbacks": (0x37BE2, bytes.fromhex(
            "0c2e0002000064161d7c00020000102e002a9028002a6a0270001d40002a7c00"
            "4e750c2e0002000067261d7c00020000102e002a9028002a6a0270001d40002a"
            "117c00020000317c040000027c004e75117c00020000317c040200027c004e75"
            "0c2e0002000067201d7c000200007c001028002a902e002a67206a144228002a"
            "44001d40002a4e754228002a7c004e751140002a422e002a4e754228002a422e"
            "002a4e750c2e0002000067141028002ab02e002a650a422e002a1d7c00020000"
            "4e750c2e0002000067161d7c00020000102e002a9028002a6a0270001d40002a"
            "7c004e75")),
        "contact_class14_pool_init": (0x28DA, bytes.fromhex(
            "41ed132e4ded5bc61b7c003c1810303c003b7200224e3e3c000f"
            "22c151cffffc1d7c000e001e310e4dee004051c8ffe63b48183a")),
        "contact_class14_pool_scheduler": (0x2CF90, bytes.fromhex(
            "4ded5bc63e3c003b4a2e0000670e7000102e0020e548207b000e"
            "4e904dee004051cfffe64e75")),
        "contact_class14_handler_dispatch": (0x2CFB6,
            bytes.fromhex("0002cfc20002cfd60002d78e")),
        "stage0_class12_pool_init": (0x2756, bytes.fromhex(
            "41ed12b64ded4cc61b7c003c180d303c003b224e3e3c000f"
            "22c151cffffc1d7c000c001e310e4dee004051c8ffe63b481834")),
        "stage0_class12_pool_alloc": (0x2B9C, bytes.fromhex(
            "4a2d180d6712306d183438583b481834532d180d70014e7570004e75")),
        "stage0_visibility_queue_call": (0x116F0,
            bytes.fromhex("4eb81f4c4eb82244")),
        "stage0_contact_mode_fields": (0x116BE, bytes.fromhex(
            "61ce3d7c0202002a1d7c0002000e1d7c00040013")),
        "stage0_visibility_mode_table": (0x2282,
            bytes.fromhex("fffe00a000d201160148017a01ac01f002220254")),
        "stage0_visibility_dispatch": (0x2244, bytes.fromhex(
            "4a2e00006736422e0001302e0006906d0408064000200c4001c06220"
            "302e000a906d040c0c4001106212522e00017000102e001e303b0008"
            "4efb00044e75fffe")),
        "stage0_class12_visibility_callback": (0x242E, bytes.fromhex(
            "7000102e000e303b00064efb000200060016")),
        "stage0_class12_queue_insert": (0x2452, bytes.fromhex(
            "306d1838310e3b481838522d180f4e75")),
        "stage0_render_queue_1838_drain": (0x3B7A0, bytes.fromhex(
            "102d180f6714386d1838325c610000d2532d180f66f43b4c18384e75")),
        "render_queue_actor_interpreter": (0x3B880, bytes.fromhex(
            "30290006906d0408064000403229000a926d040c4601524124690014")),
        "stage0_contact_queue_selectors": (0x2398,
            bytes.fromhex("7000102e000e303b00064efb000200040014")),
        "stage0_contact_queue_insert_a": (0x23AA,
            bytes.fromhex("306d1824310e3b481824522d18054e75")),
        "stage0_contact_queue_insert_b": (0x23BA,
            bytes.fromhex("306d1826310e3b481826522d18064e75")),
    }
    contact_checks_valid = {
        name: data[address:address + len(expected)] == expected
        for name, (address, expected) in contact_checks.items()
    }
    if not all(contact_checks_valid.values()):
        failed = ", ".join(name for name, ok in contact_checks_valid.items()
                            if not ok)
        raise ValueError(f"player contact scan bytes changed: {failed}")

    def static_references(target_address: int) -> tuple[list[str], list[str]]:
        direct: list[str] = []
        for address in range(len(data) - 4):
            opcode = int.from_bytes(data[address:address + 2], "big")
            if opcode in (0x4EB8, 0x4EF8):  # JSR/JMP absolute word.
                destination = int.from_bytes(data[address + 2:address + 4], "big")
            elif opcode in (0x4EB9, 0x4EF9):  # JSR/JMP absolute long.
                destination = int.from_bytes(data[address + 2:address + 6], "big")
            elif opcode in (0x4EBA, 0x4EFA, 0x41FA):  # PC-relative JSR/JMP/LEA.
                displacement = int.from_bytes(
                    data[address + 2:address + 4], "big", signed=True)
                destination = address + 2 + displacement
            elif opcode >> 12 == 0x6:  # Bcc/BSR short or word.
                low = opcode & 0xFF
                if low == 0:
                    displacement = int.from_bytes(
                        data[address + 2:address + 4], "big", signed=True)
                    destination = address + 4 + displacement
                else:
                    displacement = low if low < 0x80 else low - 0x100
                    destination = address + 2 + displacement
            else:
                continue
            if destination == target_address:
                direct.append(f"0x{address:06x}")

        word = target_address.to_bytes(2, "big")
        literal: list[str] = []
        search_from = 0
        while (found := data.find(word, search_from)) >= 0:
            literal.append(f"0x{found:06x}")
            search_from = found + 1
        return direct, literal

    direct_xrefs, literal_word_matches = static_references(0xC78A)
    animation_dispatch_xrefs, animation_dispatch_literals = static_references(0xC8F2)
    target = 0xC78A

    return {
        "source": "offline Willow USA 68000 region",
        "verified": validated,
        "separate_input_controller_state_table": {
            "dispatcher": "0x00c3b6",
            "table_address": "0x00c3dc",
            "state_0_handler": "0x00c3e8",
            "state_2_handler": "0x00c400",
            "state_4_handler": "0x00c4f0",
            "entries": ["0x000c", "0x0024", "0x0114"],
            "entry_selector": "object +4; table at 0xc3e2 is chosen when a5+0x03dc is nonzero",
        },
        "stage0_actor_route": {
            "allocation": "0x00a2a6 creates an active actor at (160,80), with object +32 = 3",
            "object_handler_table": "entry 3 in the table at 0x00ffc8 dispatches +32 = 3 to handler 0x00125d0",
            "first_state_path": "cleared fields route through 0x125d0 (+33=0), then 0x125ea (+2=0), 0x12602 (+33=0), and initializer 0x1261c; that initializer advances +2 to 2",
            "normal_state_entry": "with +2=2, handler 0x1272e dispatches +3=0 to 0x12780, which advances +3 to 2 and loads the initial 0x13154 animation",
            "movement": "initializer 0x1261c sets object +24 to vector table 0x136b0; normal substate 0 at 0x127b2 calls 0x208c then terrain probe 0x3b0a",
            "input_link": "indirect link validated: fixed controller 0xc400 maps joystick direction into shared target movement through 0xc52a/0x208c; Willow helper 0x12d44 compares actor +6 against that target at a5+0x184c, and the actor uses the resulting +15 row with its 0x136b0 vector table. Normal entry initializes +70 to 64 ticks; expiry at 0x12ede calls 0x12f30 to refresh +15 from the target. The actor handler does not read joystick bits directly",
            "bytes_validated": {name: validated[name] for name in (
                "stage0_actor_object_handler_dispatch",
                "stage0_actor_initial_state_dispatch",
                "stage0_actor_state0_dispatch",
                "stage0_actor_normal_state_dispatch",
                "stage0_actor_normal_state_table",
                "stage0_actor_first_handler",
                "stage0_actor_vector_table_init",
                "stage0_actor_normal_state_entry",
                "stage0_actor_vector_move_and_ground_probe",
                "stage0_actor_target_direction_timer",
                "stage0_actor_target_facing_refresh",
            )},
        },
        "separate_controller_state_2_input_path": {
            "address": "0x00c400",
            "stage0_link": "active through fixed record a5+0x1846 after its stage-0 state transition; shared target coordinates are read by Willow's verified actor handler 0x125d0",
            "calls": ["0x00c52a", "0x00c716", "0x00c610", "0x00c550"],
            "button2_edge_gate": {
                "address": "0x00c716",
                "input_latch": "a5-0x8e92",
                "tested_mask": "0x20 (input bit 5)",
                "effect": "returns nonzero when Button 2 edge is present and object +36 is clear; the normal state-2 handler branches to 0xc434",
            },
            "button2_followup_0xc434": "clears object +4, sets state +3 to 2, then checks a5+0x006a bit 2; if set it changes to state 10 (or state 6 on the signed +70 test), otherwise it calls 0xc610",
            "byte_validation": {name: contact_checks_valid[name] for name in (
            "separate_controller_state_dispatch",
            "separate_controller_state2_button2_branch",
            "separate_controller_button2_followup",
            "separate_controller_charge_helper",
            )},
            "jump_state_entry": "Button 2 edge through 0xc716 takes 0xc434, which sets +3=2 when Down is clear; active Stage 0 major-state handler 0xc13e indexes 0xc164 at byte offset 2, whose word 0x0626 dispatches to internal jump handler 0xc78a and launch entry 0xc7bc",
        },
        "debug_dip_jump_transition": {
            "flag_sampler": "0x1a8e clears a5+0x799e, then increments it when inverted DSWC bits 7 and 3 are both set",
            "dip_source": "0x1a80 reads 0x80001e into a5+0x68; MAME maps 0x800018-0x80001f to DSW banks, with offset 3 reading DSWC",
            "willow_test_mode_meaning": "MAME Willow DSWC bit 7 selects Game/Test mode; bit 3 is the debug Invulnerability switch in Test mode, so both active-high bits correspond to Test mode plus Invulnerability On",
            "transition": "0x00c33e tests a5+0x799e and a Y threshold, then sets actor +2/+3/+4 to 2, +24 to 0x00c01c, +56 to 0x0e00, and selects animation 0x00d27a",
            "shared_caller": "0x00c03c dispatches actor +3 then calls 0x00c170 and 0x00c33e",
            "stage0_actor_link": "the debug-gated transition is byte-validated, but the caller's actor has not been proven to be Willow's normal Stage 0 player",
            "byte_validation": {name: validated[name] for name in (
                "debug_dip_jump_flag_sampler",
                "debug_jump_flag_dip_source",
                "normal_controller_debug_jump_transition",
                "debug_jump_shared_state_caller",
            )},
        },
        "internal_jump_state": {
            "dispatcher": "0x00c78a",
            "substate_field": "object +4",
            "launch_substate": "0",
            "launch_handler": "0x00c7bc",
            "up_bit": "a5+0x006a bit 3 selects launch velocity 0x0580 instead of 0x0440",
            "entry_caller": "Stage 0 computed dispatch: Button 2 edge 0xc400 -> 0xc434 sets fixed-record +3=2 -> 0xc13e indexes table 0xc164 word 0x0626 -> 0xc78a; verified only when Down is clear and the fixed record is in normal controller mode",
        },
        "player_knockback_dispatch": {
            "handler": "0x00c72e",
            "validated_direct_branch_callers": list(knockback_callers),
            "bytes_validated": knockback_callers_valid,
            "common_prelude": "JSR 0x38f6, then BRA.W 0xc72e",
            "meaning": "These player-state paths integrate field +58 and inspect +36; the references alone do not establish that an enemy attack set the contact flag.",
            "class8_peer_collision_scan": {
                "pool": "the 20-record class-8 pool initialized at 0x26f2; records are 64 bytes at a5+0x42c6",
                "scan": "0x37b5e walks the pool, filters active records, checks their rectangular extents, then dispatches overlap callbacks by candidate byte +32 through the relative table at 0x37bd0",
                "callback_table_offsets": ["0x0012", "0x0034", "0x0034", "0x0072", "0x00b6", "0x00d4", "0x0030", "0x0072", "0x00d4"],
                "callback_effects": "validated callback bodies at 0x37be2-0x37cc5 change state byte +0 and collision counter +42; two paths also set candidate state +0=2 and timer +2=0x0400/0x0402",
                "player_damage_result": "this is peer collision within the charge-child pool; the validated callback block does not write Willow's +36 contact flag or a health field, so it does not establish enemy-to-player damage",
                "bytes_validated": {name: contact_checks_valid[name] for name in (
                    "class8_pairwise_collision_scan",
                    "class8_pairwise_collision_callback_table",
                    "class8_pairwise_collision_callbacks",
                )},
            },
            "class_list_contact_scan_743a": {
                "scan": "0x743a walks candidate list a5+0x1805 with head a5+0x1824, filters active records by the byte map at 0x7474, then dispatches through the callback offsets at 0x748c",
                "candidate_filter": "the validated byte map admits candidate behavior types 2 and 3; Willow's normal Stage 0 actor sets +32=3 but is inserted into a5+0x1838 rather than the scanned a5+0x1805/+0x1824 list",
                "type2_and_type3_response": "the shared callback at 0x748e checks the current/candidate actor bounds, repositions current Y, clears its vertical fraction, sets current +36=1, and adds candidate +44 to current X",
                "caller_effects": {
                    "0x19344": "a contact result moves the current actor to behavior substate 8",
                    "0x1c16e": "a contact result advances the current actor's state and selects an animation",
                    "0x1c2e6": "a contact result sets an actor timer/state and enters its follow-on animation handler",
                    "0x1c63a": "a contact result decrements a shared counter and attempts a child allocation",
                    "0x11c2e": "a contact result enters a timed actor response; a miss continues through terrain collision",
                    "0x11c98": "a contact result repeats the current actor state; a miss exits the update",
                    "0x11eb2": "a contact result enters an actor response state; a miss continues through terrain collision",
                },
                "player_damage_result": "the scan's candidate list and behavior filter exclude Willow's verified Stage 0 queue entry; these callers establish actor-to-actor interaction, not an enemy-to-Willow hurt event",
                "bytes_validated": {name: contact_checks_valid[name] for name in (
                    "actor_contact_scan_743a",
                    "actor_contact_type_map_7474",
                    "actor_contact_callback_offsets_748c",
                    "actor_contact_type2_response_748e",
                    "actor_contact_caller_11c2e",
                    "actor_contact_caller_11c98",
                    "actor_contact_caller_11eb2",
                    "actor_contact_caller_19344",
                    "actor_contact_caller_1c16e",
                    "actor_contact_caller_1c2e6",
                    "actor_contact_caller_1c63a",
                )},
            },
            "contact_flag_producer": {
                "queue_scan": "0x006e44 iterates both actor queues at a5+0x1805/0x1806 and dispatches each candidate using byte +32",
                "candidate_type_filter": "0x006e7a indexes the byte map at 0x006e96 by candidate +32; entries 0, 1, and 2 have callbacks, while behavior type 3 is 0xff and is rejected before overlap handling",
                "overlap_writer": "0x006eb8 checks candidate byte +33 and bounding ranges; its matching branch repositions the current record, clears vertical fraction, and sets current +36",
                "caller": "0x002d1aa calls 0x6e44 on its periodic branch before testing current +36; caller executes in the class-14 actor-pool scheduler",
                "caller_pool": "initializer 0x28de creates 60 records of 64 bytes at a5+0x5bc6 and sets object +30=14; scheduler 0x2cf90 walks those records and dispatches handlers through +32",
                "terrain_contact_writer": "0x0038f6 calls 0x3e28/0x3e4c for the profile-selected terrain probes, ORs their contact results, and stores the mask to current +36 at 0x3972; this is the +36 value consumed by the six player movement branches before 0xc72e",
                "terrain_bytes_validated": {
                    "player_terrain_probe_calls": contact_checks_valid[
                        "player_terrain_probe_calls"],
                    "player_terrain_contact_writer": contact_checks_valid[
                        "player_terrain_contact_writer"],
                },
                "bytes_validated": contact_checks_valid,
                "interaction_scan_39ade": {
                    "current_record": "fixed work-area record at a5+0x1846; the scan runs only while byte +0 is 1",
                    "stage0_activation": "normal Stage 0 setup at 0x00a0e0 calls 0x002f08; the initializer selects a stage-counter-indexed helper, then sets fixed-record byte +0 to 1",
                    "coordinate_alias": "record words +6/+10 alias a5+0x184c/+0x1850, which are also used as global target coordinates; this identifies the fixed record as the Stage 0 controller/target work area rather than Willow's separate actor",
                    "candidate_lists": ["a5+0x1805", "a5+0x1806"],
                    "candidate_type_map": [2, 4, 6, None, 8, 10, 12, 14],
                    "type0_response": "0x39bba checks overlap; one horizontal half repositions current Y to candidate Y+22, clears current +12/+58, and sets current +36=1/candidate +36=2; the other sets candidate +36=1 only; a miss clears candidate +36",
                    "fixed_record_identity": "the active a5+0x1846 record is the Stage 0 fixed player/input record; its intro sprite is 0x0033 via animation 0xd15a. The actor allocated at 0xa2a6 is a separate scripted character",
                    "player_damage_connection": "the 0x39ade scan uses the fixed input-controller record, while Willow's normal actor is in list a5+0x1838; neither this scan nor the separate 0x6e44/0x3b25c scans proves an enemy-to-Willow hurt event",
                    "stage0_player_queue_route": "Willow's normal handler at 0x1270c sets +14=2; its class-12 pool record reaches visibility callback 0x242e, whose selector maps +14=2 to 0x2452 and inserts into list a5+0x1838 (count a5+0x180f). The normal handler calls visibility helper 0x2244 at 0x1276a. The 0x6e44 and 0x39ade scans use lists a5+0x1805/0x1806; the 0x3b25c pairwise scan uses a5+0x181e/0x1826. None scans Willow's verified queue",
                    "behavior4_actor_render_route": "the main-pool allocation loop at 0x00cc90 creates active selector-4 records at the current actor X and Y (or Y-8 when +62 is set), writing child +15 from 7 down to 0. Selector 4 dispatches to handler 0x013bd2; its state-2 branch at 0x013c16 writes +14=2 and state 2 later calls visibility helper 0x2244. This proves a selector-4 actor can enter the rendering queue; it does not connect that actor or handler to Willow damage",
                    "stage0_player_queue_consumer": "routine 0x3b7a0 drains a5+0x1838 and calls 0x3b880 for each entry; the consumer interprets object animation data and emits render entries. This is a rendering queue, not an established enemy/player collision scan",
                    "bytes_validated": {name: contact_checks_valid[name] for name in (
                        "contact_interaction_scan_39ade",
                        "contact_interaction_type_map_39b1a",
                        "contact_interaction_callback_offsets_39b32",
                        "contact_interaction_type0_response_39bba",
                        "stage0_player_normal_queue_selector",
                        "behavior4_handler_render_queue_selector",
                        "behavior4_actor_allocation_loop",
                        "stage0_player_visibility_queue_call",
                        "stage0_class12_pool_init",
                        "stage0_class12_pool_alloc",
                        "stage0_visibility_dispatch",
                        "stage0_class12_visibility_callback",
                        "stage0_class12_queue_insert",
                        "stage0_render_queue_1838_drain",
                        "render_queue_actor_interpreter",
                        "class2_pairwise_contact_scan_lists",
                    )},
                    "state2_activation": {
                        "writer": "0x3a66c sets a5+0x1846 byte 0 to 2, then services timers at a5+0x035a..0x0364",
                        "verified_call_contexts": {
                            "0x00c320": "player terrain probe 0x3724 reports contact",
                            "0x029084": "an object-overlap check reaches the shared writer",
                        },
                        "interpretation": "the writer is shared by terrain and object-overlap branches; this does not identify the a5+0x1846 state as Willow's hurt state",
                        "bytes_validated": {name: contact_checks_valid[name] for name in (
                            "interaction_record_state2_writer_3a66c",
                            "interaction_record_terrain_collision_caller",
                            "interaction_record_object_overlap_caller",
                        )},
                    },
                },
                "limitation": "The verified Stage 0 Willow actor enters list a5+0x1838, while the 0x39ade and 0x6e44 scans read a5+0x1805/0x1806 and the 0x3b25c pairwise scan reads a5+0x181e/0x1826. These known scans do not establish an enemy-to-Willow hit; other player damage writers or list paths remain to be traced.",
                "stage0_actor_route": {
                    "visibility_call": "Stage 0 type-2 actor calls 0x2244 at 0x116f4 during its visible startup substate",
                    "pool": "queue class 12; 60 records of 64 bytes at a5+0x4cc6; initializer 0x2756 sets object +30=12 and allocator 0x2b9c preserves that field",
                    "selector_field": "object +30 is a byte offset into the 0x2282 jump table; value 12 dispatches to the callback at 0x242e",
                    "contact_list_route": "callback 0x242e uses object +14; value 2 dispatches to 0x2452 and appends to list a5+0x1838, count a5+0x180f",
                    "stage0_object_plus14": "the Stage 0 handler sets +14=2 at 0x116c6",
                    "result": "the verified Stage 0 actor enters the a5+0x1838 rendering queue, which is consumed by 0x3b880; contact scanner 0x6e44 reads only lists a5+0x1805 and a5+0x1806, so no path from this actor into those scanned lists is established",
                    "bytes_validated": {name: contact_checks_valid[name] for name in (
                        "stage0_class12_pool_init",
                        "stage0_class12_pool_alloc",
                        "stage0_visibility_queue_call",
                        "stage0_contact_mode_fields",
                        "stage0_visibility_mode_table",
                        "stage0_visibility_dispatch",
                        "stage0_class12_visibility_callback",
                        "stage0_class12_queue_insert",
                        "stage0_render_queue_1838_drain",
                        "render_queue_actor_interpreter",
                    )},
                },
            },
        },
        "alternate_player_jump_state": {
            "controller_dispatch": "0xbf32 selects handler 0xd4ba when input actor byte +32 is 1",
            "normal_input_state": "major state +2 = 2 enters 0xda8a; substate +3 = 0 enters 0xdacc, whose state +4 = 2 handler at 0xdb00 calls 0xc716",
            "button2_transition": "0xdb30 clears +4 and sets +3 = 2 on Button 2 edge when the Down branch is not selected",
            "next_update_dispatch": "0xda8a table at 0xdab2 maps +3 = 2 to 0xdbd6; its +4 = 0 entry maps through 0xdc04 to launch handler 0xdc0a",
            "launch_effect": "0xdc0a initializes +56 to 0x0440 or 0x0580 when Up bit 3 is held, selects 0xe428, immediately applies vertical velocity, and runs 0x39fe",
            "collision_state_zero_reentry": "active ascent collision exit at 0xdca4 clears +3/+4; the next 0xda8a dispatch maps +3 = 0 to 0xdacc, whose +4 = 0 handler at 0xdaec clears +62/+71, advances +4 to 2, and selects animation table 0xe194. The common tail then runs 0xc782 movement and 0x38f6 collision; normal active input resumes through 0xdb00 on the following update",
            "native_timing": "the native Stage 0 player follows the selector-0 fixed-controller jump route at 0xc78a/0xc7bc; this selector-1 controller and its 0xe194 reset are cataloged separately and are not bound to the Stage 0 player",
            "stage0_player_route_status": "Stage 0 actor allocation at 0xa2a6 sets actor +32 = 3 and dispatches through 0xffc8 to 0x125d0; the separate 0xbf32 input task dispatches to 0xd4ba only for its own actor +32 = 1. No verified Stage 0 transition connects the actor at 0xa2a6 to that controller",
            "native_reset_status": "the native Stage 0 jump does not use this selector-1 reset; the source +62/+71 fields and 0xe194 animation remain separate from the Stage 0 player model",
        },
        "stage0_fixed_controller_route": {
            "record": "fixed work-area record a5+0x1846",
            "stage0_activation": "0xa0e0 calls 0x2f08; the initializer sets record byte +0 to 1",
            "stage0_type_selector": "stage-counter zero reads the first selector word at 0x2f72, whose value is zero",
            "dispatch": "0xbf00 indexes the table at 0xbf3a with record byte +32; Stage 0 therefore enters 0xbf42, while handler 0xd4ba is selected only for selector 1",
            "initial_state_route": "reset-cleared record bytes +2/+3 select major-state dispatcher 0xbf42, then substate handler 0xbf74; this entry advances substate +3 and initializes stage-task fields before calling 0x1f34",
            "stage0_gameplay_route": "after the opening task advances its major-state byte to 2, table entry 0x01ec at 0xbf54 selects 0xc13e; while a5+0x03ea is clear, substate 0 dispatches through 0xc164 entry 0x0252 to 0xc3b6, then the normal +4=0 entry at 0xc3e8 advances to input handler 0xc400",
            "live_update_order": "the active 0x8de4 phase branch increments shared frame +25, then calls 0x9ef2; after earlier actor managers, it runs the main object dispatcher 0xffa2, queue-class-10 manager 0x23516, fixed controller 0xbef8, queue-class-8 dispatcher 0xe6f4, and class-2 dispatcher 0xfb48 in that order",
            "native_target_timing": "Willow and main-object actors read the prior target; queue-class-10 runs before the controller; the controller integrates current input; queue-class-8 and class-2 actors read the updated target in the same frame",
            "button2_jump_route": "with Down clear, a new input bit-5 edge at 0xc716 enters 0xc434 and sets fixed-record +3=2; the next 0xc13e dispatch indexes 0xc164 at offset 2, whose word 0x0626 targets 0xc78a; +4=0 selects launch entry 0xc7bc, where Up bit 3 chooses 0x0580 instead of 0x0440",
            "jump_horizontal_motion": {
                "dispatch": "the common jump tail at 0xc7b4 branches to 0xc786, which jumps to 0x20b4 after vertical jump handling",
                "vector_selection": "0xc52a refreshes actor +15 from horizontal input during ascent, on entry to the eight-tick falling pose, and during falling physics",
                "motion": "0x20b4 indexes the actor's signed 8.8 vector pointer at +24 using +15 and applies only its X component to actor +6; rows 0/1 steer left/right, rows 2/3 have zero X",
                "bytes_validated": {name: stage0_fixed_controller_valid[name] for name in (
                    "stage0_jump_horizontal_tail",
                    "stage0_jump_horizontal_vector_dispatch",
                    "stage0_jump_horizontal_vector_helper",
                    "stage0_jump_ascent_input_vector_refresh",
                    "stage0_jump_fall_pose_vector_refresh",
                    "stage0_jump_fall_physics_vector_refresh",
                )},
            },
            "input_effect": "0xc400 reads horizontal bits 0/1 through 0xc52a, button-edge bit 5 through 0xc716, charge bit 4 through 0xc610, and Down bit 2 through 0xc550; Down enters state 4 at 0xc4d6, whose row-2/3 selector uses the slow ±1 X vector even with horizontal input. On release, 0xc604 clears state +4 without changing +15, so the old slow row is integrated once more by 0x208c before state 0 resumes. The 0xc782 wrapper calls 0x208c, which applies the signed vector table at 0xc01c to the fixed record's +6/+10 16.16 coordinates that alias global target a5+0x184c/+0x1850; the following 0x38f6 probes do not clamp that target.",
            "live_update_order_bytes_validated": {
                "phase_loop": stage0_fixed_controller_valid[
                    "stage0_live_frame_call_order"],
                "dispatcher_sequence": stage0_fixed_controller_valid[
                    "stage0_object_controller_class2_dispatch_order"],
            },
            "willow_link": "Stage 0 actor 0xa2a6 enters 0x125d0; its facing helper 0x12d44 compares actor X at +6 with shared target X a5+0x184c. This establishes the Stage 0 input controller's indirect target/facing link to Willow, though it does not by itself specify all native movement and state transitions",
            "alternate_jump_distinction": "normal Stage 0 Button 2 jump uses computed dispatcher 0xc78a; alternate controller 0xd4ba is a separate selector-1 route, and jump-like transition 0xc33e remains test-mode gated",
            "input_meaning_limit": "MAME maps bit 5 to Button 2; the Stage 0 jump transition is taken only on a new bit-5 edge with Down clear, while bit 3 selects the stronger launch",
            "bytes_validated": stage0_fixed_controller_valid,
        },
        "internal_jump_animation_path": {
            "dispatcher": "0x00c8f2 selects handlers from a table indexed by actor +4",
            "animation_entry": "state-zero setup at 0x00c910 initializes +60/+5/+62, advances +4, sets a six-tick timer, and falls through to 0x00c92e to select animation table 0x00d2ca",
            "next_state": "0x00c93e expires the six-tick timer and advances +4; the following handler at 0x00c952 starts an eight-tick 0x00d2ce pose before setting state +3=2 and +4=4",
            "relationship": "this is an internally coherent animation/movement state path, but no static branch/call/table pointer to dispatcher 0x00c8f2 was found; the verified Stage 0 actor at 0x00a2a6 dispatches through 0x00ffc8 to 0x00125d0",
            "direct_reference_scan": {
                "target": "0x00c8f2",
                "matches": animation_dispatch_xrefs,
                "instruction_families": ["Bcc/BSR short and word", "JSR/JMP absolute word and long", "JSR/JMP/LEA PC-relative"],
                "interpretation": "absence of direct static instructions does not rule out runtime-computed dispatch",
            },
            "literal_pointer_word_scan": {
                "target_word": "0xc8f2",
                "matches": animation_dispatch_literals,
                "interpretation": "no literal big-endian pointer word found; runtime-computed addresses remain possible",
            },
            "bytes_validated": {name: validated[name] for name in (
                "internal_jump_animation_state_dispatch",
                "internal_jump_animation_state_zero",
                "internal_jump_animation_timing",
                "internal_jump_animation_button_probe_tail",
            )},
        },
        "direct_reference_scan": {
            "target": "0x00c78a",
            "instruction_families": [
                "Bcc/BSR short and word",
                "JSR/JMP absolute word and long",
                "JSR/JMP/LEA PC-relative",
            ],
            "matches": direct_xrefs,
            "interpretation": "no direct static reference found; computed dispatch and indirect function pointers are not covered",
        },
        "literal_pointer_word_scan": {
            "target_word": "0xc78a",
            "matches": literal_word_matches,
            "interpretation": "no big-endian literal target word found in the assembled CPU region; this does not rule out runtime-computed addresses",
        },
        "active_object_dispatcher_scan": {
            "dispatcher": "0x00ffa2",
            "table": "0x00ffc8",
            "entry_count": len(handlers),
            "handler_indices_matching_jump_dispatcher": [
                index for index, address in enumerate(handlers)
                if address == target
            ],
            "interpretation": "0xc78a is not one of the active object handler entries; other tables and callbacks remain possible",
        },
        "limitations": [
            "The verified computed route belongs to fixed controller record a5+0x1846; the detailed relationship between that target jump and every Stage 0 player animation/collision transition is still incomplete.",
            "The direct xref scan does not find c78a because it is reached through the 0xc164 computed state table; the separate selector-1 route to 0xdc0a is not used for the Stage 0 Button 2 jump.",
        ],
    }
def catalog(data: bytes) -> dict[str, object]:
    dispatch = bytes.fromhex(
        "4ded1dc63e3c00314a2e0000670e7000"
        "102e0020e548207b000e4e904dee0080"
    )
    if data[OBJECT_DISPATCH:OBJECT_DISPATCH + len(dispatch)] != dispatch:
        raise ValueError("Object handler dispatcher at 0xffa2 changed")
    handlers = [
        int.from_bytes(data[HANDLER_TABLE + index * 4:
                            HANDLER_TABLE + index * 4 + 4], "big")
        for index in range(HANDLER_COUNT)
    ]
    if handlers[0] != 0x00010054 or handlers[1] != 0x000115EE \
            or handlers[3] != 0x000125D0:
        raise ValueError("Object handler table no longer matches known Stage 0 handlers")
    next_dispatch = bytes.fromhex("102e0002323b00064efb1002")
    if HANDLER_TABLE + HANDLER_COUNT * 4 != HANDLER_TABLE_END \
            or data[HANDLER_TABLE_END:HANDLER_TABLE_END + len(next_dispatch)] != next_dispatch:
        raise ValueError("Behavior handler table end at 0x10054 changed")
    charge_child = charge_release_trace(data)
    enemy_spawns = generic_enemy_spawn_trace(data)
    class6_contacts = class6_contact_pool_trace(data)
    scripted_actor_message = scripted_actor_message_sequence(data)

    allocations: list[dict[str, object]] = []
    cursor = 0
    while True:
        site = data.find(ALLOC, cursor)
        if site < 0:
            break
        cursor = site + 2
        fields: dict[str, int] = {}
        coordinate_sources: dict[str, str] = {}
        scan_end = min(site + 4 + SCAN_LIMIT, len(data))
        pos = site + 4
        while pos < scan_end:
            # Stop before another allocator call; its setup belongs to a new
            # object even when no branch or return separates the records.
            if data[pos:pos + 4] == ALLOC:
                break
            # Some child effects inherit position from the parent object.
            if pos + 6 <= scan_end and data[pos:pos + 2] == b"\x39\x6e":
                source = int.from_bytes(data[pos + 2:pos + 4], "big")
                target = int.from_bytes(data[pos + 4:pos + 6], "big")
                if source == target and source in (0x0006, 0x000A):
                    axis = "x" if source == 6 else "y"
                    fields.pop(axis, None)
                    coordinate_sources[axis] = "parent"
                    pos += 6
                    continue
            decoded = immediate_write(data, pos)
            if decoded:
                name, value, length = decoded
                fields[name] = value
                coordinate_sources.pop(name, None)
                pos += length
            else:
                pos += 2
        behavior_index = fields.get("field_32")
        behavior_handler = None
        if behavior_index is not None and behavior_index < HANDLER_COUNT:
            behavior_handler = f"0x{handlers[behavior_index]:06x}"
        allocations.append({
            "call_site": f"0x{site:06x}",
            "fields": fields,
            "behavior_handler": behavior_handler,
            "coordinate_sources": coordinate_sources,
            "position_complete": "x" in fields and "y" in fields,
            "bytes_scanned": pos - (site + 4),
        })

    # Several routines reserve multiple objects before assigning any fields.
    # A forward scan from each allocator can then attribute the first object's
    # setup to the last call in that cluster. Preserve the decoded fields as
    # leads, but mark their per-call ownership unresolved.
    groups: list[list[dict[str, object]]] = []
    for row in allocations:
        site = int(str(row["call_site"]), 16)
        if groups:
            previous = int(str(groups[-1][-1]["call_site"]), 16)
            if site - previous <= CLUSTER_GAP:
                groups[-1].append(row)
                continue
        groups.append([row])
    for group in groups:
        sites = [row["call_site"] for row in group]
        for row in group:
            row["allocation_cluster"] = sites
            row["field_attribution_confident"] = len(group) == 1

    return {
        "source": "offline Willow USA 68000 region",
        "allocator": "0x002b9c",
        "method": "find jsr $2b9c sites, decode immediate field writes and direct parent-coordinate copies, and resolve byte +32 through the verified dispatcher at 0xffa2",
        "behavior_dispatcher": {
            "address": "0x00ffa2",
            "table": "0x00ffc8",
            "object_record_count": OBJECT_RECORD_COUNT,
            "handler_entry_count": HANDLER_COUNT,
            "selector_field": "+32",
            "handlers": [f"0x{address:06x}" for address in handlers],
        },
        "stage0_charge_release_child": charge_child,
        "stage_rng_initialization": stage_rng_initialization(data),
        "player_jump_entry_trace": player_jump_entry_trace(data, handlers),
        "stage0_charge_spawn_timing": {
            "entry_handler": "0x00c610",
            "timer_store_instruction": "0x00c620",
            "initial_delay_ticks": int.from_bytes(data[0xC622:0xC624], "big"),
            "counter_active_flag_offset": "+63",
            "counter_value_offset": "+49",
            "counter_limit": int.from_bytes(data[0xC65A:0xC65C], "big"),
            "attack_threshold": int.from_bytes(data[0xC668:0xC66A], "big"),
            "spawn_reload_table_address": "0x00c706",
            "spawn_reload_table": list(data[0xC706:0xC70E]),
            "first_decrement_handler": "0x00c6c0",
            "first_counter_increment_and_decrement_timing": "next held-charge update after the entry handler returns",
        },
        "stage0_burst_startup_timing": {
            "timer_store_instruction": "0x00ce02",
            "startup_ticks": int.from_bytes(data[0xCE04:0xCE06], "big"),
            "countdown_instruction": "0x00ce18",
            "entry_update_counts_as_first_tick": True,
            "movement_condition": "horizontal speed is installed when the decremented timer reaches zero",
        },
        "player_state4_queue14_spawn": player_state4_queue14_spawn_trace(data),
        "player_state4_class12_spawn": player_state4_class12_spawn_trace(data),
        "camera_triggered_actor_batch": camera_triggered_actor_batch_trace(data),
        "generic_enemy_spawn_paths": enemy_spawns,
        "class6_contact_pool": class6_contacts,
        "scripted_actor_message_sequence": scripted_actor_message,
        "scripted_actor_spawn_candidate": scripted_actor_spawn_candidate(data),
        "limitations": [
            "Object positions inherited from earlier code are not resolved.",
            "Indirect values, copied templates, and non-immediate writes are not decoded.",
            "Parent-coordinate copies are recorded but not resolved to world positions.",
            "Object queue classes assigned outside the scanned setup block are not resolved.",
            "The +32 handler address is resolved, but a handler address alone does not classify behavior as enemy, projectile, or effect.",
            "Immediate +32 values 35 and above are outside the 35-entry table at 0xffc8; their separate dispatch path is unresolved.",
            "Field ownership is unresolved for calls grouped within 16 bytes; multiple objects may be initialized after the final allocation.",
        ],
        "allocations": allocations,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", nargs="?", type=Path, default=Path("willow.zip"))
    parser.add_argument("--output", type=Path, default=Path("build/willow_objects.json"))
    parser.add_argument("--header", type=Path)
    args = parser.parse_args()
    try:
        program = bytes(assemble_maincpu(args.archive))
        result = catalog(program)
        header = stage0_high_score_header(program) if args.header else None
    except (OSError, ValueError, KeyError) as exc:
        parser.error(str(exc))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="ascii")
    if args.header:
        args.header.parent.mkdir(parents=True, exist_ok=True)
        args.header.write_text(header, encoding="ascii")
    resolved = sum(bool(row["position_complete"]) for row in result["allocations"])
    inherited = sum(
        row["coordinate_sources"].get("x") == "parent"
        and row["coordinate_sources"].get("y") == "parent"
        for row in result["allocations"]
    )
    ambiguous = sum(
        not row["field_attribution_confident"]
        for row in result["allocations"]
    )
    print(f"Catalogued {len(result['allocations'])} object allocations; "
          f"{resolved} have explicit X/Y writes, {inherited} copy both from parents; "
          f"{ambiguous} belong to close allocation clusters with unresolved field ownership")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
