#!/usr/bin/env python3
"""Generate native Stage 0 story constants from Willow's verified script."""

from __future__ import annotations

import argparse
from pathlib import Path

from cps1_program import assemble_maincpu
from cps1_text_task import decode as decode_text_task


MESSAGE_ID = 0x2E
MESSAGE_TASK = 0x1302
MESSAGE_RECORD_BASE = 0x70000
MESSAGE_RECORD_SIZE = 0xA8
TASK_SITE = 0xA25C
OPENING_TASK_COMPLETION = 0x7976
OPENING_TASK_COMPLETION_BYTES = bytes.fromhex("422d8e58422d02a64e75")
TEXT_TASK_STATE0_ENTRY = 0x7906
PHASE_MESSAGE_ID = 0x40
PHASE_MESSAGE_STYLE = 0x1422
PHASE_CONTROLLER_STATE4 = 0x8DA8
PHASE_CONTROLLER_STATE4_BYTES = bytes.fromhex(
    "1b7c000103484eb82dfe4eb84f444eb90004035e"
    "61000cd4102d00620c00002066044eb8572c"
    "3b7c0384000a546d0008303c1002323c0008"
    "4ef81e54"
)
PHASE_CONTROLLER_TICK = 0x8DE4
PHASE_CONTROLLER_TICK_BYTES = bytes.fromhex(
    "536d000a672a4a2d03c26624"
    "522d00194eb81f884eb84ea24eb900009ef2"
    "4eb900039a524eb90003807c4ef90003b524"
)
PHASE_EVENT_TABLE = 0x8E50
PHASE_EVENT_TABLE_BYTES = bytes.fromhex("405556575859")
PHASE_DESTINATION_TABLE = 0x7A1E
PHASE_DESTINATION_BYTES = bytes.fromhex("0091091c")
PHASE_FOLLOWUP_TASK = 0x8E5C
PHASE_FOLLOWUP_TASK_BYTES = bytes.fromhex(
    "3b7c00b4000a546d00081b7c00018e58323c0600"
    "302d0344123b0018d040343b0018303c00c0"
    "207c000078f04ef815cc"
)
PHASE_FOLLOWUP_EVENT_TABLE = 0x8E8E
PHASE_FOLLOWUP_EVENT_TABLE_BYTES = bytes.fromhex("41666768696a")
PHASE_FOLLOWUP_STYLE_TABLE = 0x8E94
PHASE_FOLLOWUP_STYLE_TABLE_BYTES = bytes.fromhex("142314241424142414241424")
PHASE_FOLLOWUP_WAIT_COMPLETION = 0x8EA6
PHASE_FOLLOWUP_WAIT_COMPLETION_BYTES = bytes.fromhex(
    "536d000a66e0303c08fb4eb8786a4eb8785e"
    "546d0004426d00084ef816b4"
)
PHASE_CONTROLLER_SUBSTATE_DISPATCH = 0x8EC4
PHASE_CONTROLLER_SUBSTATE_DISPATCH_BYTES = bytes.fromhex(
    "302d0008323b00064efb1002"
)
PHASE_CONTROLLER_SUBSTATE_TABLE = 0x8ED0
PHASE_CONTROLLER_SUBSTATE_TABLE_BYTES = bytes.fromhex(
    "000ef7d6002a00ea011e018801a0"
)
TEXT_DESTINATION_TABLE = 0x7A1E
PHASE_FOLLOWUP_DESTINATION = bytes.fromhex("00910728")
SCROLL1_NATIVE_COLUMN_OFFSET = 5
POST_PHASE_STATE2 = 0x8EFA
POST_PHASE_STATE2_BYTES = bytes.fromhex(
    "1b7c000003434eb80ac4546d00083b7c000a000a"
)
POST_PHASE_MESSAGE_3E = 0x8F92
POST_PHASE_MESSAGE_3E_BYTES = bytes.fromhex(
    "1b7c00018e58323c003e343c1615303c00c0207c000078f04eb815cc"
)
POST_PHASE_STATE4 = 0x8FBA
POST_PHASE_STATE4_BYTES = bytes.fromhex(
    "4a2d8e58662c536d000a66263b7c001e000a546d0008"
)
POST_PHASE_MESSAGE_3F = 0x8FD0
POST_PHASE_MESSAGE_3F_BYTES = bytes.fromhex(
    "1b7c00018e58323c003f343c1611303c00c0207c000078f04ef815cc"
)
POST_PHASE_STATE6 = 0x8FEE
POST_PHASE_STATE6_BYTES = bytes.fromhex(
    "4a2d8e5866f8536d000a66f2546d0008"
)
POST_PHASE_FINAL_WAIT = 0x8FFA
POST_PHASE_FINAL_WAIT_BYTES = bytes.fromhex(
    "546d00083b7c0080000a"
)
POST_PHASE_STATE8 = 0x9070
POST_PHASE_STATE8_BYTES = bytes.fromhex("546d0000426d0004426d00084e75")
FRONT_END_TEXT_SEQUENCE = 0x8480
FRONT_END_TEXT_SEQUENCE_BYTES = bytes.fromhex(
    "536d000a66ee3b7c000a000a546d0008"
    "1b7c00018e58323c0034343c1607303c00c0"
    "207c000078f04ef815cc4a2d8e5866c2"
    "536d000a66bc1b7c00018e58323c0035"
    "343c1608303c00c0207c000078f04eb815cc"
    "3b7c001e000a546d00084e75"
)


def source_text_record(data: bytes, message_id: int) -> str:
    record_offset = MESSAGE_RECORD_BASE + message_id * MESSAGE_RECORD_SIZE
    record = data[record_offset:record_offset + MESSAGE_RECORD_SIZE]
    end = record.find(b"/", 8)
    if end < 0:
        raise ValueError(f"story message {message_id:#x} has no '/' terminator")
    text = record[8:end].decode("ascii")
    if not text or any(char not in " ABCDEFGHIJKLMNOPQRSTUVWXYZ,.!?'-" for char in text):
        raise ValueError(f"story message {message_id:#x} contains unsupported glyphs")
    return text


def source_glyph_record(data: bytes, message_id: int) -> bytes:
    """Return a native glyph-code message, preserving zero-valued glyphs."""
    record_offset = MESSAGE_RECORD_BASE + message_id * MESSAGE_RECORD_SIZE
    record = data[record_offset:record_offset + MESSAGE_RECORD_SIZE]
    end = record.find(b"/", 8)
    if end < 0:
        raise ValueError(f"front-end message {message_id:#x} has no '/' terminator")
    glyphs = record[8:end]
    if not glyphs or len(glyphs) > 40:
        raise ValueError(f"front-end message {message_id:#x} has invalid glyph count")
    return glyphs


def extract_front_end_text(program: bytes) -> tuple[bytes, int, int, bytes, int, int]:
    if program[FRONT_END_TEXT_SEQUENCE:
               FRONT_END_TEXT_SEQUENCE + len(FRONT_END_TEXT_SEQUENCE_BYTES)] \
            != FRONT_END_TEXT_SEQUENCE_BYTES:
        raise ValueError("front-end message task sequence changed")
    waits = (
        (0x82C6, bytes.fromhex("3b7c00b4000a"), "first attract actor wait"),
        (0x83BC, bytes.fromhex("3b7c003c000a"), "second attract actor wait"),
        (0x8486, bytes.fromhex("3b7c000a000a"), "first text countdown"),
        (0x84D4, bytes.fromhex("3b7c001e000a"), "second text countdown"),
    )
    for address, expected, label in waits:
        if program[address:address + len(expected)] != expected:
            raise ValueError(f"front-end {label} changed")
    if (int.from_bytes(program[0x82C8:0x82CA], "big")
            + int.from_bytes(program[0x83BE:0x83C0], "big") != 240):
        raise ValueError("front-end first-message schedule changed")
    specs = ((0x34, 0x1607, 6, 14), (0x35, 0x1608, 12, 18))
    messages: list[bytes] = []
    for message_id, style, expected_x, expected_y in specs:
        destination_offset = TEXT_DESTINATION_TABLE + (style & 0xFF) * 4
        destination = int.from_bytes(
            program[destination_offset:destination_offset + 4], "big")
        x, y = scroll1_cell(destination)
        if (x, y) != (expected_x, expected_y):
            raise ValueError(f"front-end message {message_id:#x} destination changed")
        messages.append(source_glyph_record(program, message_id))
    return (messages[0], specs[0][2], specs[0][3],
            messages[1], specs[1][2], specs[1][3])


def extract_post_phase_messages(
        data: bytes) -> tuple[str, int, int, str, int, int, int, int, int]:
    """Read the two exact text tasks between the Stage 0 phase message and play."""
    contexts = (
        (POST_PHASE_STATE2, POST_PHASE_STATE2_BYTES, "state-2 setup"),
        (POST_PHASE_MESSAGE_3E, POST_PHASE_MESSAGE_3E_BYTES, "message 0x3e task"),
        (POST_PHASE_STATE4, POST_PHASE_STATE4_BYTES, "state-4 timer"),
        (POST_PHASE_MESSAGE_3F, POST_PHASE_MESSAGE_3F_BYTES, "message 0x3f task"),
        (POST_PHASE_STATE6, POST_PHASE_STATE6_BYTES, "state-6 timer"),
        (POST_PHASE_FINAL_WAIT, POST_PHASE_FINAL_WAIT_BYTES, "final wait"),
        (POST_PHASE_STATE8, POST_PHASE_STATE8_BYTES, "gameplay handoff"),
    )
    for address, expected, name in contexts:
        if data[address:address + len(expected)] != expected:
            raise ValueError(f"post-phase story context changed: {name}")

    post_phase_3e_delay = int.from_bytes(data[0x8F0A:0x8F0C], "big")
    post_phase_3f_delay = int.from_bytes(data[0x8FC8:0x8FCA], "big")
    actor_transition_delay = int.from_bytes(data[0x9000:0x9002], "big")
    if (post_phase_3e_delay, post_phase_3f_delay, actor_transition_delay) \
            != (10, 30, 128):
        raise ValueError("Stage 0 post-phase wait immediates changed")

    message_specs = ((0x3E, 0x1615), (0x3F, 0x1611))
    messages: list[tuple[str, int, int]] = []
    for message_id, style in message_specs:
        destination_address = TEXT_DESTINATION_TABLE + (style & 0xFF) * 4
        destination = int.from_bytes(
            data[destination_address:destination_address + 4], "big")
        x, y = scroll1_cell(destination)
        messages.append((source_text_record(data, message_id), x, y))
    first, second = messages
    return (first[0], first[1], first[2], second[0], second[1], second[2],
            post_phase_3e_delay, post_phase_3f_delay, actor_transition_delay)


def scroll1_cell(address: int, base: int = 0x910000) -> tuple[int, int]:
    """Map a CPS Scroll 1 RAM pointer through tilemap0_scan layout."""
    relative = address - base
    if relative < 0 or relative & 3:
        raise ValueError(f"invalid Scroll 1 destination {address:#x}")
    tile_index = relative // 4
    return (tile_index >> 5) & 0x3F, (tile_index & 0x1F) | ((tile_index >> 6) & 0x20)


def extract(data: bytes) -> tuple[str, int, int]:
    # Keep the exact task construction and both timer states tied to the
    # checked source image. The sum is the delay before the task is created.
    contexts = {
        "stage0_opening_timer_entry": (0xA140,
            bytes.fromhex("3b7c0007000a")),
        "state4": (0xA234, bytes.fromhex("536d000a6616546d00083b7c003f000a")),
        "opening_task": (TASK_SITE, bytes.fromhex(
            "536d000a67024e751b7c000102a63b7c001d000a546d0008"
            "323c002e343c1302303c00c0207c000078f04eb815cc")),
        # The story parser clears its own message/task flags at the '/'
        # terminator. This completion path does not write the controller's
        # +0x03c2 phase-exit gate; that gate has a separate writer at 0xcf7c.
        "opening_task_completion": (OPENING_TASK_COMPLETION,
                                     OPENING_TASK_COMPLETION_BYTES),
        # The task's zero-state dispatch target is the generic text parser at
        # 0x7912. Its '/' terminator clears the text-active flag (+0x02a6).
        "text_task_state0_dispatch": (TEXT_TASK_STATE0_ENTRY,
                                       bytes.fromhex("000c")),
        # Destination selector 2 resolves through the source pointer table to
        # Scroll 1 cell address 0x9102b4, decoded by the text writer as (5,13).
        "destination_selector_2": (0x7A28, bytes.fromhex("02b40091")),
    }
    for name, (address, expected) in contexts.items():
        if data[address:address + len(expected)] != expected:
            raise ValueError(f"Stage 0 story source context changed: {name}")

    message = decode_text_task(data, MESSAGE_ID, MESSAGE_TASK)
    text = message['stream'][:-1].decode('ascii')
    if not text or any(char not in " ABCDEFGHIJKLMNOPQRSTUVWXYZ,.!?'-" for char in text):
        raise ValueError("Stage 0 opening message contains unsupported glyphs")
    opening_delay = (int.from_bytes(data[0xA142:0xA144], "big")
                     + int.from_bytes(data[0xA240:0xA242], "big"))
    controller_wait = int.from_bytes(data[TASK_SITE + 16:TASK_SITE + 18],
                                     "big")
    return text, opening_delay, controller_wait


def extract_phase_message(data: bytes) -> tuple[str, int]:
    if data[PHASE_CONTROLLER_STATE4:
            PHASE_CONTROLLER_STATE4 + len(PHASE_CONTROLLER_STATE4_BYTES)] \
            != PHASE_CONTROLLER_STATE4_BYTES:
        raise ValueError("Stage 0 phase controller state-4 entry changed")
    if data[PHASE_CONTROLLER_TICK:
            PHASE_CONTROLLER_TICK + len(PHASE_CONTROLLER_TICK_BYTES)] \
            != PHASE_CONTROLLER_TICK_BYTES:
        raise ValueError("Stage 0 phase countdown/update order changed")
    phase_delay = int.from_bytes(data[0x8DD0:0x8DD2], "big")
    if phase_delay != 900:
        raise ValueError(f"unexpected Stage 0 phase delay: {phase_delay}")
    if data[PHASE_EVENT_TABLE:PHASE_EVENT_TABLE + len(PHASE_EVENT_TABLE_BYTES)] \
            != PHASE_EVENT_TABLE_BYTES:
        raise ValueError("900-frame phase message table changed")
    destination_index = PHASE_MESSAGE_STYLE & 0xFF
    destination_offset = PHASE_DESTINATION_TABLE + destination_index * 4
    if data[destination_offset:destination_offset + 4] != PHASE_DESTINATION_BYTES:
        raise ValueError("900-frame phase message destination changed")
    record_offset = MESSAGE_RECORD_BASE + PHASE_MESSAGE_ID * MESSAGE_RECORD_SIZE
    record = data[record_offset:record_offset + MESSAGE_RECORD_SIZE]
    end = record.find(b"/", 8)
    if end < 0:
        raise ValueError("900-frame phase message has no '/' terminator")
    text = record[8:end].decode("ascii")
    if not text or any(char not in " ABCDEFGHIJKLMNOPQRSTUVWXYZ,.!?'-" for char in text):
        raise ValueError("900-frame phase message contains unsupported glyphs")
    return text, phase_delay


def extract_phase_followup(data: bytes) -> tuple[str, int]:
    if data[PHASE_FOLLOWUP_TASK:PHASE_FOLLOWUP_TASK + len(
            PHASE_FOLLOWUP_TASK_BYTES)] != PHASE_FOLLOWUP_TASK_BYTES:
        raise ValueError("phase follow-up task setup changed")
    followup_delay = int.from_bytes(data[PHASE_FOLLOWUP_TASK + 2:
                                          PHASE_FOLLOWUP_TASK + 4], "big")
    if followup_delay != 180:
        raise ValueError(f"unexpected Stage 0 follow-up delay: {followup_delay}")
    if data[PHASE_FOLLOWUP_EVENT_TABLE:PHASE_FOLLOWUP_EVENT_TABLE
            + len(PHASE_FOLLOWUP_EVENT_TABLE_BYTES)] \
            != PHASE_FOLLOWUP_EVENT_TABLE_BYTES:
        raise ValueError("phase follow-up event table changed")
    if data[PHASE_FOLLOWUP_STYLE_TABLE:PHASE_FOLLOWUP_STYLE_TABLE
            + len(PHASE_FOLLOWUP_STYLE_TABLE_BYTES)] \
            != PHASE_FOLLOWUP_STYLE_TABLE_BYTES:
        raise ValueError("phase follow-up style table changed")
    if data[PHASE_FOLLOWUP_WAIT_COMPLETION:
            PHASE_FOLLOWUP_WAIT_COMPLETION
            + len(PHASE_FOLLOWUP_WAIT_COMPLETION_BYTES)] \
            != PHASE_FOLLOWUP_WAIT_COMPLETION_BYTES:
        raise ValueError("phase follow-up wait completion changed")
    if data[PHASE_CONTROLLER_SUBSTATE_DISPATCH:
            PHASE_CONTROLLER_SUBSTATE_DISPATCH
            + len(PHASE_CONTROLLER_SUBSTATE_DISPATCH_BYTES)] \
            != PHASE_CONTROLLER_SUBSTATE_DISPATCH_BYTES:
        raise ValueError("phase controller substate dispatcher changed")
    if data[PHASE_CONTROLLER_SUBSTATE_TABLE:
            PHASE_CONTROLLER_SUBSTATE_TABLE
            + len(PHASE_CONTROLLER_SUBSTATE_TABLE_BYTES)] \
            != PHASE_CONTROLLER_SUBSTATE_TABLE_BYTES:
        raise ValueError("phase controller substate table changed")
    message_id = data[PHASE_FOLLOWUP_EVENT_TABLE]
    style = int.from_bytes(data[PHASE_FOLLOWUP_STYLE_TABLE:
                                PHASE_FOLLOWUP_STYLE_TABLE + 2], "big")
    if message_id != 0x41 or style != 0x1423:
        raise ValueError("Stage 0 follow-up event data changed")
    destination_offset = TEXT_DESTINATION_TABLE + (style & 0xFF) * 4
    if data[destination_offset:destination_offset + 4] != PHASE_FOLLOWUP_DESTINATION:
        raise ValueError("Stage 0 follow-up Scroll 1 destination changed")
    record_offset = MESSAGE_RECORD_BASE + message_id * MESSAGE_RECORD_SIZE
    record = data[record_offset:record_offset + MESSAGE_RECORD_SIZE]
    end = record.find(b"/", 8)
    if end < 0:
        raise ValueError("phase follow-up message has no '/' terminator")
    text = record[8:end].decode("ascii")
    if not text or any(char not in " ABCDEFGHIJKLMNOPQRSTUVWXYZ,.!?'-" for char in text):
        raise ValueError("phase follow-up message contains unsupported glyphs")
    return text, followup_delay


def c_string(value: str) -> str:
    return '"' + value.replace('\\', '\\\\').replace('"', '\\"') + '"'


def c_bytes(name: str, value: bytes) -> str:
    escaped = "".join(f"\\x{byte:02x}" for byte in value)
    return f'#define {name} ((const unsigned char *) "{escaped}")'


def header(text: str, opening_delay: int, controller_wait: int,
           phase_text: str, phase_delay: int,
           phase_followup: str, phase_followup_delay: int,
           followup_x: int, followup_y: int,
           post_phase_3e: str, post_phase_3e_x: int, post_phase_3e_y: int,
           post_phase_3f: str, post_phase_3f_x: int, post_phase_3f_y: int,
           post_phase_3e_delay: int, post_phase_3f_delay: int,
           actor_transition_delay: int,
           front_end_34: bytes, front_end_34_x: int, front_end_34_y: int,
           front_end_35: bytes, front_end_35_x: int, front_end_35_y: int) -> str:
    # The style word 0x1302 selects half-record format, three-tick glyphs and position
    # selector 2. The shared text destination table resolves selector 2 to
    # CPS Scroll 1 cell (5,13). The controller wait is not a text lifetime.
    # The parser clears its task flags at '/'; it
    # does not erase the written Scroll 1 glyphs or the phase-exit flag.
    lines = [
        "/* Generated by tools/cps1_story_catalog.py from willow.zip. */",
        f"#define WILLOW_STORY_STAGE0_OPENING_DELAY {opening_delay}",
        f"#define WILLOW_STORY_STAGE0_CONTROLLER_WAIT {controller_wait}",
        f"#define WILLOW_STORY_STAGE0_OPENING_PALETTE {MESSAGE_TASK >> 8 & 0x0F}",
        f"#define WILLOW_STORY_STAGE0_OPENING_SOURCE_X {5}",
        f"#define WILLOW_STORY_STAGE0_OPENING_X {5 - SCROLL1_NATIVE_COLUMN_OFFSET}",
        f"#define WILLOW_STORY_STAGE0_OPENING_Y {13}",
        f"static const char willow_story_stage0_opening[] = {c_string(text)};",
        f"#define WILLOW_STORY_PHASE_MESSAGE_DELAY {phase_delay}",
        f"#define WILLOW_STORY_PHASE_MESSAGE_PALETTE {(PHASE_MESSAGE_STYLE >> 8) & 0x0F}",
        f"#define WILLOW_STORY_PHASE_MESSAGE_SOURCE_X 18",
        f"#define WILLOW_STORY_PHASE_MESSAGE_X {18 - SCROLL1_NATIVE_COLUMN_OFFSET}",
        f"#define WILLOW_STORY_PHASE_MESSAGE_Y 7",
        f"static const char willow_story_phase_message[] = {c_string(phase_text)};",
        f"#define WILLOW_STORY_PHASE_FOLLOWUP_DELAY {phase_followup_delay}",
        "#define WILLOW_STORY_PHASE_FOLLOWUP_PALETTE 4",
        f"#define WILLOW_STORY_PHASE_FOLLOWUP_SOURCE_X {followup_x}",
        f"#define WILLOW_STORY_PHASE_FOLLOWUP_X {followup_x - SCROLL1_NATIVE_COLUMN_OFFSET}",
        f"#define WILLOW_STORY_PHASE_FOLLOWUP_Y {followup_y}",
        f"static const char willow_story_phase_followup[] = {c_string(phase_followup)};",
        f"#define WILLOW_STORY_POST_PHASE_3E_DELAY {post_phase_3e_delay}",
        f"#define WILLOW_STORY_POST_PHASE_3F_DELAY {post_phase_3f_delay}",
        f"#define WILLOW_STORY_POST_PHASE_ACTOR_TRANSITION {actor_transition_delay}",
        "#define WILLOW_STORY_POST_PHASE_PALETTE 6",
        f"#define WILLOW_STORY_POST_PHASE_3E_X {post_phase_3e_x - SCROLL1_NATIVE_COLUMN_OFFSET}",
        f"#define WILLOW_STORY_POST_PHASE_3E_Y {post_phase_3e_y}",
        f"static const char willow_story_post_phase_3e[] = {c_string(post_phase_3e)};",
        f"#define WILLOW_STORY_POST_PHASE_3F_X {post_phase_3f_x - SCROLL1_NATIVE_COLUMN_OFFSET}",
        f"#define WILLOW_STORY_POST_PHASE_3F_Y {post_phase_3f_y}",
        f"static const char willow_story_post_phase_3f[] = {c_string(post_phase_3f)};",
        "/* Pre-game text task glyph codes, preserved byte-for-byte. */",
        "/* Controller 0x8480 countdown values, validated by its instruction bytes. */",
        "#define WILLOW_FRONTEND_ACTOR1_WAIT 180",
        "#define WILLOW_FRONTEND_ACTOR2_WAIT 60",
        "#define WILLOW_FRONTEND_MESSAGE_INITIAL_WAIT 240",
        "#define WILLOW_FRONTEND_MESSAGE_BETWEEN_WAIT 10",
        "#define WILLOW_FRONTEND_MESSAGE_FINAL_WAIT 30",
        f"#define WILLOW_FRONTEND_MESSAGE_34_LENGTH {len(front_end_34)}",
        f"#define WILLOW_FRONTEND_MESSAGE_34_X {front_end_34_x - SCROLL1_NATIVE_COLUMN_OFFSET}",
        f"#define WILLOW_FRONTEND_MESSAGE_34_Y {front_end_34_y}",
        f"#define WILLOW_FRONTEND_MESSAGE_34_PALETTE {0x1607 >> 8 & 0x0F}",
        c_bytes("willow_frontend_message_34", front_end_34),
        f"#define WILLOW_FRONTEND_MESSAGE_35_LENGTH {len(front_end_35)}",
        f"#define WILLOW_FRONTEND_MESSAGE_35_X {front_end_35_x - SCROLL1_NATIVE_COLUMN_OFFSET}",
        f"#define WILLOW_FRONTEND_MESSAGE_35_Y {front_end_35_y}",
        f"#define WILLOW_FRONTEND_MESSAGE_35_PALETTE {0x1608 >> 8 & 0x0F}",
        c_bytes("willow_frontend_message_35", front_end_35),
        "",
    ]
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", nargs="?", type=Path, default=Path("willow.zip"))
    parser.add_argument("--output", type=Path, default=Path("build/willow_story.h"))
    args = parser.parse_args()
    try:
        program = bytes(assemble_maincpu(args.archive))
        text, opening_delay, controller_wait = extract(program)
        phase_text, phase_delay = extract_phase_message(program)
        phase_followup, phase_followup_delay = extract_phase_followup(program)
        followup_style = int.from_bytes(program[PHASE_FOLLOWUP_STYLE_TABLE:
                                                PHASE_FOLLOWUP_STYLE_TABLE + 2], "big")
        followup_destination_offset = (
            TEXT_DESTINATION_TABLE + (followup_style & 0xFF) * 4)
        followup_destination = int.from_bytes(
            program[followup_destination_offset:followup_destination_offset + 4], "big")
        followup_x, followup_y = scroll1_cell(followup_destination)
        (post_phase_3e, post_phase_3e_source_x, post_phase_3e_y,
         post_phase_3f, post_phase_3f_source_x, post_phase_3f_y,
         post_phase_3e_delay, post_phase_3f_delay,
         actor_transition_delay) = \
            extract_post_phase_messages(program)
        (front_end_34, front_end_34_x, front_end_34_y,
         front_end_35, front_end_35_x, front_end_35_y) = \
            extract_front_end_text(program)
    except (OSError, KeyError, ValueError) as exc:
        parser.error(str(exc))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(header(text, opening_delay, controller_wait,
                                  phase_text, phase_delay,
                                  phase_followup, phase_followup_delay,
                                  followup_x, followup_y,
                                  post_phase_3e, post_phase_3e_source_x,
                                  post_phase_3e_y, post_phase_3f,
                                  post_phase_3f_source_x, post_phase_3f_y,
                                  post_phase_3e_delay, post_phase_3f_delay,
                                  actor_transition_delay,
                                  front_end_34, front_end_34_x, front_end_34_y,
                                  front_end_35, front_end_35_x, front_end_35_y),
                           encoding="ascii")
    print(f"Generated Stage 0 opening message ({len(text)} characters) to {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
