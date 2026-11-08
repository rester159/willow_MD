#!/usr/bin/env python3
"""Extract Willow's eight gameplay layer-map selector grids."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from cps1_animation_catalog import follow_stream
from cps1_program import assemble_maincpu

POINTER_TABLE = 0x02EB24
MODE_TABLE = 0x02EB44
STAGE_COUNT = 8
SELECTOR_BYTES = 16 * 16
STAGE0_POST_PHASE_SELECTOR_TABLE = 0x054F2
STAGE0_POST_PHASE_SELECTOR_BYTES = bytes.fromhex(
    "ff00fff0ff0000f0ff0001f0ff0002f00000fff0000000f0000001f0000002f0"
    "0100fff0010000f0"
)
STAGE0_POST_PHASE_SCROLL2_CLEAR = 0x0548C
STAGE0_POST_PHASE_SCROLL2_CLEAR_BYTES = bytes.fromhex(
    "3d6e000600603d6e000a0062026eff000006026eff00000a7000102e0048"
    "e54841fb0044341836104eb9000436584eb90004368e303c003f323c0000"
    "3099d35830d93099d35830d93099d35830d93099d35830d951c8ffe6"
    "3d6e006000063d6e0062000a4e75"
)
STAGE0_POST_PHASE_SCROLL3_CLEAR = 0x05532
STAGE0_POST_PHASE_SCROLL3_CLEAR_BYTES = bytes.fromhex(
    "3d6e000600603d6e000a0062026eff000006026eff00000a7000102e0048"
    "e54841fb009e341836104eb900043d4e4eb900043d82303c000f323c0a00"
    "6000ff58"
)
STAGE0_POST_PHASE_ACTOR_SETUP = 0x08F0E
STAGE0_POST_PHASE_ACTOR_SETUP_BYTES = bytes.fromhex(
    "4df88402303c001c227c0002dbf82d71000000423d7c000000063b7c008000"
    "323d7c0300000a3b7c030000341d7c000500484eb8548c1d7c000900484eb8548c"
    "4df88482303c001c227c0002eb242d71000000424ded04823d7c040000063b7c040000"
    "363d7c0200000a3b7c020000381d7c000500484eb855321d7c000900484eb85532"
)
STAGE0_POST_PHASE_ACTOR_MOVE = 0x08FFA
STAGE0_POST_PHASE_ACTOR_MOVE_BYTES = bytes.fromhex(
    "546d00083b7c0080000a4df884023d7c020000063b7c028000323d7c0300000a"
    "3b7c030000341d7c000500484eb8548c1d7c000900484eb8548c"
)
STAGE0_CAMERA_TRACKER = 0x4281E
STAGE0_CAMERA_TRACKER_BYTES = bytes.fromhex(
    "6100054661000100610004ee610002f0302e0006024003ff3b400032"
    "302e000a024003ff3b400034102e000702000010122e0040b300661a"
    "0a2e00100040102e004c61000c9461000df261000e2461000efa102e"
    "000b02000010122e0041b300661a0a2e00100041102e004d61000c6a"
    "61000dc861000dfa61000f92302d0344d040303b00204ebb001c3b6e"
    "0068038e3b6e0064038c3b6e0032"
)
STAGE0_CAMERA_TARGET_TRACKER = 0x42924
STAGE0_CAMERA_TARGET_TRACKER_BYTES = bytes.fromhex(
    "7000102e0003303b00064efb0002001000b6012401420146018801c0"
    "01e64a6d034466164a2d038a6610302e006806400040b06d184c6232"
    "600a302e0068b06d184c62264a2d0362671a1b7c000103d01b7c000e"
    "1848422d1849426d184a1b7c00021846542e00034e75302e006a906e"
    "00060c4000c06e046d284e75610004ac044000c0d16e0006302e0068"
    "b06e000662083d6e006800064e751d7c0000004c4e75044000c0d16e"
    "0006302e0064b06e00066e081d7c0004004c4e753d6e00640006"
)
STAGE0_CAMERA_Y_TRACKER_DISPATCH = 0x42B1C
STAGE0_CAMERA_Y_TRACKER_DISPATCH_BYTES = bytes.fromhex(
    "610003487000102e0004303b00064efb0002001000f20114012c"
)
STAGE0_CAMERA_Y_TRACKER_NORMAL = 0x42B3E
STAGE0_CAMERA_Y_TRACKER_NORMAL_BYTES = bytes.fromhex(
    "302e006c906e000a0c4000806e4c6d1c"
    "302e005ab06e000a6f061d7c00040004"
    "422e00543d7c000000524e7504400080"
    "d16e000a302e005ab06e000a6e141d7c"
    "000c004d1d7c000100543d7c000000524e75"
    "3d6e005a000a4e75"
)
STAGE0_CAMERA_ENDPOINT_SETUP = 0x426FE
STAGE0_CAMERA_ENDPOINT_SETUP_BYTES = bytes.fromhex("3d6d038e0068")
STAGE0_NATIVE_PLAYER_ANCHOR_X = 160
STAGE0_NATIVE_PLAYER_ANCHOR_Y = 80
STAGE0_CAMERA_RETIREMENT = 0x42E4E
STAGE0_CAMERA_RETIREMENT_BYTES = bytes.fromhex(
    "322e0006044103006a024241b26e006463043d4100644e75"
)
STAGE0_CAMERA_TARGET_MODE = 0x42608
STAGE0_CAMERA_TARGET_MODE_BYTES = bytes.fromhex(
    "3d6d184c006a3d6d1850006c4a2d0348660c3d6d8fb0006a"
    "3d6d8fb2006c"
)
STAGE0_CAMERA_TRACKER_STEP = 0x42754
STAGE0_CAMERA_TRACKER_STEP_BYTES = bytes.fromhex(
    "302e006a044000c0322e0064b0416c04302e00643d400006"
)
STAGE0_CAMERA_VERTICAL_SETUP = 0x42774
STAGE0_CAMERA_VERTICAL_SETUP_BYTES = bytes.fromhex(
    "302e006c04400080322e005ab0416c06302e005a600c"
    "322e0032b0416d04302e00323d40000a"
)
STAGE0_CAMERA_PARAMETER_SETUP = 0x426FE
STAGE0_CAMERA_PARAMETER_SETUP_BYTES = bytes.fromhex(
    "3d6d038e00683d6d0390005a3d6d039200324e75"
)
STAGE0_CAMERA_MODE_FLAG_SETTER = 0x08DA8
STAGE0_CAMERA_MODE_FLAG_SETTER_BYTES = bytes.fromhex("1b7c00010348")
STAGE0_CAMERA_UPDATE_CALLS = (
    (0x0947E, bytes.fromhex("4eb9000426024ef900042f48")),
    (0x09634, bytes.fromhex("4eb9000426024ef900042f48")),
    (0x3B524, bytes.fromhex("4a2d03eb660c4eb9000426024eb900042f48")),
)
STAGE0_NORMAL_PLAYER_INIT = 0x0A114
STAGE0_NORMAL_PLAYER_INIT_BYTES = bytes.fromhex(
    "4eb82dfe546d00083b7c00c08fb03b7c02808fb2"
    "3b7c00c004083b7c0200040c3b7c00c004883b7c"
    "0200048c3b7c0007000a4ef878b8"
)
STAGE0_PLAYER_ACTOR_SETUP = 0x0A2A6
STAGE0_PLAYER_ACTOR_SETUP_BYTES = bytes.fromhex(
    "4eb82b9c197c00010000197c00030020"
    "397c00a00006397c0050000a4e75"
)
STAGE_CAMERA_PHASE_RESOLVER = 0x02FBC
STAGE_CAMERA_PHASE_RESOLVER_BYTES = bytes.fromhex(
    "102d0370d040122d03726a02720002410001d04141fa008612300000"
    "b22d039463161b4103940c010004660c4a2d038a66061b7c00030394"
    "302d0344d040303b005041fb004c7200122d0394e74941f010003018"
    "6b163d4000063d58000a044002006a0270003b40038c4e7502407fff"
    "3d4000063d58000a0240ff803b40038c0c6d000403446606066e0060"
    "00064e75"
)
STAGE_CAMERA_PHASE_POINTERS = 0x0304C
STAGE_CAMERA_PHASE_POINTER_BYTES = bytes.fromhex(
    "002c00c8017402e803e00558"
)
STAGE0_CAMERA_PHASE0_RECORD = 0x03078
STAGE0_CAMERA_PHASE0_RECORD_BYTES = bytes.fromhex(
    "00c0004803000000"
)
STAGE_TRANSITION = 0x0A93E
STAGE_TRANSITION_BYTES = bytes.fromhex(
    "0c6d000503446710526d03440c6d000603446304426d0344"
)
STAGE_TRANSITION_GATE = 0x0A902
STAGE_TRANSITION_GATE_BYTES = bytes.fromhex(
    "4a2d02a66620536d000a661a532d8e856b22"
    "556d0008303c001a4eb87d70303c00184eb87d70"
)
STAGE1_EVENT_ROUTE = 0x0A928
STAGE1_EVENT_ROUTE_BYTES = bytes.fromhex(
    "0c6d0001034466044ef80bc44e75"
)
STAGE1_EVENT_DISPATCH = 0x00BC4
STAGE1_EVENT_DISPATCH_BYTES = bytes.fromhex(
    "4ded70423e3c000f4a2e0000670e7000102e0020e548207b000e4e90"
    "4dee004051cfffe64e75"
)
STAGE1_EVENT_HANDLER_TABLE = 0x00BEA
STAGE1_EVENT_HANDLER_TABLE_BYTES = bytes.fromhex(
    "00000c0a00000d1a00000eba0000100e00001112000011f00000122400001252"
)
STAGE_TRANSITION_SETUP_BYTES = bytes.fromhex(
    "4eb82e1c4eb82d804eb82f8a4eb9000381bc"
    "426d0004426d0008426d000c422d799c"
)
STAGE_PLAYER_INIT = 0x0A0E0
STAGE_PLAYER_INIT_STAGE0_BYTES = bytes.fromhex(
    "4eb82f084a2d03a3660a0c6d00040344670000c60c6d0003034466084a2d03a0"
    "670000484a2d0348660001164a6d03446600010e4eb82dfe546d00083b7c00c0"
    "8fb03b7c02808fb23b7c00c004083b7c0200040c3b7c00c004883b7c0200048c"
    "3b7c0007000a4ef878b8"
)
STAGE_PLAYER_INIT_STAGE3_BYTES = bytes.fromhex(
    "1b7c000103a0422d03483b7c001a0008426d000c4eb82dfe3b7c00c08fb0"
    "3b7c10508fb23b7c00c004083b7c1000040c3b7c00c004883b7c1000048c"
    "3b7c01c0184c1b7c001218483b7c0007000a4eb82b9c"
)
STAGE_PLAYER_INIT_STAGE3_OBJECT_BYTES = bytes.fromhex(
    "197c00020000197c00070020397cffa00006397c1068000a"
)
STAGE_PLAYER_INIT_OTHER_BYTES = bytes.fromhex(
    "1b7c00010348546d0004426d0008426d000c4e75"
)
STAGE_SETUP_PARAMETER_TABLES = {
    "stage_field_038e": (0x02DCA, bytes.fromhex(
        "07a07e0016800e00168007a8")),
    "stage_field_0390": (0x02DBE, bytes.fromhex(
        "000000000100100000000000")),
    "stage_field_0392": (0x02DD8, bytes.fromhex(
        "0800011008001f0008000000")),
    "stage_fields_0370_0372": (0x02FA2, bytes.fromhex(
        "00c0004800c0005000c0014800c0108000c0005000c00050")),
}
UNLINKED_ACTOR_COORD_SEQUENCE_DISPATCH = 0x02D7DC
UNLINKED_ACTOR_COORD_SEQUENCE_DISPATCH_BYTES = bytes.fromhex(
    "000e00ac00be02100234027e0308")
UNLINKED_ACTOR_COORD_SEQUENCE_STATE4 = 0x02D89A
UNLINKED_ACTOR_COORD_SEQUENCE_STATE4_BYTES = bytes.fromhex(
    "542e0003303c10fb4eb8786a4eb81e887000102e0021d04041fa00c0"
    "3b7000000374d04041fa00d441f000003b50184c3b500370"
    "3b5804083b5018503b5003723b50040c")
UNLINKED_ACTOR_COORD_SEQUENCE_SELECTORS = 0x02D974
UNLINKED_ACTOR_COORD_SEQUENCE_SELECTORS_BYTES = bytes.fromhex(
    "000100020001000300010002000200040001000300050006")
UNLINKED_ACTOR_COORD_SEQUENCE_TABLE = 0x02D994
UNLINKED_ACTOR_COORD_SEQUENCE_TABLE_BYTES = bytes.fromhex(
    "004003500040065000400350004009a80040035000400650"
    "0040065000200c5000400350004009a80d40003004280c48")
ACTOR_COORD_TASK_QUEUE_HELPER = 0x001E88
ACTOR_COORD_TASK_QUEUE_HELPER_BYTES = bytes.fromhex(
    "1b7c00017891303c0c00323c00034eb81f14"
    "303c00014eb817504a2d789166f2")
ACTOR_COORD_TASK_HELPER_OTHER_CALLS = (
    (0x098BE, bytes.fromhex("4eb81e88")),
    (0x0B844, bytes.fromhex("4eb81e88")),
    (0x2DA5A, bytes.fromhex("4eb81e88")),
    (0x33F66, bytes.fromhex("4eb81e88")),
)
CLASS4_POOL_DISPATCHER = 0x00FFA2
CLASS4_POOL_DISPATCHER_BYTES = bytes.fromhex(
    "4ded1dc63e3c00314a2e0000670e7000102e0020e548"
    "207b000e4e904dee008051cfffe64e75")
CLASS4_POOL_HANDLER_POINTERS = 0x00FFC8
CLASS4_POOL_HANDLER_POINTERS_BYTES = bytes.fromhex(
    "00010054000115ee")
CLASS4_BATCH_CREATE = 0x02D55E
CLASS4_BATCH_CREATE_BYTES = bytes.fromhex(
    "4eb82bea6718197c0001000039590020395900063959000a"
    "51ceffe64e75")
CLASS4_BATCH_FIRST_RECORDS = 0x02D588
CLASS4_BATCH_FIRST_RECORDS_BYTES = bytes.fromhex(
    "01061ea0146001061ee0144001061f20")
CLASS4_BATCH_HANDLER_SELECTOR_DISPATCH = 0x0115EE
CLASS4_BATCH_HANDLER_SELECTOR_DISPATCH_BYTES = bytes.fromhex(
    "102e0021d040323b00064efb1002")
CLASS4_BATCH_HANDLER_SELECTOR_TABLE = 0x0115FC
CLASS4_BATCH_HANDLER_SELECTOR_TABLE_BYTES = bytes.fromhex(
    "0008001c00300044102e0002323b")
STAGE_TRANSITION_COUNTER_TABLE = 0x03819A
STAGE_TRANSITION_COUNTER_TABLE_BYTES = bytes.fromhex(
    "0008101818180810"
)
STAGE_TRANSITION_COUNTER_ROUTINE = 0x0381BC
STAGE_TRANSITION_COUNTER_ROUTINE_BYTES = bytes.fromhex(
    "102d034604000004640270001b400346302d0344103b00c8"
    "d02d0291640270ffb02d03466204102d03461b4003464e75"
)
STAGE_EVENT_INDEX_TABLE = 0x0A8AE
STAGE_EVENT_INDEX_BYTES = bytes.fromhex("000102010000")
STAGE_EVENT_INDEX_SETUP = 0x0A832
STAGE_EVENT_INDEX_SETUP_BYTES = bytes.fromhex(
    "4eb81eaa302d03441b7b00728e85"
)
STAGE_CONTROLLER_TABLE = 0x0A610
STAGE_CONTROLLER_TABLE_BYTES = bytes.fromhex(
    "002e006600ce010c0146016c018e01a802a402f2036c"
)
STAGE_STATE6 = 0x0A79E
STAGE_STATE6_BYTES = bytes.fromhex(
    "4a2d789166f8422d799c546d00084ef86614"
)
STAGE_STATE5 = 0x0A77C
STAGE_STATE5_BYTES = bytes.fromhex(
    "536d000a661a4eb81e88546d00081b7c00017891303c0c00"
    "323c00064ef81f144e75"
)
STAGE_STATE7 = 0x0A7B8
STAGE_STATE7_BYTES = bytes.fromhex(
    "4eb8785e303c08fb4eb8786a302d03441b7b00e603434eb80ac4"
    "546d00083b7c0100000a4ded0402"
)
STAGE_STATE7_ACTOR_TABLE = 0x0AC46
STAGE_STATE7_ACTOR_TABLE_BYTES = bytes.fromhex(
    "000001000000000004000100060001000a00010008000100"
)
STAGE1_ENTRY_TILE_TASK_SETUP = 0x0A848
STAGE1_ENTRY_TILE_TASK_SETUP_BYTES = bytes.fromhex(
    "4eb82c86665e197c00010000197c0002003c297c000ea1200034"
    "197c00040038197c0004003b197c0008003a197c00000020"
    "4eb82c86662c197c00010000197c0002003c297c000ea1c00034"
    "197c00050038197c0004003b197c0008003a197c000000204e75"
)
STAGE_ANIMATION_TASK_FREE_SCAN = 0x002C86
STAGE_ANIMATION_TASK_FREE_SCAN_BYTES = bytes.fromhex(
    "49ed7042303c000f4a2c0000660470004e7549ec004051c8fff070014e75"
)
STAGE0_ENTRY_TILE_TASKS = 0x00830C
STAGE0_ENTRY_TILE_TASKS_BYTES = bytes.fromhex(
    "4eb82c86197c00010000297c000ea6600034197c00010038197c0000003b"
    "197c0004003a197c00010020197c0002003c4eb82c86197c00010000297c"
    "000ea7000034197c00030038197c0001003b197c0004003a197c00010020"
    "197c0002003c"
)
STAGE0_ENTRY_LINKED_CHILD = 0x00836C
STAGE0_ENTRY_LINKED_CHILD_BYTES = bytes.fromhex(
    "4eb82a1a197c00010000197c00080020422c0021397c04c00006397c0040000a"
)
CLASS8_DISPATCH_TABLE = 0x00E71C
CLASS8_SELECTOR8_HANDLER_BYTES = bytes.fromhex("0000fadc")
CLASS8_BEHAVIOR8_STATE0 = 0x00FAF0
CLASS8_BEHAVIOR8_STATE0_BYTES = bytes.fromhex(
    "303c00254eb8786a542e00021d7c003e0022"
    "1d7c0014002a3d7c005000303d7c00a00032"
    "3d7c005000343d7c00a0003641faf9ae4ef81f40"
)
CLASS8_BEHAVIOR8_STATE2 = 0x00FB28
CLASS8_BEHAVIOR8_STATE2_BYTES = bytes.fromhex(
    "4eb82244532e00226604542e0002102d0019"
    "0200000166044ef8709c4ef870ae"
)
CLASS8_BEHAVIOR8_RELEASE = 0x00F512
CLASS8_BEHAVIOR8_RELEASE_BYTES = bytes.fromhex("4ef82b20")
CLASS8_EVENT_LIST_SCAN = 0x00709C
CLASS8_EVENT_LIST_SCAN_BYTES = bytes.fromhex(
    "7c001c2d180b67f65346366d183060000010"
    "7c001c2d180c67e45346366d1832345b102a000053006636"
    "41fa003a4a2e0020660441fa00507000102a0020"
    "103000006b1c206a001432280006e749207c0003b0d2"
    "41f01000323b004c4ebb104851ceffbe4e75"
)
CLASS8_BEHAVIOR8_CANDIDATE_MAP = 0x007102
CLASS8_BEHAVIOR8_CANDIDATE_MAP_BYTES = bytes.fromhex(
    "0000000000000000ff000600000000000000000000ffff000000000000000000"
)
CLASS8_BEHAVIOR0_CANDIDATE_MAP = 0x007122
CLASS8_BEHAVIOR0_CANDIDATE_MAP_BYTES = bytes.fromhex(
    "00ffffffffffff020400ffffffffff00ff00ffffffffffffff00ffffffffffff"
)
CLASS8_EVENT_CALLBACK_OFFSETS = 0x007142
CLASS8_EVENT_CALLBACK_OFFSET_BYTES = bytes.fromhex("00100008005800ac")
CLASS8_EVENT_LINKED_OVERLAP = 0x00719A
CLASS8_EVENT_LINKED_OVERLAP_BYTES = bytes.fromhex(
    "302e0006906a0006d06e0030d058"
    "322e0032d258b04162e6"
    "302e000a906a000ad06e0034d058"
    "322e0036d250b04162ce"
    "4a2a00216614286a00267000102c002149ed8eb619bc00010000"
    "157c000200007c004e75"
)
CLASS8_EVENT_DEFAULT_OVERLAP = 0x007152
CLASS8_EVENT_DEFAULT_OVERLAP_BYTES = bytes.fromhex(
    "302e0006906a0006d06e0030d058322e0032d258b041622e302e000a906a000ad06e0034d058322e0036d250b0416216157c00020000157c00040002422a0003422a00047c004e75"
)
CLASS8_EVENT_STATE6_OVERLAP = 0x0071EE
CLASS8_EVENT_STATE6_OVERLAP_BYTES = bytes.fromhex(
    "302e0006906a0006d06e0030d058322e0032d258b0416226302e000a906a000ad06e0034d058322e0036d250b041620e157c00020000157c000600027c004e75"
)
STAGE_ANIMATION_TASK_DISPATCH = 0x000BC4
STAGE_ANIMATION_TASK_DISPATCH_BYTES = bytes.fromhex(
    "4ded70423e3c000f4a2e0000670e7000102e0020e548207b000e4e904dee"
    "004051cfffe64e7500000c0a00000d1a00000eba0000100e000011120000"
    "11f00000122400001252"
)
STAGE_ANIMATION_TASK_STATE0 = 0x000C1E
STAGE_ANIMATION_TASK_STATE0_BYTES = bytes.fromhex(
    "542e00027000102e003ce148e54843f90091c00043f100007000102e0038"
    "eb4843f100002d4900301d6e003a0022422e003d422e00394e75"
)
STAGE_ANIMATION_TASK_STATE2 = 0x000C56
STAGE_ANIMATION_TASK_STATE2_BYTES = bytes.fromhex(
    "6100007a6606532e002267024e75102e003906000001b02e003b63024200"
    "1d4000391d6e003a0022226e0030206e00347000102e0039eb4841f00000"
    "7001221824110282f000f000828222c1221824110282f000f000828222c"
    "1221824110282f000f000828222c1221824110282f000f000828222c151c"
    "8ffc64e75"
)
ACTOR_POOL8_BEHAVIOR0_STATE0 = 0x026DFC
ACTOR_POOL8_BEHAVIOR0_STATE0_BYTES = bytes.fromhex(
    "1d7c00020000542e0002422e00301d7c000100224a2d038b6644"
    "522d038b1d7c0028002241fa04004eb81f404eb82c86662c"
    "197c00010000297c000e69c00034197c00030038197c0005003b"
    "197c0006003a197c00010020197c0000003c4e75"
)
ACTOR_POOL8_BEHAVIOR0_STATE2 = 0x026E82
ACTOR_POOL8_BEHAVIOR0_STATE2_BYTES = bytes.fromhex(
    "532e002267044ef81f4c4eb82c866600002e197c00010000"
    "297c000e68a00034197c00180038197c0005003b197c0006003a"
    "197c00010020197c0000003c4eb82a1a672c197c00010000197c000a0020"
    "396e00060006066cffe00006396e000a000a066c0050000a294e00262d4c0046"
    "4eb82a1a672c197c00020000197c000b0020396e00060006066cffb00006"
    "396e000a000a066c0018000a294e00262d4c0040542e00041d7c003c0022"
    "522d8e9641fa03e44eb81f40"
)
STAGE_CONTROLLER_ENTRY = 0x0A63E
STAGE_CONTROLLER_ENTRY_BYTES = bytes.fromhex(
    "4eb82884082d000103c3661a3b7c00017892302d0344e548207b00ce"
    "2b487894546d00084e75"
)
STAGE_INPUT_SCRIPT_POINTER_TABLE = 0x0A626
STAGE_INPUT_SCRIPT_POINTER_BYTES = bytes.fromhex(
    "00047e8e00047e8e00047f8e00047e8e00047e8e00047e8e"
)
STAGE_CONTROLLER_FLAG_WRITERS = (
    (0x04900, 1, "behavior routine sets the controller latch to 1"),
    (0x0D9EA, 2, "actor state sets the controller latch to 2"),
    (0x2A4EC, 2, "actor transition sets the controller latch to 2"),
    (0x2C456, 2, "actor transition sets the controller latch to 2"),
    (0x2FBD8, 2, "actor timer expiry sets the controller latch to 2"),
    (0x311F8, 2, "actor transition sets the controller latch to 2"),
    (0x35C00, 2, "actor state sets the controller latch to 2"),
    (0x35C76, 2, "actor timer expiry sets the controller latch to 2"),
    (0x374EC, 1, "actor timer expiry sets the controller latch to 1"),
    (0x3794E, 2, "actor transition sets the controller latch to 2"),
)
STAGE_CONTROLLER_FLAG_WRITER_BYTES = bytes.fromhex("1b7c000003c3")
STAGE_INPUT_SCRIPT_READER = 0x02020
STAGE_INPUT_SCRIPT_READER_BYTES = bytes.fromhex(
    "1b6d006a006b536d78926624206d789430280002660441e80004"
    "1b680001006a3b680002789241e800042b4878944e75"
)
STAGE_INPUT_SCRIPT_DISPATCH = 0x0D03A
STAGE_INPUT_SCRIPT_DISPATCH_BYTES = bytes.fromhex(
    "082d000103c366f64eb82020102d006b4600c02d006a1b408e92"
)
STAGE_STATE4 = 0x0A756
STAGE_STATE4_BYTES = bytes.fromhex(
    "4a2d8e846710303c00304eb8786a61000334600003f46100046a"
    "546d00083b7c0078000a4e75"
)
STAGE_PROGRESS_FLAG_INIT = 0x0A724
STAGE_PROGRESS_FLAG_INIT_BYTES = bytes.fromhex("1b7c00038e84")
STAGE_PROGRESS_COUNTER0_INIT = 0x0A994
STAGE_PROGRESS_COUNTER0_INIT_BYTES = bytes.fromhex(
    "42ad8e7042ad8e741b6d03c78e73102d03c60240000fd04045fb00d4"
    "47ed8e74023c00efc70ac70a64024e75"
)
STAGE_PROGRESS_COUNTER1_INIT = 0x0A9EA
STAGE_PROGRESS_COUNTER1_INIT_BYTES = bytes.fromhex(
    "42ad8e7842ad8e807000102d0362d02d03645300122d8e7b143c0008"
    "023c00efc3026402420151c8fff41b418e7b207c009108441001363c000e"
    "4ef867c8"
)
STAGE_PROGRESS_FLAG0_GATE = 0x0AA9A
STAGE_PROGRESS_FLAG0_GATE_BYTES = bytes.fromhex(
    "202d8e74b0ad03c664e8"
)
STAGE_PROGRESS_COUNTER0_TICK_BYTES = bytes.fromhex(
    "202d8e74b0ad03c664e843ed8e78202d8e7441fa006a023c00ef"
    "c308c308c308c30864042b408e74"
)
STAGE_PROGRESS_FLAG0_CLEAR = 0x0AA8C
STAGE_PROGRESS_FLAG0_CLEAR_BYTES = bytes.fromhex("08ad00008e84")
STAGE_PROGRESS_FLAG1_GATE = 0x0AB5E
STAGE_PROGRESS_FLAG1_GATE_BYTES = bytes.fromhex(
    "202d8e80b0ad8e7c64e8"
)
STAGE_PROGRESS_COUNTER1_TICK_BYTES = bytes.fromhex(
    "202d8e80b0ad8e7c64e843ed8e84202d8e8041faffaa023c00ef"
    "c308c308c308c30864042b408e80"
)
STAGE_PROGRESS_INDEX_INIT = 0x08D14
STAGE_PROGRESS_INDEX_INIT_BYTES = bytes.fromhex(
    "1b7c000503631b7c000503621b7c00050365"
)
STAGE_PROGRESS_INDEX_RESET = 0x02D56
STAGE_PROGRESS_INDEX_RESET_BYTES = bytes.fromhex(
    "1b6d79a203a4422d03641b6d79a30363"
)
STAGE_PROGRESS_INDEX_ADVANCE = 0x37E00
STAGE_PROGRESS_INDEX_ADVANCE_BYTES = bytes.fromhex(
    "542e0002303c002d4eb8786a70014a2e002167027002d02d0362"
    "b02d03636304102d03631b4003621b4003654ef84a34"
)
STAGE_PROGRESS_ACTOR_DISPATCH = 0x37DD0
STAGE_PROGRESS_ACTOR_DISPATCH_BYTES = bytes.fromhex(
    "102e0002323b00064efb1002"
)
STAGE_PROGRESS_ACTOR_SUBSTATE_TABLE = 0x37DDC
STAGE_PROGRESS_ACTOR_SUBSTATE_BYTES = bytes.fromhex(
    "0008ff400024fff0"
)
STAGE_PROGRESS_ACTOR_SCHEDULER = 0x36C76
STAGE_PROGRESS_ACTOR_SCHEDULER_BYTES = bytes.fromhex(
    "0000000200036c4e4ded6ac63e3c00134a2e0000670e7000"
    "102e0020e548207b000e4e904dee004051cfffe64e75"
)
STAGE_PROGRESS_ACTOR_HANDLER_TABLE = 0x36CA4
STAGE_PROGRESS_ACTOR_HANDLER_TABLE_BYTES = bytes.fromhex(
    "00036d7e000374c60003788400036cd000037a0800037cec"
    "00037dd000037fa200036d2c000374fa00037e80"
)
STAGE_PROGRESS_ACTOR_SELECTOR6_SELF_ROUTE = 0x393B6
STAGE_PROGRESS_ACTOR_SELECTOR6_SELF_ROUTE_BYTES = bytes.fromhex(
    "102e0002323b00064efb10020008002e01080108"
    "102d0019d0070200000f6618302e0006906d0408"
    "0c4001e0640a542e00021d7c000600204e75"
)
STAGE_PROGRESS_ACTOR_SELECTOR6_CHILD_ROUTE = 0x24560
STAGE_PROGRESS_ACTOR_SELECTOR6_CHILD_ROUTE_BYTES = bytes.fromhex(
    "0c2e0005002167304eb82b4e672a197c00010000"
    "102e00210600000119400021197c00060020"
    "196e00310031396e00320032396e003400344e75"
)
STAGE_PROGRESS_FLAG1_CLEAR = 0x0AB50
STAGE_PROGRESS_FLAG1_CLEAR_BYTES = bytes.fromhex("08ad00018e84")
STAGE_EVENT_INIT = 0x0A8B4
STAGE_EVENT_INIT_BYTES = bytes.fromhex(
    "546d00083b7c003c000a302d0344e54841fb00247200122d8e8512301000"
    "1b7c000102a6343c031b303c00c0207c000078f04ef815cc"
)
STAGE_EVENT_DISPATCH_TABLE = 0x0A8EA
STAGE_EVENT_DISPATCH_BYTES = bytes.fromhex(
    "270000002e2d000041402f00454400004600000048000000"
)
STAGE_EVENT_TASK_LAUNCH = 0x0A8D2
STAGE_EVENT_TASK_LAUNCH_BYTES = bytes.fromhex(
    "1b7c000102a6343c031b303c00c0207c000078f04ef815cc"
)
STAGE_EVENT_HANDLER = 0x078F0
STAGE_EVENT_HANDLER_BYTES = bytes.fromhex(
    "2c6d82047000102e0010323b000a4ebb10064ef81636"
)
STAGE_EVENT_HANDLER_TABLE = 0x07906
STAGE_EVENT_HANDLER_TABLE_BYTES = bytes.fromhex(
    "000c01d4024004b205cc0660"
)
STAGE_EVENT_TEXT_ENTRY = 0x07912
STAGE_EVENT_TEXT_ENTRY_BYTES = bytes.fromhex(
    "610000803a030c11002f67580c110026660a43e9000106460001603c"
)
STAGE_EVENT_TEXT_LOOKUP = 0x07994
STAGE_EVENT_TEXT_LOOKUP_PREFIX = bytes.fromhex(
    "7200122e00117400142e0012e80ae4a9e7892601e5892001e589d081d083"
    "227c00070000d3c02d49001c43e90008"
)
STAGE_EVENT_TEXT_WRITE = 0x07B1A
STAGE_EVENT_TEXT_WRITE_BYTES = bytes.fromhex(
    "0c2e000400106712286e001c700010190640700030c030d44e75"
)
STAGE_EVENT_RECORD_BASE = 0x070000
STAGE_EVENT_RECORD_STRIDE = 0x00A8
STAGE_EVENT_RECORD_TEXT_OFFSET = 8
STAGE_ASYNC_ALLOCATOR = 0x015D0
STAGE_ASYNC_ALLOCATOR_BYTES = bytes.fromhex(
    "43ed80004a310000661233bc0c0000002388000433810010338200124e73"
)
STAGE_SCHEDULER_SLOT_POINTER_STORE = 0x01506
STAGE_SCHEDULER_SLOT_POINTER_STORE_BYTES = bytes.fromhex("2b488204")
STAGE_EVENT_HANDLER_SLOT_POINTER_LOAD = 0x078F0
STAGE_EVENT_HANDLER_SLOT_POINTER_LOAD_BYTES = bytes.fromhex("2c6d8204")
TASK_ROUTER_TABLE = 0x01E34
TASK_ROUTER_BYTES = bytes.fromhex(
    "00005baa0000559400005ad20000582c0000509c000061dc"
    "0000626a00006380"
)
TASK_0C_ROUTER = 0x0582C
TASK_0C_ROUTER_BYTES = bytes.fromhex(
    "2c6d8204302e0010323b00064efb1002001c00b000e200f801"
    "0e0124012e01380142017001b601f202320272"
)
TASK_0C_STATE0 = 0x05858
TASK_0C_STATE0_BYTES = bytes.fromhex(
    "303c10007200122e00134eb81f14303c00014eb817504a6d"
    "02c067f241f900910000303c0fff323c702074006100005c"
    "41f900918000303c0fff323c000074006100004841f900914000"
    "303c0fff323c0a0074006100003470003b40029c3b40029e"
    "41f9009000006100004c41f900908000610000427000102e0013"
    "58404eb81750422d78914ef8"
)
TASK_1000_ROUTER = 0x0509C
TASK_1000_ROUTER_BYTES = bytes.fromhex(
    "2c6d8204302e0010323b00064efb1002001a0056008c00f2"
    "01ea0260028a02b402de030a03320360038e"
)
TASK_1000_STATE0 = 0x050C6
TASK_1000_STATE0_BYTES = bytes.fromhex(
    "4a6d02c066321b7c00017890343cf000610001107000102e00134eb81750"
    "0cad000001fa8a3e66e4422d78903b7c000102c0026dffc0004a4ef8165a"
)
TASK_1000_FADE = 0x051E8
TASK_1000_FADE_BYTES = bytes.fromhex(
    "42ad8a3e41f90091c00043ed02a83e3c001f614e41f90091c400"
    "43ed02ac3e3c001f613e41f90091c80043ed02b03e3c001f612e"
    "41f90091cc0043ed02b43e3c001f611e41f90091d00043ed02b8"
    "3e3c0007610e41f90091d40043ed02bc3e3c0007611451cffffc"
    "2011d1ad8a3e4e75"
)
TASK_1000_FADE_STEP = 0x05260
TASK_1000_FADE_STEP_BYTES = bytes.fromhex(
    "30104a426b02d0420240f00067123c3c0003d558d558d558d558"
    "51cefff64e7541e800202a114a426a060fc522854e750f8522854e75"
)
STAGE_TIMER_STAGE0_MODE = 0x08D74
STAGE_TIMER_STAGE0_MODE_BYTES = bytes.fromhex(
    "70011b40038a1b40038b1b4003663b7c0d00038e1b7c00060394"
)
STAGE_TIMER_STAGE0_TABLE = 0x0304C
STAGE_TIMER_INIT = 0x02E9C
STAGE_TIMER_INIT_BYTES = bytes.fromhex(
    "302d03440c400005672cd04041fa01a23030000041f000007200"
    "122d0394e74941f010003b68000403c6426d03c8"
)
STAGE_TIMER_TICK = 0x04EDA
STAGE_TIMER_TICK_BYTES = bytes.fromhex(
    "042d000103c864be1b7c003b03c87201102d03c7023c00ef8101"
    "65061b4003c7601c1b7c005903c77201102d03c6023c00ef8101"
    "1b4003c66a04426d03c6"
)
STAGE_TIMER_GATE = 0x04EA2
STAGE_TIMER_GATE_BYTES = bytes.fromhex(
    "4a6d03d266f8102d03e9802d03cf66ee"
    "0c2d000203e264e60c6d001003c66418"
    "4a2d799e6612102d00190200001f6608"
    "303c00434eb8786a"
)
STAGE_TIMER_FREEZE_SET = 0x06954
STAGE_TIMER_FREEZE_SET_BYTES = bytes.fromhex(
    "1b7c000218463b7c00ff18961b7c00041894"
    "426d187c422d18951b7c000103d670003b400042"
    "3b7c000103d2"
)
STAGE_TIMER_FREEZE_TRIGGER = 0x0F4F0
STAGE_TIMER_FREEZE_TRIGGER_BYTES = bytes.fromhex(
    "1d7c000200004a2d03d6660e303c00234eb8786a"
    "1b7c00ff03d61d7c00060002"
)
STAGE_TIMER_FREEZE_CLEAR = 0x0690C
STAGE_TIMER_FREEZE_CLEAR_BYTES = bytes.fromhex(
    "4a2d03d667346b403b7c00ff1896546d0042"
    "0c6d010000426512426d0042426d03d2422d03d6"
)
STAGE_TIMER_ACTOR_FREEZE_GATE = 0x102C0
STAGE_TIMER_ACTOR_FREEZE_GATE_BYTES = bytes.fromhex(
    "4a2e004466124a6d03d2660c102e0003323b00224ebb101e"
)
STAGE_TIMER_PHASE_SETUP = 0x08DA8
STAGE_TIMER_PHASE_SETUP_BYTES = bytes.fromhex(
    "1b7c000103484eb82dfe4eb84f444eb90004035e"
    "61000cd4102d00620c00002066044eb8572c"
    "3b7c0384000a546d0008"
)
STAGE_TIMER_PHASE_STATE_DISPATCH = 0x08CB4
STAGE_TIMER_PHASE_STATE_DISPATCH_BYTES = bytes.fromhex(
    "302d0008323b00064efb1002"
)
STAGE_TIMER_PHASE_STATE_TABLE = 0x08CC0
STAGE_TIMER_PHASE_STATE_TABLE_BYTES = bytes.fromhex(
    "001a00e80124019601e0"
)
STAGE_TIMER_PHASE_CALLER = 0x08DE4
STAGE_TIMER_PHASE_CALLER_BYTES = bytes.fromhex(
    "536d000a672a4a2d03c26624522d00194eb81f884eb84ea2"
    "4eb900009ef24eb900039a524eb90003807c4ef90003b524"
)
STAGE_TIMER_PHASE_EARLY_EXIT_WRITER = 0x0CF6A
STAGE_TIMER_PHASE_EARLY_EXIT_WRITER_BYTES = bytes.fromhex(
    "70001d4000312d4000003d4000043b4003761b7c000103c2"
    "6100f2ac303c00c04ef8168c"
)
STAGE_ASYNC_TASK_FREE_VECTOR = 0x00088
STAGE_ASYNC_TASK_FREE_VECTOR_BYTES = bytes.fromhex("00001690")
STAGE_ASYNC_TASK_FREE_HANDLER = 0x001690
STAGE_ASYNC_TASK_FREE_HANDLER_BYTES = bytes.fromhex(
    "49ed80004a34000067187200298100102981001429810018"
    "2981001c19bc000000004e73"
)
STAGE_PHASE_PALETTE_UPDATE = 0x00C230
STAGE_PHASE_PALETTE_UPDATE_BYTES = bytes.fromhex(
    "207c000e50804a2e0020670441e8002043f90091c000"
    "4a2e0020670443e90020323c000320110280f000f000"
    "809822c020110280f000f000809822c051c9ffe64e75"
)
STAGE_TIMER_PHASE_EXIT = 0x08E14
STAGE_TIMER_PHASE_EXIT_BYTES = bytes.fromhex(
    "4eb8785e3b6d8e6200183b6d8e600284422d1846546d0008"
    "1b7c00018e58323c0600302d0344123b0014343c1422303c00c0"
    "207c000078f04ef815cc"
)
STAGE_TIMER_PHASE_EVENT_IDS = 0x08E50
STAGE_TIMER_PHASE_EVENT_IDS_BYTES = bytes.fromhex("405556575859")
STAGE_TIMER_PHASE_MESSAGE_STYLE = 0x1422
STAGE_EVENT_TEXT_DESTINATION_TABLE = 0x07A1E
STAGE_TIMER_PHASE_MESSAGE_DESTINATION_BYTES = bytes.fromhex("0091091c")
STAGE_TIMER_ACTIVE_CALLERS = (
    (0x09540, bytes.fromhex("66564a2d03c5666e4a2d03c3667c4eb81f884eb84ea2")),
    (0x09600, bytes.fromhex("4a2d799e66084a6d03c66700ff624a2d03c5670e4eb81f884eb84ea2")),
)
STAGE_TIMER_DRAW = 0x04F18
STAGE_TIMER_DRAW_BYTES = bytes.fromhex(
    "207c0091140c7000102d03c67400363c0003610007f4"
    "207c0091158c7000102d03c77401363c0003600007de"
)
STAGE0_VIDEO_BASE_SETUP = 0x00424
STAGE0_VIDEO_BASE_SETUP_BYTES = bytes.fromhex(
    "33fcffc00080010c33fc00000080010e33fc900000800100"
    "33fc91000080010233fc91800080010433fc914000800106"
)


def cps1_scroll1_cell(address: int, scroll1_base: int) -> tuple[int, int]:
    """Map a Scroll 1 RAM address through CPS1's tilemap0_scan layout."""
    relative = address - scroll1_base
    if relative < 0 or relative & 3:
        raise ValueError(f"unaligned Scroll 1 tile address {address:#x}")
    tile_index = relative // 4
    # CPS1 tilemap0_scan: row low 5 bits, then 6 column bits, then row bit 5.
    column = (tile_index >> 5) & 0x3F
    row = (tile_index & 0x1F) | ((tile_index >> 6) & 0x20)
    return column, row


def source_sprite_frame_candidates(stream: dict[str, object]) -> list[dict[str, object]]:
    """Resolve record tile grids with the native CPS row and flip rules."""
    candidates = []
    for record in stream["records"]:
        words = [int(word, 16) for word in record["words"]]
        descriptor = words[1]
        code = words[5]
        timer = int(record["timer"])
        width = ((descriptor >> 8) & 0x0F) + 1
        height = ((descriptor >> 12) & 0x0F) + 1
        if not (0 < timer <= 0x1000 and code < 0x8000
                and width <= 8 and height <= 8):
            continue
        hflip = bool(descriptor & 0x0020)
        rows = []
        for row in range(height):
            rows.append([
                (code & 0xFFF0)
                + ((code + (width - 1 - column if hflip else column)) & 0x000F)
                + (row << 4)
                for column in range(width)
            ])
        candidates.append({
            "record_address": record["rom_offset"],
            "hold_ticks": timer,
            "width": width,
            "height": height,
            "hflip": hflip,
            "tile_codes": [[f"0x{tile:04x}" for tile in row] for row in rows],
        })
    return candidates



def stage_timer(program: bytes) -> dict[str, object]:
    """Extract Stage 0's timer value and its BCD update/render paths."""
    regions = (
        (STAGE_TIMER_STAGE0_MODE, STAGE_TIMER_STAGE0_MODE_BYTES,
         "Stage 0 timer mode setup at 0x8d74"),
        (STAGE_TIMER_INIT, STAGE_TIMER_INIT_BYTES,
         "timer initializer at 0x2e9c"),
        (STAGE_TIMER_TICK, STAGE_TIMER_TICK_BYTES,
         "BCD timer decrement at 0x4eda"),
        (STAGE_TIMER_GATE, STAGE_TIMER_GATE_BYTES,
         "timer update gate at 0x4ea2"),
        (STAGE_TIMER_FREEZE_SET, STAGE_TIMER_FREEZE_SET_BYTES,
         "global actor freeze setter at 0x6954"),
        (STAGE_TIMER_FREEZE_TRIGGER, STAGE_TIMER_FREEZE_TRIGGER_BYTES,
         "global actor freeze mode trigger at 0xf4f0"),
        (STAGE_TIMER_FREEZE_CLEAR, STAGE_TIMER_FREEZE_CLEAR_BYTES,
         "global actor freeze clearer at 0x690c"),
        (STAGE_TIMER_ACTOR_FREEZE_GATE, STAGE_TIMER_ACTOR_FREEZE_GATE_BYTES,
         "actor update freeze gate at 0x102c0"),
        (STAGE_TIMER_PHASE_SETUP, STAGE_TIMER_PHASE_SETUP_BYTES,
         "900-frame timer phase setup at 0x8da8"),
        (STAGE_TIMER_PHASE_STATE_DISPATCH,
         STAGE_TIMER_PHASE_STATE_DISPATCH_BYTES,
         "stage controller state dispatcher at 0x8cb4"),
        (STAGE_TIMER_PHASE_STATE_TABLE,
         STAGE_TIMER_PHASE_STATE_TABLE_BYTES,
         "stage controller state table at 0x8cc0"),
        (STAGE_TIMER_PHASE_CALLER, STAGE_TIMER_PHASE_CALLER_BYTES,
         "900-frame timer phase caller at 0x8de4"),
        (STAGE_TIMER_PHASE_EARLY_EXIT_WRITER,
         STAGE_TIMER_PHASE_EARLY_EXIT_WRITER_BYTES,
         "900-frame phase early-exit writer at 0xcf6a"),
        (STAGE_ASYNC_TASK_FREE_VECTOR, STAGE_ASYNC_TASK_FREE_VECTOR_BYTES,
         "TRAP #2 asynchronous task-free vector"),
        (STAGE_ASYNC_TASK_FREE_HANDLER, STAGE_ASYNC_TASK_FREE_HANDLER_BYTES,
         "TRAP #2 asynchronous task-free handler at 0x1690"),
        (STAGE_PHASE_PALETTE_UPDATE, STAGE_PHASE_PALETTE_UPDATE_BYTES,
         "phase-task palette update at 0xc230"),
        (STAGE_TIMER_PHASE_EXIT, STAGE_TIMER_PHASE_EXIT_BYTES,
         "900-frame timer phase exit at 0x8e14"),
        (STAGE_TIMER_PHASE_EVENT_IDS, STAGE_TIMER_PHASE_EVENT_IDS_BYTES,
         "900-frame phase event ID table at 0x8e50"),
        (STAGE_TIMER_DRAW, STAGE_TIMER_DRAW_BYTES,
         "timer display writer at 0x4f18"),
    )
    for offset, expected, label in regions:
        if program[offset:offset + len(expected)] != expected:
            raise ValueError(f"{label} changed")
    for offset, expected in STAGE_TIMER_ACTIVE_CALLERS:
        if program[offset:offset + len(expected)] != expected:
            raise ValueError(f"active-state timer caller at {offset:#x} changed")
    if program[STAGE0_VIDEO_BASE_SETUP:STAGE0_VIDEO_BASE_SETUP +
               len(STAGE0_VIDEO_BASE_SETUP_BYTES)] != STAGE0_VIDEO_BASE_SETUP_BYTES:
        raise ValueError("Stage 0 CPS Scroll 1 base/scroll initialization changed")

    stage0_relative = int.from_bytes(
        program[STAGE_TIMER_STAGE0_TABLE:STAGE_TIMER_STAGE0_TABLE + 2],
        "big", signed=True)
    stage0_record = STAGE_TIMER_STAGE0_TABLE + stage0_relative
    stage0_mode = int.from_bytes(
        program[STAGE_TIMER_STAGE0_MODE + 0x16:
                STAGE_TIMER_STAGE0_MODE + 0x18], "big")
    initial_bcd = int.from_bytes(
        program[stage0_record + stage0_mode * 8 + 4:
                stage0_record + stage0_mode * 8 + 6], "big")
    minute_tens, minute_ones = (initial_bcd >> 12) & 0xF, (initial_bcd >> 8) & 0xF
    second_tens, second_ones = (initial_bcd >> 4) & 0xF, initial_bcd & 0xF
    if any(digit > 9 for digit in
           (minute_tens, minute_ones, second_tens, second_ones)):
        raise ValueError(f"invalid Stage 0 timer BCD value {initial_bcd:#06x}")
    scroll1_base = int.from_bytes(
        program[STAGE0_VIDEO_BASE_SETUP + 26:STAGE0_VIDEO_BASE_SETUP + 28],
        "big") << 8
    scroll1_destinations = (0x91140C, 0x91158C)
    scroll1_cells = [cps1_scroll1_cell(address, scroll1_base)
                     for address in scroll1_destinations]
    if scroll1_base != 0x910000 or scroll1_cells != [(40, 3), (43, 3)]:
        raise ValueError("Stage 0 timer destinations no longer map to Scroll 1 cells (40,3)/(43,3)")
    # The timer writer emits two adjacent digits at each destination; each
    # digit advances the Scroll 1 RAM pointer by 0x80 bytes, one map column.
    # CPS-1 scroll registers are signed pixel offsets over a 512x512 map.
    scroll1_scroll_x = int.from_bytes(
        program[STAGE0_VIDEO_BASE_SETUP + 2:
                STAGE0_VIDEO_BASE_SETUP + 4], "big", signed=True)
    scroll1_scroll_y = int.from_bytes(
        program[STAGE0_VIDEO_BASE_SETUP + 10:
                STAGE0_VIDEO_BASE_SETUP + 12], "big", signed=True)
    if (scroll1_scroll_x, scroll1_scroll_y) != (-64, 0):
        raise ValueError("Stage 0 Scroll 1 timer transform changed")
    timer_digit_cells = []
    for column, row in scroll1_cells:
        for digit in range(2):
            screen_x = (column * 8 + scroll1_scroll_x) % 512
            screen_y = (row * 8 + scroll1_scroll_y) % 512
            timer_digit_cells.append([screen_x // 8, screen_y // 8])
            column += 1
    phase_message_id = STAGE_TIMER_PHASE_EVENT_IDS_BYTES[0]
    phase_message_offset = (STAGE_EVENT_RECORD_BASE
                            + phase_message_id * STAGE_EVENT_RECORD_STRIDE)
    phase_message_start = phase_message_offset + STAGE_EVENT_RECORD_TEXT_OFFSET
    phase_message_end = program.find(b"/", phase_message_start,
                                     phase_message_offset + STAGE_EVENT_RECORD_STRIDE)
    if phase_message_end < 0:
        raise ValueError("initial 900-frame phase message has no '/' terminator")
    phase_message = program[phase_message_start:phase_message_end].decode("ascii")
    phase_message_destination_index = (
        STAGE_TIMER_PHASE_MESSAGE_STYLE & 0xFF)
    phase_message_destination_offset = (
        STAGE_EVENT_TEXT_DESTINATION_TABLE + phase_message_destination_index * 4)
    expected_destination_end = (phase_message_destination_offset
                                + len(STAGE_TIMER_PHASE_MESSAGE_DESTINATION_BYTES))
    if program[phase_message_destination_offset:expected_destination_end] \
            != STAGE_TIMER_PHASE_MESSAGE_DESTINATION_BYTES:
        raise ValueError("900-frame phase message destination style changed")
    phase_message_destination = int.from_bytes(
        STAGE_TIMER_PHASE_MESSAGE_DESTINATION_BYTES, "big")
    phase_message_cell = cps1_scroll1_cell(phase_message_destination,
                                           scroll1_base)
    return {
        "source_table": f"0x{STAGE_TIMER_STAGE0_TABLE:06x}",
        "stage0_record": f"0x{stage0_record:06x}",
        "stage0_mode_index": stage0_mode,
        "record_bytes": program[stage0_record + stage0_mode * 8:
                                stage0_record + stage0_mode * 8 + 8].hex(),
        "initial_time_bcd": f"0x{initial_bcd:04x}",
        "initial_minutes": minute_tens * 10 + minute_ones,
        "initial_seconds": second_tens * 10 + second_ones,
        "initial_time_minutes_seconds":
            f"{minute_tens * 10 + minute_ones:02d}:"
            f"{second_tens * 10 + second_ones:02d}",
        "timer_initializer_offset": f"0x{STAGE_TIMER_INIT:06x}",
        "timer_initializer_instruction_bytes": STAGE_TIMER_INIT_BYTES.hex(),
        "initializer_behavior": "selects a stage record and an 8-byte mode row, loads its BCD time from record+4 into a5+0x03c6, and clears the frame subcounter at a5+0x03c8",
        "timer_update_offset": f"0x{STAGE_TIMER_TICK:06x}",
        "timer_update_instruction_bytes": STAGE_TIMER_TICK_BYTES.hex(),
        "update_behavior": "decrements the frame subcounter; on borrow it reloads 59 and decrements the BCD seconds/minutes in a5+0x03c6/0x03c7",
        "update_gate_offset": f"0x{STAGE_TIMER_GATE:06x}",
        "update_gate_instruction_bytes": STAGE_TIMER_GATE_BYTES.hex(),
        "update_gate_behavior": "skips when word a5+0x03d2 is nonzero, either byte a5+0x03e9 or a5+0x03cf is nonzero, or byte a5+0x03e2 is at least 2; when remaining BCD time is below 16, a5+0x799e or a5+0x0019 low five bits can defer a timer cue before decrement",
        "global_freeze": {
            "set_offset": f"0x{STAGE_TIMER_FREEZE_SET:06x}",
            "set_instruction_bytes": STAGE_TIMER_FREEZE_SET_BYTES.hex(),
            "trigger_offset": f"0x{STAGE_TIMER_FREEZE_TRIGGER:06x}",
            "trigger_instruction_bytes": STAGE_TIMER_FREEZE_TRIGGER_BYTES.hex(),
            "clear_offset": f"0x{STAGE_TIMER_FREEZE_CLEAR:06x}",
            "clear_instruction_bytes": STAGE_TIMER_FREEZE_CLEAR_BYTES.hex(),
            "actor_gate_offset": f"0x{STAGE_TIMER_ACTOR_FREEZE_GATE:06x}",
            "actor_gate_instruction_bytes": STAGE_TIMER_ACTOR_FREEZE_GATE_BYTES.hex(),
            "behavior": "the task state at 0xf4f0 queues sound command 0x23 and sets a5+0x03d6 to -1; the per-frame manager consumes that trigger, sets a5+0x03d2 to 1, advances a5+0x0042 by 2 per update, and clears the freeze at 0x0100 (128 updates); the generic actor dispatcher skips state handling while the word is nonzero",
            "freeze_frames": 128,
        },
        "stage0_phase_setup_offset": f"0x{STAGE_TIMER_PHASE_SETUP:06x}",
        "stage0_phase_setup_instruction_bytes": STAGE_TIMER_PHASE_SETUP_BYTES.hex(),
        "stage0_phase_behavior": "after Stage 0 setup at 0x8da8, initializes the controller timer to 900 ticks and advances into the phase handled at 0x8de4",
        "stage0_phase_dispatch": {
            "dispatcher_offset": f"0x{STAGE_TIMER_PHASE_STATE_DISPATCH:06x}",
            "dispatcher_bytes": STAGE_TIMER_PHASE_STATE_DISPATCH_BYTES.hex(),
            "state_table_offset": f"0x{STAGE_TIMER_PHASE_STATE_TABLE:06x}",
            "state_table_bytes": STAGE_TIMER_PHASE_STATE_TABLE_BYTES.hex(),
            "state_index_two_target": f"0x{STAGE_TIMER_PHASE_SETUP:06x}",
            "route": "the dispatcher indexes offsets at 0x8cc0 using a5+0x0008; state 0 targets 0x8cda, which advances a5+0x0008 by 2, so the next dispatch selects 0x8da8",
            "source_state_entry_timing": "the state-0 to state-2 transition is established within this controller; the caller that activates this controller during normal Stage 0 play is not established",
        },
        "stage0_phase_caller_offset": f"0x{STAGE_TIMER_PHASE_CALLER:06x}",
        "stage0_phase_caller_instruction_bytes": STAGE_TIMER_PHASE_CALLER_BYTES.hex(),
        "stage0_phase_caller_behavior": "decrements the 900-tick controller timer, exits on expiry or nonzero a5+0x03c2, otherwise increments global frame byte a5+0x0019, updates the player, calls the gated timer routine, and tail-jumps to 0x3b524; that main update calls camera updater 0x42602 and tracker 0x42f48",
        "stage0_phase_main_update_tail_call": {
            "offset": "0x008e0e",
            "bytes": program[0x08E0E:0x08E14].hex(),
            "target": "0x03b524",
            "camera_update_gate": "0x3b524 calls 0x42602/0x42f48 while a5+0x03eb is clear",
        },
        "stage0_phase_early_exit_writer_offset": f"0x{STAGE_TIMER_PHASE_EARLY_EXIT_WRITER:06x}",
        "stage0_phase_early_exit_writer_instruction_bytes": STAGE_TIMER_PHASE_EARLY_EXIT_WRITER_BYTES.hex(),
        "stage0_phase_early_exit_writer_behavior": "clears the current task record's +0/+4/+49 fields and work-area word a5+0x0376, sets a5+0x03c2 to 1, then passes task-slot offset 0x00c0 to TRAP #2, whose handler frees that asynchronous task slot; the 900-frame loop uses this byte to exit early, but the task-state caller is not yet mapped",
        "async_task_free_vector": f"0x{int.from_bytes(STAGE_ASYNC_TASK_FREE_VECTOR_BYTES, 'big'):06x}",
        "async_task_free_handler_offset": f"0x{STAGE_ASYNC_TASK_FREE_HANDLER:06x}",
        "async_task_free_handler_instruction_bytes": STAGE_ASYNC_TASK_FREE_HANDLER_BYTES.hex(),
        "phase_palette_update_offset": f"0x{STAGE_PHASE_PALETTE_UPDATE:06x}",
        "phase_palette_update_instruction_bytes": STAGE_PHASE_PALETTE_UPDATE_BYTES.hex(),
        "phase_palette_update_behavior": "selects a four-color source row at 0x0e5080 (offset by task byte +0x20 when set) and merges it into CPS palette RAM at 0x91c000+offset",
        "stage0_phase_exit_offset": f"0x{STAGE_TIMER_PHASE_EXIT:06x}",
        "stage0_phase_exit_instruction_bytes": STAGE_TIMER_PHASE_EXIT_BYTES.hex(),
        "stage0_phase_event_ids": [f"0x{value:02x}" for value in STAGE_TIMER_PHASE_EVENT_IDS_BYTES],
        "stage0_phase_initial_message": {
            "event_id": f"0x{phase_message_id:02x}",
            "text": phase_message,
            "style_word": f"0x{STAGE_TIMER_PHASE_MESSAGE_STYLE:04x}",
            "palette_group": (STAGE_TIMER_PHASE_MESSAGE_STYLE >> 8) & 0x0F,
            "scroll1_destination_table_offset": f"0x{phase_message_destination_offset:06x}",
            "scroll1_destination_address": f"0x{phase_message_destination:06x}",
            "scroll1_destination_cell": list(phase_message_cell),
        },
        "stage0_phase_exit_behavior": "queues system command 0xf4, copies stage work-area words into actor fields +0x18 and +0x0284, clears a5+0x1846, advances controller substate by two, sets a5-0x71a8, then overwrites temporary D1=0x0600 with a message ID from the six-byte table at 0x8e50 and dispatches asynchronous text task type 0x0c00 with style 0x1422; stage counter zero selects message 0x40",
        "active_state_callers": [f"0x{offset:06x}" for offset, _ in STAGE_TIMER_ACTIVE_CALLERS],
        "timer_display_offset": f"0x{STAGE_TIMER_DRAW:06x}",
        "timer_display_instruction_bytes": STAGE_TIMER_DRAW_BYTES.hex(),
        "display_destinations": ["0x91140c", "0x91158c"],
        "scroll1_initialization": {
            "offset": f"0x{STAGE0_VIDEO_BASE_SETUP:06x}",
            "instruction_bytes": STAGE0_VIDEO_BASE_SETUP_BYTES.hex(),
            "base_register_value": "0x9100",
            "base_address": f"0x{scroll1_base:06x}",
            "scroll_x": "0xffc0",
            "scroll_y": "0x0000",
            "timer_digit_cells": timer_digit_cells,
            "tilemap_mapper": "CPS1 Scroll 1 64x64 mapper: row-low-5 + column<<5 + row-bit-5<<11",
            "destination_cells": [
                {"address": f"0x{address:06x}", "column": column, "row": row}
                for address, (column, row) in zip(scroll1_destinations,
                                                  scroll1_cells)
            ],
            "native_screen_transform": "for timer digits only, map each CPS cell to pixel coordinates as (column*8 + signed scrollX, row*8 + signed scrollY) modulo 512, then divide by 8; startup scroll values place the four cells at (32,3), (33,3), (35,3), (36,3)",
        },
        "runtime_callers": ["0x8df8", "0x9552", "0x9618"],
        "native_port_status": "native BCD countdown and timer digit positions are generated from source Scroll 1 destination cells and startup scroll registers; Scroll 1 glyph layouts are uncompressed, while their source palette is remapped to the closest active Genesis background palette",
    }


def stage_transition(program: bytes) -> dict[str, object]:
    """Decode the observed normal stage advance at 0xa93e."""
    stage0_end = STAGE_PLAYER_INIT + len(STAGE_PLAYER_INIT_STAGE0_BYTES)
    if program[STAGE_PLAYER_INIT:stage0_end] != STAGE_PLAYER_INIT_STAGE0_BYTES:
        raise ValueError("normal player initializer at 0xa0e0 changed")
    stage3_at = STAGE_PLAYER_INIT + stage0_end - STAGE_PLAYER_INIT
    stage3_end = stage3_at + len(STAGE_PLAYER_INIT_STAGE3_BYTES)
    if program[stage3_at:stage3_end] != STAGE_PLAYER_INIT_STAGE3_BYTES:
        raise ValueError("stage-3 player initializer at 0xa14a changed")
    stage3_object_at = stage3_end
    if (program[stage3_object_at:stage3_object_at
                + len(STAGE_PLAYER_INIT_STAGE3_OBJECT_BYTES)]
            != STAGE_PLAYER_INIT_STAGE3_OBJECT_BYTES):
        raise ValueError("stage-3 object initializer at 0xa19c changed")
    other_at = 0x0A220
    if program[other_at:other_at + len(STAGE_PLAYER_INIT_OTHER_BYTES)] != STAGE_PLAYER_INIT_OTHER_BYTES:
        raise ValueError("other-stage initialization branch at 0xa220 changed")
    gate_end = STAGE_TRANSITION_GATE + len(STAGE_TRANSITION_GATE_BYTES)
    if program[STAGE_TRANSITION_GATE:gate_end] != STAGE_TRANSITION_GATE_BYTES:
        raise ValueError("stage transition gate at 0xa902 changed")
    route_end = STAGE1_EVENT_ROUTE + len(STAGE1_EVENT_ROUTE_BYTES)
    if program[STAGE1_EVENT_ROUTE:route_end] != STAGE1_EVENT_ROUTE_BYTES:
        raise ValueError("stage-1 event route at 0xa928 changed")
    dispatch_end = STAGE1_EVENT_DISPATCH + len(STAGE1_EVENT_DISPATCH_BYTES)
    if program[STAGE1_EVENT_DISPATCH:dispatch_end] != STAGE1_EVENT_DISPATCH_BYTES:
        raise ValueError("stage-1 16-record dispatcher at 0x0bc4 changed")
    handler_table_end = (STAGE1_EVENT_HANDLER_TABLE
                         + len(STAGE1_EVENT_HANDLER_TABLE_BYTES))
    if (program[STAGE1_EVENT_HANDLER_TABLE:handler_table_end]
            != STAGE1_EVENT_HANDLER_TABLE_BYTES):
        raise ValueError("stage-1 actor handler table at 0x0bea changed")
    end = STAGE_TRANSITION + len(STAGE_TRANSITION_BYTES)
    if program[STAGE_TRANSITION:end] != STAGE_TRANSITION_BYTES:
        raise ValueError("stage advance at 0xa93e no longer matches the traced 68000 code")
    setup_at = end
    setup_end = setup_at + len(STAGE_TRANSITION_SETUP_BYTES)
    if program[setup_at:setup_end] != STAGE_TRANSITION_SETUP_BYTES:
        raise ValueError("stage reinitialization after 0xa93e changed")
    for name, (offset, expected) in STAGE_SETUP_PARAMETER_TABLES.items():
        if program[offset:offset + len(expected)] != expected:
            raise ValueError(f"stage setup table {name} at {offset:#x} changed")
    counter_table_end = (STAGE_TRANSITION_COUNTER_TABLE
                         + len(STAGE_TRANSITION_COUNTER_TABLE_BYTES))
    if (program[STAGE_TRANSITION_COUNTER_TABLE:counter_table_end]
            != STAGE_TRANSITION_COUNTER_TABLE_BYTES):
        raise ValueError("stage transition counter table at 0x3819a changed")
    counter_routine_end = (STAGE_TRANSITION_COUNTER_ROUTINE
                           + len(STAGE_TRANSITION_COUNTER_ROUTINE_BYTES))
    if (program[STAGE_TRANSITION_COUNTER_ROUTINE:counter_routine_end]
            != STAGE_TRANSITION_COUNTER_ROUTINE_BYTES):
        raise ValueError("stage transition counter routine at 0x381bc changed")
    event_table_end = STAGE_EVENT_INDEX_TABLE + len(STAGE_EVENT_INDEX_BYTES)
    if program[STAGE_EVENT_INDEX_TABLE:event_table_end] != STAGE_EVENT_INDEX_BYTES:
        raise ValueError("stage event index table at 0xa8ae changed")
    event_index_setup_end = STAGE_EVENT_INDEX_SETUP + len(STAGE_EVENT_INDEX_SETUP_BYTES)
    if program[STAGE_EVENT_INDEX_SETUP:event_index_setup_end] != STAGE_EVENT_INDEX_SETUP_BYTES:
        raise ValueError("stage event index setup at 0xa832 changed")
    controller_end = STAGE_CONTROLLER_TABLE + len(STAGE_CONTROLLER_TABLE_BYTES)
    if program[STAGE_CONTROLLER_TABLE:controller_end] != STAGE_CONTROLLER_TABLE_BYTES:
        raise ValueError("stage controller dispatch table at 0xa610 changed")
    controller_entry_end = STAGE_CONTROLLER_ENTRY + len(STAGE_CONTROLLER_ENTRY_BYTES)
    if program[STAGE_CONTROLLER_ENTRY:controller_entry_end] != STAGE_CONTROLLER_ENTRY_BYTES:
        raise ValueError("stage controller entry at 0xa63e changed")
    script_pointer_end = (STAGE_INPUT_SCRIPT_POINTER_TABLE
                          + len(STAGE_INPUT_SCRIPT_POINTER_BYTES))
    if program[STAGE_INPUT_SCRIPT_POINTER_TABLE:script_pointer_end] != STAGE_INPUT_SCRIPT_POINTER_BYTES:
        raise ValueError("stage input-script pointer table at 0xa626 changed")
    for offset, value, description in STAGE_CONTROLLER_FLAG_WRITERS:
        expected = bytearray(STAGE_CONTROLLER_FLAG_WRITER_BYTES)
        expected[3] = value
        if program[offset:offset + len(expected)] != expected:
            raise ValueError(f"controller latch writer at {offset:#x} changed")
    for offset, expected, label in (
        (STAGE_INPUT_SCRIPT_READER, STAGE_INPUT_SCRIPT_READER_BYTES,
         "stage-indexed input script reader at 0x2020"),
        (STAGE_INPUT_SCRIPT_DISPATCH, STAGE_INPUT_SCRIPT_DISPATCH_BYTES,
         "stage-indexed input script dispatcher at 0xd03a"),
    ):
        if program[offset:offset + len(expected)] != expected:
            raise ValueError(f"{label} changed")
    state6_end = STAGE_STATE6 + len(STAGE_STATE6_BYTES)
    if program[STAGE_STATE6:state6_end] != STAGE_STATE6_BYTES:
        raise ValueError("stage controller state 6 at 0xa79e changed")
    state5_end = STAGE_STATE5 + len(STAGE_STATE5_BYTES)
    if program[STAGE_STATE5:state5_end] != STAGE_STATE5_BYTES:
        raise ValueError("stage controller state 5 at 0xa77c changed")
    state7_end = STAGE_STATE7 + len(STAGE_STATE7_BYTES)
    if program[STAGE_STATE7:state7_end] != STAGE_STATE7_BYTES:
        raise ValueError("stage controller state 7 at 0xa7b8 changed")
    state7_actor_table_end = (STAGE_STATE7_ACTOR_TABLE
                              + len(STAGE_STATE7_ACTOR_TABLE_BYTES))
    if (program[STAGE_STATE7_ACTOR_TABLE:state7_actor_table_end]
            != STAGE_STATE7_ACTOR_TABLE_BYTES):
        raise ValueError("stage state-7 actor coordinate table at 0xac46 changed")
    stage1_tile_setup_end = (STAGE1_ENTRY_TILE_TASK_SETUP
                              + len(STAGE1_ENTRY_TILE_TASK_SETUP_BYTES))
    if (program[STAGE1_ENTRY_TILE_TASK_SETUP:stage1_tile_setup_end]
            != STAGE1_ENTRY_TILE_TASK_SETUP_BYTES):
        raise ValueError("stage-1 entry tile-task setup at 0xa848 changed")
    stage1_free_scan_end = (STAGE_ANIMATION_TASK_FREE_SCAN
                            + len(STAGE_ANIMATION_TASK_FREE_SCAN_BYTES))
    if (program[STAGE_ANIMATION_TASK_FREE_SCAN:stage1_free_scan_end]
            != STAGE_ANIMATION_TASK_FREE_SCAN_BYTES):
        raise ValueError("stage-animation-task first-free scan at 0x2c86 changed")
    for offset, expected, label in (
        (STAGE_ANIMATION_TASK_DISPATCH,
         STAGE_ANIMATION_TASK_DISPATCH_BYTES,
         "16-record Stage animation-task dispatcher"),
        (STAGE_ANIMATION_TASK_STATE0, STAGE_ANIMATION_TASK_STATE0_BYTES,
         "Stage animation-task initializer"),
        (STAGE_ANIMATION_TASK_STATE2, STAGE_ANIMATION_TASK_STATE2_BYTES,
         "Stage animation-task frame updater"),
    ):
        if program[offset:offset + len(expected)] != expected:
            raise ValueError(f"{label} at {offset:#x} changed")
    batch_actor_tag = int.from_bytes(CLASS4_BATCH_FIRST_RECORDS_BYTES[:2], "big")
    selector_6_word = int.from_bytes(
        CLASS4_BATCH_HANDLER_SELECTOR_TABLE_BYTES[12:14], "big")
    selector_6_target = CLASS4_BATCH_HANDLER_SELECTOR_TABLE + selector_6_word
    if batch_actor_tag != 0x0106:
        raise ValueError("class-4 fixed batch actor tag changed")
    if CLASS4_BATCH_HANDLER_SELECTOR_TABLE_BYTES[:8] != bytes.fromhex(
            "0008001c00300044"):
        raise ValueError("handler 0x115ee's first four selector offsets changed")
    if selector_6_target & 1 == 0:
        raise ValueError("selector 6 no longer computes the odd jump address")
    stage0_tile_setup_end = (STAGE0_ENTRY_TILE_TASKS
                              + len(STAGE0_ENTRY_TILE_TASKS_BYTES))
    if (program[STAGE0_ENTRY_TILE_TASKS:stage0_tile_setup_end]
            != STAGE0_ENTRY_TILE_TASKS_BYTES):
        raise ValueError("Stage 0 tile-task setup at 0x830c changed")
    stage0_child_end = STAGE0_ENTRY_LINKED_CHILD + len(STAGE0_ENTRY_LINKED_CHILD_BYTES)
    if (program[STAGE0_ENTRY_LINKED_CHILD:stage0_child_end]
            != STAGE0_ENTRY_LINKED_CHILD_BYTES):
        raise ValueError("Stage 0 linked-child setup at 0x836c changed")
    if (program[CLASS8_DISPATCH_TABLE + 8 * 4:
                CLASS8_DISPATCH_TABLE + 8 * 4
                + len(CLASS8_SELECTOR8_HANDLER_BYTES)]
            != CLASS8_SELECTOR8_HANDLER_BYTES):
        raise ValueError("class-8 behavior-8 selector at 0xe73c changed")
    if (program[CLASS8_BEHAVIOR8_STATE0:
                CLASS8_BEHAVIOR8_STATE0 + len(CLASS8_BEHAVIOR8_STATE0_BYTES)]
            != CLASS8_BEHAVIOR8_STATE0_BYTES):
        raise ValueError("class-8 behavior-8 state 0 at 0xfaf0 changed")
    if (program[CLASS8_BEHAVIOR8_STATE2:
                CLASS8_BEHAVIOR8_STATE2 + len(CLASS8_BEHAVIOR8_STATE2_BYTES)]
            != CLASS8_BEHAVIOR8_STATE2_BYTES):
        raise ValueError("class-8 behavior-8 state 2 at 0xfb28 changed")
    if (program[CLASS8_BEHAVIOR8_RELEASE:
                CLASS8_BEHAVIOR8_RELEASE
                + len(CLASS8_BEHAVIOR8_RELEASE_BYTES)]
            != CLASS8_BEHAVIOR8_RELEASE_BYTES):
        raise ValueError("class-8 behavior-8 state 4 release at 0xf512 changed")
    event_regions = (
        (CLASS8_EVENT_LIST_SCAN, CLASS8_EVENT_LIST_SCAN_BYTES),
        (CLASS8_BEHAVIOR8_CANDIDATE_MAP, CLASS8_BEHAVIOR8_CANDIDATE_MAP_BYTES),
        (CLASS8_BEHAVIOR0_CANDIDATE_MAP, CLASS8_BEHAVIOR0_CANDIDATE_MAP_BYTES),
        (CLASS8_EVENT_CALLBACK_OFFSETS, CLASS8_EVENT_CALLBACK_OFFSET_BYTES),
        (CLASS8_EVENT_DEFAULT_OVERLAP, CLASS8_EVENT_DEFAULT_OVERLAP_BYTES),
        (CLASS8_EVENT_LINKED_OVERLAP, CLASS8_EVENT_LINKED_OVERLAP_BYTES),
        (CLASS8_EVENT_STATE6_OVERLAP, CLASS8_EVENT_STATE6_OVERLAP_BYTES),
    )
    for address, expected in event_regions:
        if program[address:address + len(expected)] != expected:
            raise ValueError(f"class-8 event scan region at {address:#08x} changed")
    behavior8_callbacks = []
    for behavior, selector in enumerate(CLASS8_BEHAVIOR8_CANDIDATE_MAP_BYTES):
        if selector & 0x80:
            target = None
        else:
            offset = int.from_bytes(
                CLASS8_EVENT_CALLBACK_OFFSET_BYTES[selector:selector + 2], "big")
            target = f"0x{CLASS8_EVENT_CALLBACK_OFFSETS + offset:06x}"
        behavior8_callbacks.append({"candidate_behavior": behavior,
                                    "callback": target})
    stage1_state0_end = (ACTOR_POOL8_BEHAVIOR0_STATE0
                         + len(ACTOR_POOL8_BEHAVIOR0_STATE0_BYTES))
    if (program[ACTOR_POOL8_BEHAVIOR0_STATE0:stage1_state0_end]
            != ACTOR_POOL8_BEHAVIOR0_STATE0_BYTES):
        raise ValueError("separate eight-record actor-pool state 0 at 0x26dfc changed")
    stage1_state2_end = (ACTOR_POOL8_BEHAVIOR0_STATE2
                         + len(ACTOR_POOL8_BEHAVIOR0_STATE2_BYTES))
    if (program[ACTOR_POOL8_BEHAVIOR0_STATE2:stage1_state2_end]
            != ACTOR_POOL8_BEHAVIOR0_STATE2_BYTES):
        raise ValueError("separate eight-record actor-pool state 2 at 0x26e82 changed")
    state4_end = STAGE_STATE4 + len(STAGE_STATE4_BYTES)
    if program[STAGE_STATE4:state4_end] != STAGE_STATE4_BYTES:
        raise ValueError("stage controller state 4 at 0xa756 changed")
    progress_regions = (
        (STAGE_PROGRESS_FLAG_INIT, STAGE_PROGRESS_FLAG_INIT_BYTES,
         "stage progress flags initialized at 0xa724"),
        (STAGE_PROGRESS_INDEX_INIT, STAGE_PROGRESS_INDEX_INIT_BYTES,
         "stage sequence index initialized at 0x8d14"),
        (STAGE_PROGRESS_INDEX_RESET, STAGE_PROGRESS_INDEX_RESET_BYTES,
         "stage sequence index reset at 0x2d56"),
        (STAGE_PROGRESS_INDEX_ADVANCE, STAGE_PROGRESS_INDEX_ADVANCE_BYTES,
         "stage sequence index advance at 0x37e00"),
        (STAGE_PROGRESS_ACTOR_DISPATCH, STAGE_PROGRESS_ACTOR_DISPATCH_BYTES,
         "stage sequence actor substate dispatcher at 0x37dd0"),
        (STAGE_PROGRESS_ACTOR_SUBSTATE_TABLE, STAGE_PROGRESS_ACTOR_SUBSTATE_BYTES,
         "stage sequence actor substate table at 0x37ddc"),
        (STAGE_PROGRESS_ACTOR_SCHEDULER,
         STAGE_PROGRESS_ACTOR_SCHEDULER_BYTES,
         "20-record stage actor scheduler at 0x36c76"),
        (STAGE_PROGRESS_ACTOR_HANDLER_TABLE,
         STAGE_PROGRESS_ACTOR_HANDLER_TABLE_BYTES,
         "stage actor handler table at 0x36ca4"),
        (STAGE_PROGRESS_ACTOR_SELECTOR6_SELF_ROUTE,
         STAGE_PROGRESS_ACTOR_SELECTOR6_SELF_ROUTE_BYTES,
         "stage actor self-transition to selector 6 at 0x393c2"),
        (STAGE_PROGRESS_ACTOR_SELECTOR6_CHILD_ROUTE,
         STAGE_PROGRESS_ACTOR_SELECTOR6_CHILD_ROUTE_BYTES,
         "stage actor child registration to selector 6 at 0x24560"),
        (STAGE_PROGRESS_COUNTER0_INIT, STAGE_PROGRESS_COUNTER0_INIT_BYTES,
         "stage progress counter 0 initializer at 0xa994"),
        (STAGE_PROGRESS_COUNTER1_INIT, STAGE_PROGRESS_COUNTER1_INIT_BYTES,
         "stage progress counter 1 initializer at 0xa9ea"),
        (STAGE_PROGRESS_FLAG0_GATE, STAGE_PROGRESS_COUNTER0_TICK_BYTES,
         "stage progress counter 0 BCD update at 0xaa9a"),
        (STAGE_PROGRESS_FLAG1_GATE, STAGE_PROGRESS_COUNTER1_TICK_BYTES,
         "stage progress counter 1 BCD update at 0xab5e"),
        (STAGE_PROGRESS_FLAG0_GATE, STAGE_PROGRESS_FLAG0_GATE_BYTES,
         "stage progress flag 0 comparison at 0xaa9a"),
        (STAGE_PROGRESS_FLAG0_CLEAR, STAGE_PROGRESS_FLAG0_CLEAR_BYTES,
         "stage progress flag 0 clear at 0xaa8c"),
        (STAGE_PROGRESS_FLAG1_GATE, STAGE_PROGRESS_FLAG1_GATE_BYTES,
         "stage progress flag 1 comparison at 0xab5e"),
        (STAGE_PROGRESS_FLAG1_CLEAR, STAGE_PROGRESS_FLAG1_CLEAR_BYTES,
         "stage progress flag 1 clear at 0xab50"),
    )
    for offset, expected, label in progress_regions:
        if program[offset:offset + len(expected)] != expected:
            raise ValueError(f"{label} changed")
    event_init_end = STAGE_EVENT_INIT + len(STAGE_EVENT_INIT_BYTES)
    if program[STAGE_EVENT_INIT:event_init_end] != STAGE_EVENT_INIT_BYTES:
        raise ValueError("stage event initializer at 0xa8b8 changed")
    dispatch_end = STAGE_EVENT_DISPATCH_TABLE + len(STAGE_EVENT_DISPATCH_BYTES)
    if program[STAGE_EVENT_DISPATCH_TABLE:dispatch_end] != STAGE_EVENT_DISPATCH_BYTES:
        raise ValueError("stage event dispatch table at 0xa8ea changed")
    event_runtime_regions = (
        (STAGE_EVENT_TASK_LAUNCH, STAGE_EVENT_TASK_LAUNCH_BYTES,
         "stage event task launch at 0xa8d2"),
        (STAGE_EVENT_HANDLER, STAGE_EVENT_HANDLER_BYTES,
         "stage event handler at 0x78f0"),
        (STAGE_EVENT_HANDLER_TABLE, STAGE_EVENT_HANDLER_TABLE_BYTES,
         "stage event handler table at 0x7906"),
        (STAGE_ASYNC_ALLOCATOR, STAGE_ASYNC_ALLOCATOR_BYTES,
         "asynchronous task allocator at 0x15d0"),
        (STAGE_SCHEDULER_SLOT_POINTER_STORE,
         STAGE_SCHEDULER_SLOT_POINTER_STORE_BYTES,
         "scheduler selected-slot pointer store at 0x1506"),
        (STAGE_EVENT_HANDLER_SLOT_POINTER_LOAD,
         STAGE_EVENT_HANDLER_SLOT_POINTER_LOAD_BYTES,
         "event handler selected-slot pointer load at 0x78f0"),
    )
    for offset, expected, label in event_runtime_regions:
        if program[offset:offset + len(expected)] != expected:
            raise ValueError(f"{label} changed")
    text_regions = (
        (STAGE_EVENT_TEXT_ENTRY, STAGE_EVENT_TEXT_ENTRY_BYTES,
         "stage text parser at 0x7912"),
        (STAGE_EVENT_TEXT_LOOKUP, STAGE_EVENT_TEXT_LOOKUP_PREFIX,
         "stage text record lookup at 0x7994"),
        (STAGE_EVENT_TEXT_WRITE, STAGE_EVENT_TEXT_WRITE_BYTES,
         "stage text tile writer at 0x7b1a"),
    )
    for offset, expected, label in text_regions:
        if program[offset:offset + len(expected)] != expected:
            raise ValueError(f"{label} changed")
    event_ids = sorted({
        event_id
        for offset in range(0, len(STAGE_EVENT_DISPATCH_BYTES), 4)
        for event_id in STAGE_EVENT_DISPATCH_BYTES[offset:offset + 4]
        if event_id
    })
    event_messages = []
    for event_id in event_ids:
        record_offset = STAGE_EVENT_RECORD_BASE + event_id * STAGE_EVENT_RECORD_STRIDE
        start = record_offset + STAGE_EVENT_RECORD_TEXT_OFFSET
        raw = program[start:start + STAGE_EVENT_RECORD_STRIDE - STAGE_EVENT_RECORD_TEXT_OFFSET]
        terminator = raw.find(b"/")
        if terminator < 0:
            raise ValueError(f"stage text record {event_id:#04x} has no '/' terminator")
        message_bytes = raw[:terminator]
        if any(byte != ord("&") and not 0x20 <= byte <= 0x7E
               for byte in message_bytes):
            raise ValueError(f"stage text record {event_id:#04x} has invalid text bytes")
        message = message_bytes.decode("ascii").replace("&", "\n")
        event_messages.append({
            "event_id": f"0x{event_id:02x}",
            "record_offset": f"0x{record_offset:06x}",
            "text_offset": f"0x{start:06x}",
            "text": message,
            "lines": message.split("\n"),
        })
    task_regions = (
        (TASK_ROUTER_TABLE, TASK_ROUTER_BYTES, "task router table at 0x1e34"),
        (TASK_0C_ROUTER, TASK_0C_ROUTER_BYTES, "task 0x0c00 router at 0x582c"),
        (TASK_0C_STATE0, TASK_0C_STATE0_BYTES, "task 0x0c00 state 0 at 0x5858"),
        (TASK_1000_ROUTER, TASK_1000_ROUTER_BYTES, "task 0x1000 router at 0x509c"),
        (TASK_1000_STATE0, TASK_1000_STATE0_BYTES, "task 0x1000 state 0 at 0x50c6"),
        (TASK_1000_FADE, TASK_1000_FADE_BYTES, "task 0x1000 fade routine at 0x51e8"),
        (TASK_1000_FADE_STEP, TASK_1000_FADE_STEP_BYTES, "task 0x1000 fade step at 0x5260"),
    )
    for offset, expected, label in task_regions:
        if program[offset:offset + len(expected)] != expected:
            raise ValueError(f"{label} changed")
    stage_setup_parameters = []
    for stage in range(6):
        def word_at(name: str, stride: int = 2, word_index: int = 0) -> int:
            offset = (STAGE_SETUP_PARAMETER_TABLES[name][0]
                      + stage * stride + word_index * 2)
            return int.from_bytes(program[offset:offset + 2], "big")

        stage_setup_parameters.append({
            "stage": stage,
            "field_038e": f"0x{word_at('stage_field_038e'):04x}",
            "field_0390": f"0x{word_at('stage_field_0390'):04x}",
            "field_0392": f"0x{word_at('stage_field_0392'):04x}",
            "field_0370": f"0x{word_at('stage_fields_0370_0372', 4):04x}",
            "field_0372": f"0x{word_at('stage_fields_0370_0372', 4, 1):04x}",
        })
    for offset, expected, label in (
        (UNLINKED_ACTOR_COORD_SEQUENCE_DISPATCH,
         UNLINKED_ACTOR_COORD_SEQUENCE_DISPATCH_BYTES,
         "unlinked actor coordinate-state dispatch"),
        (UNLINKED_ACTOR_COORD_SEQUENCE_STATE4, UNLINKED_ACTOR_COORD_SEQUENCE_STATE4_BYTES,
         "unlinked actor coordinate-state handler"),
        (UNLINKED_ACTOR_COORD_SEQUENCE_SELECTORS,
         UNLINKED_ACTOR_COORD_SEQUENCE_SELECTORS_BYTES,
         "unlinked actor coordinate selectors"),
        (UNLINKED_ACTOR_COORD_SEQUENCE_TABLE, UNLINKED_ACTOR_COORD_SEQUENCE_TABLE_BYTES,
         "unlinked actor coordinate table"),
        (ACTOR_COORD_TASK_QUEUE_HELPER, ACTOR_COORD_TASK_QUEUE_HELPER_BYTES,
         "coordinate-handler asynchronous task launcher"),
        *[(offset, expected, f"shared task-helper callsite at {offset:#x}")
          for offset, expected in ACTOR_COORD_TASK_HELPER_OTHER_CALLS],
        (CLASS4_POOL_DISPATCHER, CLASS4_POOL_DISPATCHER_BYTES,
         "50-record class-4 pool dispatcher"),
        (CLASS4_POOL_HANDLER_POINTERS, CLASS4_POOL_HANDLER_POINTERS_BYTES,
         "class-4 handler pointer table prefix"),
        (CLASS4_BATCH_CREATE, CLASS4_BATCH_CREATE_BYTES,
         "class-4 fixed-batch allocator loop"),
        (CLASS4_BATCH_FIRST_RECORDS, CLASS4_BATCH_FIRST_RECORDS_BYTES,
         "class-4 fixed-batch first records"),
        (CLASS4_BATCH_HANDLER_SELECTOR_DISPATCH,
         CLASS4_BATCH_HANDLER_SELECTOR_DISPATCH_BYTES,
         "class-4 fixed-batch handler selector dispatch"),
        (CLASS4_BATCH_HANDLER_SELECTOR_TABLE,
         CLASS4_BATCH_HANDLER_SELECTOR_TABLE_BYTES,
         "class-4 fixed-batch handler selector table and adjacent bytes"),
    ):
        if program[offset:offset + len(expected)] != expected:
            raise ValueError(f"{label} at {offset:#x} changed")
    stage_actor_coordinate_candidates = []
    for actor_sequence, selector in enumerate(
            int.from_bytes(UNLINKED_ACTOR_COORD_SEQUENCE_SELECTORS_BYTES[index:index + 2],
                           "big")
            for index in range(0, len(UNLINKED_ACTOR_COORD_SEQUENCE_SELECTORS_BYTES), 2)):
        # State +4 multiplies this selector by two and reads six consecutive
        # words into global target, camera-setup, and player-position fields.
        row_offset = selector * 2
        words = [int.from_bytes(
            UNLINKED_ACTOR_COORD_SEQUENCE_TABLE_BYTES[row_offset + index:
                                             row_offset + index + 2], "big")
            for index in range(0, 12, 2)]
        stage_actor_coordinate_candidates.append({
            "actor_plus_33": actor_sequence,
            "table_selector": selector,
            "candidate_controller_target_xy": words[0:1] + words[3:4],
            "candidate_player_world_xy": [words[2], words[5]],
            "candidate_camera_parameters": [words[1], words[4]],
        })
    return {
        "controller_entry_offset": f"0x{STAGE_CONTROLLER_ENTRY:06x}",
        "controller_entry_instruction_bytes": STAGE_CONTROLLER_ENTRY_BYTES.hex(),
        "controller_entry_behavior": "calls 0x2884 to reset the 20-record actor pool; if bit 1 of a5+0x03c3 is clear, sets timer a5+0x7892 to 1, selects the stage-indexed input-script pointer from the table at 0xa626 into a5+0x7894, and advances controller state; if set, enters controller state 4",
        "controller_entry_stage_input_script_pointer_table": [
            f"0x{int.from_bytes(STAGE_INPUT_SCRIPT_POINTER_BYTES[index * 4:index * 4 + 4], 'big'):06x}"
            for index in range(6)
        ],
        "stage_input_script": {
            "stage_pointer_table_offset": f"0x{STAGE_INPUT_SCRIPT_POINTER_TABLE:06x}",
            "stage_pointer_table_bytes": STAGE_INPUT_SCRIPT_POINTER_BYTES.hex(),
            "record_stride": 4,
            "record_fields": ["byte ignored by reader", "input mask", "frame duration (word)"],
            "reader_offset": f"0x{STAGE_INPUT_SCRIPT_READER:06x}",
            "reader_instruction_bytes": STAGE_INPUT_SCRIPT_READER_BYTES.hex(),
            "reader_behavior": "on timer expiry reads input mask at record +1 and frame duration at +2, advances the stage script pointer by four bytes, and updates a5+0x006a",
            "dispatcher_offset": f"0x{STAGE_INPUT_SCRIPT_DISPATCH:06x}",
            "dispatcher_instruction_bytes": STAGE_INPUT_SCRIPT_DISPATCH_BYTES.hex(),
            "dispatcher_behavior": "when bit 1 of a5+0x03c3 is clear, advances the stage input script, computes newly pressed bits from current and previous masks at a5+0x006a/+0x006b, and writes the edge mask to a5-0x8e92",
            "interpretation": "stage-indexed timed input playback; this path does not establish ordinary player input or stage-completion semantics",
        },
        "controller_entry_gate": "bit 1 of shared latch a5+0x03c3; its normal Stage 0 value and active writer have not been established",
        "controller_latch": {
            "address": "a5+0x03c3",
            "values_observed": [0, 1, 2],
            "writers": [
                {
                    "offset": f"0x{offset:06x}",
                    "value": value,
                    "instruction_bytes": STAGE_CONTROLLER_FLAG_WRITER_BYTES[:3].hex()
                        + f"{value:02x}"
                        + STAGE_CONTROLLER_FLAG_WRITER_BYTES[4:].hex(),
                    "behavior": description,
                    "stage0_reachability": "not established",
                }
                for offset, value, description in STAGE_CONTROLLER_FLAG_WRITERS
            ],
            "interpretation": "This shared latch has multiple actor-state writers. Controller entry tests bit 1, and the main update at 0x9530 also checks the byte; it is not a standalone completion flag.",
        },
        "controller_dispatch_table_offset": f"0x{STAGE_CONTROLLER_TABLE:06x}",
        "controller_dispatch_offsets": [
            int.from_bytes(STAGE_CONTROLLER_TABLE_BYTES[index:index + 2], "big", signed=True)
            for index in range(0, len(STAGE_CONTROLLER_TABLE_BYTES), 2)
        ],
        "controller_dispatch_targets": [
            f"0x{STAGE_CONTROLLER_TABLE + int.from_bytes(STAGE_CONTROLLER_TABLE_BYTES[index:index + 2], 'big', signed=True):06x}"
            for index in range(0, len(STAGE_CONTROLLER_TABLE_BYTES), 2)
        ],
        "state_7_target": f"0x{STAGE_CONTROLLER_TABLE + int.from_bytes(STAGE_CONTROLLER_TABLE_BYTES[14:16], 'big', signed=True):06x}",
        "state_8_target": f"0x{STAGE_CONTROLLER_TABLE + int.from_bytes(STAGE_CONTROLLER_TABLE_BYTES[16:18], 'big', signed=True):06x}",
        "state_9_target": f"0x{STAGE_CONTROLLER_TABLE + int.from_bytes(STAGE_CONTROLLER_TABLE_BYTES[18:20], 'big', signed=True):06x}",
        "state_6_offset": f"0x{STAGE_STATE6:06x}",
        "state_6_instruction_bytes": STAGE_STATE6_BYTES.hex(),
        "state_6_behavior": "waits while byte a5+0x7891 is nonzero; clears a5+0x799c; increments state a5+0x0008; jumps to 0x6614",
        "state_5_offset": f"0x{STAGE_STATE5:06x}",
        "state_5_instruction_bytes": STAGE_STATE5_BYTES.hex(),
        "state_5_behavior": "counts down word a5+0x000a; on zero sets the asynchronous task lock, queues task 0x0c00 with parameter 3, waits for the lock to clear, advances to state 6, sets the lock again, and queues task 0x0c00 with parameter 6",
        "state_5_queued_tasks": [
            {"word": "0x0c00", "parameter": 3, "queue": "0x1f14 task queue", "wait_for_completion": True},
            {"word": "0x0c00", "parameter": 6, "queue": "0x1f14 task queue", "wait_for_completion": False},
        ],
        "state_4_offset": f"0x{STAGE_STATE4:06x}",
        "state_4_instruction_bytes": STAGE_STATE4_BYTES.hex(),
        "state_4_behavior": "when byte a5-0x717c is clear, calls 0xabd8, advances controller state to 5, and starts the 120-tick timer consumed by state 5; when nonzero, queues sound 0x30 and diverts through 0xaa9a/0xab5e",
        "stage_progress_gate": {
            "flag_byte": "a5-0x8e84",
            "initial_value": 3,
            "initialization_offset": f"0x{STAGE_PROGRESS_FLAG_INIT:06x}",
            "initialization_instruction_bytes": STAGE_PROGRESS_FLAG_INIT_BYTES.hex(),
            "counter_initializers": [
                {
                    "offset": f"0x{STAGE_PROGRESS_COUNTER0_INIT:06x}",
                    "instruction_bytes": STAGE_PROGRESS_COUNTER0_INIT_BYTES.hex(),
                    "writes": ["a5-0x8e70", "a5-0x8e74", "a5-0x8e73"],
                },
                {
                    "offset": f"0x{STAGE_PROGRESS_COUNTER1_INIT:06x}",
                    "instruction_bytes": STAGE_PROGRESS_COUNTER1_INIT_BYTES.hex(),
                    "writes": ["a5-0x8e78", "a5-0x8e80", "a5-0x8e7b"],
                },
            ],
            "counter_updates": [
                {
                    "offset": f"0x{STAGE_PROGRESS_FLAG0_GATE:06x}",
                    "instruction_bytes": STAGE_PROGRESS_COUNTER0_TICK_BYTES.hex(),
                    "counter": "a5-0x8e74",
                    "limit": "a5+0x03c6",
                    "step": "packed BCD +1 per controller update; clear flag bit 0 at the limit",
                },
                {
                    "offset": f"0x{STAGE_PROGRESS_FLAG1_GATE:06x}",
                    "instruction_bytes": STAGE_PROGRESS_COUNTER1_TICK_BYTES.hex(),
                    "counter": "a5-0x8e80",
                    "limit": "a5-0x8e7c",
                    "step": "packed BCD +1 per controller update; clear flag bit 1 at the limit",
                },
            ],
            "stage_sequence_index": {
                "current": "a5+0x0362",
                "maximum": "a5+0x0363",
                "display_copy": "a5+0x0365",
                "progress_offset": "a5+0x0364",
                "initialization_offset": f"0x{STAGE_PROGRESS_INDEX_INIT:06x}",
                "initialization_instruction_bytes": STAGE_PROGRESS_INDEX_INIT_BYTES.hex(),
                "progress_reset_offset": f"0x{STAGE_PROGRESS_INDEX_RESET:06x}",
                "progress_reset_instruction_bytes": STAGE_PROGRESS_INDEX_RESET_BYTES.hex(),
                "actor_advance_offset": f"0x{STAGE_PROGRESS_INDEX_ADVANCE:06x}",
                "actor_advance_instruction_bytes": STAGE_PROGRESS_INDEX_ADVANCE_BYTES.hex(),
                "actor_advance_behavior": "sound command 0x2d advances the current sequence index by 1 or 2 according to actor byte +33, clamps it to the maximum, mirrors it to +0x0365, then refreshes the Scroll 1 display",
                "actor_substate_dispatch": {
                    "dispatcher_offset": f"0x{STAGE_PROGRESS_ACTOR_DISPATCH:06x}",
                    "dispatcher_instruction_bytes": STAGE_PROGRESS_ACTOR_DISPATCH_BYTES.hex(),
                    "field": "object byte +2",
                    "table_offset": f"0x{STAGE_PROGRESS_ACTOR_SUBSTATE_TABLE:06x}",
                    "table_bytes": STAGE_PROGRESS_ACTOR_SUBSTATE_BYTES.hex(),
                    "substate_4_target": f"0x{STAGE_PROGRESS_ACTOR_SUBSTATE_TABLE + 0x24:06x}",
                },
                "actor_scheduler": {
                    "offset": f"0x{STAGE_PROGRESS_ACTOR_SCHEDULER:06x}",
                    "instruction_bytes": STAGE_PROGRESS_ACTOR_SCHEDULER_BYTES.hex(),
                    "record_base": "a5+0x6ac6",
                    "record_count": 20,
                    "record_stride": 64,
                    "active_test": "record byte +0",
                    "handler_selector": "record byte +32",
                    "handler_table_offset": f"0x{STAGE_PROGRESS_ACTOR_HANDLER_TABLE:06x}",
                    "handler_table_bytes": STAGE_PROGRESS_ACTOR_HANDLER_TABLE_BYTES.hex(),
                    "sequence_actor_selector": 6,
                    "sequence_actor_handler": "0x37dd0",
                    "selector_6_entry_routes": [
                        {
                            "offset": f"0x{STAGE_PROGRESS_ACTOR_SELECTOR6_SELF_ROUTE:06x}",
                            "instruction_bytes": STAGE_PROGRESS_ACTOR_SELECTOR6_SELF_ROUTE_BYTES.hex(),
                            "behavior": "an already active record changes its handler selector to 6 when the global frame cadence matches and its X is within 480 pixels of the camera",
                            "stage_0_reachability": "not established",
                        },
                        {
                            "offset": f"0x{STAGE_PROGRESS_ACTOR_SELECTOR6_CHILD_ROUTE:06x}",
                            "instruction_bytes": STAGE_PROGRESS_ACTOR_SELECTOR6_CHILD_ROUTE_BYTES.hex(),
                            "behavior": "a parent handler allocates a child, sets it active, selects handler 6, and copies parent fields +49, +50, and +52",
                            "stage_0_reachability": "not established",
                        },
                    ],
                    "provenance": "the scheduler invokes active class records; this validates dispatch, not the allocator or the sequence actor's Stage 0 creation trigger",
                },
            },
            "controller_behavior": "state 4 remains on its retry path while either low flag bit is set; once both clear it advances to the post-stage message sequence",
            "flag_0": {
                "comparison_offset": f"0x{STAGE_PROGRESS_FLAG0_GATE:06x}",
                "comparison_instruction_bytes": STAGE_PROGRESS_FLAG0_GATE_BYTES.hex(),
                "compares": "a5-0x8e74 against a5+0x03c6",
                "clear_offset": f"0x{STAGE_PROGRESS_FLAG0_CLEAR:06x}",
                "clear_instruction_bytes": STAGE_PROGRESS_FLAG0_CLEAR_BYTES.hex(),
                "clear_condition": "unsigned comparison at 0xaa9a finds a5-0x8e74 >= a5+0x03c6",
            },
            "flag_1": {
                "comparison_offset": f"0x{STAGE_PROGRESS_FLAG1_GATE:06x}",
                "comparison_instruction_bytes": STAGE_PROGRESS_FLAG1_GATE_BYTES.hex(),
                "compares": "a5-0x8e80 against a5-0x8e7c",
                "clear_offset": f"0x{STAGE_PROGRESS_FLAG1_CLEAR:06x}",
                "clear_instruction_bytes": STAGE_PROGRESS_FLAG1_CLEAR_BYTES.hex(),
                "clear_condition": "unsigned comparison at 0xab5e finds a5-0x8e80 >= a5-0x8e7c",
            },
            "native_port_status": "source flag initialization, counter initialization, BCD update, threshold comparisons, and sequence-index actor advancement are byte-validated; exact stage-event mapping and native gameplay integration are not established",
        },
        "transition_task_chain": {
            "dispatch_table_offset": f"0x{TASK_ROUTER_TABLE:06x}",
            "task_0c00_handler": f"0x{TASK_0C_ROUTER:06x}",
            "task_0c00_substate_zero": f"0x{TASK_0C_STATE0:06x}",
            "task_0c00_substate_zero_behavior": "queues task 0x1000 with the original completion parameter, waits for a5+0x02c0, clears CPS scroll/video memories, posts completion parameter+4, clears a5+0x7891, and returns through the asynchronous task scheduler",
            "task_1000_handler": f"0x{TASK_1000_ROUTER:06x}",
            "task_1000_substate_zero": f"0x{TASK_1000_STATE0:06x}",
            "task_1000_substate_zero_behavior": "sets the palette-transition busy flag; for each indexed palette group, subtracts 0x1000 from four consecutive palette words when the first word has a nonzero brightness nibble, otherwise sets that group's bit in a status mask; accumulates the masks until the source threshold reaches 0x1fa, signals a5+0x02c0, then clears the busy flag",
            "task_1000_fade_palette_ranges": ["0x91c000-0x91c3ff", "0x91c400-0x91c7ff", "0x91c800-0x91cbff", "0x91cc00-0x91cfff", "0x91d000-0x91d0ff", "0x91d400-0x91d4ff"],
            "native_meaning": "stage transition steps CPS palette brightness to zero and waits for the group-mask threshold, then clears old CPS video memory and releases the stage controller lock; CPS brightness zero renders at one-third intensity, so the old scene reaches minimum brightness before its display memory is cleared",
        },
        "state_7_offset": f"0x{STAGE_STATE7:06x}",
        "state_7_instruction_bytes": STAGE_STATE7_BYTES.hex(),
        "state_7_behavior": "queues sound words through 0x785e/0x786a, advances to state 8, sets a5+0x000a to 256, initializes stage-dependent actor and camera data; its continuation at 0xa832 loads the stage's final message index from 0xa8ae into byte a5-0x8e85",
        "state_7_actor_coordinate_table": {
            "offset": f"0x{STAGE_STATE7_ACTOR_TABLE:06x}",
            "bytes": STAGE_STATE7_ACTOR_TABLE_BYTES.hex(),
            "stride": 4,
            "records_xy": [
                [int.from_bytes(STAGE_STATE7_ACTOR_TABLE_BYTES[index:index + 2], "big"),
                 int.from_bytes(STAGE_STATE7_ACTOR_TABLE_BYTES[index + 2:index + 4], "big")]
                for index in range(0, len(STAGE_STATE7_ACTOR_TABLE_BYTES), 4)
            ],
            "interpretation": "state 7 indexes this table by stage counter and copies each X/Y word to actor record +6/+10; their low 10 bits are also copied to a5+0x32/+0x34. These are stage-sequence actor coordinates, not verified Willow spawn coordinates.",
        },
        "stage0_entry_tile_animation_tasks": {
            "setup_offset": f"0x{STAGE0_ENTRY_TILE_TASKS:06x}",
            "setup_instruction_bytes": STAGE0_ENTRY_TILE_TASKS_BYTES.hex(),
            "pool": "16-record stage tile-animation task pool at a5+0x7042, stride 64",
            "records": [
                {"active": 1, "source_data_pointer": "0x0ea660",
                 "field_32": 1, "destination_tile": 1,
                 "frame_hold_ticks": 4, "last_frame_index": 0,
                 "graphics_bank": 2},
                {"active": 1, "source_data_pointer": "0x0ea700",
                 "field_32": 1, "destination_tile": 3,
                 "frame_hold_ticks": 4, "last_frame_index": 1,
                 "graphics_bank": 2},
            ],
            "following_linked_child": {
                "setup_offset": f"0x{STAGE0_ENTRY_LINKED_CHILD:06x}",
                "instruction_bytes": STAGE0_ENTRY_LINKED_CHILD_BYTES.hex(),
                "pool": "20-record queue-class-8 pool at a5+0x42c6; allocator 0x2a1a",
                "behavior": 8,
                "behavior_handler": "class-8 dispatcher 0xe6f4 routes selector 8 through table 0xe71c to 0xfadc",
                "state0": {
                    "offset": f"0x{CLASS8_BEHAVIOR8_STATE0:06x}",
                    "instruction_bytes": CLASS8_BEHAVIOR8_STATE0_BYTES.hex(),
                    "effects": "queues sound command 0x25; enters state 2 with timer 62; initializes fields +42=20, +48/+52=80, +50/+54=160; installs animation pointer 0xf4d0",
                },
                "state2": {
                    "offset": f"0x{CLASS8_BEHAVIOR8_STATE2:06x}",
                    "instruction_bytes": CLASS8_BEHAVIOR8_STATE2_BYTES.hex(),
                    "effects": "calls visibility helper 0x2244, decrements timer, enters state 4 on expiry; every other global frame dispatches shared event-list updates 0x709c/0x70ae",
                },
                "event_scan": {
                    "byte_validation": "passed",
                    "regions": [{"offset": f"0x{address:06x}",
                                 "bytes": expected.hex()}
                                for address, expected in event_regions],
                    "list_entries": [
                        {"entry": "0x00709c", "count_a5_offset": "0x180b",
                         "pointer_a5_offset": "0x1830"},
                        {"entry": "0x0070ae", "count_a5_offset": "0x180c",
                         "pointer_a5_offset": "0x1832"},
                    ],
                    "candidate_gate": "candidate byte +0 must equal 1",
                    "map_selection": "caller byte +0x20 nonzero selects 0x7102; zero selects 0x7122; behavior 8 therefore uses 0x7102",
                    "candidate_callbacks": behavior8_callbacks,
                    "bounds_source": "0x3b0d2 + (word at candidate animation pointer +6)*8",
                    "overlap": "unsigned 16-bit X/Y comparisons; equality is accepted; caller bounds at +48/+50/+52/+54 are combined with candidate bounds",
                    "callback_effects": {
                        "0x007152": "on overlap sets candidate +0=2, +2=4, clears +3/+4, and ends the scan",
                        "0x0071ee": "on overlap sets candidate +0=2, +2=6, and ends the scan",
                        "0x00719a": "used by the zero-behavior caller map for candidate behavior 8; on overlap optionally marks a linked flag, sets candidate +0=2, and ends the scan",
                    },
                    "reachability_limit": "the filter does not establish which candidates are populated in each event list during Stage 0",
                },
                "state4_release": {
                    "offset": f"0x{CLASS8_BEHAVIOR8_RELEASE:06x}",
                    "instruction_bytes": CLASS8_BEHAVIOR8_RELEASE_BYTES.hex(),
                    "effects": "returns the actor record through queue-class-8 free routine 0x2b20 on the next scheduler update",
                },
                "position_xy": [1216, 64],
                "native_port_status": "source creation, dispatch, state-0 initialization, and state-2 update bytes are validated; event-list table selection and callback effects are also byte-validated; runtime lifecycle, event-list population, and render path remain unimplemented",
            },
        },
        "stage1_entry_tile_animation_tasks": {
            "setup_offset": f"0x{STAGE1_ENTRY_TILE_TASK_SETUP:06x}",
            "setup_instruction_bytes": STAGE1_ENTRY_TILE_TASK_SETUP_BYTES.hex(),
            "gate": "controller state 7 creates these only when stage counter a5+0x0344 equals 1",
            "pool": "16-record stage tile-animation task pool at a5+0x7042, stride 64",
            "task_dispatcher_offset": f"0x{STAGE_ANIMATION_TASK_DISPATCH:06x}",
            "task_dispatcher_instruction_bytes": STAGE_ANIMATION_TASK_DISPATCH_BYTES.hex(),
            "task_handler": "0x000c0a",
            "task_state0_offset": f"0x{STAGE_ANIMATION_TASK_STATE0:06x}",
            "task_state0_instruction_bytes": STAGE_ANIMATION_TASK_STATE0_BYTES.hex(),
            "task_state2_offset": f"0x{STAGE_ANIMATION_TASK_STATE2:06x}",
            "task_state2_instruction_bytes": STAGE_ANIMATION_TASK_STATE2_BYTES.hex(),
            "graphics_destination_formula": "0x91c000 + graphics_bank*0x400 + destination_tile*32; each task copies one 32-byte 8x8 four-plane tile frame",
            "records": [
                {
                    "active": 1,
                    "source_data_pointer": "0x0ea120",
                    "graphics_bank": 2,
                    "destination_tile": 4,
                    "graphics_destination": "0x91c880",
                    "frame_hold_ticks": 8,
                    "last_frame_index": 4,
                    "frame_count": 5,
                    "frame_source_bytes": [program[0x0ea120 + frame * 32:0x0ea140 + frame * 32].hex() for frame in range(5)],
                },
                {
                    "active": 1,
                    "source_data_pointer": "0x0ea1c0",
                    "graphics_bank": 2,
                    "destination_tile": 5,
                    "graphics_destination": "0x91c8a0",
                    "frame_hold_ticks": 8,
                    "last_frame_index": 4,
                    "frame_count": 5,
                    "frame_source_bytes": [program[0x0ea1c0 + frame * 32:0x0ea1e0 + frame * 32].hex() for frame in range(5)],
                },
            ],
            "behavior": "the entry setup allocates animated tile descriptors, not screen actors; the handler targets CGC graphics memory and writes no world coordinates",
            "initial_frame_note": "state 0 initializes the destination pointer, frame index 0, and 8-tick hold; state 2 advances and copies the next 32-byte frame on each eligible expiry",
        },
        "separate_class2_actor_pool_behavior0": {
            "pool": "8-record actor pool at a5+0x7490, stride 128; separate from the Stage tile-animation pool at a5+0x7042",
            "dispatcher_offset": "0x026da6",
            "behavior_0_handler": "0x026de8",
            "state0_offset": f"0x{ACTOR_POOL8_BEHAVIOR0_STATE0:06x}",
            "state0_instruction_bytes": ACTOR_POOL8_BEHAVIOR0_STATE0_BYTES.hex(),
            "state0_stream": follow_stream(program, 0x027222),
            "state2_offset": f"0x{ACTOR_POOL8_BEHAVIOR0_STATE2:06x}",
            "state2_instruction_bytes": ACTOR_POOL8_BEHAVIOR0_STATE2_BYTES.hex(),
            "state2_stream": follow_stream(program, 0x027318, full_records=True),
            "stage1_entry_relationship": "no reference links the Stage 1 tile-animation task descriptors at 0xa848 to this separate actor pool; do not treat its streams as the actors created at 0xa848",
        },
        "event_index_table_offset": f"0x{STAGE_EVENT_INDEX_TABLE:06x}",
        "event_index_table": list(STAGE_EVENT_INDEX_BYTES),
        "event_index_table_normal_route": list(STAGE_EVENT_INDEX_BYTES[:6]),
        "event_initializer_offset": f"0x{STAGE_EVENT_INIT:06x}",
        "event_initializer_instruction_bytes": STAGE_EVENT_INIT_BYTES.hex(),
        "event_index_setup_offset": f"0x{STAGE_EVENT_INDEX_SETUP:06x}",
        "event_index_setup_instruction_bytes": STAGE_EVENT_INDEX_SETUP_BYTES.hex(),
        "event_index_setup_behavior": "stage setup at 0xa832 loads the stage-specific starting message index from table 0xa8ae into byte a5-0x8e85",
        "event_initializer_behavior": "controller state 8 at 0xa8b4 advances a5+0x0008 to state 9, sets word a5+0x000a to 60, uses the current countdown index at a5-0x8e85 to select a byte from the stage message record at 0xa8ea, then dispatches that message ID through 0x15cc",
        "event_dispatch_table_offset": f"0x{STAGE_EVENT_DISPATCH_TABLE:06x}",
        "event_dispatch_table_bytes": STAGE_EVENT_DISPATCH_BYTES.hex(),
        "event_dispatch_table_stage_records": [
            list(STAGE_EVENT_DISPATCH_BYTES[index * 4 : index * 4 + 4])
            for index in range(len(STAGE_EVENT_DISPATCH_BYTES) // 4)
        ],
        "event_dispatch_table_stage_message_ids": [
            [f"0x{event_id:02x}" for event_id in
             STAGE_EVENT_DISPATCH_BYTES[index * 4 : index * 4 + 4]
             if event_id]
            for index in range(len(STAGE_EVENT_DISPATCH_BYTES) // 4)
        ],
        "event_display_sequences_by_stage": [
            [f"0x{event_id:02x}" for event_id in reversed(
                [value for value in STAGE_EVENT_DISPATCH_BYTES[index * 4:index * 4 + 4]
                 if value][:STAGE_EVENT_INDEX_BYTES[index] + 1])]
            for index in range(6)
        ],
        "event_task_launch_offset": f"0x{STAGE_EVENT_TASK_LAUNCH:06x}",
        "event_task_launch_behavior": "stage state 8 clears D1, loads the selected message ID into D1's low byte, then creates asynchronous task type 0x0c00 at slot offset 0x00c0; trap 0x15d0 stores the whole D1 word at task +0x10, while argument 0x031b is stored at +0x12",
        "event_handler_dispatcher_offset": f"0x{STAGE_EVENT_HANDLER:06x}",
        "event_handler_dispatch_table_offset": f"0x{STAGE_EVENT_HANDLER_TABLE:06x}",
        "event_handler_dispatch_table_prefix": STAGE_EVENT_HANDLER_TABLE_BYTES.hex(),
        "event_task_byte_lane": {
            "stage_event_register_setup": "D1 is cleared at 0xa8c8, loaded with the countdown index from a5-0x8e85 at 0xa8ca, then the selected message ID at 0xa8ce",
            "allocator_word_store": "TRAP #0 handler 0x15d0 stores D1.W at task +0x10",
            "handler_byte_read": "entry 0x78f0 reads task byte +0x10, the high byte of that stored word, as a handler type",
            "payload_byte": "The low byte at task +0x11 holds the selected message ID and is consumed by handler type 0 at 0x7912",
            "scheduler_pointer_store": "scheduler instruction at 0x1506 stores selected slot A0 at a5-0x7dfc",
            "handler_pointer_load": "entry 0x78f0 loads A6 from a5-0x7dfc",
            "same_record_result": "The scheduler and handler use the same selected-slot pointer; word 0x00xx selects generic text handler type 0 while byte +0x11 carries the message ID",
            "status": "verified two-byte handler-type and event-payload layout",
        },
        "event_handler_dispatch_note": "entry 0x78f0 dispatches on task byte +0x10, the high byte of the word stored by trap 0x15d0. Stage state 8 writes handler type zero and the message ID in the low byte at +0x11. Type zero selects the text parser at 0x7912, which reads +0x11, resolves a fixed-stride record in ROM at 0x70000 + message_id*0xa8, treats '&' as a line break and '/' as the end marker, and writes character tile codes beginning at 0x7000. These are narrative records, not enemy spawn IDs",
        "event_text_parser_offset": f"0x{STAGE_EVENT_TEXT_ENTRY:06x}",
        "event_text_record_lookup_offset": f"0x{STAGE_EVENT_TEXT_LOOKUP:06x}",
        "event_text_tile_writer_offset": f"0x{STAGE_EVENT_TEXT_WRITE:06x}",
        "event_text_record_base": f"0x{STAGE_EVENT_RECORD_BASE:06x}",
        "event_text_record_stride": f"0x{STAGE_EVENT_RECORD_STRIDE:04x}",
        "event_messages": event_messages,
        "event_timer": "controller state 9 decrements word a5+0x000a once per frame; at zero it decrements the message index byte at a5-0x8e85. If the result is nonnegative, it clears the text tilemap rows and loops back to state 8, which displays the next lower record index; if negative, it advances the stage",
        "stage1_event_route": {
            "gate_offset": f"0x{STAGE1_EVENT_ROUTE:06x}",
            "gate_bytes": STAGE1_EVENT_ROUTE_BYTES.hex(),
            "condition": "after the event sequence ends, stage counter a5+0x0344 equal to 1 dispatches to 0x0bc4",
            "dispatcher_offset": f"0x{STAGE1_EVENT_DISPATCH:06x}",
            "dispatcher_bytes": STAGE1_EVENT_DISPATCH_BYTES.hex(),
            "active_records": 16,
            "record_stride": 64,
            "handler_table_offset": f"0x{STAGE1_EVENT_HANDLER_TABLE:06x}",
            "handler_addresses": [
                f"0x{int.from_bytes(STAGE1_EVENT_HANDLER_TABLE_BYTES[i:i+4], 'big'):06x}"
                for i in range(0, len(STAGE1_EVENT_HANDLER_TABLE_BYTES), 4)
            ],
            "meaning": "stage-counter-1 actor update path; it does not establish a player spawn coordinate",
        },
        "event_display_ticks_per_message": 60,
        "gate_offset": f"0x{STAGE_TRANSITION_GATE:06x}",
        "gate_instruction_bytes": STAGE_TRANSITION_GATE_BYTES.hex(),
        "gate_condition": "a5+0x02a6 must be clear; word a5+0x000a must decrement to zero; byte a5-0x8e85 then decrements below zero",
        "routine_offset": f"0x{STAGE_TRANSITION:06x}",
        "instruction_bytes": STAGE_TRANSITION_BYTES.hex(),
        "normal_route_stage_values": list(range(6)),
        "stage_values": [
            {"from": value, "to": value if value == 5 else value + 1 if value < 5 else 0}
            for value in range(8)
        ],
        "advance": "0 through 4 increment by one; 5 remains 5",
        "unexpected_values": "values above 5 increment then reset to 0 when the result exceeds 6",
        "stage_setup_calls": ["0x002e1c", "0x002d80", "0x002f8a", "0x0381bc"],
        "player_initialization_routes": {
            "stage0": {
                "gate": "stage counter 0, a5+0x0348 clear, and a5+0x0344 clear",
                "player_world_xy": [192, 512],
                "companion_world_xy": [192, 512],
                "verified_by": "0x00a114 writes player and companion coordinates after 0x2dfe",
            },
            "stage3_special": {
                "gate": "stage counter 3 and a5+0x03a0 clear",
                "player_world_xy": [192, 4096],
                "companion_world_xy": [192, 4096],
                "additional_object_world_xy": [-96, 4200],
                "verified_by": "0x00a14a writes both coordinate pairs and initializes the actor at 0x00a19c",
            },
            "other_stage_counters": {
                "behavior": "sets a5+0x0348, advances controller state, clears timers, and returns without writing player coordinates",
                "verified_by": "0x00a220",
                "spawn_coordinates": "not established by this initializer",
            },
        },
        "stage_setup_parameters": {
            "tables": {
                name: {
                    "offset": f"0x{offset:06x}",
                    "bytes": expected.hex(),
                }
                for name, (offset, expected) in STAGE_SETUP_PARAMETER_TABLES.items()
            },
            "rows": stage_setup_parameters,
            "interpretation": "source stage-indexed words loaded by routines 0x2d80 and 0x2f8a; their use as native player spawn coordinates has not been established",
        },
        "unlinked_actor_coordinate_sequence": {
            "state_dispatch_offset": f"0x{UNLINKED_ACTOR_COORD_SEQUENCE_DISPATCH:06x}",
            "state_dispatch_bytes": UNLINKED_ACTOR_COORD_SEQUENCE_DISPATCH_BYTES.hex(),
            "state_4_handler_offset": f"0x{UNLINKED_ACTOR_COORD_SEQUENCE_STATE4:06x}",
            "state_4_handler_bytes": UNLINKED_ACTOR_COORD_SEQUENCE_STATE4_BYTES.hex(),
            "task_queue_helper_offset": f"0x{ACTOR_COORD_TASK_QUEUE_HELPER:06x}",
            "task_queue_helper_instruction_bytes": ACTOR_COORD_TASK_QUEUE_HELPER_BYTES.hex(),
            "queued_task": {
                "lock_byte": "a5+0x7891",
                "task_word": "0x0c00",
                "parameter": 3,
                "enqueue_helper": "0x1f14",
                "wait_helper": "0x1750",
            },
            "controller_state5_overlap": {
                "controller_state5_offset": f"0x{STAGE_STATE5:06x}",
                "controller_state5_instruction_bytes": STAGE_STATE5_BYTES.hex(),
                "shared_helper": "0x1e88",
                "shared_task": "0x0c00",
                "shared_parameter": 3,
                "other_call_sites": [
                    {"offset": f"0x{offset:06x}",
                     "instruction_bytes": expected.hex()}
                    for offset, expected in ACTOR_COORD_TASK_HELPER_OTHER_CALLS
                ],
                "interpretation": "this handler and controller state 5 both call the same helper to wait for task 0x0c00 parameter 3 to complete. The helper also appears in other flows, so this establishes a shared task operation, not that the actor is created or dispatched by the normal stage-transition route",
            },
            "state_4_entry_behavior": (
                "increments actor substate by two, sends sound command "
                "0x10fb through 0x786a, calls 0x1e88 to queue asynchronous "
                "task 0x0c00 with parameter 3, then selects "
                "the actor+33 coordinate selector and writes its row into "
                "the shared target, camera, and player-coordinate fields; "
                "the selector owner's creation path is not linked to the "
                "normal stage-transition controller"
            ),
            "selector_table_offset": f"0x{UNLINKED_ACTOR_COORD_SEQUENCE_SELECTORS:06x}",
            "selector_table_bytes": UNLINKED_ACTOR_COORD_SEQUENCE_SELECTORS_BYTES.hex(),
            "coordinate_table_offset": f"0x{UNLINKED_ACTOR_COORD_SEQUENCE_TABLE:06x}",
            "coordinate_table_bytes": UNLINKED_ACTOR_COORD_SEQUENCE_TABLE_BYTES.hex(),
            "rows": stage_actor_coordinate_candidates,
            "standard_class4_pool_route": {
                "dispatcher_offset": f"0x{CLASS4_POOL_DISPATCHER:06x}",
                "dispatcher_instruction_bytes": CLASS4_POOL_DISPATCHER_BYTES.hex(),
                "handler_pointer_table_offset": f"0x{CLASS4_POOL_HANDLER_POINTERS:06x}",
                "handler_pointer_table_prefix": CLASS4_POOL_HANDLER_POINTERS_BYTES.hex(),
                "batch_allocator_offset": f"0x{CLASS4_BATCH_CREATE:06x}",
                "batch_allocator_instruction_bytes": CLASS4_BATCH_CREATE_BYTES.hex(),
                "batch_first_records_offset": f"0x{CLASS4_BATCH_FIRST_RECORDS:06x}",
                "batch_first_records_bytes": CLASS4_BATCH_FIRST_RECORDS_BYTES.hex(),
                "handler_selector_dispatch_offset": f"0x{CLASS4_BATCH_HANDLER_SELECTOR_DISPATCH:06x}",
                "handler_selector_dispatch_bytes": CLASS4_BATCH_HANDLER_SELECTOR_DISPATCH_BYTES.hex(),
                "handler_selector_table_offset": f"0x{CLASS4_BATCH_HANDLER_SELECTOR_TABLE:06x}",
                "handler_selector_table_adjacent_bytes": CLASS4_BATCH_HANDLER_SELECTOR_TABLE_BYTES.hex(),
                "selector_6_table_word": f"0x{int.from_bytes(CLASS4_BATCH_HANDLER_SELECTOR_TABLE_BYTES[12:14], 'big'):04x}",
                "selector_6_computed_jump_address": f"0x{CLASS4_BATCH_HANDLER_SELECTOR_TABLE + int.from_bytes(CLASS4_BATCH_HANDLER_SELECTOR_TABLE_BYTES[12:14], 'big'):06x}",
                "selector_6_dispatch_status": "the dispatch calculation reads into handler instructions and computes an odd 68000 instruction address; this does not establish a valid actor-entry route",
                "interpretation": "the fixed batch writes word 0x0106 at actor +32; the ordinary 50-record dispatcher reads byte +32 (1) and routes that record to 0x00115ee through the pointer table. That handler reads byte +33 (6), outside the four leading selector offsets, so the batch does not provide a valid route to the separate 0x2d7d0 substate table or prove the creator of handler 0x2d89a",
            },
            "interpretation": "an actor state-4 handler indexes the selector table with actor +33, then reads six words into controller target, camera setup, and player world-coordinate fields. Its asynchronous task call matches controller state 5, but the actor creation and dispatch path is unlinked; rows remain candidate coordinates and are not native spawn data",
        },
        "transition_counter_update": {
            "routine_offset": f"0x{STAGE_TRANSITION_COUNTER_ROUTINE:06x}",
            "routine_bytes": STAGE_TRANSITION_COUNTER_ROUTINE_BYTES.hex(),
            "stage_table_offset": f"0x{STAGE_TRANSITION_COUNTER_TABLE:06x}",
            "stage_table_bytes": STAGE_TRANSITION_COUNTER_TABLE_BYTES.hex(),
            "stage_table_values": list(STAGE_TRANSITION_COUNTER_TABLE_BYTES[:6]),
            "behavior": "subtracts four from a5+0x0346 with floor zero, looks up the stage byte, adds a5+0x0291 with byte overflow saturating to 0xff, then raises a5+0x0346 to that lower bound if needed",
            "player_position_effect": "does not write player world-position fields +0x0408/+0x040c",
        },
        "reset_after_setup": ["word a5+0x0004", "word a5+0x0008", "word a5+0x000c", "byte a5+0x799c"],
        "map_grids_6_7_reached_by_normal_route": False,
    }


def front_end_attract_trace(program: bytes) -> dict[str, object]:
    """Validate the ordinary pre-game state machine's first visible task."""
    animation_select_address = 0x1F30
    animation_select_bytes = bytes.fromhex(
        "102e000f4880d0003030000041f000002d480014"
        "3d680008001c4e75")
    if program[animation_select_address:
               animation_select_address + len(animation_select_bytes)] \
            != animation_select_bytes:
        raise ValueError("shared actor animation selector at 0x1f30 changed")
    # The routine indexes a signed 16-bit relative-offset table with actor
    # byte +15, then adds that offset to the actor's data-table base. The
    # These even-offset entries occur in source-backed attract actor data
    # blocks. Their addresses resolve under 0x1f30's lookup rule, but the
    # actors' live +15 values have not yet been observed.
    indexed_actor_targets = []
    for actor_base, selectors in (
            (0x2EA24, (0x50, 0x51)), (0x2F24C, (0x68, 0x69))):
        entries = []
        for selector in selectors:
            word_address = actor_base + selector * 2
            relative_offset = int.from_bytes(
                program[word_address:word_address + 2], "big", signed=True)
            if relative_offset == 0:
                continue
            target = actor_base + relative_offset
            if target < 0 or target & 1 or target + 12 > len(program):
                raise ValueError(
                    f"attract actor selector {selector:#x} points outside ROM")
            record_walk = follow_stream(program, target, selector)
            entries.append({
                "candidate_selector": f"0x{selector:02x}",
                "table_word_address": f"0x{word_address:06x}",
                "relative_offset": f"0x{relative_offset & 0xffff:04x}",
                "resolved_address": f"0x{target:06x}",
                "word_at_target_plus_8": f"0x{int.from_bytes(program[target + 8:target + 10], 'big'):04x}",
                "target_bytes": program[target:target + 16].hex(),
                "record_walk": record_walk,
                "frame_candidates": source_sprite_frame_candidates(record_walk),
            })
        indexed_actor_targets.append({
            "actor_data_base": f"0x{actor_base:06x}",
            "selectors": [f"0x{selector:02x}" for selector in selectors],
            "nonzero_entries": entries,
        })
    attract_scene_sites = (
        (0x8266, bytes.fromhex("302d0004323b00064efb1002"),
         "attract scene main-state-2 dispatcher"),
        (0x8272, bytes.fromhex("000a02a806100a420c52"),
         "attract scene state table"),
        (0x827C, bytes.fromhex("302d0008323b00064efb1002"),
         "attract scene substate dispatcher"),
        (0x8288, bytes.fromhex("000c012801f802240258027e"),
         "attract scene substate table"),
        (0x8294, bytes.fromhex("302d000c323b00064efb1002"),
         "attract scene setup dispatcher"),
        (0x82A0, bytes.fromhex("0004001e"),
         "attract scene setup-state table"),
        (0x82A4, bytes.fromhex(
            "546d000c3b7c13b8004a422d03434eb80ac4"
            "4eb8090e4ef82648"),
         "attract scene palette and display setup"),
        (0x82BE, bytes.fromhex(
            "546d0008426d000c3b7c00b4000a4ded0402"
            "303c001c227c0002dbf82d7100000042"
            "3d7c040000063b7c000000323d7c0000000a"
            "3b7c000000341d7c000500484eb8548c"
            "1d7c000900484eb8548c4eb82c86"),
         "attract scene timer and first actor initialization"),
        (0x83B0, bytes.fromhex(
            "536d000a660000be546d00083b7c003c000a"
            "4ded0482303c001c227c0002eb242d7100000042"
            "70003d4000063b4000363d40000a3b400038"
            "1d7c000500484eb855321d7c000900484eb85532"
            "207c0091790c20bc0a0400044eb82c86"),
         "attract scene delayed actor initialization"),
        (0x8390, bytes.fromhex(
            "323c00044eb81e54303c1014323c00044eb81e54"
            "303c1016323c00044ef81e54"),
         "attract scene asynchronous task submissions"),
        (0x8468, bytes.fromhex("303c1018323c00044ef81e54"),
         "attract scene delayed task submission"),
        (0x50AC, bytes.fromhex(
            "001a0056008c00f201ea0260028a02b402de030a"
            "03320360038e"),
         "native front-end palette task dispatch table"),
        (0x53B6, bytes.fromhex(
            "4aad02a8671e343c100041f90091c00043ed02a8"
            "6100ff327000102e00134eb8175060dc4ef8165a"),
         "palette task 0x1012 handler"),
        (0x53DE, bytes.fromhex(
            "006d0020004a4aad02ac671e343c100041f90091c400"
            "43ed02ac6100ff047000102e00134eb8175060d64ef8165a"),
         "palette task 0x1014 handler"),
        (0x540C, bytes.fromhex(
            "006d0010004a4aad02b0671e343c100041f90091c800"
            "43ed02b06100fed67000102e00134eb8175060d64ef8165a"),
         "palette task 0x1016 handler"),
        (0x543A, bytes.fromhex(
            "006d0008004a4aad02b4671e343c100041f90091cc00"
            "43ed02b46100fea87000102e00134eb8175060d64ef8165a"),
         "palette task 0x1018 handler"),
        (0x2DC14, bytes.fromhex("0002ea24"),
         "first attract actor script pointer"),
        (0x2EB40, bytes.fromhex("0002f24c"),
         "second attract actor script pointer"),
        (0x2EAC4, bytes.fromhex("50005100000000000000000000000000"),
         "first attract actor opaque data block A"),
        (0x2EAE4, bytes.fromhex("004d004e000000000000000000000000"),
         "first attract actor opaque data block B"),
        (0x2EB04, bytes.fromhex("404142434445464748494a4b4c00004f"),
         "first attract actor opaque data block C"),
        (0x2EB14, bytes.fromhex("1b1c3f3f191a3b3c3d3e1d1e00000000"),
         "first attract actor opaque data block D"),
        (0x2F31C, bytes.fromhex("20002100222300000000000000000000"),
         "second attract actor opaque data block A"),
        (0x2F33C, bytes.fromhex("0506090a07081e1f0000000000000000"),
         "second attract actor opaque data block B"),
    )
    for address, expected, label in attract_scene_sites:
        if program[address:address + len(expected)] != expected:
            raise ValueError(f"{label} bytes at {address:#x} changed")
    startup_copy_sites = (
        (0x0A92, bytes.fromhex(
            "7000102d0343e74841fb00e63b58004c3b58004e"
            "3b5800503b5000524e75"), "startup palette selector"),
        (0x0AC4, bytes.fromhex(
            "61cc6100000e610000386100002460000040"),
         "startup palette copy dispatcher"),
        (0x0AD6, bytes.fromhex(
            "70004a2d03436704303c040043f90091c800"
            "207c000e9920d1c06000ff06"),
         "startup conditional palette copy"),
        (0x0AF4, bytes.fromhex(
            "43f90091c400207c000e72a06000fef6"),
         "startup palette copy to CPS work RAM"),
        (0x0B04, bytes.fromhex(
            "43f90091cc00207c000eb7006000fee6"),
         "startup palette copy to CPS work RAM"),
        (0x0B14, bytes.fromhex(
            "43f90091c200207c000e5280323c000f6000fed6"),
         "startup half-palette copy to CPS work RAM"),
        (0x09F8, bytes.fromhex(
            "323c001f" + "22d8" * 8 + "51c9ffee4e75"),
         "startup palette-copy loop"),
        (0x51E8, bytes.fromhex(
            "42ad8a3e41f90091c00043ed02a83e3c001f614e"
            "41f90091c40043ed02ac3e3c001f613e"
            "41f90091c80043ed02b03e3c001f612e"
            "41f90091cc0043ed02b43e3c001f611e"
            "41f90091d00043ed02b83e3c0007610e"
            "41f90091d40043ed02bc3e3c00076114"
            "51cffffc2011d1ad8a3e4e75"),
         "pre-game palette-group scan"),
        (0x5260, bytes.fromhex(
            "30104a426b02d0420240f00067123c3c0003"
            "d558d558d558d55851cefff64e7541e80020"
            "2a114a426a060fc522854e750f8522854e75"),
         "pre-game palette fade and brightness-mask update"),
    )
    for address, expected, label in startup_copy_sites:
        if program[address:address + len(expected)] != expected:
            raise ValueError(f"{label} bytes at {address:#x} changed")
    checks = (
        (0x80F2, bytes.fromhex(
            "302d0000323b00184ebb10144eb81e726100001a"
            "303c00014eb8175060ca"), "front-end main state dispatcher"),
        (0x8110, bytes.fromhex(
            "010e011e0156011e0f6e011e0fc0"),
         "front-end main-state relative table"),
        (0x821E, bytes.fromhex(
            "546d00004eb82de6422d03434ef80ac4"),
         "front-end initial state setup"),
        (0x822E, bytes.fromhex(
            "302d0004323b00064efb100200040018"),
         "front-end scripted task state dispatcher"),
        (0x823E, bytes.fromhex(
            "546d0004426d02c0303c1000323c00034ef81e54"),
         "front-end initial system task"),
        (0x8252, bytes.fromhex(
            "4a6d02c0670c546d0000426d00044ef865a44e75"),
         "front-end text-task completion gate"),
        (0x1E54, bytes.fromhex(
            "3400e04a024000ff207b20d64ef8160a"),
         "front-end system task dispatch"),
        (0x1E44, bytes.fromhex("0000509c"),
         "front-end task type 0x10 callback pointer"),
        (0x509C, bytes.fromhex("2c6d8204302e0010323b00064efb1002"),
         "front-end task state dispatcher"),
        (0x50C6, bytes.fromhex(
            "4a6d02c066321b7c00017890343cf00061000110"),
         "front-end task's first state and map update"),
        (0x65A4, bytes.fromhex(
            "41f900910000303c03ff323c7020740061000048"
            "41f900918000303c03ff323c0000740061000034"
            "41f900914000303c03ff323c0a00740061000020"),
         "post-task CPS tilemap reset"),
    )
    for address, expected, label in checks:
        if program[address:address + len(expected)] != expected:
            raise ValueError(f"{label} bytes at {address:#x} changed")
    return {
        "main_dispatcher": "0x0080f2",
        "state_table_address": "0x008110",
        "state_zero_handler": "0x00821e",
        "initialization": "advances the main state and calls 0x2de6 before entering the scripted controller at 0x822e",
        "first_scripted_task": {
            "state_handler": "0x00823e",
            "task_code": "0x1000",
            "style": 3,
            "submission": "0x1e54",
            "task_callback": "0x00509c selected by task type 0x10 at 0x1e44",
            "task_state_zero": "0x0050c6 updates Scroll 1 map state while a5+0x02c0 is clear",
            "wait_handler": "0x008252",
            "completion_gate": "a5+0x02c0",
            "next_transition": "advances the main state and enters 0x65a4 when a5+0x02c0 is nonzero",
            "map_reset": "0x65a4 resets Scroll 1, Scroll 3, and Scroll 2 tilemap pages",
        },
        "startup_palette_copies": {
            "initializer": "0x0ac4 calls the four-word setup selector at 0x0a92, then copies four color blocks into CPS palette RAM",
            "setup_selector": {
                "routine": "0x0a92",
                "selector_field": "a5+0x0343",
                "table": "four 16-bit setup values per entry at 0x0a82",
            },
            "copies": [
                {"source": "0x0e9920", "alternate_source": "0x0e9d20",
                 "destination": "0x91c800", "bytes": 1024,
                 "condition": "a5+0x0343 is nonzero selects source +0x400"},
                {"source": "0x0e72a0", "destination": "0x91c400",
                 "bytes": 1024},
                {"source": "0x0eb700", "destination": "0x91cc00",
                 "bytes": 1024},
                {"source": "0x0e5280", "destination": "0x91c200",
                 "bytes": 512},
            ],
            "interpretation": "color data copied into the palette address range scanned by 0x51e8 during task 0x1000; likely pre-game fade palettes, while their complete display use is unresolved",
            "fade_scan": {
                "routine": "0x51e8",
                "palette_ranges": ["0x91c000-0x91c3ff", "0x91c400-0x91c7ff", "0x91c800-0x91cbff", "0x91cc00-0x91cfff", "0x91d000-0x91d0ff", "0x91d400-0x91d4ff"],
                "groups": "four 0x400-byte blocks plus two 0x100-byte blocks",
                "mask_fields": "a5+0x02a8 through a5+0x02bf",
                "update_helper": "0x5260 tests each group's first color word, changes four color words per update, and updates the mask"
            },
            "copy_helper": "0x09f8 copies eight longwords per iteration for 32 iterations (0x400 bytes); 0x0b14 selects 16 iterations (0x200 bytes)",
        },
        "attract_scene": {
            "main_state_handler": "0x8266; entered after the initial palette fade completes and CPS tilemap reset runs",
            "state_flow": "dispatches a5+0x0004, +0x0008, and +0x000c through tables at 0x8272, 0x8288, and 0x82a0",
            "palette_setup": "0x82a4 writes 0x13b8 to a5+0x004a, clears a5+0x0343, reruns 0x0ac4 and 0x090e, then tail-calls 0x2648",
            "first_timed_actor_setup": {
                "handler": "0x82be",
                "inter_actor_wait_ticks": 180,
                "actor_record": {"record": "a5+0x0402", "data_pointer_source": "0x2dbf8+0x001c", "data_pointer": "0x2ea24", "initial_x": 0x0400, "initial_y": 0},
                "scroll_clear_selectors": [5, 9]
            },
            "async_tasks": [
                {"task": "0x1012", "style": 4, "palette_region": "0x91c000-0x91c3ff", "mask": "a5+0x02a8", "behavior": "fades the selected palette groups up with 0x1000 brightness increments"},
                {"task": "0x1014", "style": 4, "palette_region": "0x91c400-0x91c7ff", "mask": "a5+0x02ac", "behavior": "sets flag 0x20 at a5+0x004a and fades the selected palette groups up"},
                {"task": "0x1016", "style": 4, "palette_region": "0x91c800-0x91cbff", "mask": "a5+0x02b0", "behavior": "sets flag 0x10 at a5+0x004a and fades the selected palette groups up"}
            ],
            "delayed_actor_setup": {
                "handler": "0x83b0",
                "elapsed_wait_ticks_before_entry": 180,
                "next_wait_ticks": 60,
                "actor_record": {"record": "a5+0x0482", "data_pointer_source": "0x2eb24+0x001c", "data_pointer": "0x2f24c", "initial_x": 0, "initial_y": 0},
                "scroll_clear_selectors": [5, 9],
                "async_task": {"task": "0x1018", "style": 4, "palette_region": "0x91cc00-0x91cfff", "mask": "a5+0x02b4", "behavior": "sets flag 0x08 at a5+0x004a and fades the selected palette groups up"}
            },
            "opaque_actor_data": [
                {
                    "actor_data_pointer": "0x2ea24",
                    "records": [
                        {"address": "0x2eac4", "bytes": "50005100000000000000000000000000"},
                        {"address": "0x2eae4", "bytes": "004d004e000000000000000000000000"},
                        {"address": "0x2eb04", "bytes": "404142434445464748494a4b4c00004f"},
                        {"address": "0x2eb14", "bytes": "1b1c3f3f191a3b3c3d3e1d1e00000000"},
                    ],
                },
                {
                    "actor_data_pointer": "0x2f24c",
                    "records": [
                        {"address": "0x2f31c", "bytes": "20002100222300000000000000000000"},
                        {"address": "0x2f33c", "bytes": "0506090a07081e1f0000000000000000"},
                    ],
                },
            ],
            "candidate_indexed_actor_targets": {
                "selector_routine": "0x1f30",
                "selector_routine_bytes": animation_select_bytes.hex(),
                "lookup": "if actor byte +15 equals a candidate selector, the routine reads its BE16 relative offset and adds it to the actor data-table base",
                "actors": indexed_actor_targets,
            },
            "interpretation": "the native front end must reproduce staged actor and asynchronous-task setup after its palette fade; actor script frame meanings, full tilemap setup, and exact onscreen composition are still unresolved"
        },
        "interpretation": "source-backed ordinary pre-game task, initial palette copies, and map transition; the task-code text/content and full title/attract screen composition are not yet identified",
        "byte_checks": [
            {"address": f"0x{address:06x}", "length": len(expected)}
            for address, expected, _label in (*checks, *startup_copy_sites,
                                                *attract_scene_sites)
        ] + [{"address": f"0x{animation_select_address:06x}",
              "length": len(animation_select_bytes)}],
    }


def front_end_start_trace(program: bytes) -> dict[str, object]:
    """Trace the attract-loop edge that accepts Start and enters gameplay."""
    start_gate_address = 0x1B12
    start_gate_bytes = bytes.fromhex(
        "102d00660200003f0c00000766081b7c000102a4"
        "4e75422d02a44e75"
    )
    start_gate_caller_address = 0x1AA0
    start_gate_caller_bytes = bytes.fromhex(
        "102d006802400080ee481b40028f67106100006008"
        "2d000300686704522d799e"
    )
    loop_address = 0x811E
    loop_bytes = bytes.fromhex(
        "4a2d02a467066100006066564a2d02a2675070003b400000"
        "3b4000023b4000043b4000063b4000083b40000a3b40000c"
        "3b40000e3b4000103b400012303c00c04eb8168c303c00e0"
        "4eb8168c4eb81708303c0040207c000090e84eb815cc4ef81636"
    )
    input_address = 0x8186
    input_bytes = bytes.fromhex(
        "102d00594600c02d00580200003067ec74000800000567027401"
    )
    entry_tail_address = 0x81A6
    entry_tail_bytes = bytes.fromhex(
        "3b4000003b4000023b4000043b4000063b4000083b40000a"
        "3b40000c3b40000e3b4000103b4000121b4002931b40799f"
        "1b4002a3303c00c04eb8168c303c00e04eb8168c4eb81708"
        "426d03444eb8097870003b40002e3b407442303c01003b400030"
        "3b407444303c0060207c000092d24eb815cc4ef81636"
    )
    for label, address, expected in (
        ("front-end Start gate", start_gate_address, start_gate_bytes),
        ("front-end Start gate caller", start_gate_caller_address,
         start_gate_caller_bytes),
        ("front-end start loop", loop_address, loop_bytes),
        ("front-end start input", input_address, input_bytes),
        ("front-end gameplay entry", entry_tail_address, entry_tail_bytes),
    ):
        if program[address:address + len(expected)] != expected:
            raise ValueError(f"{label} bytes at {address:#x} changed")
    return {
        "dispatcher_loop": f"0x{loop_address:06x}",
        "start_enable_gate": {
            "writer": f"0x{start_gate_address:06x}",
            "caller": f"0x{start_gate_caller_address:06x}",
            "condition": "caller requires bit 7 of a5+0x0068; writer sets a5+0x02a4 when (a5+0x0066 & 0x3f) equals 7, otherwise clears it",
            "input_source": "a5+0x0066/0x0068 are the sampled DSWC/DSWD values",
        },
        "entry_gate": "a5+0x02a4 must be nonzero before input sampling",
        "input_handler": f"0x{input_address:06x}",
        "edge_expression": "(~a5+0x0059 & a5+0x0058) & 0x30",
        "accepted_edge_bits": ["0x10", "0x20"],
        "player_count_byte": "a5+0x02a5 receives 1 when bit 5 is present, otherwise 0",
        "start_transition": "clears front-end state, initializes common display fields, calls gameplay reset 0x0978, queues callback 0x92d2 with D0=96, then transfers to 0x1636",
        "interpretation": "verified Start/coin input transition into gameplay; title and attract screen drawing before this edge remain unresolved",
        "byte_checks": [
            {"address": f"0x{address:06x}", "length": len(expected)}
            for address, expected in (
                (start_gate_address, start_gate_bytes),
                (start_gate_caller_address, start_gate_caller_bytes),
                (loop_address, loop_bytes),
                (input_address, input_bytes),
                (entry_tail_address, entry_tail_bytes),
            )
        ],
    }


def catalog(program: bytes) -> dict:
    checks = (
        (STAGE_CAMERA_PHASE_RESOLVER, STAGE_CAMERA_PHASE_RESOLVER_BYTES, "stage camera phase resolver"),
        (STAGE_CAMERA_PHASE_POINTERS, STAGE_CAMERA_PHASE_POINTER_BYTES, "stage camera phase pointer table"),
        (STAGE0_CAMERA_PHASE0_RECORD, STAGE0_CAMERA_PHASE0_RECORD_BYTES, "Stage 0 phase-zero camera record"),
        (STAGE0_CAMERA_TRACKER, STAGE0_CAMERA_TRACKER_BYTES, "Stage 0 camera field publisher"),
        (STAGE0_CAMERA_TARGET_TRACKER, STAGE0_CAMERA_TARGET_TRACKER_BYTES, "camera target tracker dispatch and Stage 0 path"),
        (STAGE0_CAMERA_Y_TRACKER_DISPATCH,
         STAGE0_CAMERA_Y_TRACKER_DISPATCH_BYTES,
         "camera Y target-follow dispatch and normal-mode entry"),
        (STAGE0_CAMERA_Y_TRACKER_NORMAL,
         STAGE0_CAMERA_Y_TRACKER_NORMAL_BYTES,
         "normal camera Y target-follow and 128-pixel deadband"),
        (STAGE0_CAMERA_ENDPOINT_SETUP, STAGE0_CAMERA_ENDPOINT_SETUP_BYTES,
         "camera endpoint setup from stage parameter 0x038e"),
        (STAGE0_NORMAL_PLAYER_INIT, STAGE0_NORMAL_PLAYER_INIT_BYTES,
         "normal Stage 0 player world-position initializer"),
        (STAGE0_PLAYER_ACTOR_SETUP, STAGE0_PLAYER_ACTOR_SETUP_BYTES,
         "normal Stage 0 player actor screen-position initializer"),
        (STAGE0_CAMERA_RETIREMENT, STAGE0_CAMERA_RETIREMENT_BYTES, "camera lower-bound retirement helper"),
        (STAGE0_CAMERA_TARGET_MODE, STAGE0_CAMERA_TARGET_MODE_BYTES,
         "camera target source selection"),
        (STAGE0_CAMERA_TRACKER_STEP, STAGE0_CAMERA_TRACKER_STEP_BYTES,
         "camera target-to-position tracker"),
        (STAGE0_CAMERA_VERTICAL_SETUP, STAGE0_CAMERA_VERTICAL_SETUP_BYTES,
         "camera initial Y target, bounds, and camera position"),
        (STAGE0_CAMERA_PARAMETER_SETUP, STAGE0_CAMERA_PARAMETER_SETUP_BYTES,
         "camera Stage 0 vertical bounds loaded from stage parameters"),
        (STAGE0_CAMERA_MODE_FLAG_SETTER, STAGE0_CAMERA_MODE_FLAG_SETTER_BYTES,
         "camera controller-mode flag setter"),
        (STAGE0_POST_PHASE_SELECTOR_TABLE, STAGE0_POST_PHASE_SELECTOR_BYTES, "Stage 0 post-phase map selector table"),
        (STAGE0_POST_PHASE_SCROLL2_CLEAR, STAGE0_POST_PHASE_SCROLL2_CLEAR_BYTES, "Stage 0 Scroll 2 map clear helper"),
        (STAGE0_POST_PHASE_SCROLL3_CLEAR, STAGE0_POST_PHASE_SCROLL3_CLEAR_BYTES, "Stage 0 Scroll 3 map clear helper"),
        (STAGE0_POST_PHASE_ACTOR_SETUP, STAGE0_POST_PHASE_ACTOR_SETUP_BYTES, "Stage 0 post-phase actor setup"),
        (STAGE0_POST_PHASE_ACTOR_MOVE, STAGE0_POST_PHASE_ACTOR_MOVE_BYTES, "Stage 0 post-phase actor map move"),
    )
    for offset, expected, label in checks:
        if program[offset:offset + len(expected)] != expected:
            raise ValueError(f"{label} changed")
    for offset, expected in STAGE0_CAMERA_UPDATE_CALLS:
        if program[offset:offset + len(expected)] != expected:
            raise ValueError(f"camera update call site at {offset:#x} changed")
    stages = []
    for index in range(STAGE_COUNT):
        ptr_at = POINTER_TABLE + index * 4
        pointer = int.from_bytes(program[ptr_at : ptr_at + 4], "big")
        mode = program[MODE_TABLE + index]
        if pointer + SELECTOR_BYTES > len(program):
            raise ValueError(f"stage {index} selector pointer {pointer:#x} is outside ROM")
        if mode != 0:
            raise ValueError(f"stage {index} uses unsupported selector mode {mode:#x}")
        selectors = list(program[pointer : pointer + SELECTOR_BYTES])
        stages.append({
            "stage": index,
            "selector_pointer": f"0x{pointer:06x}",
            "selector_mode": mode,
            "scroll2_palette_pointer": f"0x{0x0E7D20 + index * 0x400:06x}",
            "scroll3_palette_pointer": f"0x{int.from_bytes(program[0x44050 + index * 4:0x44054 + index * 4], 'big'):06x}",
            "scroll3_palette_destination_flag": f"0x{program[0x44074 + index]:02x}",
            "grid_width": 16,
            "grid_height": 16,
            "scroll2_chunk_tile_width": 16,
            "scroll2_chunk_tile_height": 16,
            "scroll3_chunk_tile_width": 32,
            "scroll3_chunk_tile_height": 32,
            "nonzero_selectors": sum(value != 0 for value in selectors),
            "chunk_ids": selectors,
        })
    transition_data = stage_transition(program)
    coordinate_rows = transition_data["unlinked_actor_coordinate_sequence"]["rows"]
    for candidate in coordinate_rows:
        world_x, world_y = candidate["candidate_player_world_xy"]
        if 0 <= world_x < 4096 and 0 <= world_y < 4096:
            chunk_x = (world_x >> 8) & 15
            chunk_y = 15 - ((world_y >> 8) & 15)
            chunk_ids = {
                str(stage["stage"]): stage["chunk_ids"][chunk_y * 16 + chunk_x]
                for stage in stages
            }
            candidate["selector_grid_cell"] = [chunk_x, chunk_y]
            candidate["chunk_id_by_stage"] = chunk_ids
            candidate["nonempty_stage_indices"] = [
                int(stage_index) for stage_index, chunk_id in chunk_ids.items()
                if chunk_id
            ]
        else:
            candidate["selector_grid_cell"] = None
            candidate["chunk_id_by_stage"] = {}
            candidate["nonempty_stage_indices"] = []
    transition_data["unlinked_actor_coordinate_sequence"]["geometry_note"] = (
        "The generated nonempty-stage list tests which candidate XY points land "
        "inside nonzero selector chunks. This is a geometry filter only; it does "
        "not link actor selectors to stage transitions or validate spawn usage."
    )
    source_camera_center_x = int.from_bytes(
        STAGE0_CAMERA_PHASE0_RECORD_BYTES[:2], "big")
    source_camera_endpoint = int(
        transition_data["stage_setup_parameters"]["rows"][0]["field_038e"], 16)
    stage0_setup_parameters = transition_data["stage_setup_parameters"]["rows"][0]
    source_camera_y_min = int(stage0_setup_parameters["field_0390"], 16)
    source_camera_y_max = int(stage0_setup_parameters["field_0392"], 16)
    native_camera_x_offset = (
        source_camera_center_x - STAGE0_NATIVE_PLAYER_ANCHOR_X)
    source_camera_lower_bound = 0
    stage0_max_column = max(index % 16 for index, chunk in
                            enumerate(stages[0]["chunk_ids"]) if chunk)
    stage0_nonzero_rows = [row for row in range(16)
                           if any(stages[0]["chunk_ids"][row * 16 + column]
                                  for column in range(16))]
    stage0_map_world_top = (15 - max(stage0_nonzero_rows)) * 256
    stage0_map_world_bottom = (16 - min(stage0_nonzero_rows)) * 256
    native_camera_y_min = stage0_map_world_top
    native_camera_y_max = max(
        native_camera_y_min,
        stage0_map_world_bottom - 224,
    )
    map_right_edge_camera_max = (stage0_max_column + 1) * 256 - 320
    if source_camera_endpoint + native_camera_x_offset != map_right_edge_camera_max:
        raise ValueError("Stage 0 source camera endpoint does not match its map right edge")
    player_world_x = int.from_bytes(program[0x0A12A:0x0A12C], "big")
    player_world_y = int.from_bytes(program[0x0A130:0x0A132], "big")
    player_anchor_x = int.from_bytes(program[0x0A2B8:0x0A2BA], "big")
    player_anchor_y = int.from_bytes(program[0x0A2BE:0x0A2C0], "big")
    if (player_world_x, player_world_y) != (192, 512):
        raise ValueError("Stage 0 player world coordinates changed")
    if (player_anchor_x, player_anchor_y) != (160, 80):
        raise ValueError("Stage 0 player actor anchor changed")
    return {
        "stage0_post_phase_map_clears": {
            "selector_table_offset": f"0x{STAGE0_POST_PHASE_SELECTOR_TABLE:06x}",
            "selector_table_bytes": STAGE0_POST_PHASE_SELECTOR_BYTES.hex(),
            "actor_setup_offset": f"0x{STAGE0_POST_PHASE_ACTOR_SETUP:06x}",
            "actor_setup_bytes": STAGE0_POST_PHASE_ACTOR_SETUP_BYTES.hex(),
            "actor_move_offset": f"0x{STAGE0_POST_PHASE_ACTOR_MOVE:06x}",
            "actor_move_bytes": STAGE0_POST_PHASE_ACTOR_MOVE_BYTES.hex(),
            "actor_origins": [[0x0000, 0x0300], [0x0400, 0x0200], [0x0200, 0x0300]],
            "table_offsets": [[0x0000, 0x00f0], [0x0100, 0x00f0]],
            "interpretation": "the source initializes actor records at the listed origins, calls the two map helpers with selectors 5 and 9, then moves the first actor origin from x=0 to x=0x200 before behavior 8",
        },
        "source": "offline Willow 68000 ROM; selector table chosen at 0x43984-0x43aa0",
        "front_end_attract": front_end_attract_trace(program),
        "front_end_start": front_end_start_trace(program),
        "stage_transition": transition_data,
        "stage0_player_spawn": {
            "world_initializer_offset": f"0x{STAGE0_NORMAL_PLAYER_INIT:06x}",
            "world_initializer_bytes": STAGE0_NORMAL_PLAYER_INIT_BYTES.hex(),
            "world_coordinates": [player_world_x, player_world_y],
            "actor_initializer_offset": f"0x{STAGE0_PLAYER_ACTOR_SETUP:06x}",
            "actor_initializer_bytes": STAGE0_PLAYER_ACTOR_SETUP_BYTES.hex(),
            "native_player_anchor": [player_anchor_x, player_anchor_y],
            "initial_native_camera": [
                player_world_x - player_anchor_x,
                player_world_y - player_anchor_y,
            ],
            "interpretation": "normal Stage 0 writes global player world coordinates separately from the player object's initial screen-space anchor; the native camera uses their difference",
        },
        "stage0_camera_tracker": {
            "phase_resolver_offset": f"0x{STAGE_CAMERA_PHASE_RESOLVER:06x}",
            "phase_resolver_bytes": STAGE_CAMERA_PHASE_RESOLVER_BYTES.hex(),
            "stage_phase_pointer_table_offset": f"0x{STAGE_CAMERA_PHASE_POINTERS:06x}",
            "stage_phase_pointer_table_bytes": STAGE_CAMERA_PHASE_POINTER_BYTES.hex(),
            "stage0_phase_zero_record_offset": f"0x{STAGE0_CAMERA_PHASE0_RECORD:06x}",
            "stage0_phase_zero_record_bytes": STAGE0_CAMERA_PHASE0_RECORD_BYTES.hex(),
            "stage0_phase_zero_candidate": {
                "stage_parameter_0370_0372": ["0x00c0", "0x0048"],
                "phase_lookup_index": 0,
                "phase": 0,
                "camera_target_xy": [192, 72],
                "initial_retirement_lower_bound": 0,
                "genesis_camera_x_for_160_pixel_player_anchor": 32,
                "call_path_status": "normal stage setup calls 0x2f8a to load parameters (0x00c0,0x0048) before Stage 0 initialization; 0xa0e0 calls 0x2f08, whose stage-counter-zero dispatch selects resolver 0x2fbc, so this is the active Stage 0 phase-zero camera record",
            },
            "publisher_offset": f"0x{STAGE0_CAMERA_TRACKER:06x}",
            "publisher_bytes": STAGE0_CAMERA_TRACKER_BYTES.hex(),
            "target_tracker_offset": f"0x{STAGE0_CAMERA_TARGET_TRACKER:06x}",
            "target_tracker_bytes": STAGE0_CAMERA_TARGET_TRACKER_BYTES.hex(),
            "endpoint_setup_offset": f"0x{STAGE0_CAMERA_ENDPOINT_SETUP:06x}",
            "endpoint_setup_bytes": STAGE0_CAMERA_ENDPOINT_SETUP_BYTES.hex(),
            "endpoint_setup_behavior": (
                "copies stage field a5+0x038e into camera object +104; "
                "Stage 0 loads 0x07a0 from the validated stage parameter table, "
                "and the camera tracker uses +104 as a movement clamp"
            ),
            "native_camera_x_mapping": {
                "source_camera_lower_bound": source_camera_lower_bound,
                "source_camera_endpoint": source_camera_endpoint,
                "source_center_x": source_camera_center_x,
                "genesis_player_anchor_x": STAGE0_NATIVE_PLAYER_ANCHOR_X,
                "genesis_camera_x_offset": native_camera_x_offset,
                "genesis_min_x": source_camera_lower_bound + native_camera_x_offset,
                "genesis_max_x": source_camera_endpoint + native_camera_x_offset,
                "map_right_edge_cross_check": map_right_edge_camera_max,
            },
            "retirement_helper_offset": f"0x{STAGE0_CAMERA_RETIREMENT:06x}",
            "retirement_helper_bytes": STAGE0_CAMERA_RETIREMENT_BYTES.hex(),
            "target_source_selection": {
                "offset": f"0x{STAGE0_CAMERA_TARGET_MODE:06x}",
                "bytes": STAGE0_CAMERA_TARGET_MODE_BYTES.hex(),
                "flag_offset": "+0x0348",
                "flag_clear_source": "a5-0x8fb0/a5-0x8fb2 initialized to (192,640) in normal Stage 0",
                "flag_set_source": "shared controller target a5+0x184c/a5+0x1850",
                "flag_setter_offset": f"0x{STAGE0_CAMERA_MODE_FLAG_SETTER:06x}",
                "flag_setter_bytes": STAGE0_CAMERA_MODE_FLAG_SETTER_BYTES.hex(),
                "normal_play_activation": "the state handler sets the flag and starts a 900-tick phase; its normal Stage 0 activation call path remains unverified",
            },
            "target_tracker_step": {
                "offset": f"0x{STAGE0_CAMERA_TRACKER_STEP:06x}",
                "bytes": STAGE0_CAMERA_TRACKER_STEP_BYTES.hex(),
                "horizontal_position": "max(target_x - 192, camera_lower_bound)",
                "vertical_position": "target_y - 128, clamped by the stage camera bounds",
            },
            "vertical_target_tracker": {
                "dispatch_offset": f"0x{STAGE0_CAMERA_Y_TRACKER_DISPATCH:06x}",
                "dispatch_bytes": STAGE0_CAMERA_Y_TRACKER_DISPATCH_BYTES.hex(),
                "normal_mode_offset": f"0x{STAGE0_CAMERA_Y_TRACKER_NORMAL:06x}",
                "normal_mode_bytes": STAGE0_CAMERA_Y_TRACKER_NORMAL_BYTES.hex(),
                "target_field": "+108",
                "camera_position_field": "+10",
                "deadband_pixels": 128,
                "stage0_initial_target_y": 640,
                "stage0_initial_camera_y": 512,
                "source_camera_y_bounds": [source_camera_y_min,
                                            source_camera_y_max],
                "native_map_camera_y_bounds_before_translation": [
                    native_camera_y_min, native_camera_y_max],
                "translated_native_camera_y_bounds": [
                    source_camera_y_min - STAGE0_NATIVE_PLAYER_ANCHOR_Y,
                    min(source_camera_y_max, native_camera_y_max)
                    - STAGE0_NATIVE_PLAYER_ANCHOR_Y,
                ],
                "source_to_native_initial_y_offset": (
                    player_world_y - player_anchor_y
                    - (640 - 128)
                ),
                "bound_follow_ticks_at_two_pixels_per_tick": {
                    "source_from_stage0_initial_camera": (
                        (512 - source_camera_y_min + 1) // 2
                    ),
                    "native_from_translated_stage0_initial_camera": (
                        (player_world_y - player_anchor_y
                         - (source_camera_y_min
                            - STAGE0_NATIVE_PLAYER_ANCHOR_Y) + 1) // 2
                    ),
                    "difference": (
                        (512 - source_camera_y_min + 1) // 2
                        - (player_world_y - player_anchor_y
                           - (source_camera_y_min
                              - STAGE0_NATIVE_PLAYER_ANCHOR_Y) + 1) // 2
                    ),
                },
                "initializer_offset": f"0x{STAGE0_CAMERA_VERTICAL_SETUP:06x}",
                "initializer_bytes": STAGE0_CAMERA_VERTICAL_SETUP_BYTES.hex(),
                "stage_bound_loader_offset": f"0x{STAGE0_CAMERA_PARAMETER_SETUP:06x}",
                "stage_bound_loader_bytes": STAGE0_CAMERA_PARAMETER_SETUP_BYTES.hex(),
                "stage0_camera_bound_fields": {
                    "field_90_from_stage_parameter_0390": source_camera_y_min,
                    "field_50_from_stage_parameter_0392": source_camera_y_max,
                },
                "normal_mode_step": "when the target is less than 128 pixels from camera Y, apply target_y - camera_y - 128 to camera Y; Stage 0 target updates by -2 per tick, so the first update moves camera Y by -2 per tick",
                "native_coordinate_alignment": "native camera coordinates are source camera Y minus the 80-pixel Genesis player anchor; signed bounds preserve that translation through the source lower bound, with blank map cells before source world Y=0",
            },
            "update_call_sites": [
                {"offset": f"0x{offset:06x}", "bytes": expected.hex()}
                for offset, expected in STAGE0_CAMERA_UPDATE_CALLS
            ],
            "verified_behavior": "the phase resolver at 0x2fbc selects a stage camera record and updates camera object position and lower bound; camera field +104 is loaded from stage parameter a5+0x038e (0x07a0 in Stage 0) and serves as a movement clamp; the active camera update publishes object field +100 as global lower bound +0x038c, and helper 0x42e4e advances that bound monotonically to max(previous, camera object X - 768), clamped at zero",
            "unresolved_behavior": "the Stage 0 X scroll bounds now map to native coordinates and normal-mode vertical follow cadence is byte-verified; source camera X target/lag behavior, the normal-play caller for flagged camera mode, and the complete vertical source-to-Genesis alignment and lower-bound mapping remain unverified",
        },
        "stage_timer": stage_timer(program),
        "selector_lookup": {
            "routine": "0x436c2",
            "mode_0_index": "((y & 0x0f00) >> 4) | ((x >> 8) & 0x0f)",
            "grid": "16x16 chunk selectors",
        },
        "chunk_data": {
            "scroll2_base": "0x080000",
            "scroll2_stride_bytes": 1024,
            "scroll2_cells": "16x16 code/attribute pairs",
            "scroll3_base": "0x0c0000",
            "scroll3_stride_bytes": 256,
            "scroll3_cells": "8x8 code/attribute pairs",
        },
        "usage": "Gameplay stage data; each selector byte chooses matching Scroll 2 and Scroll 3 chunks",
        "stages": stages,
    }


def to_header(data: dict) -> str:
    timer = data["stage_timer"]
    camera = data["stage0_camera_tracker"]
    stage0_camera = camera["native_camera_x_mapping"]
    stage0_endpoint = int(data["stage_transition"]["stage_setup_parameters"]
                          ["rows"][0]["field_038e"], 16)
    stage0_chunks = data["stages"][0]["chunk_ids"]
    stage0_max_column = max(index % 16 for index, chunk in enumerate(stage0_chunks)
                            if chunk)
    stage0_map_camera_max = (stage0_max_column + 1) * 256 - 320
    if stage0_endpoint + stage0_camera["genesis_camera_x_offset"] != stage0_map_camera_max:
        raise ValueError("Stage 0 camera endpoint no longer matches its mapped right edge")
    stage0_rows = [row for row in range(16)
                   if any(stage0_chunks[row * 16 + column]
                          for column in range(16))]
    stage0_map_world_top = (15 - max(stage0_rows)) * 256
    stage0_map_world_bottom = (16 - min(stage0_rows)) * 256
    stage0_map_camera_y_max = max(
        stage0_map_world_top,
        stage0_map_world_bottom - 224,
    )
    stage0_source_camera_y_min = int(data["stage0_camera_tracker"]
        ["vertical_target_tracker"]["source_camera_y_bounds"][0])
    stage0_camera_y_offset = -STAGE0_NATIVE_PLAYER_ANCHOR_Y
    stage0_camera_y_min = stage0_source_camera_y_min + stage0_camera_y_offset
    stage0_camera_y_max = stage0_map_camera_y_max + stage0_camera_y_offset
    rows = [
        "/* Generated from Willow's gameplay layer-map selector tables. */",
        "#ifndef WILLOW_STAGE_MAPS_H",
        "#define WILLOW_STAGE_MAPS_H",
        "#include <genesis.h>",
        "#define WILLOW_STAGE_COUNT 8",
        "#define WILLOW_STAGE_MAP_WIDTH 16",
        "#define WILLOW_STAGE_MAP_HEIGHT 16",
        f"#define WILLOW_STAGE0_PLAYER_WORLD_X {data['stage0_player_spawn']['world_coordinates'][0]}",
        f"#define WILLOW_STAGE0_PLAYER_WORLD_Y {data['stage0_player_spawn']['world_coordinates'][1]}",
        f"#define WILLOW_STAGE0_PLAYER_ANCHOR_X {data['stage0_player_spawn']['native_player_anchor'][0]}",
        f"#define WILLOW_STAGE0_PLAYER_ANCHOR_Y {data['stage0_player_spawn']['native_player_anchor'][1]}",
        f"#define WILLOW_STAGE0_CAMERA_MIN_X {stage0_camera['genesis_min_x']}",
        f"#define WILLOW_STAGE0_CAMERA_MAX_X {stage0_endpoint + stage0_camera['genesis_camera_x_offset']}",
        f"#define WILLOW_STAGE0_CAMERA_X_OFFSET {stage0_camera['genesis_camera_x_offset']}",
        f"#define WILLOW_STAGE0_CAMERA_Y_OFFSET ({stage0_camera_y_offset})",
        f"#define WILLOW_STAGE0_CAMERA_MIN_Y ({stage0_camera_y_min})",
        f"#define WILLOW_STAGE0_CAMERA_MAX_Y ({stage0_camera_y_max})",
        f"#define WILLOW_STAGE0_CAMERA_RETIREMENT_INITIAL {stage0_camera['source_camera_lower_bound']}",
        f"#define WILLOW_STAGE0_TIMER_MINUTES {timer['initial_minutes']}",
        f"#define WILLOW_STAGE0_TIMER_SECONDS {timer['initial_seconds']}",
        "static const u8 willow_stage0_timer_digit_x[4] = { "
        + ", ".join(str(cell[0]) for cell in timer["scroll1_initialization"]["timer_digit_cells"])
        + " };",
        "static const u8 willow_stage0_timer_digit_y[4] = { "
        + ", ".join(str(cell[1]) for cell in timer["scroll1_initialization"]["timer_digit_cells"])
        + " };",
        "#define WILLOW_STAGE_TIMER_SUBCOUNTER_INITIAL 0",
        "#define WILLOW_STAGE_TIMER_SUBCOUNTER_RELOAD 59",
        "#define WILLOW_STAGE0_POST_PHASE_CLEAR_PAIR_COUNT 2",
        "static const u16 willow_stage0_post_phase_actor_origins[3][2] = {",
        "    {0x0000, 0x0300}, {0x0400, 0x0200}, {0x0200, 0x0300}",
        "};",
        "static const u16 willow_stage0_post_phase_clear_offsets[2][2] = {",
        "    {0x0000, 0x00f0}, {0x0100, 0x00f0}",
        "};",
    ]
    rows.append("static const u8 willow_stage_map_ids[WILLOW_STAGE_COUNT][256] = {")
    for stage in data["stages"]:
        rows.append("    {")
        ids = stage["chunk_ids"]
        for offset in range(0, len(ids), 16):
            rows.append("        " + ", ".join(f"0x{x:02x}" for x in ids[offset:offset + 16]) + ",")
        rows.append("    },")
    rows.extend(["};", "#endif", ""])
    return "\n".join(rows)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", nargs="?", type=Path, default=Path("willow.zip"))
    parser.add_argument("--json", type=Path, default=Path("build/willow_stage_maps.json"))
    parser.add_argument("--header", type=Path, default=Path("build/willow_stage_maps.h"))
    args = parser.parse_args()
    try:
        data = catalog(assemble_maincpu(args.archive))
    except (OSError, ValueError, KeyError) as exc:
        parser.error(str(exc))
    args.json.parent.mkdir(parents=True, exist_ok=True)
    args.header.parent.mkdir(parents=True, exist_ok=True)
    args.json.write_text(json.dumps(data, indent=2) + "\n")
    args.header.write_text(to_header(data), encoding="ascii")
    for stage in data["stages"]:
        print(f"Stage {stage['stage']}: {stage['nonzero_selectors']}/256 nonzero chunk selectors")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
