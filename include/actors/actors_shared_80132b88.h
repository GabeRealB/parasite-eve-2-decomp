#ifndef ACTORS_SHARED_80132B88_H
#define ACTORS_SHARED_80132B88_H

#include "common.h"

/// Work block the actor parks in the task's `Task::idMap` slot (that slot is
/// not a `TaskIdMap` here). `field_4EE` is the second of the pair of angles at
/// 0x4E6/0x4EA/0x4EE the neighbouring facing helpers write.
typedef struct ActorShared80132b88Work {
    /* 0x000 */ byte pad_0[0x4EE];
    /* 0x4EE */ u16  field_4EE;
} ActorShared80132b88Work;

typedef struct ActorShared80132b88 {
    /* 0x00 */ byte                     pad_0[0x1C];
    /* 0x1C */ ActorShared80132b88Work* field_1C;
} ActorShared80132b88;

/// Script command payload; `field_2` is the angle to latch.
typedef struct ActorShared80132b88Cmd {
    /* 0x0 */ byte pad_0[2];
    /* 0x2 */ u16  field_2;
} ActorShared80132b88Cmd;

s32 ActorsShared80132b88(ActorShared80132b88* arg0, s32 arg1, ActorShared80132b88Cmd* arg2);

#endif
