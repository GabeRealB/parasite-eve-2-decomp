#ifndef ACTOR_110600_H
#define ACTOR_110600_H

#include "common.h"

#include "actors/actors_shared_8013411c.h"
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
    /* 0x006 */ byte  pad_6[2];
    /* 0x008 */ s16   field_8;
    /* 0x00A */ byte  pad_A[0x882];
    /* 0x88C */ s16   field_88C;
    /* 0x88E */ s16   field_88E;
    /* 0x890 */ byte  pad_890[2];
    /* 0x892 */ s16   field_892;
    /* 0x894 */ byte  pad_894[2];
    /* 0x896 */ s16   field_896;
    /* 0x898 */ byte  pad_898[0xC];
    /* 0x8A4 */ s16   field_8A4;
    /* 0x8A6 */ byte  pad_8A6[0x12];
    /* 0x8B8 */ GpObj field_8B8;
    /* 0x8D8 */ byte  pad_8D8[0x78];
    /* 0x950 */ GpObj field_950;
    /* 0x970 */ byte  pad_970[0x120];
    /* 0xA90 */ GpObj field_A90;
    /* 0xAB0 */ byte  pad_AB0[0xCC];
    /* 0xB7C */ u16   field_B7C;
    /* 0xB7E */ byte  pad_B7E[4];
    /* 0xB82 */ s16   field_B82;
    /* 0xB84 */ byte  pad_B84[0x50];
    /* 0xBD4 */ Task* field_BD4;
    /* 0xBD8 */ Task* field_BD8;
    /* 0xBDC */ byte  pad_BDC[4];
    /* 0xBE0 */ s16   field_BE0;
    /* 0xBE2 */ byte  pad_BE2[2];
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

/// Rebuilds `coord`'s Y rotation from its current yaw (`ratan2` of
/// `-m[2][0], m[2][2]`), scaled independently on each axis through a
/// 0x34-byte block borrowed from the scratchpad. Marks the coordinate dirty.
void func_actor_110600_80138680(GsCOORDINATE2* coord, s16 sx, s16 sy, s16 sz);

s32  func_actor_110600_801387C0(Task* arg0);
void func_actor_110600_801388A4(Actor110600* arg0);

/// The actor's per-tick model update, driven from `Task::idMap` /
/// `Task::spawnArg2` off the pointer it is handed.
void func_actor_110600_80134728(Actor110600* arg0);

/// Enters work state 2 (`field_88C`) on a live actor: clear the model object,
/// clear bit 0x8000 of `field_A90.flags` and set 0x4000 of `field_950.flags`,
/// tag the enemy's link node, arm the `field_892` / `field_896` timers, then run
/// 20 update ticks before parking `field_896` at -8 and ticking once more.
void func_actor_110600_80138CA4(Actor110600* arg0);

/// Placement opcode: seeds the model's root coordinate from `placement`, then
/// rebuilds and rescales it from the actor's own heading.
s32 func_actor_110600_80133E48(Task* task, s32 arg1, ActorShared8013411cPlacement* placement);

/// Five-frame shake counter. Incremented each call, wraps at 5, and drives
/// `Display_ClampField126` with the low bit (0 or 1). Returns 1 on wrap.
extern s16 D_actor_110600_8014865C;
s32        func_actor_110600_80138900(void);

/// `Task::exitCallback` installed by the spawn handler: bump the two helper
/// tasks' `state` if present, unlink the three display nodes, drop the enemy's
/// `field_54` slot, clear the screen shake, then `Gp_DestroyEnemy`.
void func_actor_110600_801387F4(Task* task);

#endif
