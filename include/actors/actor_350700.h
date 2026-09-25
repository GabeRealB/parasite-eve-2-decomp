#ifndef ACTOR_350700_H
#define ACTOR_350700_H

#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/1BC.h"
#include "main/task.h"
#include "main/tmd.h"

/// Work block allocated by `func_actor_350700_80162404` (`memCalloc(0x4C8)`)
/// and parked in that task's `Task::work` slot -- that slot is not a
/// `TaskIdMap` here. `func_actor_350700_801624B4` republishes the two matrices
/// onto `TmdObject::lightMtx` / `field_20`, the light/colour pair
/// `Gp_BindDefaultMtx` otherwise points at `Gp_DefaultMtx` / `Gp_DefaultMtx2`.
///
/// The size is the allocation, and the fields below are the ones the init
/// seeds: the three `sb` bytes at 0x43D/0x43E/0x4C5 are set to -1, and the
/// three words at 0x4A0..0x4A8 are cleared.
typedef struct Actor350700Work {
    /* 0x000 */ GpAnimCtx  anim;
    /* 0x014 */ GpAnimSlot slots[0x13];  // the slot array `func_800B3F84` is handed
    /* 0x30C */ byte       poses[0x130]; // pose buffer `func_800B3F84` is handed
    /* 0x43C */ s8         field_43C;    // animation-tick enable
    /* 0x43D */ s8         field_43D;
    /* 0x43E */ s8         field_43E;
    /* 0x43F */ s8         field_43F;
    /* 0x440 */ MATRIX     light;
    /* 0x460 */ MATRIX     color;
    /* 0x480 */ VECTOR3    target;
    /* 0x48C */ byte       pad_48C[0x4];
    /* 0x490 */ VECTOR3    step; // local-space offset `ApplyMatrixLV` rotates into world space
    /* 0x49C */ byte       pad_49C[0x4];
    /* 0x4A0 */ s32        field_4A0;
    /* 0x4A4 */ s32        field_4A4;
    /* 0x4A8 */ s32        field_4A8;
    /* 0x4AC */ byte       pad_4AC[0x4];
    /* 0x4B0 */ SVECTOR    limit;     // per-axis stop threshold; 0x7FFF on all three disables it
    /* 0x4B8 */ u16        field_4B8;
    /* 0x4BA */ u16        field_4BA; // target yaw the turn-to-face body steers toward
    /* 0x4BC */ u16        field_4BC;
    /* 0x4BE */ byte       pad_4BE[0x2];
    /* 0x4C0 */ u16        field_4C0; // body counter the turn-to-face body clears on arrival
    /* 0x4C2 */ u16        field_4C2; // index into the state-handler table `D_actor_350700_80161E30`
    /* 0x4C4 */ s8         field_4C4;
    /* 0x4C5 */ s8         field_4C5;
    /* 0x4C6 */ byte       pad_4C6[0x2];
} Actor350700Work;
STATIC_ASSERT_SIZEOF(Actor350700Work, 0x4C8);

#endif
