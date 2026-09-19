#ifndef ACTOR_213100_H
#define ACTOR_213100_H

#include "common.h"

#include <psyq/libgte.h>

#include "main/task.h"
#include "main/tmd.h"

/// Work block allocated by `func_actor_213100_8014A118` (`memCalloc(0x488)`)
/// and parked in that task's `Task::work` slot -- that slot is not a
/// `TaskIdMap` here. `func_actor_213100_8014A23C` republishes the two matrices
/// onto `TmdObject::lightMtx` / `field_20`, the light/colour pair
/// `Gp_BindDefaultMtx` otherwise points at `Gp_DefaultMtx` / `Gp_DefaultMtx2`,
/// exactly as `func_actor_503500_801324EC` does for `Actor503500ColorMtx`.
///
/// The size is the allocation, and every field below is anchored by an access
/// in this overlay: the three `lb`/`sb` bytes at 0x43C..0x43E (the init sets
/// 0x43D and 0x43E to -1), the matrix pair, and the child task the init spawns
/// with `Task_SpawnFromTable` next to its -1 seed word.
typedef struct Actor213100Work {
    /* 0x000 */ byte         pad_0[0x43C];
    /* 0x43C */ s8           field_43C;
    /* 0x43D */ s8           field_43D;
    /* 0x43E */ s8           field_43E;
    /* 0x43F */ byte         pad_43F[0x1];
    /* 0x440 */ MATRIX       light;
    /* 0x460 */ MATRIX       color;
    /* 0x480 */ struct Task* field_480;
    /* 0x484 */ s32          field_484;
} Actor213100Work;
STATIC_ASSERT_SIZEOF(Actor213100Work, 0x488);

/// 0x14-byte animation preset the init `func_actor_213100_8014A118` builds on
/// its stack and hands to `func_actor_213100_8014A258`, the overlay's copy of
/// the shared animation body that installs it on the task's model through
/// `Gp_AnimResetSlot` / `Gp_AnimTickIndex` and `func_800B3F84` /
/// `func_800B4114`. `field_0` indexes the anim-block table
/// `D_actor_213100_801521A4` and is latched into `Actor213100Work::field_43E`;
/// `field_4` plays the same role for `field_43D`, seeded to -1 so that it
/// always differs; a nonzero `field_8` takes the `func_800B4114` path, which
/// also passes `field_C`; `field_10` is unread here. The init passes
/// `{ 0, 5, 0, 0, 0 }` -- the same five-word shape as `GpAnimArg` and
/// `Actor503500AnimPreset`.
typedef struct Actor213100AnimPreset {
    /* 0x00 */ s32 field_0;
    /* 0x04 */ s32 field_4;
    /* 0x08 */ s32 field_8;
    /* 0x0C */ s32 field_C;
    /* 0x10 */ s32 field_10;
} Actor213100AnimPreset;
STATIC_ASSERT_SIZEOF(Actor213100AnimPreset, 0x14);

void func_actor_213100_8014A23C(Task* arg0);

s32 func_actor_213100_8014A258(Task* arg0, s32 arg1, Actor213100AnimPreset* arg2, s32 arg3);

#endif
