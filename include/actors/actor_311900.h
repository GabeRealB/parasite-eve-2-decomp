#ifndef ACTOR_311900_H
#define ACTOR_311900_H

#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/1BC.h"
#include "main/task.h"
#include "main/tmd.h"

/// A `MATRIX` plus the word-wise view `func_actor_311900_8016278C` splats the
/// light / colour pair through: five aligned stores rather than nine halfword
/// ones (the same idiom as `ActorsShared8016a538Mat` and `Actor141000MatWords`).
typedef union Actor311900MatWords {
    MATRIX mat;
    struct {
        /* 0x00 */ s32 m00_m01;
        /* 0x04 */ s32 m02_m10;
        /* 0x08 */ s32 m11_m12;
        /* 0x0C */ s32 m20_m21;
        /* 0x10 */ s16 m22;
    } ident;
} Actor311900MatWords;
STATIC_ASSERT_SIZEOF(Actor311900MatWords, 0x20);

/// Animation view of the work block's 0x474-byte prefix. The spawn handler
/// hands the block itself to `func_800B3F84` as a `GpAnimCtx`, the
/// `GpAnimSlot` array at 0x14 as its fourth argument and the packed-pose run
/// at 0x334 as its third -- `func_800B3448` reaches that run as `GpPackedSvec`
/// at a 0x10 stride, one group per slot. Both counts are what fills the gap:
/// the 20 slots of 0x28 reach 0x334 and the 20 pose groups reach 0x474.
typedef struct Actor311900Anim {
    /* 0x000 */ GpAnimCtx  context;
    /* 0x014 */ GpAnimSlot slots[0x14];
    /* 0x334 */ byte       poses[0x140];
} Actor311900Anim;
STATIC_ASSERT_SIZEOF(Actor311900Anim, 0x474);

/// Work block allocated by the spawn state `func_actor_311900_8016228C`
/// (`Mem_Calloc(0x4CC)`) and parked in that task's `Task::work` slot -- that
/// slot is not a `TaskIdMap` here. `func_actor_311900_8016278C` republishes the
/// two matrices onto `TmdObject::field_1C` / `field_20`, the light / colour pair
/// `Gp_BindDefaultMtx` otherwise points at `Gp_DefaultMtx` / `Gp_DefaultMtx2`,
/// exactly as `func_actor_350700_801624B4` does for `Actor350700Work`.
///
/// The size is the allocation, and the fields below are the ones the spawn
/// state seeds: 2 into the halfword at 0x474, 1 into the one at 0x478, and
/// zero into 0x4C4 / 0x4C6. `func_actor_311900_80162100` turns that pair into
/// the animation request `field_474` and the two ids beside it: `field_478` is
/// the id the slots are seeded with, `field_476` latches it as the one now
/// playing once the slots have been seeded, `field_47C` is the rate byte every
/// seeding writes into `GpAnimSlot::field_9`, and `field_47A` counts frames
/// while `field_474` is 3 -- the running step, which is where both seeding
/// steps leave it.
typedef struct Actor311900Work {
    /* 0x000 */ Actor311900Anim anim;
    /* 0x474 */ s16             field_474;
    /* 0x476 */ s16             field_476;
    /* 0x478 */ u16             field_478;
    /* 0x47A */ u16             field_47A;
    /* 0x47C */ u8              field_47C;
    /* 0x47D */ byte            pad_47D[0x7];
    /* 0x484 */ MATRIX          light;
    /* 0x4A4 */ MATRIX          color;
    /* 0x4C4 */ u16             field_4C4;
    /* 0x4C6 */ u16             field_4C6;
    /* 0x4C8 */ u8              field_4C8; ///< CLUT grey-fade step, func_actor_311900_80161E3C
    /* 0x4C9 */ byte            pad_4C9[0x3];
} Actor311900Work;
STATIC_ASSERT_SIZEOF(Actor311900Work, 0x4CC);

void func_actor_311900_8016278C(Task* task);

s32 func_actor_311900_80162658(GsCOORDINATE2* arg0, s16 arg1);

#endif
