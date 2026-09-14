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
    /* 0x4A0 */ byte   pad_4A0[0x5E];
    /* 0x4FE */ s16    field_4FE;
    /* 0x500 */ s16    field_500;
    /* 0x502 */ s16    field_502;
} Actor323300Work;
STATIC_ASSERT_SIZEOF(Actor323300Work, 0x504);

/// Word-wise view of a `MATRIX` used to splat an identity rotation: five
/// aligned stores instead of nine halfword ones, each word holding two adjacent
/// `m[][]` entries. The same shape `Actor206100Matrix` and `Actor403100Matrix`
/// have.
typedef union Actor323300Matrix {
    MATRIX mat;
    struct {
        /* 0x00 */ s32 m00_m01;
        /* 0x04 */ s32 m02_m10;
        /* 0x08 */ s32 m11_m12;
        /* 0x0C */ s32 m20_m21;
        /* 0x10 */ s16 m22;
    } ident;
} Actor323300Matrix;
STATIC_ASSERT_SIZEOF(Actor323300Matrix, 0x20);

/// The larger of the two work blocks this overlay parks in `Task::idMap`: the
/// `Mem_Calloc(0x6B0)` that `func_actor_323300_80162BE4` allocates, as opposed
/// to the 0x504 `Actor323300Work` `func_actor_323300_80161E78` allocates. The
/// two are different allocations of different sizes, but both carry a
/// light/colour `MATRIX` pair republished onto `TmdObject::field_1C` /
/// `field_20` by the display path -- here at 0x670/0x690, so the trailing
/// `color` ends flush with the allocation. Only that pair is spelled out;
/// prefix fields are unreferenced by the bodies that write them.
typedef struct Actor323300MtxWork {
    /* 0x000 */ byte              pad_0[0x670];
    /* 0x670 */ Actor323300Matrix light;
    /* 0x690 */ Actor323300Matrix color;
} Actor323300MtxWork;
STATIC_ASSERT_SIZEOF(Actor323300MtxWork, 0x6B0);

void func_actor_323300_801626D0(Task* arg0);
void func_actor_323300_80162748(Task* arg0);
void func_actor_323300_801627B4(Task* arg0);

#endif
