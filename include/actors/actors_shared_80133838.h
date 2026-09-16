#ifndef ACTORS_SHARED_80133838_H
#define ACTORS_SHARED_80133838_H

#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

/// Work block the enemies carrying this body reach as `task->idMap`, which is
/// not a `TaskIdMap` here. `field_42` is the despawn frame counter the body
/// ticks and destroys the enemy at, and `field_46` selects which of the two
/// per-enemy death flags the first tick raises. The third field is read from
/// the *parent* task's block: it is the sound id the teardown queues.
typedef struct ActorsShared80133838Work {
    /* 0x000 */ byte pad_0[0x42];
    /* 0x042 */ u16  field_42;
    /* 0x044 */ byte pad_44[2];
    /* 0x046 */ s16  field_46;
    /* 0x048 */ byte pad_48[0x2D4];
    /* 0x31C */ s32  field_31C;
} ActorsShared80133838Work;

/// The task carrying that block, reached as `task->field_1C`; the body asks the
/// same view of `task->parent` for the sound id.
typedef struct ActorsShared80133838Task {
    /* 0x00 */ byte                      pad_0[0x1C];
    /* 0x1C */ ActorsShared80133838Work* field_1C;
} ActorsShared80133838Task;

void ActorsShared80133838(GpEnemy* enemy, Task* task);

#endif // ACTORS_SHARED_80133838_H
