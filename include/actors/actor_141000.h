#ifndef ACTOR_141000_H
#define ACTOR_141000_H

#include "common.h"

#include <psyq/libgte.h>

#include "main/task.h"

typedef struct Actor141000 Actor141000;

typedef struct Actor141000Point {
    /* 0x0 */ s16  field_0;
    /* 0x2 */ s16  field_2;
    /* 0x4 */ byte pad_4[4];
} Actor141000Point;

/// Work block allocated by `func_actor_141000_8013392C` (`Mem_Calloc(0x4CC)`)
/// and parked in that task's `Task::idMap` slot -- that slot is not a
/// `TaskIdMap` here. `func_actor_141000_801339DC` republishes the two matrices
/// onto `TmdObject::field_1C` / `field_20`, the light/colour pair
/// `Gp_BindDefaultMtx` otherwise points at `Gp_DefaultMtx` / `Gp_DefaultMtx2`,
/// exactly as `func_actor_350700_801624B4` does for `Actor350700Work`.
///
/// The size is the allocation, and the fields below are the ones the init
/// seeds: the three `sb` bytes at 0x43D/0x43E/0x4C9 are set to -1, and the
/// three words at 0x4A0..0x4A8 are cleared.
typedef struct Actor141000Work {
    /* 0x000 */ byte   pad_0[0x43D];
    /* 0x43D */ s8     field_43D;
    /* 0x43E */ s8     field_43E;
    /* 0x43F */ byte   pad_43F[0x1];
    /* 0x440 */ MATRIX light;
    /* 0x460 */ MATRIX color;
    /* 0x480 */ byte   pad_480[0x20];
    /* 0x4A0 */ s32    field_4A0;
    /* 0x4A4 */ s32    field_4A4;
    /* 0x4A8 */ s32    field_4A8;
    /* 0x4AC */ byte   pad_4AC[0x1D];
    /* 0x4C9 */ s8     field_4C9;
    /* 0x4CA */ byte   pad_4CA[0x2];
} Actor141000Work;
STATIC_ASSERT_SIZEOF(Actor141000Work, 0x4CC);

void func_actor_141000_801339DC(Task* arg0);
void func_actor_141000_80131E94(Actor141000* arg0, Actor141000Point* arg1, s32 arg2);
void func_actor_141000_801323F0(Actor141000* arg0, Actor141000Point* arg1, s32* arg2, s32* arg3);
void func_actor_141000_80133260(Actor141000* arg0);

#endif
