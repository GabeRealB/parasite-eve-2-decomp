#ifndef ACTORS_SHARED_80133474_H
#define ACTORS_SHARED_80133474_H

#include "common.h"

#include "main/task.h"

/// Work block the actors sharing this body hang off the task's `Task::work`
/// slot (0x1C), which is not a `TaskIdMap` here. The carriers allocate blocks
/// of different sizes, so only the prefix this body reaches is described.
///
/// `field_4F4` is the task the carrier's spawn routine stores after spawning
/// its enemy through `Gp_SpawnEnemyFromTable` / `Task_Reparent`; the model this
/// opcode drives alongside the actor's own is that task's.
typedef struct ActorsShared80133474Work {
    /* 0x000 */ byte  pad_0[0x4F4];
    /* 0x4F4 */ Task* field_4F4;
} ActorsShared80133474Work;

/// Script opcode: set the visibility flags of the actor's model and of the
/// model owned by the task its spawn routine paired it with. `flags` bit 0
/// hides both models (`TmdObject::flags` = 0) and its absence restores the
/// default 0x80; bit 1 additionally ORs in 0x4, the same bit `Tmd_Create` sets
/// for its own `flags & 1`. With no enemy paired (`Task::spawnArg1` == 0) the
/// actor drives its own model twice.
///
/// The `actor_460200` carriers reach the pair task at 0x4F4; the twin
/// `func_actor_460200_80132B98` in that overlay belongs to its 0x4F8-sized
/// class, whose pair task sits at 0x4F0, and is not part of this body.
s32 ActorsShared80133474(Task* task, s32 arg1, s32 flags);

#endif
