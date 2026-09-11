#ifndef ACTORS_SHARED_80139C00_H
#define ACTORS_SHARED_80139C00_H

#include "common.h"

#include "main/task.h"

/// The heading `ActorsShared80139c00` steers, at the same offset in
/// `Actor400600Work` and `Actor405800Work`.
typedef struct ActorsShared80139c00Work {
    /* 0x00 */ byte pad_0[0x82];
    /* 0x82 */ u16  yaw;
} ActorsShared80139c00Work;

/// Turns the actor's heading by `step` toward the world point `target` (only
/// `vx` / `vz` are read), leaving it alone inside a 0x100 dead zone. Clears the
/// model root's `flg` first. Shared by `actor_400600` and `actor_405800`.
void ActorsShared80139c00(Task* arg0, SVECTOR* target, s32 step);

#endif
