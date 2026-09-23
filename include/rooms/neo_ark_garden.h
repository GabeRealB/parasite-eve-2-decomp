#ifndef ROOMS_NEO_ARK_GARDEN_H
#define ROOMS_NEO_ARK_GARDEN_H

#include "common.h"

/// The block the garden's ambience task reaches through `Task::spawnArg2`.
/// Only `soundDelay` is read here; what precedes it belongs to whoever owns the
/// block, and the type's true size is not known.
typedef struct NeoArkGardenAmbience {
    u8  pad_0[0x24];
    s16 soundDelay; // Frames left before the view's loops are re-enqueued; set to 4 on every view change
} NeoArkGardenAmbience;

#endif // ROOMS_NEO_ARK_GARDEN_H
