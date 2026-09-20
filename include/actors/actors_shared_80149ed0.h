#ifndef ACTORS_SHARED_80149ED0_H
#define ACTORS_SHARED_80149ED0_H

#include "actors/actors_shared_80149e54.h"

/* Compact phase records used by the dynamic 10-column screen-wave mesh. */
typedef struct ActorWaveRec6 {
    /* 0x0 */ s16 phase;
    /* 0x2 */ s16 offset;
    /* 0x4 */ s16 speed;
} ActorWaveRec6;
STATIC_ASSERT_SIZEOF(ActorWaveRec6, 6);

/// Draws a 10 by 30 textured screen-wave mesh, ramping the distortion from
/// the task spawn argument. Each overlay binds its own context and phase data.
/// The scalar read of Task::state (0x30) preserves its dependency on the decode
/// mode store; an ordinary member read allows GCC to move it across that store.
void ActorsShared80149ed0(Task* task);

#endif
