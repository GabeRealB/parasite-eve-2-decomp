#ifndef ACTORS_SHARED_80132074_H
#define ACTORS_SHARED_80132074_H

#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/message.h"
#include "main/task.h"

/// Places the actor: builds the root part's local matrix from the opcode's
/// Euler angles, drops the translation into it and clears `flg` so
/// `_gpUpdateCoordTree` recomputes the world matrix from it.
///
/// Shared verbatim by `actor_110700`, `actor_510900`, `actor_511000` and
/// `actor_521100`; `actor_100300` / `actor_200300` carry the same body inside
/// their own `actor_100300_text` unit as `Actor00300_Fn05388`.
s32 ActorsShared80132074(Task* task, s32 arg1, GpXformArg* args);

#endif
