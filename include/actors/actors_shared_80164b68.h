#ifndef ACTORS_SHARED_80164B68_H
#define ACTORS_SHARED_80164B68_H

#include "common.h"

#include "gameplay/3A34.h"
#include "main/task.h"

/// Work block the actors sharing this body hang off the task's `Task::idMap`
/// slot (0x1C), which is not a `TaskIdMap` here. Each overlay's spawn routine
/// allocates it with `Mem_Calloc` and stores it into that field; the carriers
/// ask for different sizes, so only the prefix this body reaches is described
/// here rather than a whole-block size that would be wrong for some of them.
///
/// The three `GpObj` nodes are the actor's collision objects. `obj_2AC` and
/// `obj_2CC` share the eight-slot record table at `rec_2EC`; `obj_3AC` has its
/// own two-slot table at `rec_3CC`.
typedef struct ActorsShared80164b68Work {
    /* 0x000 */ byte    pad_0[0x2AC];
    /* 0x2AC */ GpObj   obj_2AC;
    /* 0x2CC */ GpObj   obj_2CC;
    /* 0x2EC */ GpRec18 rec_2EC[8];
    /* 0x3AC */ GpObj   obj_3AC;
    /* 0x3CC */ GpRec18 rec_3CC[2];
} ActorsShared80164b68Work;
STATIC_ASSERT_SIZEOF(ActorsShared80164b68Work, 0x3FC);

/// Links the actor's three collision objects onto `Gp_ObjLists[2]` and clears
/// their record tables.
void ActorsShared80164b68(Task* task);

#endif
