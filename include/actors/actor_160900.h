#ifndef ACTOR_160900_H
#define ACTOR_160900_H

#include "common.h"

#include "main/task.h"

/// Work block this overlay hangs off the task's `Task::idMap` slot (0x1C),
/// which is not a `TaskIdMap` here. Reach it with
/// `(Actor160900Work*)task->idMap`.
typedef struct Actor160900Work {
    /* 0x00 */ byte pad_0[0x4C];
    /* 0x4C */ s16  field_4C;
    /* 0x4E */ s16  field_4E;
    /* 0x50 */ byte pad_50[4];
    /* 0x54 */ s16  field_54;
    /* 0x56 */ s16  field_56;
    /* 0x58 */ byte pad_58[4];
    /* 0x5C */ s16  field_5C;
    /* 0x5E */ s16  field_5E;
} Actor160900Work;

extern Task* D_actor_160900_8013FBB4;

void func_actor_160900_801347F0(void);

#endif
