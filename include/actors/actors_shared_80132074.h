#ifndef ACTORS_SHARED_80132074_H
#define ACTORS_SHARED_80132074_H

#include "common.h"

#include <psyq/libgte.h>

#include "main/task.h"

/// Argument block of the script opcode this body handles (0x7D4 in
/// `actor_110700`'s table): a world translation followed by the Euler angles
/// handed to `RotMatrix`. The two together are exactly the `GsCOORDINATE2`
/// `coord` the actor's root part draws with.
typedef struct ActorsShared80132074Args {
    /* 0x00 */ VECTOR  pos;
    /* 0x10 */ SVECTOR rot;
} ActorsShared80132074Args;
STATIC_ASSERT_SIZEOF(ActorsShared80132074Args, 0x18);

/// Places the actor: builds the root part's local matrix from the opcode's
/// Euler angles, drops the translation into it and clears `flg` so
/// `Gp_UpdateCoordTree` recomputes the world matrix from it.
///
/// Shared verbatim by `actor_110700`, `actor_510900`, `actor_511000` and
/// `actor_521100`; `actor_100300` / `actor_200300` carry the same body inside
/// their own `actor_100300_text` unit as `Actor00300_Fn05388`.
s32 ActorsShared80132074(Task* task, s32 arg1, ActorsShared80132074Args* args);

#endif
