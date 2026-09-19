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
/// `Gp_BindDefaultMtx` otherwise points at `Gp_DefaultMtx` / `Gp_DefaultMtx2`,
/// exactly as `func_actor_350500_8016247C` does for `Actor350500Work`.
///
/// The size is the allocation, and the fields below are the ones the init
/// seeds: the three `sb` bytes at 0x43D/0x43E/0x4C5 are set to -1, and the
/// three words at 0x4A0..0x4A8 are cleared.
typedef struct Actor350700Work {
    /* 0x000 */ byte       pad_0[0x14];
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

/// 0x14-byte animation preset `func_actor_350700_80162764` builds for
/// `func_actor_350700_80162860`, the same five-word shape as
/// `Actor141000AnimPreset` and `Actor335800AnimPreset`. The turn-to-face body
/// fills `field_0` with 0, `field_4` and `field_8` with 1, `field_C` with 4 and
/// `field_10` with 0.
typedef struct Actor350700AnimPreset {
    /* 0x00 */ s32 field_0;
    /* 0x04 */ s32 field_4;
    /* 0x08 */ s32 field_8;
    /* 0x0C */ s32 field_C;
    /* 0x10 */ s32 field_10;
} Actor350700AnimPreset;
STATIC_ASSERT_SIZEOF(Actor350700AnimPreset, 0x14);

/// Spawn placement `func_actor_350700_801621B4` copies into the work block:
/// the position into `Actor350700Work::target`, the rotation into
/// `field_4B8..field_4BC`.
typedef struct Actor350700Placement {
    /* 0x00 */ VECTOR  pos;
    /* 0x10 */ SVECTOR rot;
} Actor350700Placement;
STATIC_ASSERT_SIZEOF(Actor350700Placement, 0x18);

/// Optional start animation for the same handler: the preset's `field_4`
/// and the `field_43F` byte. Absent, the defaults are anim 3 (or 2 once
/// `field_4C4` is set) and 1.
typedef struct Actor350700SpawnAnim {
    /* 0x00 */ s32 field_0;
    /* 0x04 */ u8  field_4;
} Actor350700SpawnAnim;

/// A `MATRIX`'s word-wise view, for the identity splat
/// `func_actor_350700_80162764` writes over the root coordinate before
/// `RotMatrix` overwrites the 3x3: five aligned stores rather than nine
/// halfword ones (the same shape as `Actor141000MatWords`).
typedef struct Actor350700MatWords {
    /* 0x00 */ s32 m00_m01;
    /* 0x04 */ s32 m02_m10;
    /* 0x08 */ s32 m11_m12;
    /* 0x0C */ s32 m20_m21;
    /* 0x10 */ s16 m22;
} Actor350700MatWords;
STATIC_ASSERT_SIZEOF(Actor350700MatWords, 0x14);

/// Work block allocated by `func_actor_350700_80162B30` (`memCalloc(0x50C)`)
/// and parked in that task's `Task::work` slot -- that slot is not a
/// `TaskIdMap` here, just as with `Actor350700Work`. This is the parent
/// actor's block, the same shape as `Actor335800MainWork`: the init seeds the
/// two `sb` bytes at 0x475/0x476 and the word at 0x508 to -1, clears the
/// three words at 0x4D8..0x4E0, and stores the three child tasks it spawns
/// from `D_actor_350700_801708DC` at 0x4FC/0x500/0x504. `ActorsShared80132f24`
/// then republishes the light/colour matrix pair onto the parent's
/// `TmdObject::lightMtx` / `field_20`, exactly as `func_actor_350700_801624B4`
/// does for `Actor350700Work`.
///
/// The size is the allocation; the fields below are the ones the init, the
/// spawned-task bookkeeping and the per-frame tick touch. The tick keeps a
/// 16.16 accumulator triple at 0x4D8..0x4E0, fed from the deltas at
/// 0x4C8..0x4D0; only each accumulator's high half reaches the root
/// coordinate, and the low half is re-zeroed every frame.
typedef struct Actor350700MainWork {
    /* 0x000 */ byte  pad_0[0x474];
    /* 0x474 */ s8    field_474; // non-zero while the animation slots tick
    /* 0x475 */ s8    field_475;
    /* 0x476 */ s8    field_476;
    /* 0x477 */ byte  pad_477[0x51];
    /* 0x4C8 */ s32   field_4C8; // per-frame local-space deltas the accumulators take
    /* 0x4CC */ s32   field_4CC;
    /* 0x4D0 */ s32   field_4D0;
    /* 0x4D4 */ byte  pad_4D4[0x4];
    /* 0x4D8 */ s32   field_4D8; // 16.16 accumulators; only the high half reaches the coordinate
    /* 0x4DC */ s32   field_4DC;
    /* 0x4E0 */ s32   field_4E0;
    /* 0x4E4 */ byte  pad_4E4[0x14];
    /* 0x4F8 */ s16   field_4F8; // selects which of the two handlers the tick runs
    /* 0x4FA */ byte  pad_4FA[0x2];
    /* 0x4FC */ Task* field_4FC;
    /* 0x500 */ Task* field_500;
    /* 0x504 */ Task* field_504;
    /* 0x508 */ s32   field_508;
} Actor350700MainWork;
STATIC_ASSERT_SIZEOF(Actor350700MainWork, 0x50C);

/// The constant local-space offset `func_actor_350700_8016261C` rotates,
/// `{ 0, 0, 0x200000, 0 }` -- straight ahead along the part's own +Z, the same
/// offset body `ActorsShared80132920` uses. The overlay keeps its own copy in
/// `.rodata`, so the address comes from the per-overlay symbol map.
extern VECTOR D_actor_350700_80161E40;

/// Ground-shadow quad, the gameplay function `func_actor_350700_80161E88`
/// feeds the second part's world translation to; declared here the way the
/// other actor headers that call it do.
void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);

/// Installs an animation on the task's model: first argument is the preset
/// `func_actor_350700_80162764` fills, second the anim id it plays (0x7D3).
void func_actor_350700_80162860(Task* arg0, s32 arg1, Actor350700AnimPreset* arg2, s32 arg3);

/// Exit callback `func_actor_350700_80162404` installs; tears the task down.
void func_actor_350700_80162494(Task* arg0);

/// The empty first entry of the parent's two-handler table, selected by
/// `Actor350700MainWork::field_4F8` -- the no-op half of the pair whose other
/// entry is the shared `ActorsShared801327f8`.
void func_actor_350700_801633F8(Task* arg0);

void func_actor_350700_801624B4(Task* arg0);

/// Exit callback `func_actor_350700_80162B30` installs, the same
/// `Gp_EnemyTaskExit` teardown `func_actor_350700_80162494` performs.
void func_actor_350700_801633BC(Task* arg0);

#endif
