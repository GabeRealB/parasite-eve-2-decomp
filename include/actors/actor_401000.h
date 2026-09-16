#ifndef ACTOR_401000_H
#define ACTOR_401000_H

#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3FB8.h"
#include "main/task.h"
#include "main/tmd.h"

/// Private work block of the actor 401000 task, hanging off `Task::idMap`.
///
/// Only the fields the decompiled code touches are named, so the struct is
/// deliberately open-ended: the three `GpObj` display nodes the teardown hands
/// back to `Gp_UnlinkObj`, and the two child tasks it kills. The nodes sit
/// 8 bytes later than the 0x8C8/0xA08/0xB48 triple on actor 01900/401800, with
/// the same 0x140 stride.
/// `field_4` is the live-actor flag `func_actor_401000_8013DB10` tests, and
/// `field_B50.flags` / `field_A10.flags` are the two masks it writes. The
/// halfwords at 0x898..0x8A2 are the same animation-state slots
/// `Actor01900_Fn0A7C0` writes; `func_actor_401000_8013DB6C` is that body
/// with `field_A10.flags |= 0x4000` in place of the sibling's `&= 0xBFFF`.
typedef struct Actor401000Work {
    /* 0x000 */ s16      field_0;
    /* 0x002 */ byte     pad_2[2];
    /* 0x004 */ s16      field_4;
    /* 0x006 */ byte     pad_6[0x54];
    /* 0x05A */ u16      field_5A;
    /* 0x05C */ byte     pad_5C[0xC];
    /* 0x068 */ u16      field_68;
    /* 0x06A */ byte     pad_6A[0x82A];
    /* 0x894 */ s32      field_894;
    /* 0x898 */ s16      field_898;
    /* 0x89A */ s16      field_89A;
    /* 0x89C */ byte     pad_89C[2];
    /* 0x89E */ s16      field_89E;
    /* 0x8A0 */ byte     pad_8A0[2];
    /* 0x8A2 */ s16      field_8A2;
    /* 0x8A4 */ byte     pad_8A4[0x14];
    /* 0x8B8 */ GpEffArg field_8B8;
    /* 0x8C0 */ byte     pad_8C0[0x10];
    /* 0x8D0 */ GpObj    field_8D0;
    /* 0x8F0 */ byte     pad_8F0[0x120];
    /* 0xA10 */ GpObj    field_A10;
    /* 0xA30 */ byte     pad_A30[0x120];
    /* 0xB50 */ GpObj    field_B50;
    /* 0xB70 */ byte     pad_B70[0xAC];
    /// The two helper tasks killed before the nodes are unlinked; the same
    /// pair `Actor01900Work` keeps at +0xC38 / +0xC3C.
    /* 0xC1C */ Task* field_C1C;
    /* 0xC20 */ Task* field_C20;
} Actor401000Work;

/// Per-task actor context: `field_1C` is the work block above (the same
/// pointer `Task::idMap` holds), `field_20` the `GpEnemy` in
/// `Task::spawnArg2`, and `field_2C` the actor's `TmdObject`. Same shape as
/// `Actor01900`.
typedef struct Actor401000 {
    /* 0x00 */ byte             pad_0[0x1C];
    /* 0x1C */ Actor401000Work* field_1C;
    /* 0x20 */ GpEnemy*         field_20;
    /* 0x24 */ byte             pad_24[8];
    /* 0x2C */ TmdObject*       field_2C;
} Actor401000;

/// Message 0x3FF payload of `func_actor_401000_801383F0` and
/// `func_actor_401000_801385B0`: the animation argument the player task reads
/// when the actor's live-actor flag goes up.
extern GpAnimArg D_actor_401000_80154F1C;

void func_actor_401000_80132EF0(Actor401000* arg0);

/// `Task::exitCallback` teardown: kill the two helper tasks, unlink the three
/// display nodes, drop the enemy's `field_54` slot, then `Gp_DestroyEnemy`.
void func_actor_401000_8013DA78(Task* task);
void func_actor_401000_8013DB10(Actor401000* arg0);
void func_actor_401000_8013DB6C(Actor401000* arg0);
void func_actor_401000_8013DC14(Actor401000* arg0);
void func_actor_401000_8013DCC0(Actor401000* arg0);

#endif // ACTOR_401000_H
