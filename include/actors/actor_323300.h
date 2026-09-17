#ifndef ACTOR_323300_H
#define ACTOR_323300_H

#include "common.h"

#include <psyq/libgte.h>

#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/D4.h"

/// Work block allocated by `func_actor_323300_80161E78` (`Mem_Calloc(0x504)`)
/// and parked in that task's `Task::idMap` slot -- that slot is not a
/// `TaskIdMap` here. `func_actor_323300_801626D0` republishes the two matrices
/// onto `TmdObject::field_1C` / `field_20`, the light/colour pair
/// `Gp_BindDefaultMtx` otherwise points at `Gp_DefaultMtx` / `Gp_DefaultMtx2`,
/// exactly as `func_actor_350500_8016247C` does for `Actor350500Work`.
///
/// The block opens with animation head `anim`: `func_actor_323300_801628B8`
/// hands the block itself to `func_800B3F84` as its `GpAnimCtx`, the slot
/// array inline at 0x14 as the `GpAnimSlot*`, and 0x30C as the `GpAnimMtxRec`
/// table -- the same three addresses `Actor503500Effect4CC` passes. The 19
/// 0x28-byte slots run exactly up to that table, and every tick loop walks
/// indices 1..0x13, leaving slot 0 alone.
///
/// The display node at +0x480 is the one the exit callback
/// `func_actor_323300_8016269C` hands back to `Gp_UnlinkObj` before tearing
/// the enemy task down. The size is the allocation, and the fields below are
/// the ones the init seeds: the two `sb` bytes at 0x43D/0x43E and the `sh` at
/// 0x502 are set to -1, and 0x500 is set to 1. `rec` is the one-entry `GpRec18`
/// collision table `Gp_InitRec18Table` seeds at 0x4A0; `obj.field_C` addresses
/// it and `Gp_FindNearestSlot` walks it through that pointer.
typedef struct Actor323300Work {
    /* 0x000 */ GpAnimCtx  anim;
    /* 0x014 */ GpAnimSlot slots[19];
    /* 0x30C */ byte       pad_30C[0x130];
    /* 0x43C */ s8         field_43C; // set once the slots have been started
    /* 0x43D */ s8         field_43D; // animation id the slots were seeded with
    /* 0x43E */ s8         field_43E; // animation bank index
    /* 0x43F */ byte       pad_43F[0x1];
    /* 0x440 */ MATRIX     light;
    /* 0x460 */ MATRIX     color;
    /* 0x480 */ GpObj      obj;
    /* 0x4A0 */ GpRec18    rec; // seed table `obj.field_C` points at
    /* 0x4B8 */ byte       pad_4B8[0x14];
    /* 0x4CC */ s32        field_4CC;
    /* 0x4D0 */ s32        field_4D0;
    /* 0x4D4 */ s32        field_4D4;
    /* 0x4D8 */ byte       pad_4D8[0x1E];
    /* 0x4F6 */ u16        field_4F6; // target yaw the turn-to-face body steers toward
    /* 0x4F8 */ byte       pad_4F8[0x4];
    /* 0x4FC */ s16        field_4FC;
    /* 0x4FE */ s16        field_4FE;
    /* 0x500 */ s16        field_500;
    /* 0x502 */ s16        field_502;
} Actor323300Work;
STATIC_ASSERT_SIZEOF(Actor323300Work, 0x504);

/// 0x14-byte animation preset `func_actor_323300_80162748` and
/// `func_actor_323300_801627B4` hand `func_actor_323300_801628B8`: the low byte
/// of `field_0` is the animation bank index the helper compares against
/// `Actor323300Work::field_43E`, `field_4`'s low byte the animation id it
/// compares against `field_43D`, and `field_8` selects between `func_800B4114`
/// -- which also takes `field_C` -- and `Gp_AnimResetSlot`. `field_10` is
/// unread, as it is in `Actor503500AnimPreset`, the same record shape.
typedef struct {
    /* 0x00 */ s32 field_0;
    /* 0x04 */ s32 field_4;
    /* 0x08 */ s32 field_8;
    /* 0x0C */ s32 field_C;
    /* 0x10 */ s32 field_10;
} Actor323300AnimPreset;
STATIC_ASSERT_SIZEOF(Actor323300AnimPreset, 0x14);

/// 0x18-byte placement `func_actor_323300_801629F0` splats onto the actor's
/// coordinate: it copies `pos` into the coordinate matrix translation and hands
/// `rot` to `RotMatrix`, which converts it into that matrix's rotation. The
/// shape is `VECTOR` followed by `SVECTOR`, so `rot` sits at 0x10 rather than
/// abutting `pos`. `D_actor_323300_8017259C` is the one the spawn handler
/// installs.
typedef struct {
    /* 0x00 */ VECTOR  pos;
    /* 0x10 */ SVECTOR rot;
} Actor323300Placement;
STATIC_ASSERT_SIZEOF(Actor323300Placement, 0x18);

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
/// `color` ends flush with the allocation.
///
/// The prefix is the same animation shape the 0x504 block opens with: the
/// `GpAnimCtx` at 0, the `GpAnimSlot` array inline at 0x14 and the
/// `GpAnimMtxRec` table at 0x30C -- the three addresses
/// `func_actor_323300_80163718` hands `func_800B3F84`. Its animation state
/// sits in the four `s32` words past that table rather than in the byte fields
/// `Actor323300Work` uses: `field_440` is the preset bank index, `field_444`
/// the preset animation id (`func_actor_323300_80162BE4` seeds both to -1) and
/// `field_43C` the once-only flag its tick path sets. `field_44C` is the 0x3000
/// that same initialiser stores.
typedef struct Actor323300MtxWork {
    /* 0x000 */ GpAnimCtx         anim;
    /* 0x014 */ GpAnimSlot        slots[19];
    /* 0x30C */ byte              pad_30C[0x130];
    /* 0x43C */ s32               field_43C; // set once the slots have been started
    /* 0x440 */ s32               field_440; // animation bank index the slots were seeded with
    /* 0x444 */ s32               field_444; // animation id the slots were seeded with
    /* 0x448 */ byte              pad_448[0x4];
    /* 0x44C */ s32               field_44C;
    /* 0x450 */ byte              pad_450[0x220];
    /* 0x670 */ Actor323300Matrix light;
    /* 0x690 */ Actor323300Matrix color;
} Actor323300MtxWork;
STATIC_ASSERT_SIZEOF(Actor323300MtxWork, 0x6B0);

/// Message table `func_actor_323300_80161E78` parks in `Task::field_24`:
/// `Gp_DispatchMsg` matches an incoming id against these and calls the handler.
/// Ids 0x7D3/0x7D4/0x7D5/0x7DB reach `func_actor_323300_801628B8`,
/// `func_actor_323300_801629F0`, `func_actor_323300_80162208` and
/// `func_actor_323300_80162360`; the 0x7FFFFFFF terminator ends the walk.
extern GpMsgEntry D_actor_323300_80172574[];

/// Placement `func_actor_323300_80161E78` hands `func_actor_323300_801629F0`.
extern Actor323300Placement D_actor_323300_8017259C;

/// Animation presets the spawn handler and `func_actor_323300_80162748` hand
/// `func_actor_323300_801628B8`.
extern Actor323300AnimPreset D_actor_323300_801725B4;
extern Actor323300AnimPreset D_actor_323300_801725C8;
extern Actor323300AnimPreset D_actor_323300_801725DC;

/// Animation source table `func_actor_323300_80163718` indexes by the 0x6B0
/// block's bank index: one `void*` per bank, exactly as `func_actor_335800_80162C80`
/// indexes `D_actor_335800_8016EAD8`. `func_actor_323300_80162BE4` applies the
/// preset `D_actor_323300_80174A74` through it on the block's first anim start.
extern void* D_actor_323300_80174A70[];

void func_actor_323300_801626D0(Task* arg0);
void func_actor_323300_8016269C(Task* arg0);
void func_actor_323300_80163188(GsCOORDINATE2* coord, s16 angle);
void func_actor_323300_80162748(Task* arg0);
void func_actor_323300_801627B4(Task* arg0);
void func_actor_323300_801628B8(Task* arg0, s32 arg1, Actor323300AnimPreset* arg2, s32 arg3);
s32  func_actor_323300_80163718(Task* arg0, s32 arg1, Actor323300AnimPreset* arg2, s32 arg3);
s32  func_actor_323300_80162208(Task* arg0, s32 arg1, s32 arg2, s32 arg3);
s32  func_actor_323300_801629F0(Task* arg0, s32 arg1, Actor323300Placement* arg2, s32 arg3);

#endif
