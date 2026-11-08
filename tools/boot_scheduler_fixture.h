#ifndef WILLOW_BOOT_SCHEDULER_FIXTURE_H
#define WILLOW_BOOT_SCHEDULER_FIXTURE_H
#include "../src/willow_native_boot_globals.h"
/* Native representation: control/free stack/ring offsets, continuations,
 * original task records/command ring, and sparse boot globals. CPU saved
 * SR/PC/USP fields use the already-verified native continuation convention. */
#define BOOT_SCHEDULER_FIXTURE_BYTES (84+512+256+sizeof(WillowNativeBootGlobals))
#endif
