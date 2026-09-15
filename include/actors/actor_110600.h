#ifndef ACTOR_110600_H
#define ACTOR_110600_H

#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "main/task.h"
#include "main/tmd.h"

/// Work block this overlay parks in the task's `Task::idMap` slot (0x1C),
/// which is not a `TaskIdMap` here. `func_actor_110600_80134AB4` allocates it
/// with `Mem_Calloc(0xBEC, 0)`, so the size below is the allocation.
///
/// `field_0` is the state index `func_actor_110600_801387C0` writes. `field_4`
/// is the live-actor flag `func_actor_110600_801388A4` tests, and
/// `field_A90.flags` / `field_950.flags` are the two masks it writes. The three
/// `GpObj` display nodes are the ones the spawn handler links (with `GpRec18`
/// tables of 5 / 12 / 1 records filling the gaps) and the exit callback
/// `func_actor_110600_801387F4` hands back to `Gp_UnlinkObj`. `field_BD4` /
/// `field_BD8` are optional helpers the spawn clears; teardown increments
/// `Task::state` when they are non-NULL.
typedef struct Actor110600Work {
    /* 0x000 */ s16   field_0;
    /* 0x002 */ byte  pad_2[2];
    /* 0x004 */ s16   field_4;
    /* 0x006 */ byte  pad_6[0x8B2];
    /* 0x8B8 */ GpObj field_8B8;
    /* 0x8D8 */ byte  pad_8D8[0x78];
    /* 0x950 */ GpObj field_950;
    /* 0x970 */ byte  pad_970[0x120];
    /* 0xA90 */ GpObj field_A90;
    /* 0xAB0 */ byte  pad_AB0[0x124];
    /* 0xBD4 */ Task* field_BD4;
    /* 0xBD8 */ Task* field_BD8;
    /* 0xBDC */ byte  pad_BDC[8];
    /* 0xBE4 */ s16   field_BE4;
    /* 0xBE6 */ s16   field_BE6;
    /* 0xBE8 */ byte  pad_BE8[4];
} Actor110600Work;
STATIC_ASSERT_SIZEOF(Actor110600Work, 0xBEC);

/// Per-task actor context: `field_1C` is the work block above (the same
/// pointer `Task::idMap` holds), `field_20` the `GpEnemy` in
/// `Task::spawnArg2`, and `field_2C` the actor's `TmdObject`. Same shape as
/// `Actor01900` / `Actor401800`.
typedef struct Actor110600 {
    /* 0x00 */ byte             pad_0[0x1C];
    /* 0x1C */ Actor110600Work* field_1C;
    /* 0x20 */ GpEnemy*         field_20;
    /* 0x24 */ byte             pad_24[8];
    /* 0x2C */ TmdObject*       field_2C;
} Actor110600;

s32  func_actor_110600_801387C0(Task* arg0);
void func_actor_110600_801388A4(Actor110600* arg0);

/// Five-frame shake counter. Incremented each call, wraps at 5, and drives
/// `Display_ClampField126` with the low bit (0 or 1). Returns 1 on wrap.
extern s16 D_actor_110600_8014865C;
s32        func_actor_110600_80138900(void);

/// `Task::exitCallback` installed by the spawn handler: bump the two helper
/// tasks' `state` if present, unlink the three display nodes, drop the enemy's
/// `field_54` slot, clear the screen shake, then `Gp_DestroyEnemy`.
void func_actor_110600_801387F4(Task* task);

#endif
