#ifndef ACTORS_SHARED_80135B64_H
#define ACTORS_SHARED_80135B64_H

#include "common.h"

#include "main/task.h"

/// Work block of the enemy actors that share `ActorsShared80135b64`, reached
/// through the *parent* task's `Task::idMap`. Only the two matrices the model
/// object is pointed at are modelled here; the same pair sits at the same
/// offsets in `Actor02000Work` (`include/actors/actor_102000.h`), which
/// describes the rest of the block.
typedef struct ActorsShared80135b64Work {
    /* 0x000 */ byte   pad_0[0x43C];
    /* 0x43C */ MATRIX field_43C; ///< colour matrix, handed to TmdObject::field_20
    /* 0x45C */ MATRIX field_45C; ///< light matrix, handed to TmdObject::field_1C
} ActorsShared80135b64Work;

void ActorsShared80135b64(void* arg0, Task* task);

#endif
