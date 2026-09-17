#ifndef ACTOR_335800_H
#define ACTOR_335800_H

#include "common.h"

#include <psyq/libgte.h>

#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"

/// Work block allocated by `func_actor_335800_80163AA0` (`Mem_Calloc(0x4C8)`)
/// and parked in that task's `Task::idMap` slot -- that slot is not a
/// `TaskIdMap` here. `func_actor_335800_80163B54` republishes the two matrices
/// onto `TmdObject::field_1C` / `field_20`, the light/colour pair
/// `Gp_BindDefaultMtx` otherwise points at `Gp_DefaultMtx` / `Gp_DefaultMtx2`,
/// exactly as `func_actor_213100_8014A23C` does for `Actor213100Work`.
///
/// The size is the allocation, and the fields below are the ones the init
/// seeds: the two `sb` bytes at 0x43D/0x43E and the `sh` at 0x4C4 are set to
/// -1, and the three words at 0x4A0..0x4A8 are cleared.
typedef struct Actor335800Work {
    /* 0x000 */ GpAnimCtx  anim;
    /* 0x014 */ GpAnimSlot slots[0x13];
    /* 0x30C */ byte       field_30C[0x130];
    /* 0x43C */ s8         field_43C;
    /* 0x43D */ s8         field_43D;
    /* 0x43E */ s8         field_43E;
    /* 0x43F */ byte       pad_43F[0x1];
    /* 0x440 */ MATRIX     light;
    /* 0x460 */ MATRIX     color;
    /* 0x480 */ VECTOR3    target; // world position the shared turn-to-face body steers toward
    /* 0x48C */ byte       pad_48C[0x4];
    /* 0x490 */ VECTOR3    step;   // local-space offset `ApplyMatrixLV` rotates into world space
    /* 0x49C */ byte       pad_49C[0x4];
    /* 0x4A0 */ s32        field_4A0;
    /* 0x4A4 */ s32        field_4A4;
    /* 0x4A8 */ s32        field_4A8;
    /* 0x4AC */ byte       pad_4AC[0x4];
    /* 0x4B0 */ SVECTOR    limit;     // per-axis stop threshold; 0x7FFF on all three disables it
    /* 0x4B8 */ byte       pad_4B8[0xA];
    /* 0x4C2 */ u16        field_4C2; // index into the state-handler table `D_actor_335800_80161E68`
    /* 0x4C4 */ s16        field_4C4;
    /* 0x4C6 */ byte       pad_4C6[0x2];
} Actor335800Work;
STATIC_ASSERT_SIZEOF(Actor335800Work, 0x4C8);

/// The constant local-space offset `func_actor_335800_80163CA0` rotates,
/// `{ 0, 0, 0x200000, 0 }` -- straight ahead along the part's own +Z, the same
/// offset body `ActorsShared80132920` uses. The overlay keeps its own copy in
/// `.rodata`, so the address comes from the per-overlay symbol map.
extern VECTOR D_actor_335800_80161E78;

/// Work block allocated by `func_actor_335800_80162640` (`Mem_Calloc(0x50C)`)
/// and parked in that task's `Task::idMap` slot -- that slot is not a
/// `TaskIdMap` here, just as with `Actor335800Work`. This is the parent
/// actor's block: the init seeds the two `sb` bytes at 0x475/0x476 and the
/// `sh` at 0x506 to -1, clears the three words at 0x4D8..0x4E0, and stores the
/// two child tasks it spawns from `D_actor_335800_8016EADC` at 0x4FC/0x500.
/// `func_actor_335800_80162F9C` republishes the light/colour matrix pair onto
/// the parent's `TmdObject::field_1C` / `field_20`, exactly as
/// `func_actor_335800_80163B54` does for `Actor335800Work`.
///
/// The size is the allocation; the fields below are the ones the init and the
/// message handlers touch.
typedef struct Actor335800MainWork {
    /* 0x000 */ GpAnimCtx  anim;
    /* 0x014 */ GpAnimSlot slots[0x14];
    /* 0x334 */ byte       field_334[0x140];
    /* 0x474 */ s8         field_474;
    /* 0x475 */ s8         field_475;
    /* 0x476 */ s8         field_476;
    /* 0x477 */ s8         field_477; // preset byte the turn-to-face body passes as `field_4`
    /* 0x478 */ MATRIX     light;
    /* 0x498 */ MATRIX     color;
    /* 0x4B8 */ s32        field_4B8;
    /* 0x4BC */ s32        field_4BC;
    /* 0x4C0 */ s32        field_4C0;
    /* 0x4C4 */ byte       pad_4C4[0x14];
    /* 0x4D8 */ s32        field_4D8;
    /* 0x4DC */ s32        field_4DC;
    /* 0x4E0 */ s32        field_4E0;
    /* 0x4E4 */ byte       pad_4E4[0xC];
    /* 0x4F0 */ u16        field_4F0;
    /* 0x4F2 */ u16        field_4F2; // target yaw the turn-to-face body steers toward
    /* 0x4F4 */ u16        field_4F4;
    /* 0x4F6 */ byte       pad_4F6[0x2];
    /* 0x4F8 */ s16        field_4F8; // body counters the turn-to-face body clears on arrival
    /* 0x4FA */ s16        field_4FA;
    /* 0x4FC */ Task*      field_4FC;
    /* 0x500 */ Task*      field_500;
    /* 0x504 */ s16        field_504;
    /* 0x506 */ s16        field_506;
    /* 0x508 */ s32        field_508;
} Actor335800MainWork;
STATIC_ASSERT_SIZEOF(Actor335800MainWork, 0x50C);

/// Payload the sender of message 0x7DB passes as `Gp_DispatchMsg`'s `arg2`.
/// `func_dryfield_night_garage_801809A4` points it at a 4-byte record
/// (`{ 0x1803, 2 }` then `{ 0x1803, 3 }`); `func_actor_104600_80133D74`
/// likewise switches on the halfword at 0x2.
typedef struct Actor335800Msg {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
} Actor335800Msg;
STATIC_ASSERT_SIZEOF(Actor335800Msg, 0x4);

/// Per-view state the actor reaches through the sprite table
/// (`Gp_SprtTables[stage - 1][room - 1].field_0[view - 1]->field_1CC`).
/// `func_actor_335800_801622C0` writes both bytes to the same value -- 0 for
/// its 0 argument, otherwise the argument itself, alongside game flag 0x7F
/// nibble 1. The byte at 0x14 is the one the room family carrying the same
/// object writes (`DwtwSprtViewState.field_C`'s neighbour); 0x1C is written
/// with it, the pair the views' two sprite commands are gated on.
typedef struct Actor335800SprtView {
    /* 0x00 */ byte pad_0[0x14];
    /* 0x14 */ u8   field_14;
    /* 0x15 */ byte pad_15[0x7];
    /* 0x1C */ u8   field_1C;
} Actor335800SprtView;

/// The record `Gp_SprtTables[stage - 1][room - 1].field_0[view - 1]` really
/// points at: a view-sized block, far larger than the 0xC-byte `GpSprtRec` the
/// table's element type declares, so the caller reaches its tail through a
/// cast -- as the rooms carrying the same record do (`DwtwSprtRec`,
/// `MineForkedTunnelSprtRec`). Its tail is the `Actor335800SprtView` pointer.
typedef struct Actor335800SprtRec {
    /* 0x000 */ byte                 pad_0[0x1CC];
    /* 0x1CC */ Actor335800SprtView* field_1CC;
} Actor335800SprtRec;

/// 0x14-byte animation preset `func_actor_335800_801631A4` builds for
/// `func_actor_335800_801632A4`: the turn-to-face body fills `field_0` with 0,
/// `field_4` with the `field_477` byte, `field_8` with 1, `field_C` with 5 and
/// `field_10` with 0 -- the same five-word shape as `Actor141000AnimPreset`.
typedef struct Actor335800AnimPreset {
    /* 0x00 */ s32 field_0;
    /* 0x04 */ s32 field_4;
    /* 0x08 */ s32 field_8;
    /* 0x0C */ s32 field_C;
    /* 0x10 */ s32 field_10;
} Actor335800AnimPreset;
STATIC_ASSERT_SIZEOF(Actor335800AnimPreset, 0x14);

/// Spawn placement `func_actor_335800_80162C80` copies into the parent block:
/// the position into `field_4B8..field_4C0`, the rotation into
/// `field_4F0..field_4F4`.
typedef struct Actor335800Placement {
    /* 0x00 */ VECTOR  pos;
    /* 0x10 */ SVECTOR rot;
} Actor335800Placement;

/// Optional start animation for `func_actor_335800_80162C80`: the preset's
/// `field_4` and the `field_477` byte. Absent, the defaults are 0xD and 1.
typedef struct Actor335800SpawnAnim {
    /* 0x00 */ s32 field_0;
    /* 0x04 */ u8  field_4;
} Actor335800SpawnAnim;

/// A `MATRIX`'s word-wise view, for the identity splat `func_actor_335800_801631A4`
/// writes over the root coordinate before `RotMatrix` overwrites the 3x3: five
/// aligned stores rather than nine halfword ones (the same shape as
/// `Actor141000MatWords`).
typedef struct Actor335800MatWords {
    /* 0x00 */ s32 m00_m01;
    /* 0x04 */ s32 m02_m10;
    /* 0x08 */ s32 m11_m12;
    /* 0x0C */ s32 m20_m21;
    /* 0x10 */ s16 m22;
} Actor335800MatWords;
STATIC_ASSERT_SIZEOF(Actor335800MatWords, 0x14);

void func_actor_335800_80163B34(Task* arg0);
void func_actor_335800_80163B54(Task* arg0);

#endif
