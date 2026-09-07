#ifndef ACTORS_SHARED_801368EC_H
#define ACTORS_SHARED_801368EC_H

#include "common.h"

#include "main/task.h"

/// Work block of the enemy actors that share `ActorsShared801368ec`, reached
/// through the *parent* task's `Task::idMap`. Only the two matrices the model
/// object is pointed at are modelled here; the same pair sits at the same
/// offsets in `Actor02000Work` (`include/actors/actor_102000.h`), which
/// describes the rest of the block.
typedef struct ActorsShared801368ecWork {
    /* 0x000 */ byte   pad_0[0x43C];
    /* 0x43C */ MATRIX field_43C; ///< colour matrix, handed to TmdObject::field_20
    /* 0x45C */ MATRIX field_45C; ///< light matrix, handed to TmdObject::field_1C
} ActorsShared801368ecWork;

void ActorsShared801368ec(void* arg0, Task* task);

#endif
