#ifndef ACTORS_SHARED_80136930_H
#define ACTORS_SHARED_80136930_H

#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

/// Work block of the enemy actors that share `ActorsShared80136930`, reached
/// through the *parent* task's `Task::idMap`. Only the spawn table and the
/// request flag are modelled here; both sit at the same offsets in
/// `Actor02000Work` (`include/actors/actor_102000.h`), which describes the
/// rest of the block.
typedef struct ActorsShared80136930Work {
    /* 0x000 */ byte      pad_0[0x66C];
    /* 0x66C */ TaskDesc* field_66C; ///< spawn table for the companion effect
    /* 0x670 */ byte      pad_670[0x4A];
    /* 0x6BA */ s16       field_6BA; ///< non-zero asks for a spawn; cleared on use
} ActorsShared80136930Work;

void ActorsShared80136930(GpEnemy* enemy, Task* task);

#endif
