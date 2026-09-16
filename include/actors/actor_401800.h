#ifndef ACTOR_401800_H
#define ACTOR_401800_H

#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3FB8.h"
#include "main/task.h"
#include "main/tmd.h"

/// Private work block of the actor 401800 task, hanging off `Task::idMap`.
///
/// Only the fields the decompiled code touches are named, so the struct is
/// deliberately open-ended: the three `GpObj` display nodes the teardown hands
/// back to `Gp_UnlinkObj`, and the two child tasks it kills. The offsets match
/// the same-shaped teardown of actor 01900 (`Actor01900Work`), whose display
/// nodes sit at the same three addresses; only the child-task pair differs.
/// `field_4` is the live-actor flag `func_actor_401800_8013E138` tests, and
/// `field_A08.flags` / `field_B48.flags` are the two masks it writes. The
/// halfwords at 0x898..0x8A2 are the same animation-state slots
/// `Actor01900_Fn0A7C0` writes; `func_actor_401800_8013E194` is that body.
typedef struct Actor401800Work {
    /* 0x000 */ s16  field_0;
    /* 0x002 */ byte pad_2[2];
    /* 0x004 */ s16  field_4;
    /* 0x006 */ byte pad_6[0x54];
    /* 0x05A */ u16  field_5A;
    /* 0x05C */ byte pad_5C[0xC];
    /* 0x068 */ u16  field_68;
    /* 0x06A */ byte pad_6A[0x82A];
    /* 0x894 */ s32  field_894;
    /* 0x898 */ s16  field_898;
    /* 0x89A */ s16  field_89A;
    /* 0x89C */ byte pad_89C[2];
    /* 0x89E */ s16  field_89E;
    /* 0x8A0 */ byte pad_8A0[2];
    /* 0x8A2 */ s16  field_8A2;
    /* 0x8A4 */ s16  field_8A4;
    /* 0x8A6 */ byte pad_8A6[8];
    /* 0x8AE */ s16  field_8AE;
    /* 0x8B0 */ s16  field_8B0;
    /* 0x8B2 */ byte pad_8B2[6];
    /// `func_800FDB18` argument record: the coordinate handed to it plus the
    /// effect scale / count pair. Same slot `Actor01900Work.field_8B8` keeps.
    /* 0x8B8 */ GpEffArg field_8B8;
    /* 0x8C0 */ byte     pad_8C0[8];
    /* 0x8C8 */ GpObj    field_8C8;
    /* 0x8E8 */ byte     pad_8E8[0x120];
    /* 0xA08 */ GpObj    field_A08;
    /* 0xA28 */ byte     pad_A28[0x120];
    /* 0xB48 */ GpObj    field_B48;
    /* 0xB68 */ byte     pad_B68[0xAC];
    /// The two helper tasks killed before the nodes are unlinked; the same
    /// pair `Actor01900Work` keeps at +0xC38 / +0xC3C.
    /* 0xC14 */ Task* field_C14;
    /* 0xC18 */ Task* field_C18;
} Actor401800Work;

/// Per-task actor context: `field_1C` is the work block above (the same
/// pointer `Task::idMap` holds), `field_20` the `GpEnemy` in
/// `Task::spawnArg2`, and `field_2C` the actor's `TmdObject`. Same shape as
/// `Actor01900`.
typedef struct Actor401800 {
    /* 0x00 */ byte             pad_0[0x1C];
    /* 0x1C */ Actor401800Work* field_1C;
    /* 0x20 */ GpEnemy*         field_20;
    /* 0x24 */ byte             pad_24[8];
    /* 0x2C */ TmdObject*       field_2C;
} Actor401800;

/// Payload of the `0x3FF` message `func_actor_401800_80138F5C` sends: the same
/// 0x14-byte animation record other actors keep as `GpAnimArg` data
/// (`D_actor_356100_80173244` and friends); `field_4` is the animation id.
extern GpAnimArg D_actor_401800_80155A0C;

void func_actor_401800_80133EB8(Actor401800* arg0);
void func_actor_401800_8013E0A0(Task* task);
void func_actor_401800_8013E138(Actor401800* arg0);
void func_actor_401800_8013E194(Actor401800* arg0);
void func_actor_401800_8013E23C(Actor401800* arg0);
void func_actor_401800_8013E2E8(Actor401800* arg0);
void func_actor_401800_8013E394(Actor401800* arg0);
void func_actor_401800_8013E44C(Actor401800* arg0);

#endif // ACTOR_401800_H
