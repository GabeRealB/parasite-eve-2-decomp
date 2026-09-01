#ifndef ACTOR_323300_H
#define ACTOR_323300_H

#include "common.h"

#include <psyq/libgte.h>

#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/3A34.h"

/// Work block allocated by `func_actor_323300_80161E78` (`Mem_Calloc(0x504)`)
/// and parked in that task's `Task::idMap` slot -- that slot is not a
/// `TaskIdMap` here. `func_actor_323300_801626D0` republishes the two matrices
/// onto `TmdObject::field_1C` / `field_20`, the light/colour pair
/// `Gp_BindDefaultMtx` otherwise points at `Gp_DefaultMtx` / `Gp_DefaultMtx2`,
/// exactly as `func_actor_350500_8016247C` does for `Actor350500Work`.
///
/// The display node at +0x480 is the one the exit callback
/// `func_actor_323300_8016269C` hands back to `Gp_UnlinkObj` before tearing
/// the enemy task down. The size is the allocation, and the fields below are
/// the ones the init seeds: the two `sb` bytes at 0x43D/0x43E and the `sh` at
/// 0x502 are set to -1, and 0x500 is set to 1.
typedef struct Actor323300Work {
    /* 0x000 */ byte   pad_0[0x43D];
    /* 0x43D */ s8     field_43D;
    /* 0x43E */ s8     field_43E;
    /* 0x43F */ byte   pad_43F[0x1];
    /* 0x440 */ MATRIX light;
    /* 0x460 */ MATRIX color;
    /* 0x480 */ GpObj  obj;
    /* 0x4A0 */ byte   pad_4A0[0x60];
    /* 0x500 */ s16    field_500;
    /* 0x502 */ s16    field_502;
} Actor323300Work;
STATIC_ASSERT_SIZEOF(Actor323300Work, 0x504);

void func_actor_323300_801626D0(Task* arg0);

#endif
