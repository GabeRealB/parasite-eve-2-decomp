#ifndef ACTOR_120400_H
#define ACTOR_120400_H

#include "common.h"

#include <psyq/libgte.h>

#include "main/task.h"
#include "main/tmd.h"

/// Work block allocated by `func_actor_120400_80131E5C` (`Mem_Calloc(0x504)`)
/// and parked in that task's `Task::idMap` slot -- that slot is not a
/// `TaskIdMap` here, just as with `Actor335800MainWork` / `Actor350700MainWork`,
/// which this body is a two-child copy of: the init seeds the two `sb` bytes at
/// 0x475/0x476 and the `sh` at 0x500 to -1 and clears the three words at
/// 0x4D8..0x4E0, then spawns its two children from `D_actor_120400_8013E748`.
/// `ActorsShared80132f24` republishes the light/colour matrix pair onto the
/// parent's `TmdObject::field_1C` / `field_20` from 0x478 / 0x498, exactly as it
/// does for those two.
///
/// The size is the allocation; the fields below are the ones the init touches.
typedef struct Actor120400MainWork {
    /* 0x000 */ byte pad_0[0x475];
    /* 0x475 */ s8   field_475;
    /* 0x476 */ s8   field_476;
    /* 0x477 */ byte pad_477[0x4D8 - 0x477];
    /* 0x4D8 */ s32  field_4D8;
    /* 0x4DC */ s32  field_4DC;
    /* 0x4E0 */ s32  field_4E0;
    /* 0x4E4 */ byte pad_4E4[0x500 - 0x4E4];
    /* 0x500 */ s16  field_500;
    /* 0x502 */ byte pad_502[0x504 - 0x502];
} Actor120400MainWork;
STATIC_ASSERT_SIZEOF(Actor120400MainWork, 0x504);

#endif
