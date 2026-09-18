#ifndef ACTORS_SHARED_80134DBC_H
#define ACTORS_SHARED_80134DBC_H

#include "common.h"

#include "main/task.h"

/// Work block the actors sharing this body hang off `Task::work`. They
/// allocate different total sizes, so only the prefix this body reaches is
/// described rather than a whole-block size that would be wrong for one of
/// them. `promptKind` is the display mode forwarded to `func_800D4E78`.
typedef struct ActorsShared80134dbcWork {
    /* 0x00 */ byte pad_0[6];
    /* 0x06 */ s8   promptKind;
} ActorsShared80134dbcWork;

/// Spawns the action prompt for the actor's current step: clears the prompt's
/// highlight state, then re-spawns it at the coordinates the gameplay side left
/// in `D_80114D28` with the display mode the actor picked, and steps the task on
/// to state 4.
///
/// Shared verbatim by `actor_143000` and `actor_548100`; the room overlays carry
/// the same body as the shared `Room_Util21`.
void ActorsShared80134dbc(Task* task);

#endif
