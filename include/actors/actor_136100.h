#ifndef ACTOR_136100_H
#define ACTOR_136100_H

#include "common.h"

#include "main/task.h"

/// Work block for the `actor_136100` overlay's cutscene actor.
///
/// `func_actor_136100_80133A88` allocates it with `Mem_Malloc(0x4F0, 0)`,
/// zeroes it with `Mem_Set` and parks the pointer in the task's `Task::idMap`
/// slot (0x1C) -- that slot is not a `TaskIdMap` here, so reach the block with
/// `(Actor136100Work*)task->idMap`.  The same function publishes the task
/// itself in `D_actor_136100_8014078C` and stores the `Game_GetPtrSlot(3)`
/// task in `field_4B4`.
///
/// The block opens with the 0x14-byte animation context and its twenty
/// 0x28-byte animation slots -- `func_actor_136100_80133A88` hands
/// `func_800B3F84` both `work + 0x14` and `work + 0x334`, and
/// `func_actor_136100_801347B8` walks slots 1..19 through `Gp_AnimResetSlot`.
/// The three pairs at 0x4C4, 0x4CC and 0x4D4 are value/countdown pairs the
/// overlay's small setters write together.
typedef struct Actor136100Work {
    /* 0x000 */ byte  pad_0[0x4B4];
    /* 0x4B4 */ Task* field_4B4; // Game_GetPtrSlot(3) task
    /* 0x4B8 */ byte  pad_4B8[0xC];
    /* 0x4C4 */ s16   field_4C4; // set by func_actor_136100_80134838
    /* 0x4C6 */ s16   field_4C6; // cleared alongside field_4C4
    /* 0x4C8 */ byte  pad_4C8[0x4];
    /* 0x4CC */ s16   field_4CC; // set by func_actor_136100_80134858
    /* 0x4CE */ s16   field_4CE; // cleared alongside field_4CC
    /* 0x4D0 */ byte  pad_4D0[0x4];
    /* 0x4D4 */ s16   field_4D4; // set by func_actor_136100_80134878
    /* 0x4D6 */ s16   field_4D6; // cleared alongside field_4D4
    /* 0x4D8 */ byte  pad_4D8[0x8];
    /* 0x4E0 */ s16   field_4E0; // animation slot count reset by func_actor_136100_801347B8
    /* 0x4E2 */ byte  pad_4E2[0xA];
    /* 0x4EC */ s16   field_4EC; // player-eff flag: Gp_KillPlayerEffs / Gp_SpawnWeaponEff
    /* 0x4EE */ byte  pad_4EE[0x2];
} Actor136100Work;
STATIC_ASSERT_SIZEOF(Actor136100Work, 0x4F0);

#endif // ACTOR_136100_H
