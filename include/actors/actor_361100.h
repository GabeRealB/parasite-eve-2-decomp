#ifndef ACTOR_361100_H
#define ACTOR_361100_H

#include "common.h"

#include <psyq/libgte.h>

#include "main/task.h"
#include "main/tmd.h"

/// Work block allocated by `func_actor_361100_80163410` (`Mem_Calloc(0x4A4)`)
/// and parked in that task's `Task::idMap` slot -- that slot is not a
/// `TaskIdMap` here. `func_actor_361100_801634B4` republishes the two matrices
/// onto `TmdObject::field_1C` / `field_20`, the light/colour pair
/// `Gp_BindDefaultMtx` otherwise points at `Gp_DefaultMtx` / `Gp_DefaultMtx2`,
/// exactly as `func_actor_350700_801624B4` does for `Actor350700Work`.
///
/// The size is the allocation, and the fields below are the ones the inits
/// seed: the three `sb` bytes at 0x43D/0x43E/0x4A2 are set to -1, and
/// `func_actor_361100_80162D28` also clears the three words at 0x480..0x488.
typedef struct Actor361100Work {
    /* 0x000 */ byte   pad_0[0x43D];
    /* 0x43D */ s8     field_43D;
    /* 0x43E */ s8     field_43E;
    /* 0x43F */ byte   pad_43F[0x1];
    /* 0x440 */ MATRIX light;
    /* 0x460 */ MATRIX color;
    /* 0x480 */ s32    field_480;
    /* 0x484 */ s32    field_484;
    /* 0x488 */ s32    field_488;
    /* 0x48C */ byte   pad_48C[0x16];
    /* 0x4A2 */ s8     field_4A2;
    /* 0x4A3 */ byte   pad_4A3[0x1];
} Actor361100Work;
STATIC_ASSERT_SIZEOF(Actor361100Work, 0x4A4);

void func_actor_361100_801634B4(Task* arg0);

#endif
