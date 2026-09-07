#ifndef ACTORS_SHARED_80135A24_H
#define ACTORS_SHARED_80135A24_H

#include "common.h"

#include "main/task.h"

/// Work block of the enemy actors that share `ActorsShared80135a24`, reached
/// through the *parent* task's `Task::idMap`. Only the two matrices the model
/// object is pointed at and the state field the spawn handler seeds are
/// modelled here; the same offsets sit in `Actor02000Work`
/// (`include/actors/actor_102000.h`), which describes the rest of the block.
typedef struct ActorsShared80135a24Work {
    /* 0x000 */ byte   pad_0[0x43C];
    /* 0x43C */ MATRIX field_43C; ///< colour matrix, handed to TmdObject::field_20
    /* 0x45C */ MATRIX field_45C; ///< light matrix, handed to TmdObject::field_1C
    /* 0x47C */ byte   pad_47C[0x25C];
    /* 0x6D8 */ s16    field_6D8;
} ActorsShared80135a24Work;

void ActorsShared80135a24(void* arg0, Task* task);

#endif
