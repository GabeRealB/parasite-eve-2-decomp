#ifndef ROOMS_ACROPOLIS_FOUNTAIN_H
#define ROOMS_ACROPOLIS_FOUNTAIN_H

#include "common.h"

#include "rooms/room_common.h"

/// Work block the fountain's splash task keeps at `Task::spawnArg2`.
/// `func_acropolis_fountain_8017E014` is the only unit-2 user: it latches the
/// camera the splash effect was spawned under in `viewIndex` and compares it
/// against `Gp_GetViewIndex()` every frame, so a camera cut re-runs the
/// `D_acropolis_fountain_8017E7FC` table task (arg 1 to tear the old view's
/// splash down, arg 0 to build the new one). Only views whose bit is set in
/// `0x100FE` - 2..8 and 17 - see the fountain, the rest skip the spawn.
typedef struct AcropolisFountainSplash {
    /* 0x00 */ byte pad_0[0x24];
    /* 0x24 */ s16  viewIndex;
} AcropolisFountainSplash;

#endif // ROOMS_ACROPOLIS_FOUNTAIN_H
