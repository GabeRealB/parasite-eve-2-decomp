#ifndef ACTORS_SHARED_80138548_H
#define ACTORS_SHARED_80138548_H

#include "common.h"

#include "gameplay/message.h"
#include "main/task.h"

/// Head of the enemy work block `actor_104000`, `actor_204000` and
/// `actor_304000` park in `Task::work` -- that slot is not a `TaskIdMap` here.
/// `field_170` is the motion state the per-frame handler reads and `field_174`
/// the requested animation id; each overlay's own `0x...87B4` sets the same
/// pair directly. The type stops after the last field this shared body
/// reaches; whatever each overlay keeps around them differs per actor.
typedef struct ActorShared80138548Work {
    /* 0x000 */ byte pad_0[0x170];
    /* 0x170 */ s16  field_170;
    /* 0x172 */ byte pad_172[2];
    /* 0x174 */ u16  field_174;
} ActorShared80138548Work;

/// Handler for message 0x7D3 in all three overlays: latches the requested
/// animation and picks the motion state the flag asks for.
s32 ActorsShared80138548(Task* task, s32 arg1, GpAnimArg* msg, s32 arg3);

#endif
