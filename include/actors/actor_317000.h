#ifndef ACTOR_317000_H
#define ACTOR_317000_H

#include "common.h"

#include <psyq/libgte.h>

#include "main/task.h"
#include "main/tmd.h"

/// Work block allocated by `func_actor_317000_8016267C` (`Mem_Calloc(0x4CC)`)
/// and parked in that task's `Task::idMap` slot -- that slot is not a
/// `TaskIdMap` here. `func_actor_317000_80162744` republishes the two matrices
/// onto `TmdObject::field_1C` / `field_20`, the light/colour pair
/// `Gp_BindDefaultMtx` otherwise points at `Gp_DefaultMtx` / `Gp_DefaultMtx2`,
/// exactly as `func_actor_350700_801624B4` does for `Actor350700Work`.
///
/// The size is the allocation, and the fields below are the ones the init
/// seeds: the two `sb` bytes at 0x43D/0x43E and the `sh` at 0x4C8 are set to
/// -1, and the three words at 0x4A0..0x4A8 are cleared.
typedef struct Actor317000Work {
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
    /* 0x4AC */ byte   pad_4AC[0x1C];
    /* 0x4C8 */ s16    field_4C8;
    /* 0x4CA */ byte   pad_4CA[0x2];
} Actor317000Work;
STATIC_ASSERT_SIZEOF(Actor317000Work, 0x4CC);

void func_actor_317000_80162744(Task* arg0);

#endif
