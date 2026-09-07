#ifndef ACTORS_SHARED_80164AF0_H
#define ACTORS_SHARED_80164AF0_H

#include "common.h"

#include "main/task.h"

/// Head of the enemy work block `actor_323000`, `actor_323400` and
/// `actor_421600` park in `Task::idMap` -- that slot is not a `TaskIdMap` here.
/// `field_0` is the animation state the per-frame handler reads and `field_2`
/// its frame counter; `field_16` is the actor's cached yaw, taken back off the
/// root coordinate by `ActorsShared80164954`; `field_82E` is the requested
/// animation id.
typedef struct ActorShared80164af0Work {
    /* 0x000 */ s16  field_0;
    /* 0x002 */ s16  field_2;
    /* 0x004 */ byte pad_4[0x12];
    /* 0x016 */ s16  field_16;
    /* 0x018 */ byte pad_18[0x816];
    /* 0x82E */ u16  field_82E;
} ActorShared80164af0Work;

/// Payload the sender of message 0x7D3 passes as `Gp_DispatchMsg`'s `arg2`;
/// only the animation id at 0x4 is read.
typedef struct ActorShared80164af0Msg {
    /* 0x0 */ byte pad_0[0x4];
    /* 0x4 */ u16  field_4;
} ActorShared80164af0Msg;

/// Handler for message 0x7D3 in all three overlays: latches the requested
/// animation and restarts the animation state machine.
s32 ActorsShared80164af0(Task* task, s32 arg1, ActorShared80164af0Msg* msg, s32 arg3);

#endif
