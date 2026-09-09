#ifndef KYLE_800102_H
#define KYLE_800102_H

#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/3A34.h"
#include "gameplay/3FB8.h"
#include "main/session.h"
#include "main/task.h"

/// Work block for Kyle's thrown-object task. Same shape as the grenade work
/// block in `weapons/m4a1_grenade`: two `GpObj` nodes, one `GpRec18` slot for
/// each of them, and the `GpActorD4Rec` the second node points at.
typedef struct Kyle800102Work {
    /* 0x00 */ GpObj        obj;
    /* 0x20 */ GpObj        obj2;
    /* 0x40 */ GpRec18      rec0[1];
    /* 0x58 */ GpRec18      rec1[1];
    /* 0x70 */ GpActorD4Rec d4rec;
    /* 0x88 */ GpFixed16    field_88;
    /* 0x8C */ s32          field_8C;
    /* 0x90 */ s32          field_90;
    /* 0x94 */ SVECTOR      dir;
    /* 0x9C */ byte         pad_9C[4];
} Kyle800102Work;
STATIC_ASSERT_SIZEOF(Kyle800102Work, 0xA0);

/// One entry of the task's state table; the dispatcher passes the task itself.
typedef void (*Kyle800102StateFn)(Task* task);

/// Launch offset per attachment index, in the muzzle coordinate's local space.
extern SVECTOR D_kyle_800102_80177424[2];
/// Launch speed per attachment index, shifted left 16 into `field_88`.
extern u8 D_kyle_800102_8017743C[4];

void func_kyle_800102_80167A84(Task* arg0);
void func_kyle_800102_80167DE0(Task* arg0);
void func_kyle_800102_80168244(Task* arg0);
void func_kyle_800102_80168270(Task* arg0);
void func_kyle_800102_801682B4(Task* arg0);

#endif
