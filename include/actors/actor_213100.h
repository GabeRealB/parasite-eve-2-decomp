#ifndef ACTOR_213100_H
#define ACTOR_213100_H

#include "common.h"

#include <psyq/libgte.h>

#include "main/task.h"
#include "main/tmd.h"

/// Work block allocated by `func_actor_213100_8014A118` (`Mem_Calloc(0x488)`)
/// and parked in that task's `Task::idMap` slot -- that slot is not a
/// `TaskIdMap` here. `func_actor_213100_8014A23C` republishes the two matrices
/// onto `TmdObject::field_1C` / `field_20`, the light/colour pair
/// `Gp_BindDefaultMtx` otherwise points at `Gp_DefaultMtx` / `Gp_DefaultMtx2`,
/// exactly as `func_actor_503500_801324EC` does for `Actor503500ColorMtx`.
///
/// The size is the allocation, and every field below is anchored by an access
/// in this overlay: the three `lb`/`sb` bytes at 0x43C..0x43E (the init sets
/// 0x43D and 0x43E to -1), the matrix pair, and the child task the init spawns
/// with `Task_SpawnFromTable` next to its -1 seed word.
typedef struct Actor213100Work {
    /* 0x000 */ byte          pad_0[0x43C];
    /* 0x43C */ s8            field_43C;
    /* 0x43D */ s8            field_43D;
    /* 0x43E */ s8            field_43E;
    /* 0x43F */ byte          pad_43F[0x1];
    /* 0x440 */ MATRIX        light;
    /* 0x460 */ MATRIX        color;
    /* 0x480 */ struct _Task* field_480;
    /* 0x484 */ s32           field_484;
} Actor213100Work;
STATIC_ASSERT_SIZEOF(Actor213100Work, 0x488);

void func_actor_213100_8014A23C(Task* arg0);

#endif
