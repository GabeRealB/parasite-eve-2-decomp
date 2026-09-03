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

/// Four-byte work block `func_acropolis_fountain_8017E3D4` allocates into
/// `Task::idMap` (`Mem_Calloc(4, 0)`) and hands to
/// `func_acropolis_fountain_8017E15C`, which drives the fountain's waterfall
/// loop. `state` walks 0 -> 1 -> 2 -> 0: state 0 waits for the streamed
/// fountain video to reach its playing window, state 2 (re)starts the loop for
/// the camera the player is on, and `started` latches that the window was
/// entered so the sound is not retriggered every frame.
typedef struct AcropolisFountainSndWork {
    /* 0x0 */ u16 state;
    /* 0x2 */ u16 started;
} AcropolisFountainSndWork;

#endif // ROOMS_ACROPOLIS_FOUNTAIN_H
