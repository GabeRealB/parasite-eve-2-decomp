#ifndef ACTORS_SHARED_80138570_H
#define ACTORS_SHARED_80138570_H

#include "common.h"

#include "main/task.h"

#include "gameplay/3A34.h"

/// Work block the enemy's spawn function parks in the task's `Task::idMap`
/// slot (that slot is not a `TaskIdMap` here). Only the three `GpObj` display
/// nodes are reached from this shared body -- `ActorsShared80138570` is the
/// exit callback that unlinks all three -- so the type stops after the last
/// one; whatever each overlay keeps around them differs per actor.
typedef struct ActorShared80138570Work {
    /* 0x000 */ byte  pad_0[0x1DC];
    /* 0x1DC */ GpObj field_1DC;
    /* 0x1FC */ byte  pad_1FC[0x30];
    /* 0x22C */ GpObj field_22C;
    /* 0x24C */ byte  pad_24C[0x60];
    /* 0x2AC */ GpObj field_2AC;
} ActorShared80138570Work;

void ActorsShared80138570(Task* task);

#endif
