/* Reuse root's inputs, source latch, native oracle and VDP publication exactly.
 * Only the optional geometry consumer is substituted in this experiment. */
#include "willow_native_object_sat_fast.h"
#define WILLOW_DEFERRED_OBJECT_BUDGET 1
#define willowObjectSatBuildReaderDeferred willowObjectSatBuildReaderFast
#include "native_latch_presentation.c"
