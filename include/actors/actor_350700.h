#ifndef ACTOR_350700_H
#define ACTOR_350700_H

#include "common.h"

#include <psyq/libgte.h>

#include "main/task.h"
#include "main/tmd.h"

/// Work block allocated by `func_actor_350700_80162404` (`Mem_Calloc(0x4C8)`)
/// and parked in that task's `Task::idMap` slot -- that slot is not a
/// `TaskIdMap` here. `func_actor_350700_801624B4` republishes the two matrices
/// onto `TmdObject::field_1C` / `field_20`, the light/colour pair
/// `Gp_BindDefaultMtx` otherwise points at `Gp_DefaultMtx` / `Gp_DefaultMtx2`,
/// exactly as `func_actor_350500_8016247C` does for `Actor350500Work`.
///
/// The size is the allocation, and the fields below are the ones the init
/// seeds: the three `sb` bytes at 0x43D/0x43E/0x4C5 are set to -1, and the
/// three words at 0x4A0..0x4A8 are cleared.
typedef struct Actor350700Work {
    /* 0x000 */ byte    pad_0[0x43D];
    /* 0x43D */ s8      field_43D;
    /* 0x43E */ s8      field_43E;
    /* 0x43F */ byte    pad_43F[0x1];
    /* 0x440 */ MATRIX  light;
    /* 0x460 */ MATRIX  color;
    /* 0x480 */ byte    pad_480[0x10];
    /* 0x490 */ VECTOR3 step; // local-space offset `ApplyMatrixLV` rotates into world space
    /* 0x49C */ byte    pad_49C[0x4];
    /* 0x4A0 */ s32     field_4A0;
    /* 0x4A4 */ s32     field_4A4;
    /* 0x4A8 */ s32     field_4A8;
    /* 0x4AC */ byte    pad_4AC[0x4];
    /* 0x4B0 */ SVECTOR limit;     // per-axis stop threshold; 0x7FFF on all three disables it
    /* 0x4B8 */ byte    pad_4B8[0xA];
    /* 0x4C2 */ u16     field_4C2; // index into the state-handler table
    /* 0x4C4 */ s8      field_4C4;
    /* 0x4C5 */ s8      field_4C5;
    /* 0x4C6 */ byte    pad_4C6[0x2];
} Actor350700Work;
STATIC_ASSERT_SIZEOF(Actor350700Work, 0x4C8);

/// The constant local-space offset `func_actor_350700_8016261C` rotates,
/// `{ 0, 0, 0x200000, 0 }` -- straight ahead along the part's own +Z, the same
/// offset body `ActorsShared80132920` uses. The overlay keeps its own copy in
/// `.rodata`, so the address comes from the per-overlay symbol map.
extern VECTOR D_actor_350700_80161E40;

/// Exit callback `func_actor_350700_80162404` installs; tears the task down.
void func_actor_350700_80162494(Task* arg0);

void func_actor_350700_801624B4(Task* arg0);

#endif
