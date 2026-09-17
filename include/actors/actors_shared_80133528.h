#ifndef ACTORS_SHARED_80133528_H
#define ACTORS_SHARED_80133528_H

#include "common.h"

#include "actors/actors_shared_801330ac.h"
#include "main/task.h"

/// Args block of the "start animation" script opcode `ActorsShared80133528`:
/// the clip to play, a flag choosing which start path the receiving state
/// machine takes, and the reset argument only that path carries. Same shape as
/// the `Actor450800AnimArgs` and `Actor451100AnimArgs` the per-actor twins of
/// this opcode take.
typedef struct ActorsShared80133528Args {
    /* 0x0 */ byte pad_0[4];
    /* 0x4 */ s32  animId;
    /* 0x8 */ s32  withArg;
    /* 0xC */ u16  animArg;
} ActorsShared80133528Args;

/// Script opcode of the enemy work block `ActorsShared801330ac` drives: sets
/// the clip and the state that machine starts from, then hands it the task.
///
/// Both carriers (the actor_450800 and actor_535700 overlays) hold this body at
/// their own address, so it is matched once here and linked into each. It
/// references nothing of its own but `ActorsShared801330ac`, which is shared
/// for the same reason.
s32 ActorsShared80133528(Task* task, s32 arg1, ActorsShared80133528Args* args);

#endif // ACTORS_SHARED_80133528_H
