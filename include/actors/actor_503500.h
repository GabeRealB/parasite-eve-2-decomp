#ifndef ACTOR_503500_H
#define ACTOR_503500_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3FB8.h"

/// Work block allocated by `func_actor_503500_80132430`
/// (`Mem_Calloc(0x48)`) and parked in that task's `Task::idMap` slot.
/// `func_actor_503500_801324EC` republishes the two matrices onto
/// `TmdObject::field_1C` / `field_20` -- the colour/light matrix pair
/// `Gp_BindDefaultMtx` otherwise points at `Gp_DefaultMtx` / `Gp_DefaultMtx2`
/// -- so the allocation is exactly two `MATRIX`es plus a small tail.
/// `func_actor_503500_80132664` sets `field_45` / `field_40` from the message
/// mode (0..3).
typedef struct Actor503500ColorMtx {
    /* 0x00 */ MATRIX light;
    /* 0x20 */ MATRIX color;
    /* 0x40 */ s16    field_40;
    /* 0x42 */ byte   pad_42[0x2];
    /* 0x44 */ s8     field_44;
    /* 0x45 */ s8     field_45;
    /* 0x46 */ byte   pad_46[0x2];
} Actor503500ColorMtx;
STATIC_ASSERT_SIZEOF(Actor503500ColorMtx, 0x48);

/// Payload `func_actor_503500_80132664` takes as `Gp_DispatchMsg`'s `arg2`;
/// `mode` selects one of four colour-matrix settings.
typedef struct Actor503500ModeMsg {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 mode;
} Actor503500ModeMsg;
STATIC_ASSERT_SIZEOF(Actor503500ModeMsg, 0x4);

/// Overlay of `GsCOORDINATE2` at `TmdObject::field_8`, as this overlay places
/// its parts: offset 0x44 (libgs's `param`, with `super` at 0x48) holds the
/// Euler angles the code writes and hands straight to `RotMatrix`, the same
/// reuse `ActorsShared8013231cCoord` and `RoomCoord` document.
typedef struct Actor503500Coord {
    /* 0x00 */ s32     flg;
    /* 0x04 */ MATRIX  coord;
    /* 0x24 */ MATRIX  workm;
    /* 0x44 */ SVECTOR rot;
} Actor503500Coord;
STATIC_ASSERT_SIZEOF(Actor503500Coord, 0x4C);

/// Placement argument block: a world translation followed by the Euler angles
/// handed to `RotMatrix`, the same shape `ActorsShared8013231cArgs` takes.
typedef struct Actor503500PlaceArgs {
    /* 0x00 */ VECTOR  pos;
    /* 0x10 */ SVECTOR rot;
} Actor503500PlaceArgs;
STATIC_ASSERT_SIZEOF(Actor503500PlaceArgs, 0x18);

/// Message payload at `D_actor_503500_8017655C`. `func_actor_503500_80132DEC`
/// fills it from the player actor: the three words are the translation of the
/// `GsCOORDINATE2` at `TmdObject::field_8` (`MATRIX.t`), the three
/// halfwords the rotation triple at +0x50/+0x52/+0x54 of that task's `idMap`
/// block. `func_actor_503500_80132DD4` clears the position;
/// `func_actor_503500_80132E7C` hands the record to `Gp_DispatchMsg` as
/// message 0x3E9 while any position word is non-zero.
///
/// Size is bounded by the next `.bss` symbol in the overlay
/// (`D_actor_503500_80176574`, 0x18 bytes later).
typedef struct Actor503500MsgPos {
    /* 0x00 */ s32     x;
    /* 0x04 */ s32     y;
    /* 0x08 */ s32     z;
    /* 0x0C */ byte    pad_C[0x4];
    /* 0x10 */ SVECTOR rot;
} Actor503500MsgPos;
STATIC_ASSERT_SIZEOF(Actor503500MsgPos, 0x18);

/// Bytes 0x40..0x60 of an `actor_503500` work block, which the two enemies
/// that reach them disagree about. The 0x160 block parks its display node
/// there -- `func_actor_503500_801372C8` links `obj` and
/// `func_actor_503500_80138288` hands it back to `Gp_UnlinkObj`. The boss
/// links its node at `field_5D4` instead and uses the run for state flags;
/// `func_actor_503500_80136014` tests bits 0x102 of the halfword at 0x4C.
typedef union Actor503500Slot40 {
    GpObj obj;
    struct {
        /* 0x00 */ byte pad_0[0xC];
        /* 0x0C */ u16  flags_4C;
    } boss;
} Actor503500Slot40;
STATIC_ASSERT_SIZEOF(Actor503500Slot40, 0x20);

/// Shared field view of an `actor_503500` enemy work block.
///
/// The overlay hosts a dozen separate enemies. Each parks its own work block in
/// the `Task::idMap` slot -- that slot is *not* a `TaskIdMap` here -- and each
/// block is zeroed by its state-0 init, so every size below is anchored by the
/// clearing call rather than inferred from the access spread:
///
/// | state-0 init                  | block                                     | anchor              |
/// |-------------------------------|-------------------------------------------|---------------------|
/// | `func_actor_503500_80132F64`  | `D_actor_503500_80176574` (the boss)      | `Mem_Set(_, 0x7E8)` |
/// | `func_actor_503500_801372C8`  | `D_actor_503500_80176D88`                 | `Mem_Set(_, 0x160)` |
/// | `func_actor_503500_8013852C`  | `D_actor_503500_80176EE8[spawnArg1 - 2]`  | `Mem_Set(_, 0x2EC)` |
/// | `func_actor_503500_8013AD64`  | `D_actor_503500_801774C0[spawnArg1 - 4]`  | `Mem_Set(_, 0xF0)`  |
/// | `func_actor_503500_8013BEE4`  | `D_actor_503500_801776A0`                 | `Mem_Set(_, 0xF4)`  |
/// | `func_actor_503500_8013CAE4`  | `D_actor_503500_801770E8`                 | `Mem_Set(_, 0xF4)`  |
/// | `func_actor_503500_8013DD10`  | `D_actor_503500_8017797C`                 | `Mem_Set(_, 0xF0)`  |
/// | `func_actor_503500_8013ECBC`  | `D_actor_503500_80177A6C`                 | `Mem_Set(_, 0xF4)`  |
/// | `func_actor_503500_8013FA74`  | `D_actor_503500_80177B60`                 | `Mem_Set(_, 0x3D8)` |
/// | `func_actor_503500_801423C8`  | `D_actor_503500_80178AC0`                 | `Mem_Set(_, 0x224)` |
/// | `func_actor_503500_80143AC0`  | `D_actor_503500_80178F10`                 | `Mem_Set(_, 0x38)`  |
///
/// One C type is shared across all of them, the way the first decompiled
/// dispatchers here already assumed, and the observed offsets do not collide --
/// the small blocks simply stop early. The size asserted below is therefore the
/// *largest* instance (the boss); a 0xF0 block typed through this pointer is
/// only valid up to its own end. Splitting this into one type per enemy is a
/// follow-up that would have to retype the already-landed dispatchers
/// (`func_actor_503500_801383D0` reads `field_15C` out of the 0x160 block,
/// `func_actor_503500_8013BD88` reads `field_ED` out of a 0xF0 block,
/// `func_actor_503500_8013EB60` reads `field_EC` out of another 0xF0 block).
typedef struct Actor503500Work {
    /* 0x000 */ GpObj   obj; // the display node Gp_UnlinkObj takes
    /* 0x020 */ GpRec18 rec; // collision table; 0xF0 enemies pass count 8
    /* 0x038 */ byte    pad_38[0x8];
    /// Display node of the 0x160 block, which puts its own node here rather
    /// than at 0x0: `func_actor_503500_801372C8` links it (and runs
    /// `Gp_InitRec18Table` over the record area right behind it, at 0x60) and
    /// `func_actor_503500_80138288` hands it back to `Gp_UnlinkObj`.
    /* 0x040 */ Actor503500Slot40 slot40;
    /// Record table of `slot40`'s node, passed with count 8 by
    /// `func_actor_503500_801382FC`, so the 0x160 block's table really runs to
    /// 0x120. Only the first record is named here; the 0x224 block already
    /// names `field_7E` inside that run.
    /* 0x060 */ GpRec18 rec60[1];
    /* 0x078 */ byte    pad_78[0x6];
    /* 0x07E */ u16     field_7E; // GpObj::flags of obj[3] in the 0x224 block
    /* 0x080 */ byte    pad_80[0x60];
    /// Coordinate the 0xF4 block at `D_actor_503500_80177A6C` republishes
    /// alongside its display node: `func_actor_503500_8013ECBC` stores the
    /// task's own `TmdObject::field_8` here together with the 0xE4 / 0xE6
    /// pair it seeds to 0x600 and 3.
    /* 0x0E0 */ GsCOORDINATE2* field_E0;
    /* 0x0E4 */ s16            field_E4;
    /* 0x0E6 */ s16            field_E6;
    /* 0x0E8 */ s16            field_E8; // per-frame countdown, clamped at 0
    /* 0x0EA */ s16            field_EA;
    /* 0x0EC */ s8             field_EC; // sub-state index
    /* 0x0ED */ s8             field_ED; // sub-state index
    /* 0x0EE */ s8             field_EE; // sub-state phase, cleared with field_ED
    /* 0x0EF */ byte           pad_EF[0x1];
    /* 0x0F0 */ s8             field_F0; // sub-state index
    /* 0x0F1 */ s8             field_F1; // sub-state phase, cleared with field_F0
    /* 0x0F2 */ byte           pad_F2[0x2E];
    /// The same coordinate / 0x124 / 0x126 trio as `field_E0`, at the 0x160
    /// block's own offsets: `func_actor_503500_801372C8` republishes the task's
    /// `TmdObject::field_8` here alongside the 0x400 / 3 pair it seeds.
    /* 0x120 */ GsCOORDINATE2* field_120;
    /* 0x124 */ s16            field_124;
    /* 0x126 */ s16            field_126;
    /* 0x128 */ byte           pad_128[0x30];
    /// Per-frame countdown of the 0x160 block, stepped down and floored at 0 by
    /// `func_actor_503500_801382FC`.
    /* 0x158 */ s16 field_158;
    /// Frame counter of the 0x160 block's sub-state, stepped by
    /// `func_actor_503500_801374BC` / `_80137678` and reset whenever
    /// `field_15C` changes.
    /* 0x15A */ u16  field_15A;
    /* 0x15C */ s8   field_15C; // sub-state index
    /* 0x15D */ s8   field_15D; // sub-state phase, cleared with field_15C
    /* 0x15E */ s8   field_15E; // seeded to -1 by func_actor_503500_801372C8
    /* 0x15F */ byte pad_15F[0x1];
    /// The two display nodes of the 0x3D8 block, linked by
    /// `func_actor_503500_8013FA74` and both handed back to `Gp_UnlinkObj` by
    /// `func_actor_503500_80141D04`. The 0x224 block puts a node at 0x160 too
    /// (`Actor503500Work224::obj2`); the shorter blocks stop before it.
    /* 0x160 */ GpObj obj160;
    /// Record table of `obj160`, passed with count 8 by
    /// `func_actor_503500_801420C4`, so the 0x3D8 block's table really runs to
    /// 0x240. Only the part before the 0x224 block's `field_221` is named here;
    /// the two blocks disagree about the bytes from 0x210 on.
    /* 0x180 */ GpRec18 rec180[6];
    /* 0x210 */ byte    pad_210[0x11];
    /* 0x221 */ s8      field_221; // sub-state index
    /* 0x222 */ byte    pad_222[0x1E];
    /* 0x240 */ GpObj   obj240;
    /* 0x260 */ GpRec18 rec260[4]; // obj240's table, count 4
    /* 0x2C0 */ byte    pad_2C0[0x14];
    /* 0x2D4 */ s16     field_2D4; // sub-state index of the 0x2EC block
    /* 0x2D6 */ s16     field_2D6;
    /// Per-frame countdown of the 0x2EC block, stepped down and floored at 0 by
    /// `func_actor_503500_8013AA44`; cleared outright by
    /// `func_actor_503500_801390C4` / `_801395BC` and gated on by
    /// `func_actor_503500_80139A20`.
    /* 0x2D8 */ s16  field_2D8;
    /* 0x2DA */ s16  field_2DA;
    /* 0x2DC */ byte pad_2DC[0x2];
    /* 0x2DE */ s16  field_2DE; // sub-state frame counter, 0x2EC block
    /* 0x2E0 */ byte pad_2E0[0x4];
    /* 0x2E4 */ s8   field_2E4; // sub-state phase, cleared with field_2D4
    /* 0x2E5 */ s8   field_2E5;
    /* 0x2E6 */ byte pad_2E6[0x4];
    /* 0x2EA */ s8   field_2EA;
    /* 0x2EB */ s8   field_2EB; // TMD buffer countdown, 0x2EC block
    /* 0x2EC */ byte pad_2EC[0xB0];
    /* 0x39C */ s32  field_39C;
    /* 0x3A0 */ byte pad_3A0[0x4];
    /* 0x3A4 */ s16  field_3A4;
    /* 0x3A6 */ s16  field_3A6;
    /* 0x3A8 */ s16  field_3A8; // countdown, floored at 0 by func_actor_503500_801420C4
    /* 0x3AA */ s16  field_3AA;
    /* 0x3AC */ byte pad_3AC[0x2];
    /* 0x3AE */ s16  field_3AE;
    /* 0x3B0 */ u16  field_3B0;
    /* 0x3B2 */ u16  field_3B2; // fade level, stepped by 0x10 up to 0x1000
    /* 0x3B4 */ byte pad_3B4[0x1C];
    /* 0x3D0 */ s8   field_3D0; // sub-state index
    /* 0x3D1 */ s8   field_3D1; // sub-state phase, cleared with field_3D0
    /* 0x3D2 */ byte pad_3D2[0x4];
    /* 0x3D6 */ s8   field_3D6;
    /* 0x3D7 */ s8   field_3D7; // TMD buffer countdown, 0x3D8 block
    /* 0x3D8 */ byte pad_3D8[0x12C];
    /// Private copies of two of the boss model's part coordinates (parts 4 and
    /// 10), refreshed by `func_actor_503500_80136DDC` when bits 0x20 / 0x800 of
    /// `field_7AC` are set and then scaled by `field_5A4` / `field_5B4`.
    /* 0x504 */ GsCOORDINATE2 coord504;
    /* 0x554 */ GsCOORDINATE2 coord554;
    /* 0x5A4 */ VECTOR        field_5A4; // scale of coord504
    /* 0x5B4 */ VECTOR        field_5B4; // scale of coord554
    /* 0x5C4 */ VECTOR        field_5C4; // scale of model part 16, bit 0x10000
                                         /// Display node + collision record of the boss's second body part:
                                         /// `func_actor_503500_80132F64` links it and seeds `field_5F4`,
                                         /// `func_actor_503500_80136A88` re-places the pair and
                                         /// `func_actor_503500_80136228` hands `field_5D4` back to
                                         /// `Gp_UnlinkObj` on teardown.
    /* 0x5D4 */ GpObj field_5D4;
    /// Record table of `field_5D4`'s node -- `func_actor_503500_80132F64`
    /// parks this address in that `GpObj` and `func_actor_503500_80136A88`
    /// re-places the pair with count 8.
    /* 0x5F4 */ GpRec18 rec5F4[8];
    /* 0x6B4 */ byte    pad_6B4[0x10];
    /// World position of the boss's placed part in 16.16 fixed point:
    /// `func_actor_503500_80137088` stores the placement argument's
    /// integer translation here shifted left by 16, alongside dropping the
    /// same translation into the coordinate's own matrix.
    /* 0x6C4 */ VECTOR   field_6C4;
    /* 0x6D4 */ byte     pad_6D4[0x18];
    /* 0x6EC */ GpEnemy* enemies[0x11];
    /// Two parallel per-slot halfword arrays covering the same 0x11 slots as
    /// `enemies`: `func_actor_503500_80136F40` writes both when it asks a slot
    /// to die, `func_actor_503500_80136FDC` reads `field_752` as a gate on
    /// `field_730`.
    /* 0x730 */ s16  field_730[0x11];
    /* 0x752 */ s16  field_752[0x11];
    /* 0x774 */ u32  field_774; // one "already asked to die" bit per slot
    /* 0x778 */ byte pad_778[0x34];
    /* 0x7AC */ s32  field_7AC; // part-scale enable bits, see coord504
    /* 0x7B0 */ s16  field_7B0; // boss state index
    /* 0x7B2 */ u16  field_7B2;
    /* 0x7B4 */ s16  field_7B4; // per-frame countdown, clamped at 0
                                /// Yaw of the placed part, recovered by `func_actor_503500_80137088`
                                /// from the rotation matrix it just built (`ratan2` of `m[0][2]` over
                                /// `m[2][2]`).
    /* 0x7B6 */ s16  field_7B6;
    /* 0x7B8 */ byte pad_7B8[0x2];
    /* 0x7BA */ s16  field_7BA;
    /* 0x7BC */ s16  field_7BC;
    /* 0x7BE */ s16  field_7BE;
    /* 0x7C0 */ s16  field_7C0;
    /* 0x7C2 */ s16  field_7C2; // slot index being asked to die
    /* 0x7C4 */ byte pad_7C4[0x4];
    /* 0x7C8 */ u16  field_7C8;
    /* 0x7CA */ s16  field_7CA;
    /* 0x7CC */ s16  field_7CC;
    /* 0x7CE */ byte pad_7CE[0x4];
    /* 0x7D2 */ s16  field_7D2;
    /// 1 once `func_actor_503500_80135950` has seeded the animation slots and
    /// ticked them; that helper clears it again whenever it loads a new bank,
    /// and `func_actor_503500_80136D30` uses it as the gate on its own tick.
    /* 0x7D4 */ s8   field_7D4;
    /* 0x7D5 */ s8   field_7D5; // boss sub-state index, seeded with field_7D6/7D9
    /* 0x7D6 */ byte pad_7D6[0x3];
    /* 0x7D9 */ s8   field_7D9; // mode last set by func_actor_503500_80137158
    /* 0x7DA */ u8   field_7DA; // per-state step counter
    /* 0x7DB */ u8   field_7DB; // cleared alongside field_7DA
    /* 0x7DC */ byte pad_7DC[0x2];
    /* 0x7DE */ s8   field_7DE;
    /* 0x7DF */ byte pad_7DF[0x1];
    /* 0x7E0 */ s8   field_7E0;
    /* 0x7E1 */ s8   field_7E1;
    /* 0x7E2 */ s8   field_7E2;
    /* 0x7E3 */ byte pad_7E3[0x5];
} Actor503500Work;
STATIC_ASSERT_SIZEOF(Actor503500Work, 0x7E8);

/// Matrix table of the 0x3D8 block, which runs from 0x40 up to the block's
/// `obj160` display node: `func_actor_503500_80141FC8`'s sub-state 0 resets
/// entries 1..8 to an identity rotation with zero translation. The shared
/// `Actor503500Work` view names other blocks' fields over the same bytes.
typedef struct Actor503500Work3D8Mtx {
    /* 0x000 */ byte   pad_0[0x40];
    /* 0x040 */ MATRIX mats[9];
} Actor503500Work3D8Mtx;
STATIC_ASSERT_SIZEOF(Actor503500Work3D8Mtx, 0x160);

/// Work block shape of the `actor_503500` effect tasks -- the ones whose
/// state-0 init `Mem_Calloc`s the block instead of pointing `Task::idMap` at a
/// static global: `func_actor_503500_80144300` (0xC0),
/// `func_actor_503500_801448E8` (0xB4), `func_actor_503500_80144E8C` (0xD0),
/// `func_actor_503500_801455A4` (0x44), `func_actor_503500_80145A2C` (0xAC)
/// and `func_actor_503500_8014642C` (0x4CC). Each starts with the `GpObj`
/// display node their exit callback hands to `Gp_UnlinkObj`, and the ones that
/// place geometry follow it with the `GpRec18` collision record
/// (`func_actor_503500_801448E8` derives its `GpRec18` base as `block + 0x20`).
/// Only that head is shared; the payload after it differs per task, which is
/// why this type stops at 0x38.
typedef struct Actor503500ObjWork {
    /* 0x00 */ GpObj   obj;
    /* 0x20 */ GpRec18 rec;
} Actor503500ObjWork;
STATIC_ASSERT_SIZEOF(Actor503500ObjWork, 0x38);

/// The 0x44 block `func_actor_503500_801455A4` allocates: the shared head plus
/// the effect task it reparents itself under.
typedef struct Actor503500Work44 {
    /* 0x00 */ Actor503500ObjWork head;
    /* 0x38 */ Task*              field_38;
    /* 0x3C */ s16                field_3C; // frame counter within `field_40`'s phase
    /* 0x3E */ byte               pad_3E[0x2];
    /* 0x40 */ s8                 field_40; // phase, advanced by `func_actor_503500_80145754`
    /* 0x41 */ byte               pad_41[0x3];
} Actor503500Work44;
STATIC_ASSERT_SIZEOF(Actor503500Work44, 0x44);

/// Head of the work block used by the two enemies whose teardown clears a
/// four-entry record table (`func_actor_503500_801454E0` and
/// `func_actor_503500_80145F18`, the same body twice). It follows the gameplay
/// `GpActorD4` convention: the display node's `field_C` points at the
/// `GpActorD4Rec` directly behind it, whose `field_14` in turn points at the
/// `GpRec18` table that `Gp_InitRec18Table(_, 4, 0)` zeroes at 0x38. Like
/// `Actor503500ObjWork` this type stops where the two blocks stop agreeing:
/// `func_actor_503500_80144E8C` allocates 0xD0 and `func_actor_503500_80145A2C`
/// allocates 0xAC, both with `Mem_Calloc(_, 0)`.
typedef struct Actor503500WorkRec4 {
    /* 0x00 */ GpObj        obj;
    /* 0x20 */ GpActorD4Rec d4;
    /* 0x38 */ GpRec18      rec[4];
} Actor503500WorkRec4;
STATIC_ASSERT_SIZEOF(Actor503500WorkRec4, 0x98);

/// `SVECTOR`-shaped offset read through unsigned halfwords: every consumer
/// copies the component straight into an `s16` field, so the sign of the load
/// never reaches the result and the original emits `lhu`.
typedef struct Actor503500UVec {
    /* 0x0 */ u16 vx;
    /* 0x2 */ u16 vy;
    /* 0x4 */ u16 vz;
    /* 0x6 */ u16 pad_6;
} Actor503500UVec;
STATIC_ASSERT_SIZEOF(Actor503500UVec, 8);

/// The 0xD0 block `func_actor_503500_80144E8C` allocates, which is
/// `Actor503500WorkRec4` plus this task's own payload: the effect task it
/// reparents itself under, a rotation it seeds to identity next to the one in
/// its `GsCOORDINATE2`, and the pair of words plus the halfword that
/// `func_actor_503500_801450A0` reads and writes every frame.
typedef struct Actor503500WorkD0 {
    /* 0x00 */ Actor503500WorkRec4 head;
    /* 0x98 */ Task*               field_98;
    /* 0x9C */ MATRIX              field_9C;
    /* 0xBC */ s32                 field_BC;
    /* 0xC0 */ s32                 field_C0;
    /* 0xC4 */ s16                 field_C4;
    /* 0xC6 */ byte                pad_C6[0xA];
} Actor503500WorkD0;
STATIC_ASSERT_SIZEOF(Actor503500WorkD0, 0xD0);

/// The 0xAC block `func_actor_503500_80145A2C` allocates: `Actor503500WorkRec4`
/// plus the effect task it reparents itself under.
typedef struct Actor503500WorkAC {
    /* 0x00 */ Actor503500WorkRec4 head;
    /* 0x98 */ Task*               field_98;
    /* 0x9C */ byte                pad_9C[0x10];
} Actor503500WorkAC;
STATIC_ASSERT_SIZEOF(Actor503500WorkAC, 0xAC);

/// The 0xC0 block `func_actor_503500_80144300` allocates: the display node,
/// the four-entry `GpRec18` table its `field_C` points at, the effect task it
/// reparents itself under, and the payload `func_actor_503500_80144778` steps
/// every frame. `field_84` is the world position (the coordinate's translation
/// in 16.16) and `field_94` the copy it restores from; `field_A4` is the
/// forward offset `ApplyMatrixLV` rotates out of `Task::spawnArg2`.
typedef struct Actor503500WorkC0 {
    /* 0x00 */ GpObj   obj;
    /* 0x20 */ GpRec18 rec[4];
    /* 0x80 */ Task*   field_80;
    /* 0x84 */ VECTOR  field_84;
    /* 0x94 */ VECTOR  field_94;
    /* 0xA4 */ VECTOR  field_A4;
    /* 0xB4 */ s32     field_B4;
    /* 0xB8 */ s16     field_B8;
    /* 0xBA */ byte    pad_BA[0x4];
    /* 0xBE */ s8      field_BE; // set when a record's kind (field_4 high half) is 1
    /* 0xBF */ s8      field_BF; // nonzero skips the push-back step
} Actor503500WorkC0;
STATIC_ASSERT_SIZEOF(Actor503500WorkC0, 0xC0);

/// The 0xB4 block `func_actor_503500_801448E8` allocates: the same head and
/// world-position pair as `Actor503500WorkC0`, then the payload
/// `func_actor_503500_80144B40` steps every frame. `field_A8` is the speed,
/// `Task::spawnArg2` or 0x100000 when the spawner passes none.
typedef struct Actor503500WorkB4 {
    /* 0x00 */ GpObj   obj;
    /* 0x20 */ GpRec18 rec[4];
    /* 0x80 */ Task*   field_80;
    /* 0x84 */ VECTOR  field_84;
    /* 0x94 */ VECTOR  field_94;
    /* 0xA4 */ byte    pad_A4[0x4];
    /* 0xA8 */ s32     field_A8;
    /* 0xAC */ s16     field_AC;
    /* 0xAE */ s16     field_AE;
    /* 0xB0 */ s8      field_B0;
    /* 0xB1 */ byte    pad_B1[0x3];
} Actor503500WorkB4;
STATIC_ASSERT_SIZEOF(Actor503500WorkB4, 0xB4);

/// Animation head of the boss block (`D_actor_503500_80176574`,
/// `Mem_Set(_, 0x7E8)`), viewed through its own type rather than the shared
/// `Actor503500Work`: the boss fronts its allocation with a `GpAnimCtx` --
/// `func_actor_503500_80136D30` passes the block itself to `Gp_AnimTickIndex`
/// -- whose slot array is inline at 0x14 and whose `GpAnimMtxRec` table starts
/// at 0x334, the two addresses `func_actor_503500_80135950` hands to
/// `func_800B3F84`. Twenty 0x28-byte slots fit exactly between them, and both
/// tick loops walk indices 1..0x13. That run covers 0x40..0x60, where the
/// shared view names the 0x160 block's display node, so the two blocks
/// genuinely disagree about it: the halfword the shared view calls
/// `Actor503500Slot40::boss.flags_4C` is `slots[1].field_10`, that animation
/// slot's flags word. This type stops at the matrix table.
typedef struct Actor503500WorkBoss {
    /* 0x000 */ GpAnimCtx  anim;
    /* 0x014 */ GpAnimSlot slots[20];
} Actor503500WorkBoss;
STATIC_ASSERT_SIZEOF(Actor503500WorkBoss, 0x334);

/// 0x14-byte entry of the animation-preset table `func_actor_503500_80135950`
/// is handed a pointer into (`D_actor_503500_8016EAC0`, indexed by preset id).
/// That helper copies `field_0`'s low byte into `Actor503500Work::field_7D6`
/// and re-seeds the whole slot array whenever it changes, stores `field_4` in
/// `field_7D5`, and uses `field_8` to choose between `func_800B4114` -- which
/// also takes `field_C` -- and `Gp_AnimResetSlot`. `field_10` is unread.
/// `func_actor_503500_8014652C` takes the same record as its message payload
/// and applies it to `Actor503500Effect4CC`, with `field_0` indexing the bank
/// table `D_actor_503500_80176520`.
typedef struct Actor503500AnimPreset {
    /* 0x00 */ s32 field_0;
    /* 0x04 */ s32 field_4;
    /* 0x08 */ s32 field_8;
    /* 0x0C */ s32 field_C;
    /* 0x10 */ s32 field_10;
} Actor503500AnimPreset;
STATIC_ASSERT_SIZEOF(Actor503500AnimPreset, 0x14);

/// Work block of the enemy whose state-0 init is
/// `func_actor_503500_801423C8` (`Mem_Set(_, 0x224)`), viewed through its own
/// type rather than the shared `Actor503500Work`: its three display nodes sit
/// at 0x60, 0x140 and 0x160, and the node at 0x140 covers 0x15C, where the
/// shared view already names the 0x160 block's `field_15C`. That collision is
/// why this enemy gets a separate view -- the two blocks genuinely disagree
/// about those four bytes. `func_actor_503500_801423C8` links all three nodes
/// (each followed by a `Gp_InitRec18Table` on the record area at 0x180) and
/// `func_actor_503500_80143F78` hands all three back to `Gp_UnlinkObj`.
typedef struct Actor503500Work224 {
    /* 0x000 */ byte    pad_0[0x60];
    /* 0x060 */ GpObj   obj0;
    /* 0x080 */ GpRec18 rec0[8]; // obj0's table; obj1 and obj2 share rec1
    /* 0x140 */ GpObj   obj1;
    /* 0x160 */ GpObj   obj2;
    /* 0x180 */ GpRec18 rec1[4];
    /* 0x1E0 */ byte    pad_1E0[0x38];
    /* 0x218 */ s16     field_218; // per-frame countdown, clamped at 0
    /* 0x21A */ u16     field_21A; // sub-state frame counter
    /* 0x21C */ byte    pad_21C[0x5];
    /* 0x221 */ s8      field_221; // sub-state index
    /* 0x222 */ s8      field_222; // sub-state phase, cleared with field_221
    /* 0x223 */ byte    pad_223[0x1];
} Actor503500Work224;
STATIC_ASSERT_SIZEOF(Actor503500Work224, 0x224);

/// Work block of the 0xF4 enemy whose state-0 init is
/// `func_actor_503500_8013ECBC` (`D_actor_503500_80177A6C`), viewed through its
/// own type rather than the shared `Actor503500Work`: it keeps a halfword at
/// 0xEC -- `func_actor_503500_8013F4A4` stores one there and
/// `func_actor_503500_8013F9D4` clears it -- where the shared view already
/// names the 0xF0 block's `field_EC` / `field_ED` byte pair. That collision is
/// why this enemy gets a separate view. Its sub-state index is `field_F0`, the
/// one `func_actor_503500_8013F8AC` dispatches on.
typedef struct Actor503500WorkF4 {
    /* 0x00 */ GpObj obj;      // the display node, as in `Actor503500Work`
    /* 0x20 */ byte  pad_20[0xC8];
    /* 0xE8 */ s16   field_E8; // per-frame countdown, as in `Actor503500Work`
    /* 0xEA */ u16   field_EA; // sub-state frame counter
    /* 0xEC */ s16   field_EC;
    /* 0xEE */ byte  pad_EE[0x2];
    /* 0xF0 */ s8    field_F0; // sub-state index
    /* 0xF1 */ s8    field_F1; // sub-state phase, cleared with field_F0
    /* 0xF2 */ byte  pad_F2[0x2];
} Actor503500WorkF4;
STATIC_ASSERT_SIZEOF(Actor503500WorkF4, 0xF4);

/// The second 0xF4 block: the enemy whose state-0 init is
/// `func_actor_503500_8013CAE4` (`Mem_Set` over slot `spawnArg1` of
/// `D_actor_503500_801770E8`), viewed through its own type rather than the
/// shared `Actor503500Work`. It is named after its array because size no
/// longer tells the two 0xF4 shapes apart: this one keeps its two sub-state
/// counters as halfwords at 0xEC and 0xEE -- `func_actor_503500_8013D1CC` and
/// `func_actor_503500_8013D558` step 0xEC and `func_actor_503500_8013DBA8`
/// clears both -- where the shared view names the 0xEC / 0xED / 0xEE byte
/// triple the other 0xF0 blocks dispatch on, and where `Actor503500WorkF4`
/// puts its counter pair at 0xEA / 0xEC. Its sub-state index is `field_F0`,
/// the one `func_actor_503500_8013D990` dispatches on.
typedef struct Actor503500Work770E8 {
    /* 0x00 */ GpObj          obj;
    /* 0x20 */ GpRec18        rec[8]; // Gp_InitRec18Table(rec, 8, 0)
    /* 0xE0 */ GsCOORDINATE2* field_E0;
    /* 0xE4 */ s16            field_E4;
    /* 0xE6 */ s16            field_E6;
    /* 0xE8 */ s16            field_E8; // per-frame countdown, as in `Actor503500Work`
    /* 0xEA */ byte           pad_EA[0x2];
    /* 0xEC */ u16            field_EC; // sub-state frame counter
    /* 0xEE */ s16            field_EE;
    /* 0xF0 */ s8             field_F0; // sub-state index
    /* 0xF1 */ s8             field_F1; // sub-state phase, cleared with field_F0
    /* 0xF2 */ byte           pad_F2[0x2];
} Actor503500Work770E8;
STATIC_ASSERT_SIZEOF(Actor503500Work770E8, 0xF4);

/// Element of `D_actor_503500_801774C0`, the two 0xF0 blocks
/// `func_actor_503500_8013AD64` clears for spawn slots 4 and 5. The shared
/// `Actor503500Work` cannot be indexed at this stride, so the array gets its
/// own type; the fields agree with the shared view, and the task's
/// `field_1C` still points at the block through that view. `field_EC` holds
/// the slot (`spawnArg1 - 4`) that selects this enemy's parent part and
/// local offset.
typedef struct Actor503500Work774C0 {
    /* 0x00 */ GpObj          obj;
    /* 0x20 */ GpRec18        rec;
    /* 0x38 */ byte           pad_38[0xA8];
    /* 0xE0 */ GsCOORDINATE2* field_E0;
    /* 0xE4 */ s16            field_E4;
    /* 0xE6 */ s16            field_E6;
    /* 0xE8 */ byte           pad_E8[0x4];
    /* 0xEC */ s8             field_EC;
    /* 0xED */ byte           pad_ED[0x3];
} Actor503500Work774C0;
STATIC_ASSERT_SIZEOF(Actor503500Work774C0, 0xF0);

/// The 0x4CC effect work block, allocated by `func_actor_503500_8014642C`
/// (`Mem_Calloc(0x4CC)`) and parked in that task's `Task::idMap` slot -- that
/// slot is not a `TaskIdMap` here. Unlike the tasks covered by
/// `Actor503500ObjWork` this one exits through `ActorsShared801327b4`, which
/// only calls `Gp_EnemyTaskExit`, so the block does not open with a `GpObj`.
/// `func_actor_503500_80146508` republishes the two matrices onto
/// `TmdObject::field_1C` / `field_20`, the light/colour pair
/// `Gp_BindDefaultMtx` otherwise points at `Gp_DefaultMtx` / `Gp_DefaultMtx2`,
/// exactly as `func_actor_503500_801324EC` does for `Actor503500ColorMtx`.
///
/// The size is the allocation, and the fields below are the ones the init
/// seeds: the three `sb` bytes at 0x43D/0x43E/0x4C8 are set to -1, and the
/// three words at 0x4A0..0x4A8 are cleared. This is the same layout as
/// `Actor317000Work` and its siblings in the other actor overlays, except that
/// those write 0x4C8 as a halfword.
typedef struct Actor503500Effect4CC {
    /* 0x000 */ GpAnimCtx  anim;
    /* 0x014 */ GpAnimSlot slots[0x13]; // the slot array `func_800B3F84` is handed
    /* 0x30C */ byte       field_30C[0x130];
    /* 0x43C */ s8         field_43C;   // set once the slots have been started
    /* 0x43D */ s8         field_43D;   // animation id the slots were seeded with
    /* 0x43E */ s8         field_43E;   // bank index into `D_actor_503500_80176520`
    /* 0x43F */ byte       pad_43F[0x1];
    /* 0x440 */ MATRIX     light;
    /* 0x460 */ MATRIX     color;
    /* 0x480 */ byte       pad_480[0x20];
    /* 0x4A0 */ s32        field_4A0;
    /* 0x4A4 */ s32        field_4A4;
    /* 0x4A8 */ s32        field_4A8;
    /* 0x4AC */ byte       pad_4AC[0x4];
    /* 0x4B0 */ s32        field_4B0;
    /* 0x4B4 */ s32        field_4B4;
    /* 0x4B8 */ s32        field_4B8;
    /* 0x4BC */ byte       pad_4BC[0x4];
    /* 0x4C0 */ s16        field_4C0;
    /* 0x4C2 */ byte       pad_4C2[0x6];
    /* 0x4C8 */ s8         field_4C8;
    /* 0x4C9 */ byte       pad_4C9[0x3];
} Actor503500Effect4CC;
STATIC_ASSERT_SIZEOF(Actor503500Effect4CC, 0x4CC);

/// `Task` as this overlay's enemies use it. The layout is `Task`'s
/// (`include/main/task.h`); only two slots are retyped: `idMap` holds the
/// actor's own work block rather than a `TaskIdMap`, and `spawnArg2` is the
/// `GpEnemy` that `Gp_AllocEnemy` parked there (the same object the gameplay
/// code also reaches through its sparse `GpObj5D` view).
typedef struct Actor503500 {
    /* 0x00 */ TaskNode         node;
    /* 0x08 */ Task*            parent;
    /* 0x0C */ Task*            firstChild;
    /* 0x10 */ Task*            nextSibling;
    /* 0x14 */ TaskFunc         callback;
    /* 0x18 */ TaskFunc         exitCallback;
    /* 0x1C */ Actor503500Work* field_1C;
    /* 0x20 */ GpEnemy*         field_20;
    /* 0x24 */ void*            field_24;
    /* 0x28 */ u8               spawnType;
    /* 0x29 */ u8               priority;
    /* 0x2A */ s16              killCountdown;
    /* 0x2C */ TmdObject*       extra;
    /* 0x30 */ s32              state;
    /* 0x34 */ s32              spawnArg1;
    /* 0x38 */ u8               flags;
    /* 0x39 */ byte             unknown_39[3];
    /* 0x3C */ s32              extraState;
    /* 0x40 */ byte             unknown_40[8];
} Actor503500;
STATIC_ASSERT_SIZEOF(Actor503500, 0x48);

/// Overlay-local view of the gameplay `Gp_StateC08` block (`GpStateC08` in
/// `include/gameplay/gameplay.h`). Only the byte this overlay touches is
/// named: `field_6` is the HUD/attach flags byte, and
/// `func_actor_503500_80132D60` sets its bit 0.
typedef struct Actor503500StateC08 {
    /* 0x00 */ byte pad_0[6];
    /* 0x06 */ u8   field_6;
    /* 0x07 */ byte pad_7[0x11];
} Actor503500StateC08;
STATIC_ASSERT_SIZEOF(Actor503500StateC08, 0x18);

extern Actor503500StateC08 Gp_StateC08;

void func_actor_503500_80146508(Task* arg0);
void func_actor_503500_8013F8AC(Actor503500* arg0);
void func_actor_503500_801440F0(Actor503500* arg0);
void func_actor_503500_8013AC6C(s32 p0, s32 p1, s32 p2, s32 p3, SVECTOR* coeff);
void func_actor_503500_801422B8(s32 p0, s32 p1, s32 p2, s32 p3, SVECTOR* coeff);

#endif
