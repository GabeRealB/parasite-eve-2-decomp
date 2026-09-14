#ifndef ACTOR_350500_H
#define ACTOR_350500_H

#include "common.h"

#include <psyq/libgte.h>

#include "main/task.h"
#include "main/tmd.h"

/// Work block allocated by `func_actor_350500_801623CC` (`Mem_Calloc(0x4C8)`)
/// and parked in that task's `Task::idMap` slot -- that slot is not a
/// `TaskIdMap` here. `func_actor_350500_8016247C` republishes the two matrices
/// onto `TmdObject::field_1C` / `field_20`, the light/colour pair
/// `Gp_BindDefaultMtx` otherwise points at `Gp_DefaultMtx` / `Gp_DefaultMtx2`,
/// exactly as `func_actor_335800_80163B54` does for `Actor335800Work`.
///
/// The size is the allocation, and the fields below are the ones the init
/// seeds: the three `sb` bytes at 0x43D/0x43E/0x4C5 are set to -1, and the
/// three words at 0x4A0..0x4A8 are cleared. `field_4C4` is latched by the
/// two-case message handler `func_actor_350500_80162ABC`.
typedef struct Actor350500Work {
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
    /* 0x4AC */ byte   pad_4AC[0x18];
    /* 0x4C4 */ s8     field_4C4; // variant the two-case message handler latches
    /* 0x4C5 */ s8     field_4C5;
    /* 0x4C6 */ byte   pad_4C6[0x2];
} Actor350500Work;
STATIC_ASSERT_SIZEOF(Actor350500Work, 0x4C8);

/// Third argument `func_actor_350500_80162ABC` reads: the same 4-byte record as
/// `Actor141000Msg` and `Actor350700Msg`, whose halfword at 0x2 -- `field_0` is
/// not read here -- selects the variant the handler latches into
/// `Actor350500Work::field_4C4`.
typedef struct Actor350500Msg {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
} Actor350500Msg;
STATIC_ASSERT_SIZEOF(Actor350500Msg, 0x4);

void func_actor_350500_8016247C(Task* arg0);

s32 func_actor_350500_80162ABC(Task* arg0, s32 arg1, Actor350500Msg* arg2);

#endif
