#!/usr/bin/env python3
"""Decode Willow's Z80 direct-OKI and sequenced sound command tables."""

from __future__ import annotations

import argparse
import json
import zipfile
from pathlib import Path

from cps1_program import assemble_maincpu

SYSTEM_FADE_UP_HANDLER = 0x03C1
SYSTEM_FADE_UP_HANDLER_BYTES = bytes.fromhex(
    "0601af3204d03a0af03203d0783205d0c9"
)
SYSTEM_SEQUENCER_RESET_HANDLER = 0x035F
SYSTEM_SEQUENCER_RESET_HANDLER_BYTES = bytes.fromhex(
    "cd830316003e0fcd17013e07cda100af3221d03222d02100d5"
    "1101d5013f003600edb0c9"
)
SYSTEM_YM_WRITE_HELPER = 0x0117
SYSTEM_YM_WRITE_HELPER_BYTES = bytes.fromhex(
    "0101f0f50acb7f20fbf10d020cf57a02f1c9"
)


def maincpu_sound_call_sites(main_rom: bytes) -> list[dict[str, object]]:
    """List immediate D0 commands sent through fixed sound helpers.

    The arcade main CPU is decoded offline only. Dynamic D0 values and other
    queue writers are intentionally omitted rather than guessed.
    """
    calls: list[dict[str, object]] = []
    for helper in (0x786A, 0x786C):
        for call_opcode in (0x4EB8, 0x4EF8):  # JSR/JMP absolute word
            pattern = (bytes((call_opcode >> 8, call_opcode & 0xFF))
                       + helper.to_bytes(2, "big"))
            offset = 0
            while True:
                call_at = main_rom.find(pattern, offset)
                if call_at < 0:
                    break
                offset = call_at + 2
                # MOVE.W #imm,D0 immediately before the sound helper.
                if call_at < 4 or main_rom[call_at - 4:call_at - 2] != b"\x30\x3c":
                    continue
                word = int.from_bytes(main_rom[call_at - 2:call_at], "big")
                call: dict[str, object] = {
                    "call_site": f"0x{call_at:06x}",
                    "helper": f"0x{helper:04x}",
                    "command_word": f"0x{word:04x}",
                    "command": f"0x{word & 0xff:02x}",
                    "control_flow_validated": False,
                    "instruction_boundary_validated": False,
                }
                # Willow's Stage 0 state-6 release calls 0x12dfa when its
                # 16-tick substate expires and player byte +33 is not 6.
                # That helper queues sequenced sound command 0x34 before
                # allocating the class-10 selector-0 effect.
                if call_at == 0x12DFE and helper == 0x786C:
                    helper_context = bytes.fromhex(
                        "303c00344eb8786c4eb82b4e673c"
                        "197c00010000197c00000020196e0030000f"
                    )
                    caller_context = bytes.fromhex(
                        "532e002266f80c2e00060021671a"
                        "542e00041d7c00100022610003ea"
                    )
                    if (main_rom[0x12DFA:0x12DFA + len(helper_context)]
                            == helper_context
                            and main_rom[0x129F6:0x129F6 + len(caller_context)]
                            == caller_context):
                        call["instruction_boundary_validated"] = True
                        call["control_flow_validated"] = True
                        call["source_context"] = {
                            "handler": "0x129f6",
                            "state": 6,
                            "substate": 4,
                            "command_load": "0x12dfa",
                            "command_write": "0x12dfe",
                            "condition": "the 16-tick timer expires and player byte +33 is not 6",
                            "note": "Verified Stage 0 sequenced cue 0x34 preceding the class-10 selector-0 actor allocation.",
                        }
                # The later state-2 substate path at 0x138ba queues the same
                # cue before allocating its second class-10 selector-0 actor.
                # Keep the audio catalog tied to the exact command write and
                # allocation sequence; the caller's two trigger branches are
                # validated separately by the object catalog.
                if call_at == 0x138BE and helper == 0x786C:
                    context = bytes.fromhex(
                        "303c00344eb8786c4eb82b4e674e"
                    )
                    if main_rom[0x138BA:0x138BA + len(context)] == context:
                        call["instruction_boundary_validated"] = True
                        call["control_flow_validated"] = True
                        call["source_context"] = {
                            "handler": "0x125d0",
                            "state": 2,
                            "substate": "later selector-0 effect path",
                            "command_load": "0x138ba",
                            "command_write": "0x138be",
                            "condition": "the state-2 range trigger or 16-tick substate timer selects the second effect path",
                            "note": "Verified Stage 0 sequenced cue 0x34 before the second class-10 selector-0 allocation; native cue playback is not implemented yet.",
                        }
                # The direct sample write is inside state zero's branch in
                # the +3 dispatcher at 0xced4. The nearby controller entry at
                # 0xcdc2 uses a separate +4 table and has no call/branch into
                # 0xced4; byte adjacency does not establish an owner. Keep
                # the dispatcher's caller and actor identity unresolved.
                if call_at == 0xcef8 and helper == 0x786A:
                    context = bytes.fromhex(
                        "102e0003323b000e4ebb100a"
                        "6100f45c6000f4ac0008003600460060"
                        "422e0031303c00834eb8786a"
                        "1d7c00280022542e0003102e000f02400001"
                        "0a0000011d40000f41fa051e4ef81f34"
                    )
                    next_state = bytes.fromhex(
                        "532e002266061d7c000600024ef81f4c"
                    )
                    if (main_rom[0xced4:0xced4 + len(context)] == context
                            and main_rom[0xcf1e:0xcf1e + len(next_state)]
                            == next_state):
                        call["instruction_boundary_validated"] = True
                        call["source_context"] = {
                            "dispatcher": "0xced4",
                            "dispatch_field": "actor byte +3",
                            "state_index": 0,
                            "state_handler": "0xcef0",
                            "command_write": "0xcef4",
                            "after_command": "sets +34=40, advances +3 to 2, toggles facing bit +15 low bit, and installs table 0xd436",
                            "next_transition": "after the 40-tick timer, state handler 0xcf1e sets byte +2 to 6",
                            "note": "The +3 state-zero branch, command write, and 40-tick transition are verified locally. No caller or owning actor has been established; neighboring routine 0xcdc2 uses a separate dispatcher and is not validated as this handler's parent.",
                        }
                # The jump collision exit at 0xc856 clears jump state and
                # tails into MOVE.W #$2b,D0 / JMP $786a at 0xc868/0xc86c.
                # Verify the complete source path before promoting this raw
                # byte-pattern hit to a runtime event.
                if call_at == 0xC86C and helper == 0x786A:
                    context = bytes.fromhex(
                        "4eb81f4c302e003848c0e18091ae000a"
                        "302e00380c4007006408d06e00343d400038"
                        "6100fce64a2e0024660a610000684eb838f6"
                        "671e422e00481d6e000f0030422e0003422e0004"
                        "303c002b4ef8786a"
                    )
                    if main_rom[0xC820:0xC820 + len(context)] == context:
                        call["instruction_boundary_validated"] = True
                        call["control_flow_validated"] = True
                        call["source_context"] = {
                            "handler": "0xc820",
                            "command_load": "0xc868",
                            "command_write": "0xc86c",
                            "condition": "jump collision byte +36 is set or the 0x38f6 side probe reports contact",
                            "note": "The verified collision exit queues sequenced cue 0x2b before returning to the normal state.",
                        }
                # Profile-1 actor handler 0x1196a queues direct command 0x8a
                # when the state-6 wait timer reaches zero at 0x1198e. The
                # sound event is verified, but its actor creation path is not
                # connected to the native Stage 0 child descriptors.
                if call_at == 0x119C6 and helper == 0x786C:
                    context = bytes.fromhex(
                        "102e0004323b00064efb10020006001800"
                        "661d7c003c0022542e000441fa064c4ef81f30"
                        "532e0022672e4eb81f4c6100fee86722422e0004"
                        "6100feceb02e000f670e1d7c000800351d7c00040003"
                        "4e751d7c000800034e75303c008a4eb8786c"
                    )
                    if main_rom[0x1196A:0x1196A + len(context)] == context:
                        call["instruction_boundary_validated"] = True
                        call["control_flow_validated"] = True
                        call["source_context"] = {
                            "handler": "0x1196a",
                            "state": 6,
                            "substate": "wait timer expiry",
                            "command_load": "0x119c2",
                            "command_write": "0x119c6",
                            "condition": "the state-6 wait timer reaches zero after 60 ticks",
                            "note": "Source-verified profile-1 actor voice cue; command 0x8a maps to OKI sample 0x0a on voice mask 0x2. Native actor reachability is not established.",
                        }
                # The Stage 0 type-2 actor queues cue 0x33 after its
                # substate-6 recovery probe reports contact. Check both the
                # landing transition and the full substate-8 handler so this
                # cue is associated with the right native actor event.
                if call_at == 0x11DA4 and helper == 0x786A:
                    dispatcher = bytes.fromhex(
                        "102e0003323b000a4ebb10064ef82244"
                    )
                    substate_table = bytes.fromhex(
                        "000c0026007c00a000ea0108"
                    )
                    landing = bytes.fromhex(
                        "4eb83b0a670e0a2e0001000f"
                        "1d7c000800034e75"
                    )
                    handler = bytes.fromhex(
                        "303c00334eb8786a4eb84908542e0003"
                        "1d7c0008002241fa02344ef81f30"
                    )
                    if (main_rom[0x11CA6:0x11CA6 + len(dispatcher)] == dispatcher
                            and main_rom[0x11CB6:0x11CB6 + len(substate_table)]
                                == substate_table
                            and main_rom[0x11D78:0x11D78 + len(landing)] == landing
                            and main_rom[0x11DA0:0x11DA0 + len(handler)] == handler):
                        call["instruction_boundary_validated"] = True
                        call["control_flow_validated"] = True
                        call["source_context"] = {
                            "handler": "0x11da0",
                            "state": 4,
                            "substate": 8,
                            "command_load": "0x11da0",
                            "command_write": "0x11da4",
                            "dispatch": "0x11ca6 table 0x11cb6 maps substate 8 to 0x11da0",
                            "condition": "substate-6 terrain probe at 0x11d7c reports contact and selects substate 8",
                            "note": "Verified Stage 0 sequenced cue 0x33; it resolves to sequence 0x8f90. The native sequencer is not implemented.",
                        }
                # Charge state 0xC610 sends sequenced cue 0x31 exactly when
                # the held charge counter reaches 16. It repeats cue 0x44
                # every 16 global frames after the counter saturates at 127.
                if call_at == 0xC672 and helper == 0x786A:
                    context = bytes.fromhex(
                        "0c2e007f00316700004c522e00310c2e00100031"
                        "6608303c00314eb8786a"
                    )
                    if main_rom[0xC658:0xC658 + len(context)] == context:
                        call["instruction_boundary_validated"] = True
                        call["control_flow_validated"] = True
                        call["source_context"] = {
                            "handler": "0xc610",
                            "state": 0,
                            "command_load": "0xc66e",
                            "command_write": "0xc672",
                            "condition": "charge counter +49 increments to 16 while charge input remains held",
                            "note": "Verified sequenced cue 0x31; this is not a direct OKI sample command.",
                        }
                if call_at == 0xC6BA and helper == 0x786A:
                    context = bytes.fromhex(
                        "102d00190200000f66c0303c00444eb8786a60b6"
                    )
                    if main_rom[0xC6AC:0xC6AC + len(context)] == context:
                        call["instruction_boundary_validated"] = True
                        call["control_flow_validated"] = True
                        call["source_context"] = {
                            "handler": "0xc610",
                            "state": 0,
                            "command_load": "0xc6b6",
                            "command_write": "0xc6ba",
                            "condition": "charge counter is saturated at 127 and global frame low nibble is zero",
                            "note": "Verified repeated sequenced cue 0x44; this is not a direct OKI sample command.",
                        }
                # The burst state's substate table sends sequenced cue 0x2a
                # through the sound helper before initializing its pose timer.
                if call_at == 0xCDF0 and helper == 0x786A:
                    context = bytes.fromhex(
                        "000a00360050009a00dc303c002a4eb8786a"
                    )
                    if main_rom[0xCDE2:0xCDE2 + len(context)] == context:
                        call["instruction_boundary_validated"] = True
                        call["control_flow_validated"] = True
                        call["source_context"] = {
                            "handler": "0xcde2",
                            "state": "burst action",
                            "substate": 0,
                            "command_load": "0xcdec",
                            "command_write": "0xcdf0",
                            "condition": "burst action enters substate 0",
                            "note": "Verified sequenced cue 0x2a; this is not a direct OKI sample command.",
                        }
                # The queue-class-14 state-4 target-follow entry starts a
                # sequenced cue before moving the actor to the shared target.
                if call_at == 0x2D246 and helper == 0x786A:
                    context = bytes.fromhex(
                        "102e0003323b000a4ebb10064ef82244"
                        "0006006e009e303c002d4eb8786a542e0003"
                        "1d7c002000223d6d184c0006302d1850"
                    )
                    if main_rom[0x2D22C:0x2D22C + len(context)] == context:
                        call["instruction_boundary_validated"] = True
                        call["control_flow_validated"] = True
                        call["source_context"] = {
                            "handler": "0x2d22c",
                            "state": 4,
                            "substate": 0,
                            "command_load": "0x2d242",
                            "command_write": "0x2d246",
                            "condition": "queue-class-14 behavior-0 enters state 4 with substate 0",
                            "note": "Verified sequenced cue 0x2d starts the target-follow pose; native sequencer playback is not implemented.",
                        }
                calls.append(call)
    # A shared actor-update helper selects one of two direct OKI commands
    # when its mirrored stage-state byte changes. The command is chosen from
    # actor byte +32, then sent through the ordinary sound helper. Preserve
    # this conditional path separately because the command load is not
    # adjacent to the helper call and the actor's Stage 0 owner is unresolved.
    transition_sound = bytes.fromhex(
        "102d0364b02d0389670a1b4003894eb84ce46012"
        "102d036267506b4eb02d0365672e1b400365"
        "303c00824a2e00206704303c00844eb8786a"
    )
    stage0_controller_update = bytes.fromhex(
        "4a2d03ea66dc102e0003323b001a4ebb1016"
        "4eb837724eb839fe61000016610001e060000230"
    )
    progress_writer_blocks = (
        (0x345E8, bytes.fromhex(
            "062d000203641b6d03640389103c00014eb84d6a4eb84a34")),
        (0x34620, bytes.fromhex(
            "062d000303641b6d03640389103c00024eb84d6a4eb84a34")),
        (0x34658, bytes.fromhex(
            "062d000103641b6d03640389103c00004eb84d6a4eb84a34")),
    )
    if (main_rom[0xC182:0xC182 + len(transition_sound)] == transition_sound
            and main_rom[0xC13E:0xC13E + len(stage0_controller_update)]
                == stage0_controller_update
            and all(main_rom[address:address + len(expected)] == expected
                    for address, expected in progress_writer_blocks)):
        calls = [call for call in calls if call["call_site"] != "0x00c1b6"]
        calls.append({
            "call_site": "0x00c1b6",
            "helper": "0x786a",
            "command_word": "conditional",
            "command": "0x82 or 0x84",
            "command_candidates": ["0x82", "0x84"],
            "control_flow_validated": True,
            "instruction_boundary_validated": True,
            "source_context": {
                "handler": "0xc170",
                "dispatchers": ["0xc13e"],
                "stage0_link": "verified: the Stage 0 fixed-controller gameplay dispatcher at 0xc13e calls 0xc170 every active update",
                "condition": "a5+0x0362 is positive and a5+0x0364 differs from its mirror a5+0x0389",
                "selection": "actor byte +32 equal to zero selects 0x82; nonzero selects 0x84",
                "known_progress_writer_behavior": "the three validated direct writers at 0x345e8, 0x34620, and 0x34658 change a5+0x0364 by 2, 3, or 1 and immediately copy it to mirror a5+0x0389; these writes alone do not leave the inequality required by 0xc170",
                "native_trigger_gap": "other direct or indirect writers that could leave a5+0x0364 different from a5+0x0389, plus the fixed record's +32 value at a5+0x1866, are not modeled or proven reachable in normal Stage 0",
                "command_write": "0xc1b6",
                "next_effect": "sets actor timer +54 to 128 and animation timer +78 to 4",
                "instruction_checks": {
                    "stage0_controller_update": "0xc13e",
                    "conditional_audio_helper": "0xc182",
                    "synchronous_progress_writers": [
                        f"0x{address:06x}" for address, _expected in progress_writer_blocks
                    ],
                },
                "note": "The Stage 0 controller link, source condition, command variants, and known synchronous progress writers are byte-validated; native playback awaits proof of a reachable writer that leaves the mirror different and the selection flag value.",
            },
        })
    calls.sort(key=lambda row: (str(row["call_site"]), str(row["helper"])))
    return calls


def sound_cpu_address_region(address: int) -> str:
    """Classify Z80 sequence pointers using CPS-1's sound CPU map."""
    if address < 0x8000:
        return "fixed"
    if address < 0xC000:
        return "banked ROM"
    if 0xD000 <= address <= 0xD7FF:
        return "sound RAM"
    if address in (0xF000, 0xF001, 0xF002, 0xF004, 0xF006, 0xF008, 0xF00A):
        return "sound I/O register"
    return "unmapped"


def sound_cpu_rom_offset(address: int, bank: int = 1) -> int | None:
    """Map an executed sound-CPU pointer into the offline ROM image."""
    region = sound_cpu_address_region(address)
    if region == "fixed":
        return address
    if region == "banked ROM" and bank in (0, 1):
        return 0x8000 + bank * 0x4000 + address - 0x8000
    return None


def linear_track_event_prefix(sound_rom: bytes, offset: int,
                              limit: int = 32) -> list[dict[str, object]]:
    """Classify straight-line note/timing bytes up to the first control.

    Control events can change the stream pointer, so this deliberately does
    not step over or infer their operands. It is a bounded data view, not a
    track emulator.
    """
    events: list[dict[str, object]] = []
    for index in range(limit):
        address = offset + index
        if address >= len(sound_rom):
            break
        value = sound_rom[address]
        if value < 0x20:
            category = "control"
        elif value < 0x40:
            category = "timing"
        else:
            category = "note"
        events.append({
            "rom_offset": f"0x{address:04x}",
            "value": f"0x{value:02x}",
            "category": category,
        })
        if category == "control":
            break
    return events


def sequence_commands(sound_rom: bytes) -> tuple[dict[str, object], list[dict[str, object]]]:
    """Resolve low-bit commands through the sound CPU's two cue tables."""
    # Validate the actual Z80 byte lanes: 0x01fa loads each cue pointer's first
    # byte into IX+1 and its second byte into IX+0, then playback reconstructs
    # HL as H=(IX+0), L=(IX+1). The stored cue words are therefore little-endian
    # even though the loader's temporary IX fields are reversed.
    pointer_loader = bytes.fromhex(
        "0608c5131add7701131add7700cd1302013000dd09c110eac9"
    )
    if sound_rom[0x01FA:0x01FA + len(pointer_loader)] != pointer_loader:
        raise ValueError("Z80 cue track-pointer loader at 0x01fa changed")
    event_dispatch = bytes.fromhex(
        "cd260be6f0fe20da4a09fe40da2b09c5cda408c1"
    )
    if sound_rom[0x0581:0x0581 + len(event_dispatch)] != event_dispatch:
        raise ValueError("Z80 track event dispatcher at 0x0581 changed")
    control_restart_vectors = {
        0x18: bytes.fromhex("856f300124c9"),
        0x28: bytes.fromhex("87df5e235623c9"),
        0x30: bytes.fromhex("e1efebe9"),
    }
    for address, expected in control_restart_vectors.items():
        if sound_rom[address:address + len(expected)] != expected:
            raise ValueError(
                f"Z80 control-event restart vector at {address:#04x} changed"
            )
    control_router = bytes.fromhex("78f7cd5009c38105")
    if (sound_rom[0x0950:0x0952] != control_router[:2]
            or sound_rom[0x094A:0x094A + 6] != control_router[2:]):
        raise ValueError("Z80 RST-0x30 control-event router changed")
    timing_handler = bytes.fromhex(
        "cd3109c38105fe202802180c78e60f3cdd770addcb08cec9"
        "78e60fdd7709c9"
    )
    if sound_rom[0x092B:0x092B + len(timing_handler)] != timing_handler:
        raise ValueError("Z80 track timing handler at 0x092b changed")
    track_scheduler_gate = bytes.fromhex(
        "dd2100d10608c53e08903213d0dd7e00ddb601281b"
    )
    if sound_rom[0x0453:0x0453 + len(track_scheduler_gate)] != track_scheduler_gate:
        raise ValueError("Z80 nonzero track-pointer scheduler gate changed")
    zero_read_event_handler = bytes.fromhex(
        "cd260b3e8080672e00cb3ccb3ccb3ccb1d3a1ad0b720042215d0c9"
    )
    if sound_rom[0x0972:0x0972 + len(zero_read_event_handler)] != zero_read_event_handler:
        raise ValueError("Z80 control-event 0 handler at 0x0972 changed")
    note_table_selector = bytes.fromhex(
        "503a1ad0b720052a15d018032a17d05d6c260078070707e607d602"
        "280947cb23cb15cb1410f8"
    )
    if sound_rom[0x08A4:0x08A4 + len(note_table_selector)] != note_table_selector:
        raise ValueError("Z80 selected note-table lookup at 0x08a4 changed")
    fixed_table = 0xE6D
    fixed_count = sound_rom[fixed_table] + 1
    banked_source = 0xC000
    banked_count = sound_rom[banked_source] + 1
    event_table = 0x952
    fixed_end = fixed_table + 1 + fixed_count * 2
    banked_end = banked_source + 4 + banked_count * 2
    if fixed_end > len(sound_rom) or banked_end > len(sound_rom):
        raise ValueError("sound sequence index table extends past the ROM")

    rows: list[dict[str, object]] = []
    for command in range(0x80):
        fixed_index = command % fixed_count
        bank_index = fixed_index % banked_count
        pointer_offset = banked_source + 4 + bank_index * 2
        sequence_address = int.from_bytes(
            sound_rom[pointer_offset:pointer_offset + 2], "little"
        )
        if not 0x8000 <= sequence_address < 0xC000:
            raise ValueError(
                f"invalid banked sequence pointer {sequence_address:#06x} "
                f"for command {command:#04x}"
            )
        sequence_offset = banked_source + sequence_address - 0x8000
        track_rows = []
        for track in range(8):
            entry = sequence_offset + 1 + track * 2
            # Routine 0x01fa stores byte 0 at record+1 and byte 1 at
            # record+0; the tick code reloads H from +0 and L from +1.
            track_address = int.from_bytes(sound_rom[entry:entry + 2], "little")
            if track_address == 0:
                track_rows.append({
                    "track": track,
                    "sequence_address": "0x0000",
                    "region": "empty",
                    "rom_offset": None,
                    "prefix_hex": "",
                    "first_event": None,
                    "scheduled_by_driver": False,
                })
                continue
            track_region = sound_cpu_address_region(track_address)
            if track_region == "fixed":
                track_rom_offset = track_address
                region = "fixed"
            elif track_region == "banked ROM":
                track_rom_offset = banked_source + track_address - 0x8000
                region = "bank 1"
            else:
                # MAME's address_map constructor initializes unmapped reads to
                # zero. Willow's CPS-1 sound submap does not override that
                # value, so this is the initial byte returned by the MAME
                # reference path (not a claim about physical open-bus silicon).
                unmapped_read = {
                    "mame_default_read_value": "0x00",
                    "first_event_category": "control",
                    "first_event_dispatch_word_address": "0x0952",
                    "first_event_dispatch_word": f"0x{int.from_bytes(sound_rom[0x952:0x954], 'little'):04x}",
                    "first_event_handler": f"0x{int.from_bytes(sound_rom[0x952:0x954], 'little'):04x}",
                    "note": "MAME address-map default only; physical unmapped-bus behavior is unverified.",
                } if track_region == "unmapped" else None
                track_rows.append({
                    "track": track,
                    "sequence_address": f"0x{track_address:04x}",
                    "region": track_region,
                    "rom_offset": None,
                    "prefix_hex": "",
                    "first_event": None,
                    # The update loop at 0x0453 tests only whether the track
                    # pointer is zero; unmapped nonzero addresses still run.
                    "scheduled_by_driver": True,
                    "linear_prefix_events": [{
                        "rom_offset": None,
                        "value": "0x00",
                        "category": "control",
                        "source": "MAME unmapped-read default",
                    }],
                    "unmapped_read_analysis": unmapped_read,
                })
                continue
            first_event = sound_rom[track_rom_offset]
            if first_event < 0x20:
                category = "control"
                dispatch_offset = event_table + first_event * 2
                dispatch_word = int.from_bytes(
                    sound_rom[dispatch_offset:dispatch_offset + 2], "little"
                )
                event_details = {
                    "value": f"0x{first_event:02x}",
                    "category": category,
                    "dispatch_word_address": f"0x{dispatch_offset:04x}",
                    "dispatch_word": f"0x{dispatch_word:04x}",
                    "within_16_entry_dispatch_table": first_event < 16,
                }
            elif first_event < 0x40:
                event_details = {
                    "value": f"0x{first_event:02x}",
                    "category": "timing/control",
                    "handler": "0x092b",
                }
            else:
                event_details = {
                    "value": f"0x{first_event:02x}",
                    "category": "note",
                    "handler": "0x0581",
                }
            track_rows.append({
                "track": track,
                "sequence_address": f"0x{track_address:04x}",
                "region": region,
                "rom_offset": f"0x{track_rom_offset:04x}",
                "prefix_hex": sound_rom[track_rom_offset:track_rom_offset + 16].hex(),
                "first_event": event_details,
                "scheduled_by_driver": True,
                "linear_prefix_events": linear_track_event_prefix(
                    sound_rom, track_rom_offset
                ),
            })
        rows.append({
            "command": f"0x{command:02x}",
            "fixed_cue_index": fixed_index,
            "banked_cue_index": bank_index,
            "sequence_address": f"0x{sequence_address:04x}",
            "banked_rom_offset": f"0x{sequence_offset:04x}",
            "sequence_prefix_hex": sound_rom[sequence_offset:sequence_offset + 16].hex(),
            "track_pointers": track_rows,
        })

    metadata = {
        "fixed_table_address": f"0x{fixed_table:04x}",
        "fixed_entry_count": fixed_count,
        "bank_select": 1,
        "banked_cpu_table_address": "0x8000",
        "banked_rom_offset": f"0x{banked_source:04x}",
        "banked_entry_count": banked_count,
        "addressing_note": "Z80 handler 0x0132 routes low-bit commands through 0x014c; the second lookup uses bank 1 selected at 0x0162.",
        "sound_cpu_address_map": "CPS-1 sub_map: 0x0000-0x7fff fixed ROM, 0x8000-0xbfff banked ROM, 0xd000-0xd7ff RAM, and selected YM2151/OKI/bank/latch registers at 0xf000-0xf00a; other addresses are unmapped.",
        "unmapped_read_reference": "MAME address_map defaults unmapped reads to zero unless overridden; Willow's sound submap does not override it. This describes the software reference path, not confirmed physical open-bus behavior.",
        "track_pointer_note": "The cue header byte is followed by eight little-endian track addresses. The byte lanes are validated at Z80 routine 0x01fa and its playback loader at 0x0b26.",
        "track_scheduler_gate": "At 0x0453, the eight-track update loop skips a record only when its two-byte pointer is zero; each nonzero pointer enters the track update path.",
        "unmapped_read_event_path": "MAME's address_map default is zero and Willow's sound submap has no unmap override. An initial 0x00 fetch therefore selects validated control event 0 at 0x0972. That handler consumes one operand, loads HL with (0x80 + operand) << 8, applies three SRL H instructions and one RR L, then stores the result in global sound-RAM note-table pointer 0xd015 or 0xd017 according to selector 0xd01a. Note setup at 0x08a4 selects one of those pointers and uses it as the base for its note-table calculation. This models MAME's configured map, not physical open-bus behavior; note-table contents and YM register effects still need decoding.",
        "event_parser_note": "Z80 routine 0x0581 routes event bytes 0x00-0x1f through control handlers at 0x094a, 0x20-0x3f through timing handlers at 0x092b, and 0x40-0xff through note setup at 0x08a4/0x0609. Timing command 0x20 sets a countdown of (following byte & 0x0f)+1; other timing commands store following-byte low-nibble delay. The visible control dispatch table at 0x0952 has 16 entries; later words overlap following code. Offline track decoding follows repeat handler 0x0998 and its little-endian pointer changes. FM instrument/envelope writes and their note-to-YM register effects are not yet converted to Genesis music.",
        "control_event_restart_path": "RST 0x30 at 0x0950 pops the return address into HL, RST 0x28 doubles the full control byte and RST 0x18 adds that offset to the 0x0952 table base before loading a target and jumping to it. Only entries 0x00-0x0f are a verified table; 0x10-0x1f read overlapping code bytes, so their computed addresses are candidates, not proven event handlers.",
        "validated_driver_bytes": {
            "cue_track_pointer_loader": "0x01fa",
            "playback_track_pointer_loader": "0x0b26",
            "track_event_dispatcher": "0x0581",
            "control_event_restart_vectors": ["0x0018", "0x0028", "0x0030"],
            "control_event_router": "0x094a/0x0950",
            "track_timing_handler": "0x092b",
            "control_event_zero_note_table_pointer": "0x0972",
            "note_table_selector_and_indexer": "0x08a4",
        },
    }
    metadata["control_event_dispatch"] = {
        "table_address": "0x0952",
        "entry_count": 16,
        "entries": control_event_handlers(sound_rom),
        "event_zero_mame_unmapped_example": {
            "cue_0x2b_track_addresses": ["0xff0a", "0xff00"],
            "unmapped_event_and_operand_reads": ["0x00", "0x00"],
            "derived_note_table_pointer": "0x1000",
            "pointer_slot": "0xd015 or 0xd017, selected by 0xd01a",
            "derivation": (
                "event 0 handler reads operand 0, forms HL=0x8000, applies "
                "three SRL H and one RR L, producing 0x1000"
            ),
            "scope": (
                "MAME's default unmapped-read behavior plus validated Z80 "
                "instructions; physical open-bus behavior is not established"
            ),
        },
        "overlapping_dispatch_candidates": extended_control_targets(sound_rom),
        "note": "The first 16 targets are validated handlers. Event 0x00 consumes an operand and sets one global note-table pointer selected by 0xd01a; note setup at 0x08a4 reads that pointer. Events 0x10-0x1f read overlapping words from following code/data, so candidate destinations are recorded without claiming handler semantics. Event 0x02 changes stream flow according to per-track repeat state.",
    }
    return metadata, rows


def note_keycode_table(sound_rom: bytes) -> list[int]:
    """Extract the Z80 track player's note-index to YM2151 keycode table.

    RST 20h at 0x20 indexes through HL, and all four FM operator setup paths
    load HL=0x0b36 before using it. The table is the initial B natural-key
    code followed by eight octaves of the twelve valid YM2151 key codes and
    the final B key of block 7 (97 bytes total).
    """
    loader = bytes.fromhex("dd6600dd6e017e4723dd7400dd7501c9")
    if sound_rom[0x0B26:0x0B26 + len(loader)] != loader:
        raise ValueError("Z80 track-byte loader at 0x0b26 changed")
    octave = (0x00, 0x01, 0x02, 0x04, 0x05, 0x06,
              0x08, 0x09, 0x0A, 0x0C, 0x0D, 0x0E)
    expected = [0x0E]
    for block in range(8):
        expected.extend((block << 4) | key for key in octave)
    expected.append(0x80)
    # The byte at 0x0b36 is the special lower boundary key (B); the following
    # bytes are C0 through the final B7. A final C8 is not in this table.
    expected = expected[:-1]
    actual = list(sound_rom[0x0B36:0x0B36 + len(expected)])
    if actual != expected:
        raise ValueError("YM2151 note-keycode table at 0x0b36 changed")
    return actual


def ym2151_note_level_tables(sound_rom: bytes) -> dict[str, object]:
    """Export validated lookup data used by the arcade's note-level routine."""
    curve_helper = bytes.fromhex(
        "3a19d0cb792802c640211807e7f561cd980747f16107070707"
        "cd98070f0f0f0fe60f800f0f0fe61fc9"
    )
    nibble_multiply_helper = bytes.fromhex("e6f06f7ce60fb521a407e7c9")
    rst20_table_read = bytes.fromhex("856f3001247ec9")
    if sound_rom[0x06EF:0x06EF + len(curve_helper)] != curve_helper:
        raise ValueError("YM2151 note-level curve helper at 0x06ef changed")
    if sound_rom[0x0798:0x0798 + len(nibble_multiply_helper)] != nibble_multiply_helper:
        raise ValueError("YM2151 nibble multiplication helper at 0x0798 changed")
    if sound_rom[0x0020:0x0020 + len(rst20_table_read)] != rst20_table_read:
        raise ValueError("Z80 RST 0x20 table-read helper changed")
    curve = list(sound_rom[0x0718:0x0798])
    multiply = list(sound_rom[0x07A4:0x08A4])
    if len(curve) != 128 or len(multiply) != 256:
        raise ValueError("YM2151 note-level lookup tables are truncated")
    expected_multiply = [((index >> 4) * (index & 0x0F))
                         for index in range(256)]
    if multiply != expected_multiply:
        raise ValueError("YM2151 nibble-product lookup table changed")
    return {
        "note_curve_table": {
            "rom_address": "0x0718",
            "entry_count": len(curve),
            "indexing": "note keycode index 0-63, plus 64 when patch input bit 7 is set",
            "values_hex": bytes(curve).hex(),
        },
        "nibble_product_table": {
            "rom_address": "0x07a4",
            "entry_count": len(multiply),
            "indexing": "(A high nibble * H low nibble), result is an unsigned product 0-225",
            "values_hex": bytes(multiply).hex(),
            "validated_as": "all 256 nibble products",
        },
        "algorithm_context": {
            "calculator": "0x06ae",
            "curve_helper": "0x06ef",
            "nibble_product_helper": "0x0798",
            "note_index_source": "0xd019 = YM2151 keycode >> 1 & 0x3f at 0x0623",
            "inputs": "Three instrument bytes per operator at patch offsets +8..+19 and live track attenuation byte +0x18",
            "derived_steps": [
                "0x06ef indexes the 128-byte curve with (note_index + 64 when C bit 7 is set), then combines the curve's high and low nibbles with C's low nibble through the 16x16 product table and shifts the sum right three bits.",
                "0x06ae adds the low nibble of its A input to that result, doubles the high three bits of A, transforms track byte +0x18 by complementing it when bit 7 is clear, rotates it left once, and looks up the nibble product.",
                "The high nibble of that product is negated when track byte +0x18 bit 7 is set, then 0x10 and the first combined result are added; carry or sign saturates the return value to 0x7f.",
                "The caller at 0x067d adds live track/global attenuation and the operator's patch byte +8, +11, +14, or +17 before writing the YM2151 total-level register.",
            ],
            "scope": "Lookup tables and instruction-derived arithmetic are recorded; runtime YM2612 register execution is not implemented natively.",
        },
    }


def opm_patch_to_opn2_seed(patch: bytes) -> dict[str, object]:
    """Translate corresponding OPM fields into a YM2612 channel-0 seed.

    The four operators keep their source register-order index. OPM stores slot
    registers eight addresses apart; OPN2 stores them four apart. Key-on mask
    bit order is a separate chip convention and is not used to reorder patch
    register data. The source's dynamic total-level calculator is intentionally
    not flattened into a static patch.
    """
    if len(patch) != 0x28:
        raise ValueError("OPM patch must contain exactly 40 bytes")
    slot_offsets = (0x00, 0x04, 0x08, 0x0C)
    operator_register_mapping = [
        {
            "source_patch_offset": 20 + slot,
            "ym2151_operator_register_offset": slot * 8,
            "ym2612_operator_register_offset": slot_offsets[slot],
        }
        for slot in range(4)
    ]
    writes: list[dict[str, str]] = []
    for source_group, opn_base, mask in (
            (20, 0x30, 0x7F),       # DT/MUL: same bit fields
            (24, 0x50, 0xDF),       # KS/AR; remove OPM's AM-enable bit
            (28, 0x60, 0x1F),       # DR; AM is handled separately
            (32, 0x70, 0x1F),       # SR
            (36, 0x80, 0xFF)):      # SL/RR
        for slot, offset in enumerate(slot_offsets):
            value = patch[source_group + slot] & mask
            writes.append({
                "register": f"0x{opn_base + offset:02x}",
                "value": f"0x{value:02x}",
                "source_patch_offset": str(source_group + slot),
                "operator_slot": str(slot),
            })
    # OPM 0x20 carries channel feedback/algorithm. Genesis OPN2 places the
    # same low six bits in B0; B4 provides stereo routing and LFO sensitivities.
    writes.extend((
        {"register": "0xb0", "value": f"0x{patch[5] & 0x3f:02x}",
         "source_patch_offset": "5", "operator_slot": "channel"},
        {"register": "0xb4", "value": f"0x{0xc0 | ((patch[6] & 0x03) << 4) | ((patch[6] >> 4) & 0x07):02x}",
         "source_patch_offset": "6", "operator_slot": "channel"},
    ))
    return {
        "chip": "YM2612/OPN2",
        "channel": 0,
        "operator_register_mapping": operator_register_mapping,
        "register_writes": writes,
        "translation_scope": "Corresponding OPM DT/MUL, KS/AR, DR, SR, SL/RR, feedback/algorithm, and AMS/PMS fields; slot order preserved.",
        "untranslated": [
            "note-dependent total level from source patch bytes 8-19 and driver attenuation state",
            "OPM LFO waveform, frequency, and depth conversion",
            "chip envelope-rate and key-scaling differences",
            "YM2612 F-number/block pitch conversion",
        ],
        "fidelity": "register seed for native playback development; not a verified sonic match",
        "source_dynamic_total_level_parameters": [
            {
                "operator_slot": slot,
                "patch_offsets": [8 + slot * 3, 9 + slot * 3, 10 + slot * 3],
                "patch_bytes_hex": patch[8 + slot * 3:11 + slot * 3].hex(),
                "native_note_writer_inputs": "passes bytes to the translated 0x06ae/0x06ef level calculation with the live note index and track +0x18 attenuation",
            }
            for slot in range(4)
        ],
    }


def ym2151_patch_catalog(sound_rom: bytes,
                         verified_cue_prefixes: list[dict[str, object]],
                         startup_cue: dict[str, object]) -> dict[str, object]:
    """Extract OPM patches selected by validated cue event 0x0c.

    The Z80 handler indexes 40-byte records from 0x0eb0. Export only records
    referenced by bounded gameplay prefixes or decoded tracks from the
    source-validated Stage 0 startup cue; this avoids treating unrelated code
    or tables as instruments.
    """
    loader = bytes.fromhex(
        "cd260bdd7720672e0016001e28cd290111b00e19dd7421dd7522"
    )
    if sound_rom[0x0A2F:0x0A2F + len(loader)] != loader:
        raise ValueError("YM2151 instrument loader at 0x0a2f changed")

    # This byte-validated block applies each selected 40-byte OPM patch to
    # the source YM2151. It is useful for a native-chip converter: the arcade
    # loader does not simply copy all 40 bytes to chip registers.
    patch_writer = bytes.fromhex(
        "3a1ad0b7c023cb7e20061103001918267e070707e603573e1bcd170123"
        "567a3e18cd1701237ef680573e19cd170123567a3e19cd1701237ef6c0"
        "dd7723573a13d0c620cd170123563a13d0c638cd1701010e00093a13d0"
        "c6405f0614c57b56cd1701c6085fe6f0fe60ccbd0a23c110ecc9"
    )
    if sound_rom[0x0A4C:0x0A4C + len(patch_writer)] != patch_writer:
        raise ValueError("YM2151 patch register writer at 0x0a4c changed")

    note_level_writer = bytes.fromhex(
        "21360bdd7e11e70fe63f3219d0fd4e09fd7e0acdae06fd46080e00571e07"
        "cd7d06fd4e0cfd7e0dcdae06fd460b0e08571e05cd7d06fd4e0ffd7e10"
        "cdae06fd460e0e10571e04cd7d06fd4e12fd7e13cdae06fd46110e1857"
        "180bdd7e23e607bb30037a18133a10d0823814faa20657dd7e2482380afa"
        "a20680573803f2a406167f3a13d0c66081cd1701c9"
    )
    if sound_rom[0x0623:0x0623 + len(note_level_writer)] != note_level_writer:
        raise ValueError("YM2151 note-level writer at 0x0623 changed")
    level_calculator = bytes.fromhex(
        "47e60f67780f0f0f0fe6076fe5cdef06e18467e5cb2565dd7e18cb7f2007"
        "2f083e0f94670807cd9807e10f0f0f0fe60fddcb187e2802ed44c6108438"
        "01f03e7fc9"
    )
    if sound_rom[0x06AE:0x06AE + len(level_calculator)] != level_calculator:
        raise ValueError("YM2151 note-level calculator at 0x06ae changed")

    table_address = 0x0EB0
    record_size = 0x28
    references: dict[int, list[dict[str, str]]] = {}
    cue_traces = list(verified_cue_prefixes)
    cue_traces.append({
        "command": "0x13",
        "call_site": "Stage 0 startup queue at 0x00a114",
        "tracks": startup_cue.get("fixed_track_event_traces", []),
    })
    for cue in cue_traces:
        for track in cue.get("tracks", []):
            if not isinstance(track, dict):
                continue
            for event in track.get("events", []):
                if (not isinstance(event, dict)
                        or event.get("event") != "0x0c"
                        or event.get("kind") != "control"):
                    continue
                operand_hex = str(event.get("operands_hex", ""))
                if len(operand_hex) != 2:
                    raise ValueError("YM2151 instrument event has invalid operand")
                instrument = int(operand_hex, 16)
                references.setdefault(instrument, []).append({
                    "command": str(cue.get("command", "")),
                    "call_site": str(cue.get("call_site", "")),
                    "track": str(track.get("track", "")),
                    "event_rom_offset": str(event.get("rom_offset", "")),
                })

    patches = []
    for instrument in sorted(references):
        address = table_address + instrument * record_size
        patch = sound_rom[address:address + record_size]
        if len(patch) != record_size or address + record_size > 0x8000:
            raise ValueError(f"YM2151 patch {instrument:#04x} is outside fixed ROM")
        channel_offset = "sound RAM byte 0xd013 (per-track YM channel)"
        opm_operator_registers = []
        register = 0x40
        for patch_offset in range(20, 40):
            opm_operator_registers.append({
                "patch_offset": patch_offset,
                "register_base_plus_channel": f"0x{register:02x}",
                "value": f"0x{patch[patch_offset]:02x}",
            })
            register += 8
            if register == 0x60:
                register += 0x20

        patches.append({
            "instrument_index": instrument,
            "rom_address": f"0x{address:04x}",
            "record_size": record_size,
            "references": references[instrument],
            "source_patch_hex": patch.hex(),
            "ym2612_native_seed": opm_patch_to_opn2_seed(patch),
            "source_ym2151_write_plan": {
                "writer_address": "0x0a4c",
                "writer_bytes_validated": patch_writer.hex(),
                "load_gate": "returns without chip writes when sound RAM byte 0xd01a is nonzero",
                "channel_offset_source": channel_offset,
                "optional_lfo_writes": (
                    [
                        {"register": "0x1b", "value": f"0x{(patch[1] >> 3) & 3:02x}"},
                        {"register": "0x18", "value": f"0x{patch[2]:02x}"},
                        {"register": "0x19", "value": f"0x{patch[3] | 0x80:02x}"},
                        {"register": "0x19", "value": f"0x{patch[4]:02x}"},
                    ] if patch[1] & 0x80 else []
                ),
                "channel_writes": [
                    {"register_base": "0x20", "value": f"0x{patch[5] | 0xc0:02x}"},
                    {"register_base": "0x38", "value": f"0x{patch[6]:02x}"},
                ],
                "operator_parameter_writes": opm_operator_registers,
                "omitted_record_bytes": list(range(7, 20)),
                "note": (
                    "Register addresses are base plus the per-track channel byte. "
                    "The 20 values at patch offsets 20-39 advance by 8; the "
                    "source skips register group 0x60 by adding 0x20. This is "
                    "a YM2151 write trace, not an OPN/YM2612 conversion."
                ),
            },
            "source_note_level_inputs": {
                "handler_address": "0x0623",
                "level_calculator_address": "0x06ae",
                "writer_bytes_validated": note_level_writer.hex(),
                "calculator_bytes_validated": level_calculator.hex(),
                "operator_inputs": [
                    {
                        "operator": operator,
                        "patch_offsets": list(range(8 + operator * 3,
                                                     11 + operator * 3)),
                        "ym2151_total_level_register_base": "0x60",
                        "channel_offset": channel_offset,
                        "slot_offset": operator * 8,
                        "source_note": (
                            "the three patch bytes are read at note start and "
                            "passed through 0x06ae before the source writes "
                            "the resulting dynamic total level"
                        ),
                    }
                    for operator in range(4)
                ],
                "limits": [7, 5, 4],
                "limit_note": (
                    "The first three operators pass limits 7, 5, and 4 to "
                    "0x067d; operator four enters after that comparison. "
                    "The calculator also consumes note and track attenuation "
                    "state, so static patch bytes alone do not determine TL."
                ),
                "native_conversion_note": (
                    "Source YM2151 level calculation only; the corresponding "
                    "YM2612 TL and envelope behavior is not implemented."
                ),
            },
            "native_note": (
                "Source OPM parameter data only; native YM2612 register and "
                "envelope conversion is not implemented."
            ),
        })
    return {
        "table_address": f"0x{table_address:04x}",
        "record_size": record_size,
        "index_formula": "0x0eb0 + instrument_index * 0x28",
        "loader_handler": "0x0a2f",
        "loader_validated_prefix_hex": loader.hex(),
        "entry_count_used_by_validated_event_traces": len(patches),
        "patches": patches,
    }


def control_event_handlers(sound_rom: bytes) -> list[dict[str, object]]:
    """Describe the sound CPU's verified 16-entry control-event dispatch."""
    repeat_handler_regions = (
        (0x0998, bytes.fromhex("dd7e0dfe01205add360d001846")),
        (0x09EB, bytes.fromhex("cd260b5fcd260bdd7700dd7301c9")),
        (0x09F9, bytes.fromhex("dd6600dd6e0111020019dd7400dd7501c9")),
    )
    for address, expected in repeat_handler_regions:
        if sound_rom[address:address + len(expected)] != expected:
            raise ValueError(
                f"Z80 repeat-event handler at {address:#06x} changed")
    table = 0x0952
    targets = [int.from_bytes(sound_rom[table + i * 2:table + i * 2 + 2],
                              "little") for i in range(16)]
    expected = [0x0972, 0x0991, 0x0998, 0x09A5, 0x09BD, 0x09D5,
                0x0A0A, 0x0A0F, 0x0A16, 0x0A1D, 0x0A24, 0x0A28,
                0x0A2F, 0x0AC2, 0x0ACB, 0x0ACF]
    if targets != expected:
        raise ValueError("Z80 control-event dispatch table at 0x0952 changed")
    effects = [
        (1, "reads one operand, loads HL with (0x80 + operand) << 8, applies three SRL H and one RR L, then stores HL at global note-table pointer 0xd015 when selector 0xd01a is clear or 0xd017 when set; note setup 0x08a4 selects these pointers for its lookup"),
        (1, "reads one operand into track state +0x07"),
        ("state-dependent", "uses track state +0x0d to repeat or skip a little-endian stream destination"),
        (1, "reads an operand and updates repeat state +0x0d"),
        (1, "reads an operand and updates repeat state +0x0e"),
        (1, "reads an operand and updates repeat state +0x0f"),
        (0, "sets track flag bit 0"),
        (1, "reads one operand into track state +0x10"),
        (1, "reads one operand into track state +0x13"),
        (1, "reads one operand into track state +0x18"),
        (1, "consumes one operand"),
        (2, "consumes two operands"),
        (1, "reads an instrument index and selects its source patch"),
        (1, "reads one operand, negates it, and stores track state +0x24"),
        (1, "consumes one operand"),
        (0, "cue terminator; unwinds event dispatch into the audio-driver cleanup path"),
    ]
    end_handler = bytes.fromhex("e1e1c13a1ad0b72039")
    if sound_rom[0x0ACF:0x0ACF + len(end_handler)] != end_handler:
        raise ValueError("Z80 cue-terminator handler at 0x0acf changed")
    return [{
        "event": f"0x{index:02x}",
        "handler_address": f"0x{target:04x}",
        "operand_bytes": operands,
        "verified_effect": effect,
    } for index, (target, (operands, effect)) in enumerate(zip(targets, effects))]


def mame_unmapped_nop_span(address: int) -> dict[str, object] | None:
    """Describe only the contiguous MAME zero-fill opcode span before a map."""
    if sound_cpu_address_region(address) != "unmapped":
        return None
    next_boundary = min((boundary for boundary in (0xD000, 0xF000, 0x10000)
                         if boundary > address), default=0x10000)
    # MAME's Willow sound map leaves this address range unmapped; the default
    # address-map read is zero. Z80 opcode 00 is NOP. Stop at the next mapped
    # address range because its contents/state need separate analysis.
    if any(sound_cpu_address_region(pc) != "unmapped"
           for pc in (address, next_boundary - 1)):
        raise ValueError("unmapped Z80 fetch span crosses an unexpected region")
    return {
        "semantics": "MAME address-map default read, as opcode fetch",
        "start_cpu_address": f"0x{address:04x}",
        "end_cpu_address_exclusive": f"0x{next_boundary:04x}",
        "fetch_value": "0x00",
        "instruction": "Z80 NOP",
        "instruction_count_before_next_region": next_boundary - address,
        "next_region_cpu_address": f"0x{next_boundary:04x}",
        "limit": "No behavior is inferred after execution reaches the next mapped region.",
    }


def extended_control_targets(sound_rom: bytes) -> list[dict[str, object]]:
    """Record computed dispatch candidates for control bytes 0x10-0x1f."""
    event_11_prefix = bytes.fromhex(
        "8b65636563656821ca8a6a686a686a6dcc072e0c2860"
    )
    track_register_seed = bytes.fromhex(
        "dd7e13b72830dd6611dd6e1206004fcb7f28010509"
    )
    if sound_rom[0x3E0B:0x3E0B + len(event_11_prefix)] != event_11_prefix:
        raise ValueError("Z80 extended control target at 0x3e0b changed")
    if sound_rom[0x04CD:0x04CD + len(track_register_seed)] != track_register_seed:
        raise ValueError("Z80 track register seed at 0x04cd changed")
    rows = []
    for event in range(0x10, 0x20):
        table_word = 0x0952 + event * 2
        target = int.from_bytes(sound_rom[table_word:table_word + 2], "little")
        row: dict[str, object] = {
            "event": f"0x{event:02x}",
            "dispatch_word_address": f"0x{table_word:04x}",
            "candidate_target": f"0x{target:04x}",
            "validated_rst_entry_context_inference": {
                "selected_table_entry": f"0x{table_word:04x}",
                "registers_at_target": {
                    "A": f"0x{table_word & 0xff:02x}",
                    "B": f"0x{event:02x}",
                    "DE": f"0x{table_word + 2:04x}",
                    "HL": f"0x{target:04x}",
                    "IX": "unchanged track-state record pointer",
                },
                "return_address_if_target_returns": "0x094d",
                "basis": (
                    "derived from validated LD A,B at 0x0950, RST 0x30, "
                    "restart vectors 0x18/0x28/0x30, and the little-endian "
                    "word at 0x0952 + 2*event; target semantics are unresolved"
                ),
            },
            "dispatch_status": (
                "the validated RST path reads this word and transfers via JP (HL); "
                "whether it is a valid event routine is a separate question"
            ),
        }
        if target < 0x8000:
            row["address_region"] = "fixed ROM"
            row["rom_offset"] = f"0x{target:04x}"
            row["target_bytes_hex"] = sound_rom[target:target + 8].hex()
            # This is deliberately byte-pattern evidence, not a Z80
            # disassembly claim. In particular, a target in a table or music
            # data can look like executable code when decoded linearly.
            transfer_xrefs = []
            for opcode in (0xC3, 0xCD, 0xC2, 0xCA, 0xD2, 0xDA,
                           0xE2, 0xEA, 0xF2, 0xFA):
                pattern = bytes((opcode,)) + target.to_bytes(2, "little")
                cursor = 0
                while True:
                    offset = sound_rom.find(pattern, cursor, 0x8000)
                    if offset < 0:
                        break
                    transfer_xrefs.append({
                        "rom_offset": f"0x{offset:04x}",
                        "opcode": f"0x{opcode:02x}",
                    })
                    cursor = offset + 1
            row["raw_absolute_transfer_byte_patterns"] = transfer_xrefs
            row["xref_limit_note"] = (
                "Raw fixed-ROM byte scan for absolute JP/CALL opcodes only; "
                "not a control-flow proof because instruction boundaries are "
                "unknown, and does not cover the verified indirect event jump."
            )
            if event == 0x11 and target == 0x3E0B:
                # With the validated RST entry state for event 0x11, the
                # initial ADC is 0x74 + E(0x76) + carry(0) = 0xea. The LD
                # instructions through 0x3e1a preserve Z=0, so CALL Z at
                # 0x3e1b is skipped. The following INC C/JR Z path still
                # depends on C at event entry, which has not been resolved.
                row["entry_prefix_analysis"] = {
                    "validated_prefix_bytes": event_11_prefix.hex(),
                    "initial_adc": {
                        "a_before": "0x74",
                        "e_before": "0x76",
                        "carry_in": 0,
                        "a_after": "0xea",
                        "zero_flag": False,
                    },
                    "call_z_0x2e07": "not taken; intervening LD instructions preserve Z=0",
                    "next_branch": "INC C at 0x3e1e; JR Z at 0x3e1f reaches 0x3e81 when C wraps to zero",
                    "scheduler_C_seed": {
                        "validated_bytes_address": "0x04cd",
                        "validated_bytes_hex": track_register_seed.hex(),
                        "effect": "scheduler loads track-state +0x13 into C when that byte is nonzero; otherwise it follows the zero-mode path",
                        "limit": "This is only the scheduler seed. Intervening event/timing helpers can alter C, and the value at event 0x11 entry has not been proven.",
                    },
                    "unresolved_input": "C at event entry; remaining target control flow and sound-register effects",
                }
        elif target < 0xC000:
            row["address_region"] = "banked ROM window; bank at runtime not proven for this dispatch"
        elif sound_cpu_address_region(target) == "sound RAM":
            row["address_region"] = "sound RAM"
        elif sound_cpu_address_region(target) == "sound I/O register":
            row["address_region"] = "sound I/O register"
        else:
            row["address_region"] = "unmapped sound CPU address"
            row["mame_unmapped_fetch_preview"] = mame_unmapped_nop_span(target)
        rows.append(row)
    return rows


def decode_track_prefix(sound_rom: bytes, start: int, limit: int = 512) -> dict[str, object]:
    """Decode events and follow the source's per-track repeat/jump control."""
    handlers = control_event_handlers(sound_rom)
    pc = start
    events: list[dict[str, object]] = []
    repeat_counter = 0
    for _ in range(limit):
        if pc >= len(sound_rom):
            return {"events": events, "stop": "end of sound ROM", "next_rom_offset": None}
        event_at = pc
        value = sound_rom[pc]
        pc += 1
        if value < 0x10:
            handler = handlers[value]
            operand_count = handler["operand_bytes"]
            if value == 0x0F:
                events.append({
                    "rom_offset": f"0x{event_at:04x}",
                    "kind": "cue_terminator",
                    "event": "0x0f",
                    "handler_address": handler["handler_address"],
                })
                return {
                    "events": events,
                    "stop": "cue terminator and driver cleanup",
                    "next_rom_offset": f"0x{pc:04x}",
                }
            if value == 0x02:
                # Handler 0x0998 either loads/decrements track state +0x0d,
                # then reads a little-endian destination, or takes its
                # state==1 branch and reads the destination immediately.
                # When the decrement reaches zero it skips that destination.
                control: dict[str, object] = {
                    "rom_offset": f"0x{event_at:04x}",
                    "kind": "repeat_control",
                    "event": "0x02",
                    "handler_address": handler["handler_address"],
                    "repeat_counter_before": repeat_counter,
                }
                if repeat_counter == 1:
                    repeat_counter = 0
                    control["branch"] = "state_one_jump_without_count_operand"
                else:
                    if pc >= len(sound_rom):
                        return {"events": events, "stop": "truncated repeat operand",
                                "next_rom_offset": None}
                    count_operand = sound_rom[pc]
                    pc += 1
                    control["count_operand"] = f"0x{count_operand:02x}"
                    if count_operand == 0:
                        control["branch"] = "zero_operand_jump"
                    elif repeat_counter == 0:
                        repeat_counter = count_operand
                        control["branch"] = "initialize_repeat_and_jump"
                    else:
                        repeat_counter -= 1
                        if repeat_counter == 0:
                            if pc + 2 > len(sound_rom):
                                return {"events": events,
                                        "stop": "truncated repeat destination",
                                        "next_rom_offset": None}
                            skipped = sound_rom[pc:pc + 2]
                            pc += 2
                            control["branch"] = "repeat_complete_skip_destination"
                            control["skipped_destination_hex"] = skipped.hex()
                            control["repeat_counter_after"] = repeat_counter
                            events.append(control)
                            continue
                        control["branch"] = "decrement_repeat_and_jump"
                if pc + 2 > len(sound_rom):
                    return {"events": events,
                            "stop": "truncated repeat destination",
                            "next_rom_offset": None}
                target = int.from_bytes(sound_rom[pc:pc + 2], "little")
                pc += 2
                target_offset = sound_cpu_rom_offset(target)
                control.update({
                    "target_cpu_address": f"0x{target:04x}",
                    "target_region": sound_cpu_address_region(target),
                    "repeat_counter_after": repeat_counter,
                })
                if target_offset is None or target_offset >= len(sound_rom):
                    events.append(control)
                    return {
                        "events": events,
                        "stop": "repeat destination outside mapped ROM",
                        "stop_event": "0x02",
                        "stop_rom_offset": f"0x{event_at:04x}",
                        "next_rom_offset": f"0x{event_at:04x}",
                    }
                control["target_rom_offset"] = f"0x{target_offset:04x}"
                events.append(control)
                pc = target_offset
                continue
            if operand_count == "state-dependent":
                return {
                    "events": events,
                    "stop": "state-dependent control event",
                    "stop_event": f"0x{value:02x}",
                    "stop_rom_offset": f"0x{event_at:04x}",
                    "next_rom_offset": f"0x{event_at:04x}",
                }
            operands = sound_rom[pc:pc + int(operand_count)]
            if len(operands) != int(operand_count):
                return {"events": events, "stop": "truncated control operands", "next_rom_offset": None}
            pc += int(operand_count)
            events.append({
                "rom_offset": f"0x{event_at:04x}",
                "kind": "control",
                "event": f"0x{value:02x}",
                "handler_address": handler["handler_address"],
                "operands_hex": operands.hex(),
            })
        elif value < 0x20:
            table_word = 0x0952 + value * 2
            target = int.from_bytes(sound_rom[table_word:table_word + 2], "little")
            target_region = sound_cpu_address_region(target)
            target_bytes = (sound_rom[target:target + 8].hex()
                            if target_region == "fixed" else None)
            return {
                "events": events,
                "stop": "control event target unresolved",
                "stop_event": f"0x{value:02x}",
                "stop_rom_offset": f"0x{event_at:04x}",
                "dispatch_word_address": f"0x{table_word:04x}",
                "candidate_target": f"0x{target:04x}",
                "candidate_target_region": target_region,
                "dispatch_word_overlaps_verified_table": value >= 0x10,
                "mame_unmapped_fetch_preview": (
                    mame_unmapped_nop_span(target)
                    if target_region == "unmapped" else None
                ),
                "validated_rst_entry_context_inference": ({
                    "A": f"0x{table_word & 0xff:02x}",
                    "B": f"0x{value:02x}",
                    "DE": f"0x{table_word + 2:04x}",
                    "HL": f"0x{target:04x}",
                    "IX": "unchanged track-state record pointer",
                    "return_address_if_target_returns": "0x094d",
                    "target_semantics": "unresolved",
                } if value >= 0x10 else None),
                "candidate_target_bytes_hex": target_bytes,
                "next_rom_offset": f"0x{event_at:04x}",
            }
        elif value < 0x40:
            if value == 0x20:
                if pc >= len(sound_rom):
                    return {"events": events, "stop": "truncated timing operand", "next_rom_offset": None}
                operand = sound_rom[pc]
                pc += 1
                events.append({
                    "rom_offset": f"0x{event_at:04x}",
                    "kind": "timing",
                    "event": "0x20",
                    "operand": f"0x{operand:02x}",
                    "countdown_ticks": (operand & 0x0F) + 1,
                })
            else:
                events.append({
                    "rom_offset": f"0x{event_at:04x}",
                    "kind": "timing",
                    "event": f"0x{value:02x}",
                    "delay_low_nibble": value & 0x0F,
                })
        else:
            duration_shift = ((value >> 5) & 0x07) - 2
            events.append({
                "rom_offset": f"0x{event_at:04x}",
                "kind": "note",
                "event": f"0x{value:02x}",
                "note_field": value & 0x1F,
                "note_update": "new pitch after source transpose" if value & 0x1F else "retain prior pitch",
                "duration_shift_count": duration_shift,
            })
    return {"events": events, "stop": "event limit", "next_rom_offset": f"0x{pc:04x}"}


def extended_event_11_note_entry(sound_rom: bytes,
                                track_prefix: dict[str, object]) -> dict[str, object] | None:
    """Trace event 0x11 when its verified 0x47/0x28 stream suffix sets C=1."""
    if (track_prefix.get("stop_event") != "0x11"
            or track_prefix.get("stop_rom_offset") != "0x0040"):
        return None
    events = track_prefix.get("events")
    if not isinstance(events, list):
        raise ValueError("extended event 0x11 track prefix is malformed")
    preceding = [event for event in events if isinstance(event, dict)
                 and event.get("rom_offset") in ("0x003e", "0x003f")]
    if (len(preceding) != 2
            or preceding[0].get("rom_offset") != "0x003e"
            or preceding[0].get("kind") != "note"
            or preceding[0].get("event") != "0x47"
            or preceding[1].get("rom_offset") != "0x003f"
            or preceding[1].get("kind") != "timing"
            or preceding[1].get("event") != "0x28"):
        raise ValueError("extended event 0x11 pre-event stream changed")

    checks = {
        0x0581: bytes.fromhex("cd260be6f0fe20da4a09fe40da2b09c5cda408c1"),
        0x05B9: bytes.fromhex("cd09063a1ad0b7c0cd230621360b"),
        0x0623: bytes.fromhex(
            "21360bdd7e11e70fe63f3219d0fd4e09fd7e0acdae06fd46080e00571e07"
            "cd7d06fd4e0cfd7e0dcdae06fd460b0e08571e05cd7d06fd4e0ffd7e10"
            "cdae06fd460e0e10571e04cd7d06fd4e12fd7e13"
        ),
        0x06A4: bytes.fromhex("3a13d0c66081cd1701c9"),
        0x0117: bytes.fromhex("0101f0f50acb7f20fbf10d020cf57a02f1c9"),
        0x092B: bytes.fromhex("cd3109c38105fe202802180c78e60f3cdd770addcb08cec9"),
        0x3E21: bytes.fromhex(
            "8085888f85888e85888f85680c16b1b303011e3e0df7"
        ),
        0x3F35: bytes.fromhex("c3a4"),
        0xCA09: bytes.fromhex("00000000"),
        0x0038: bytes.fromhex(
            "f30101f00acb472811cde500cd970bcd0c040101f00acb4f"
            "2809cdfd00cdf503cd3201fbed4d"
        ),
        0x00B97: bytes.fromhex("3e013204f0"),
        0x0040C: bytes.fromhex("3e003204f0"),
        0x00153: bytes.fromhex("3e003204f0"),
        0x00162: bytes.fromhex("3e013204f0"),
    }
    for address, expected in checks.items():
        if sound_rom[address:address + len(expected)] != expected:
            raise ValueError(
                f"Z80 extended event 0x11 trace at {address:#06x} changed"
            )
    event_11_bank_target = sound_cpu_rom_offset(0xA4C3, bank=1)
    if event_11_bank_target is None:
        raise ValueError("extended event 0x11 target no longer maps through bank 1")
    banked_target_bytes = sound_rom[event_11_bank_target:event_11_bank_target + 8]
    if banked_target_bytes != bytes.fromhex("ffffffffffffffff"):
        raise ValueError("extended event 0x11 target bank-1 bytes changed")
    bank0_target_offset = sound_cpu_rom_offset(0xA4C3, bank=0)
    if bank0_target_offset is None:
        raise ValueError("extended event 0x11 target no longer maps through bank 0")
    bank0_target_bytes = sound_rom[bank0_target_offset:bank0_target_offset + 8]
    if bank0_target_bytes != bytes.fromhex("ffffffffffffffff"):
        raise ValueError("extended event 0x11 target bank-0 bytes changed")

    return {
        "equivalent_dispatch_context": (
            "event 0x11 selects the same fixed dispatch word and restart-vector "
            "register seed for every cue; with the same preceding 0x47 note "
            "and 0x28 timing event, C also matches the traced path"
        ),
        "validated_preceding_stream": [
            {"rom_offset": "0x003e", "event": "0x47", "kind": "note"},
            {"rom_offset": "0x003f", "event": "0x28", "kind": "timing"},
        ],
        "source_note_path": (
            "0x0581 note dispatch -> 0x05c1 YM operator update -> 0x0623; "
            "the final operator write passes through 0x06a4 and 0x0117"
        ),
        "ym_write_helper_c_after_return": "0x01",
        "timing_event_c_effect": "0x28 handler stores the low-nibble delay and preserves C",
        "bank_selection_at_track_update": (
            "the sound tick enters 0x0b97, selects bank 1 at 0xf004, and processes "
            "the track pointers before 0x040c later restores bank 0"
        ),
        "event_0x11_entry_c": "0x01",
        "inc_c_result": "0x02",
        "jr_z_0x3e1f": "not taken; execution continues at 0x3e21",
        "conditions": [
            "the preceding note reaches the YM update path",
            "sound-driver suppression flag at 0xd01a is clear",
        ],
        "validated_code_bytes": {
            f"0x{address:04x}": value.hex()
            for address, value in checks.items()
        },
        "fallthrough_to_indirect_jump": {
            "validated_bytes_address": "0x3e21",
            "validated_bytes_hex": checks[0x3E21].hex(),
            "state_at_rst_0x30": {
                "A": "0xff",
                "B": "0x3e",
                "C": "0x1d",
                "HL": "0x3e37",
                "carry": 0,
            },
            "banked_read": {
                "cpu_address": "0x8a09",
            "bank_selected_by_driver": 1,
                "rom_offset": "0xca09",
                "bytes_hex": checks[0xCA09].hex(),
            },
            "rst_vector_calculation": (
                "RST 0x30 uses RST 0x28 and RST 0x18 to address the fixed-ROM "
                "word at 0x3f35, then JP (HL)"
            ),
            "indirect_target_word_address": "0x3f35",
            "indirect_target_word_bytes": "c3a4",
            "indirect_target_cpu_address": "0xa4c3",
            "target_bank_selected_by_driver": 1,
            "target_rom_offset": f"0x{event_11_bank_target:04x}",
            "target_bank_bytes_hex": banked_target_bytes.hex(),
            "rst_0x38_vector": {
                "validated_bytes_address": "0x0038",
                "validated_bytes_hex": checks[0x0038].hex(),
                "target_opcode": "0xff (RST 0x38)",
                "reti_return_address": "0xa4c4",
                "timer_a_bank_writes": {
                    "select_bank_1_at_0x0b97": checks[0x00B97].hex(),
                    "restore_bank_0_at_0x040c": checks[0x0040C].hex(),
                },
                "timer_b_command_bank_writes": {
                    "select_bank_0": checks[0x00153].hex(),
                    "select_bank_1": checks[0x00162].hex(),
                },
                "return_filler_by_bank": {
                    "bank_0_rom_offset": f"0x{bank0_target_offset + 1:04x}",
                    "bank_0_byte": bank0_target_bytes[1],
                    "bank_1_rom_offset": f"0x{event_11_bank_target + 1:04x}",
                    "bank_1_byte": banked_target_bytes[1],
                },
                "control_flow": "the vector disables interrupts, conditionally services YM timer A/B, and executes EI/RETI; timer A and command processing can switch the bank, but both configured ROM banks contain 0xff at the RETI return address, so it re-enters RST 0x38 instead of the normal event dispatcher",
            },
            "limit": "The nested timer service and repeated sound-tick effects while trapped in bank-1 filler are not resolved.",
        },
    }


def track2_event_11_entry_trace(sound_rom: bytes,
                                track_prefix: dict[str, object]) -> dict[str, object] | None:
    """Trace the verified fixed track-2 suffix before event 0x11."""
    if (track_prefix.get("stop_event") != "0x11"
            or track_prefix.get("stop_rom_offset") != "0x0061"):
        return None

    stream = bytes.fromhex("0acb4f2809cdfd00cdf503cd3201fbed4d2100d01101")
    if sound_rom[0x004D:0x004D + len(stream)] != stream:
        raise ValueError("fixed track-2 pre-event stream changed")
    note_write = bytes.fromhex("0101f0f50acb7f20fbf10d020cf57a02f1c9")
    if sound_rom[0x0117:0x0117 + len(note_write)] != note_write:
        raise ValueError("YM register writer at 0x0117 changed")
    timing_handler = bytes.fromhex(
        "cd3109c38105fe202802180c78e60f3cdd770addcb08cec9"
        "78e60fdd7709c9"
    )
    if sound_rom[0x092B:0x092B + len(timing_handler)] != timing_handler:
        raise ValueError("track timing handler at 0x092b changed")
    event_zero = bytes.fromhex(
        "cd260b3e8080672e00cb3ccb3ccb3ccb1d3a1ad0b720042215d0c9"
    )
    if sound_rom[0x0972:0x0972 + len(event_zero)] != event_zero:
        raise ValueError("control event 0 handler at 0x0972 changed")
    event_11 = bytes.fromhex("8b65636563656821ca8a6a686a686a6dcc072e0c2860")
    if sound_rom[0x3E0B:0x3E0B + len(event_11)] != event_11:
        raise ValueError("extended control candidate at 0x3e0b changed")

    return {
        "validated_track_stream_hex": stream.hex(),
        "entry_condition": (
            "The result below applies when the 0x4d note reaches the YM write "
            "path; that path is skipped if the source track-suppression flag "
            "at 0xd01a is nonzero."
        ),
        "pre_event_c_register_trace": [
            {
                "event": "0x4d",
                "effect": (
                    "The validated YM register-write helper loads BC=0xf001, "
                    "uses C for the 0xf000/0xf001 address pair, and returns "
                    "with C=0x01."
                ),
                "basis": "validated helper bytes at 0x0117",
            },
            {
                "event": "0x21",
                "effect": "Stores the low-nibble delay and preserves C.",
                "handler": "0x0943",
            },
            {
                "event": "0x00",
                "effect": "Sets the note-table pointer and preserves C.",
                "handler": "0x0972",
            },
        ],
        "event_0x11_dispatch": {
            "dispatch_word_address": "0x0974",
            "target": "0x3e0b",
            "target_bytes_hex": event_11.hex(),
            "inc_c_result": "0x02",
            "jr_z_at_0x3e1f": "not taken",
            "limit": (
                "The overlapping dispatch entry is followed only through its "
                "validated initial branch; later control flow and sound-chip "
                "effects are not inferred here."
            ),
        },
    }


def stage0_startup_cue(main_rom: bytes, sound_rom: bytes,
                       sequenced_by_command: dict[str, dict[str, object]]) -> dict[str, object]:
    """Validate the normal Stage 0 sound-queue write for command 0x13."""
    contexts = {
        "normal_stage0_setup": (0xA114, bytes.fromhex(
            "4eb82dfe546d00083b7c00c08fb03b7c02808fb2"
            "3b7c00c004083b7c0200040c3b7c00c004883b7c"
            "0200048c3b7c0007000a4ef878b8")),
        "queue_writer": (0x7818, bytes.fromhex(
            "4a2d029067064a2d0293661649ed0172322d001e"
            "3980100054410241007e3b41001e4e75")),
        "startup_sound_helper": (0x78B8, bytes.fromhex(
            "303c00136100ff7e303c02fa6000ff76")),
    }
    for name, (address, expected) in contexts.items():
        if main_rom[address:address + len(expected)] != expected:
            raise ValueError(f"Stage 0 startup sound source context changed: {name}")
    cue = sequenced_by_command.get("0x13")
    if cue is None or cue.get("sequence_address") != "0x8f90":
        raise ValueError("Stage 0 startup command 0x13 no longer maps to cue 0x8f90")
    track_pointers = cue["track_pointers"]
    nonzero_tracks = [track for track in track_pointers
                      if track.get("sequence_address") != "0x0000"]
    if int.from_bytes(sound_rom[0x0952:0x0954], "little") != 0x0972:
        raise ValueError("Stage 0 unmapped fetch no longer dispatches event 0")
    unmapped_initial_fetches = []
    fixed_track_event_traces = []
    for track in nonzero_tracks:
        track_address = int(str(track["sequence_address"]), 16)
        if track.get("region") == "fixed":
            trace = {
                "track": track["track"],
                "start_rom_offset": track["rom_offset"],
                **decode_track_prefix(sound_rom, track_address, limit=2048),
            }
            if track["track"] == 2:
                event_trace = track2_event_11_entry_trace(sound_rom, trace)
                if event_trace is not None:
                    trace["extended_event_entry_trace"] = event_trace
            fixed_track_event_traces.append(trace)
        if track.get("region") != "unmapped":
            continue
        operand_address = track_address + 1
        if (sound_cpu_address_region(track_address) != "unmapped"
                or sound_cpu_address_region(operand_address) != "unmapped"):
            raise ValueError(
                f"Stage 0 cue track {track['track']} no longer starts in "
                "unmapped sound CPU space"
            )
        event_value = 0
        operand = 0
        # Event 0 loads H=(0x80+operand), L=0, then shifts the
        # 16-bit value right three times before storing the table pointer.
        note_table_pointer = (((0x80 + operand) << 8) >> 3) & 0xFFFF
        if note_table_pointer != 0x1000:
            raise ValueError("Unexpected MAME unmapped-read event-0 trace")
        if track_address & 1:
            raise ValueError(
                f"Stage 0 unmapped track {track['track']} is not word-aligned"
            )
        updates_until_zero = (0x10000 - track_address) // 2
        last_event_address = (track_address + 2 * (updates_until_zero - 1)) & 0xFFFF
        last_operand_address = (last_event_address + 1) & 0xFFFF
        if (updates_until_zero <= 0
                or last_event_address != 0xFFFE
                or last_operand_address != 0xFFFF
                or sound_cpu_address_region(last_event_address) != "unmapped"
                or sound_cpu_address_region(last_operand_address) != "unmapped"):
            raise ValueError(
                f"Stage 0 unmapped track {track['track']} pointer wrap changed"
            )
        unmapped_initial_fetches.append({
            "track": track["track"],
            "track_pointer": f"0x{track_address:04x}",
            "first_fetch_address": f"0x{track_address:04x}",
            "first_fetch_value": "0x00",
            "event_handler": "0x0972",
            "operand_address": f"0x{operand_address:04x}",
            "operand_value": "0x00",
            "note_table_pointer_value": f"0x{note_table_pointer:04x}",
            "destination_by_sound_ram_selector": {
                "selector_0": "0xd015",
                "nonzero_selector": "0xd017",
                "selector_value": "0xd01a",
            },
            "mame_default_zero_playback": {
                "event_and_operand_bytes_per_update": 2,
                "updates_until_track_pointer_zero": updates_until_zero,
                "last_event_address": f"0x{last_event_address:04x}",
                "last_event_value": "0x00",
                "last_operand_address": f"0x{last_operand_address:04x}",
                "last_operand_value": "0x00",
                "pointer_after_event_handler": "0x0000",
                "next_update": "scheduler skips the track because its pointer is zero",
                "audio_effect": "repeats control event 0 without a note or timing event, then becomes inactive",
            },
            "read_semantics": "MAME address-map unmapped-read default; physical open-bus behavior is unverified",
        })
    track7_trace = next((trace for trace in fixed_track_event_traces
                         if trace["track"] == 7), None)
    if (track7_trace is None
            or track7_trace.get("stop") != "cue terminator and driver cleanup"
            or len(track7_trace.get("events", [])) != 308
            or track7_trace["events"][-1].get("rom_offset") != "0x4f74"
            or track7_trace["events"][-1].get("event") != "0x0f"):
        raise ValueError(
            "Stage 0 startup cue track 7 no longer reaches its verified terminator"
        )
    track5_trace = next((trace for trace in fixed_track_event_traces
                         if trace["track"] == 5), None)
    track5_event18_prefix = None
    if (track5_trace is not None
            and track5_trace.get("stop_event") == "0x18"
            and track5_trace.get("candidate_target") == "0x3a1d"):
        target = 0x3A1D
        straight_line = bytes.fromhex("0c7f0c11") * 18 + bytes.fromhex("0c7f")
        if (sound_cpu_address_region(target) != "fixed"
                or sound_rom[0x0982:0x0984] != bytes.fromhex("1d3a")
                or sound_rom[target:target + len(straight_line)] != straight_line
                or sound_rom[target + len(straight_line):target + len(straight_line) + 8]
                    != bytes.fromhex("0d00071605001a39")):
            raise ValueError(
                "Stage 0 startup track-5 event-0x18 target prefix changed"
            )
        after_event18 = decode_track_prefix(sound_rom, 0x000B, limit=128)
        if (after_event18.get("stop_event") != "0x11"
                or after_event18.get("stop_rom_offset") != "0x0040"
                or not isinstance(after_event18.get("events"), list)
                or len(after_event18["events"]) != 46
                or after_event18["events"][-2].get("event") != "0x47"
                or after_event18["events"][-1].get("event") != "0x28"):
            raise ValueError(
                "Stage 0 startup track-5 stream after event 0x18 changed"
            )
        event11_entry = extended_event_11_note_entry(sound_rom, after_event18)
        if event11_entry is None:
            raise ValueError(
                "Stage 0 startup track-5 event 0x11 context is not resolved"
            )
        track5_event18_prefix = {
            "event_rom_offset": "0x000a",
            "event": "0x18",
            "computed_fixed_rom_target": "0x3a1d",
            "validated_straight_line_prefix": {
                "start": "0x3a1d",
                "end_inclusive": "0x3a66",
                "bytes_hex": straight_line.hex(),
                "decoded_pattern": "18 repetitions of INC C; LD A,A; INC C; LD DE,0x7f0c, followed by INC C; LD A,A",
                "next_bytes_at_0x3a67": "0d00071605001a39",
                "first_instruction_after_prefix": "DEC C at 0x3a67",
                "known_side_effects": "register-only through 0x3a66; no branch or sound-chip write occurs in this prefix",
            },
            "entry_and_return_condition": {
                "preceding_track_event": "0xfb note at 0x0009",
                "condition": "the note reaches the YM operator-update path and the suppression flag at 0xd01a is clear",
                "C_before_target": "0x01 from the final YM write helper",
                "C_after_37_increments": "0x26",
                "C_after_DEC_C": "0x25",
                "RET_NZ_at_0x3a71": "taken; returns to the track dispatcher without a sound-chip write from this target",
            },
            "resumed_track_stream": {
                "next_rom_offset": "0x000b",
                "event_count_before_next_extended_control": len(after_event18["events"]),
                "stop_event": "0x11",
                "stop_rom_offset": "0x0040",
                "event_0x11_entry_trace": event11_entry,
            },
            "limit": "The track reaches the unresolved event-0x11 trap after resuming; timer-service and repeated sound-tick effects remain unresolved.",
        }
    return {
        "source_context_validated": True,
        "stage_controller_entry": "0x00a114",
        "queue_writer": "0x007818",
        "queue_helper": "0x0078b8",
        "queued_words": ["0x0013", "0x02fa"],
        "sound_command": "0x13",
        "sound_command_resolution": "sequenced cue",
        "cue_sequence_address": cue["sequence_address"],
        "cue_banked_rom_offset": cue["banked_rom_offset"],
        "track_pointers": track_pointers,
        "scheduler_active_nonzero_track_count": len(nonzero_tracks),
        "fixed_track_event_traces": fixed_track_event_traces,
        "track5_event18_fixed_target_prefix": track5_event18_prefix,
        "fully_decoded_track": {
            "track": 7,
            "event_count_including_terminator": 308,
            "terminator_rom_offset": "0x4f74",
            "terminator_event": "0x0f",
        },
        "mame_unmapped_track_initial_fetches": unmapped_initial_fetches,
        "scheduler_note": "The Z80 update loop enters the track handler for every nonzero pointer; unmapped targets are not proven disabled.",
        "following_fade_command": {
            "command": "0xfa",
            "interval": 2,
            "source_helper": "0x0003af",
        },
        "playback_status": "track 7 reaches a verified 0x0f terminator after 308 decoded events; tracks 1 and 3 repeatedly read MAME's unmapped-zero default until their pointers wrap to zero and the scheduler skips them; tracks 2, 5, and 6 stop at unresolved extended controls, so the complete mix is not yet resolved into native music playback",
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", nargs="?", type=Path, default=Path("willow.zip"))
    parser.add_argument("--output", type=Path, default=Path("build/willow_audio_commands.json"))
    parser.add_argument("--header", type=Path, default=Path("build/willow_audio.h"))
    args = parser.parse_args()
    try:
        with zipfile.ZipFile(args.archive) as archive:
            sound_rom = archive.read("wl_09.12b")
    except (OSError, zipfile.BadZipFile, KeyError) as exc:
        parser.error(str(exc))
    if len(sound_rom) < 0x10000:
        parser.error(f"expected at least 0x10000 sound-ROM bytes; found {len(sound_rom):#x}")
    for address, expected, label in (
        (SYSTEM_SEQUENCER_RESET_HANDLER,
         SYSTEM_SEQUENCER_RESET_HANDLER_BYTES,
         "system command 0xf4 sequencer reset at 0x035f"),
        (SYSTEM_YM_WRITE_HELPER, SYSTEM_YM_WRITE_HELPER_BYTES,
         "YM register writer at 0x0117"),
    ):
        if sound_rom[address:address + len(expected)] != expected:
            parser.error(f"{label} changed")
    if sound_rom[SYSTEM_FADE_UP_HANDLER:
                 SYSTEM_FADE_UP_HANDLER + len(SYSTEM_FADE_UP_HANDLER_BYTES)] \
            != SYSTEM_FADE_UP_HANDLER_BYTES:
        parser.error("system command 0xfb handler at 0x03c1 changed")

    # Z80 code at 0x3d2 strips bit 7, reduces the command modulo
    # (ROM[0xe42] + 1), and indexes the one-byte OKI voice/volume table at 0xe43.
    entry_count = sound_rom[0xE42] + 1
    if 0xE43 + entry_count > len(sound_rom):
        parser.error("direct-OKI command table extends past the sound ROM")
    lookup = sound_rom[0xE43:0xE43 + entry_count]
    commands = []
    # Handler 0x0132 checks the F0 control class before checking bit 7, so
    # only 0x80..0xef reaches the direct-OKI routine at 0x03d2.
    for command in range(0x80, 0xF0):
        sample_id = (command & 0x7F) % entry_count
        voice_volume = lookup[sample_id]
        commands.append({
            "command": f"0x{command:02x}",
            "sample_id": f"0x{sample_id:02x}",
            "voice_mask": f"0x{(voice_volume >> 4):x}",
            "volume_nibble": f"0x{(voice_volume & 0x0f):x}",
        })

    # The 0xf0..0xff group dispatches through a 16-word inline table after
    # masking to its low nibble in the Z80 handler at 0x02ea.
    system_commands = []
    system_table = 0x2EE
    for command in range(0xF0, 0x100):
        index = command & 0x0F
        table_offset = system_table + index * 2
        target = int.from_bytes(sound_rom[table_offset:table_offset + 2], "little")
        system_commands.append({
            "command": f"0x{command:02x}",
            "dispatch_index": index,
            "dispatch_word_address": f"0x{table_offset:04x}",
            "handler_address": f"0x{target:04x}",
        })
    system_commands[0xFB - 0xF0]["verified_effect"] = (
        "handler 0x03c1 clears fade counter d004, loads duration from latch "
        "0xf00a into d003, and sets direction d005 to +1; the periodic sound "
        "update applies that ramp to global mix value d010")
    system_commands[0xF4 - 0xF0]["verified_effect"] = (
        "handler 0x035f resets sequencer/YM state through the YM register "
        "writer at 0x0117 and clears sound-RAM sequence buffers; this path "
        "does not issue the OKI stop command at 0xf004, so active OKI voices "
        "continue")

    try:
        sequence_tables, sequenced = sequence_commands(sound_rom)
        note_keycodes = note_keycode_table(sound_rom)
        note_level_tables = ym2151_note_level_tables(sound_rom)
        arcade_main_rom = assemble_maincpu(args.archive)
    except ValueError as exc:
        parser.error(str(exc))
    except (OSError, zipfile.BadZipFile, KeyError) as exc:
        parser.error(str(exc))

    direct_by_command = {row["command"]: row for row in commands}
    system_by_command = {row["command"]: row for row in system_commands}
    sequenced_by_command = {row["command"]: row for row in sequenced}
    main_calls = maincpu_sound_call_sites(arcade_main_rom)
    for row in main_calls:
        candidate_commands = row.get("command_candidates")
        if candidate_commands:
            row["resolution"] = "conditional direct OKI"
            row["audio_mappings"] = [
                direct_by_command[str(command)]
                for command in candidate_commands
                if str(command) in direct_by_command
            ]
            continue
        command = str(row["command"])
        if command in direct_by_command:
            row["resolution"] = "direct OKI"
            row["audio_mapping"] = direct_by_command[command]
        elif command in system_by_command:
            row["resolution"] = "system command"
            row["audio_mapping"] = system_by_command[command]
        elif command in sequenced_by_command:
            cue = sequenced_by_command[command]
            row["resolution"] = "sequenced cue"
            row["audio_mapping"] = {
                "sequence_address": cue["sequence_address"],
                "banked_rom_offset": cue["banked_rom_offset"],
            }
        else:
            row["resolution"] = "unmapped"
    startup_cue = stage0_startup_cue(arcade_main_rom, sound_rom,
                                     sequenced_by_command)

    decoded_event_prefixes = []
    for row in main_calls:
        if not row.get("control_flow_validated") or row.get("resolution") != "sequenced cue":
            continue
        cue = sequenced_by_command[str(row["command"])]
        track_prefixes = []
        for track in cue["track_pointers"]:
            rom_offset = track.get("rom_offset")
            if rom_offset is None:
                continue
            track_prefix = {
                "track": track["track"],
                "start_rom_offset": rom_offset,
                # Cue 0x33's active patch track runs for 308 decoded events
                # before its 0x0f terminator. A 128-event cap stopped in the
                # middle of the instrument stream and omitted later verified
                # patch references from the native conversion catalog.
                **decode_track_prefix(
                    sound_rom, int(str(rom_offset), 16),
                    limit=(512 if row["command"] == "0x33"
                           and track["track"] == 7 else 128),
                ),
            }
            if row["command"] == "0x31" and track["track"] == 6:
                event_entry = extended_event_11_note_entry(sound_rom,
                                                           track_prefix)
                if event_entry is not None:
                    track_prefix["extended_event_entry_trace"] = event_entry
            if row["command"] == "0x34" and track["track"] == 2:
                event_entry = extended_event_11_note_entry(sound_rom,
                                                           track_prefix)
                if event_entry is not None:
                    track_prefix["extended_event_entry_trace"] = event_entry
            if row["command"] == "0x33" and track["track"] == 7:
                events = track_prefix.get("events")
                if (track_prefix.get("stop") !=
                        "cue terminator and driver cleanup"
                        or track_prefix.get("next_rom_offset") != "0x4f75"
                        or not isinstance(events, list)
                        or not events
                        or events[-1].get("event") != "0x0f"):
                    raise ValueError(
                        "Verified command 0x33 track 7 no longer reaches its "
                        "cue terminator at 0x4f74"
                    )
                track_prefix["trace_validation"] = {
                    "validated_event_count": len(events),
                    "terminator_rom_offset": "0x4f74",
                    "terminator_event": "0x0f",
                    "terminator_bytes_checked_by_decoder": True,
                }
            if row["command"] == "0x33" and track["track"] == 2:
                event_trace = track2_event_11_entry_trace(
                    sound_rom, track_prefix
                )
                if event_trace is not None:
                    track_prefix["extended_event_entry_trace"] = event_trace
            track_prefixes.append(track_prefix)
        decoded_event_prefixes.append({
            "call_site": row["call_site"],
            "command": row["command"],
            "cue_sequence_address": cue["sequence_address"],
            "tracks": track_prefixes,
        })

    try:
        ym2151_patches = ym2151_patch_catalog(sound_rom,
                                              decoded_event_prefixes,
                                              startup_cue)
    except ValueError as exc:
        parser.error(str(exc))

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps({
        "source_rom": "wl_09.12b",
        "handler_address": "0x03d2",
        "table_address": "0x0e43",
        "entry_count": entry_count,
        "direct_oki_range": "0x80-0xef",
        "mapping_note": "For commands 0x80-0xef, OKI sample_id is the low seven bits modulo entry_count; 0xf0-0xff are dispatched as system commands first.",
        "system_command_table_address": f"0x{system_table:04x}",
        "system_commands": system_commands,
        "sequenced_command_tables": sequence_tables,
        "sequenced_commands": sequenced,
        "stage0_startup_audio_cue": startup_cue,
        "verified_gameplay_cue_event_prefixes": {
            "source_note": "Bounded offline decoding of verified main-CPU sequenced-cue call sites. Note events retain their raw note field and duration bits because track transposition, instrument state, loops, and YM-to-Genesis frequency conversion are not yet fully reconstructed.",
            "cues": decoded_event_prefixes,
        },
        "ym2151_note_keycodes": {
            "rom_address": "0x0b36",
            "entry_count": len(note_keycodes),
            "indexing": "Zero-based note index used by Z80 RST 20h table lookup after note transposition and octave folding.",
            "values": [f"0x{value:02x}" for value in note_keycodes],
            "native_conversion_note": "These are source YM2151 keycode bytes; a Genesis backend must map pitches into YM2612 block/F-number pairs. This table alone does not reproduce instrument envelopes or track timing.",
        },
        "ym2151_note_level_tables": note_level_tables,
        "ym2151_verified_cue_patches": ym2151_patches,
        "maincpu_static_sound_call_note": "Raw-byte candidates for immediate MOVE.W #value,D0 followed by JSR/JMP 0x786a or 0x786c. Verified instruction/state contexts are recorded for calls at 0x00c672 (charge cue 0x31), 0x00c6ba (charge cue 0x44), 0x00c86c (jump cue 0x2b), 0x00cdf0 (burst cue 0x2a), 0x00cef8 (direct command 0x83 with runtime entry unresolved), and 0x0119c6 (profile-1 voice command 0x8a). The conditional direct-OKI pair at 0x00c1b6 is recorded separately with both command values. Dynamic sound commands and queue writes such as 0x783c are not included.",
        "maincpu_static_sound_call_sites": main_calls,
        "commands": commands,
    }, indent=2) + "\n")
    args.header.parent.mkdir(parents=True, exist_ok=True)
    header_lines = [
        "/* Generated from the offline OKI command table in wl_09.12b. */",
        "static const unsigned char willow_oki_command_sample[112] = {",
    ]
    for start in range(0, len(commands), 16):
        values = ", ".join(str(int(row["sample_id"], 16))
                            for row in commands[start:start + 16])
        header_lines.append("    " + values + ",")
    header_lines.extend(["};", "static const unsigned char willow_oki_command_voice_mask[112] = {"])
    for start in range(0, len(commands), 16):
        values = ", ".join(f"0x{int(row['voice_mask'], 16):x}"
                            for row in commands[start:start + 16])
        header_lines.append("    " + values + ",")
    header_lines.extend(["};", "static const unsigned char willow_oki_command_volume[112] = {"])
    for start in range(0, len(commands), 16):
        values = ", ".join(f"0x{int(row['volume_nibble'], 16):x}"
                            for row in commands[start:start + 16])
        header_lines.append("    " + values + ",")
    header_lines.extend(["};", ""])
    header_lines.extend([
        "/* Source YM2151 keycodes indexed by the arcade Z80 track note value. */",
        f"static const unsigned char willow_ym2151_note_keycode[{len(note_keycodes)}] = {{",
    ])
    for start in range(0, len(note_keycodes), 16):
        values = ", ".join(f"0x{value:02x}" for value in note_keycodes[start:start + 16])
        header_lines.append("    " + values + ",")
    header_lines.extend(["};", ""])
    curve_values = bytes.fromhex(
        str(note_level_tables["note_curve_table"]["values_hex"]))
    multiply_values = bytes.fromhex(
        str(note_level_tables["nibble_product_table"]["values_hex"]))
    header_lines.extend([
        "/* Source YM2151 note-level lookup data; not arcade program code. */",
        "static const unsigned char willow_ym2151_note_level_curve[128] = {",
    ])
    for start in range(0, len(curve_values), 16):
        values = ", ".join(f"0x{value:02x}"
                            for value in curve_values[start:start + 16])
        header_lines.append("    " + values + ",")
    header_lines.extend([
        "};",
        "static const unsigned char willow_ym2151_nibble_product[256] = {",
    ])
    for start in range(0, len(multiply_values), 16):
        values = ", ".join(f"0x{value:02x}"
                            for value in multiply_values[start:start + 16])
        header_lines.append("    " + values + ",")
    header_lines.extend(["};", ""])
    patches = ym2151_patches["patches"]
    header_lines.extend([
        "/* Source OPM patches selected by verified cue instrument events. */",
        f"#define WILLOW_YM2151_PATCH_COUNT {len(patches)}",
        "#define WILLOW_YM2151_PATCH_SIZE 40",
        "static const unsigned char willow_ym2151_patch_index[WILLOW_YM2151_PATCH_COUNT] = {",
        "    " + ", ".join(f"0x{int(row['instrument_index']):02x}"
                            for row in patches),
        "};",
        "static const unsigned char willow_ym2151_patch_data[WILLOW_YM2151_PATCH_COUNT][WILLOW_YM2151_PATCH_SIZE] = {",
    ])
    for row in patches:
        values = bytes.fromhex(str(row["source_patch_hex"]))
        for start in range(0, len(values), 10):
            chunk = ", ".join(f"0x{value:02x}" for value in values[start:start + 10])
            if start == 0:
                header_lines.append("    { " + chunk + ",")
            elif start + 10 >= len(values):
                header_lines.append("      " + chunk + " },")
            else:
                header_lines.append("      " + chunk + ",")
    header_lines.extend(["};", ""])
    seed_write_count = len(patches[0]["ym2612_native_seed"]["register_writes"])
    header_lines.extend([
        "/* YM2612 register/value seed: note-dependent TL and OPM-only controls are omitted. */",
        f"#define WILLOW_YM2612_PATCH_SEED_WRITE_COUNT {seed_write_count}",
        "static const unsigned char willow_ym2612_patch_seed[WILLOW_YM2151_PATCH_COUNT][WILLOW_YM2612_PATCH_SEED_WRITE_COUNT][2] = {",
    ])
    for row in patches:
        writes = row["ym2612_native_seed"]["register_writes"]
        header_lines.append("    {")
        for write in writes:
            header_lines.append(
                f"        {{ {write['register']}, {write['value']} }},")
        header_lines.append("    },")
    header_lines.extend(["};", ""])
    args.header.write_text("\n".join(header_lines), encoding="utf-8")
    print(f"Catalogued {len(commands)} direct-OKI, {len(system_commands)} system, and {len(sequenced)} sequenced commands to {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
