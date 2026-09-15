#ifndef ACTOR_510900_H
#define ACTOR_510900_H

#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "main/task.h"

/// The actor's pair of `GsCOORDINATE2`s, 0x50 apart. The frame handler clears
/// both `flg` words before `Gp_UpdateCoord`; the ground-quad helper draws at
/// `field_50`'s x/z and `field_0`'s y.
typedef struct Actor510900Coord {
    /* 0x00 */ GsCOORDINATE2 field_0;
    /* 0x50 */ GsCOORDINATE2 field_50;
} Actor510900Coord;

typedef struct Actor510900Obj2C {
    /* 0x00 */ byte              pad_0[8];
    /* 0x08 */ Actor510900Coord* field_8;
    /* 0x0C */ s16               field_C; // model flag word, as TmdObject::field_C
} Actor510900Obj2C;

typedef struct Actor510900Work {
    /* 0x000 */ GpObj  obj0;
    /* 0x020 */ byte   pad_20[0x18];
    /* 0x038 */ GpObj  obj38;
    /* 0x058 */ byte   pad_58[0x264];
    /* 0x2BC */ GpObj  obj2BC;
    /* 0x2DC */ byte   pad_2DC[0x18];
    /* 0x2F4 */ GpObj  obj2F4;
    /* 0x314 */ byte   pad_314[0x128];
    /* 0x43C */ MATRIX field_43C; ///< colour matrix, handed to TmdObject::field_20
    /* 0x45C */ MATRIX field_45C; ///< light matrix, handed to TmdObject::field_1C
    /* 0x47C */ GpObj  obj47C;
    /* 0x49C */ byte   pad_49C[0x48];
    /* 0x4E4 */ GpObj  obj4E4;
    /* 0x504 */ GpObj  obj504;
    /* 0x524 */ byte   pad_524[0x40];
    /* 0x564 */ s32*   field_564; // 0x34 receives field_594 when it changes
    /* 0x568 */ byte   pad_568[0x18];
    /* 0x580 */ s32    field_580; ///< last sound id queued
    /* 0x584 */ s16    field_584;
    /// Animation id the 0x7D3 handler reseeds slots 1..0x12 with; the handler
    /// stores `Actor510900AnimArgs::field_4 + 0x1B` here.
    /* 0x586 */ s16 field_586;
    /// Animation id the slots were last reseeded with; `func_actor_510900_8013BB20`
    /// reseeds 1..0x12 whenever this differs from `field_586`.
    /* 0x588 */ s16 field_588;
    /// Blend weight the 0x7D3 handler is handed, cleared once the reseed is done.
    /* 0x58A */ s16  field_58A;
    /* 0x58C */ byte pad_58C[2];
    /// Handler index `func_actor_510900_8013B870` dispatches on each frame:
    /// case 7 enters state 0 below, so this is the currently running one.
    /* 0x58E */ s16 field_58E;
    /// Per-handler sub-state. State 0 waits for `field_20`'s spawn block flag
    /// to fire and then hands state 1 the animation 0x14; state 1 waits out
    /// `field_58A` and drops back to state 0 with a fresh `field_59C`.
    /* 0x590 */ s16 field_590;
    /* 0x592 */ s16 field_592;
    /* 0x594 */ s16 field_594;
    /// `field_594` as last pushed to `field_564`.
    /* 0x596 */ s16 field_596;
    /* 0x598 */ s16 field_598;
    /// The `GpAnimRec::field_3` bits 0x30 latched on the previous frame. The
    /// step handler plays a sound on the frame one of them has just dropped.
    /* 0x59A */ u16 field_59A;
    /// Rolled from `Gp_LcgState` when state 1 expires.
    /* 0x59C */ s16  field_59C;
    /* 0x59E */ byte pad_59E[4];
    /// Cleared by state 0 on the frame it restarts.
    /* 0x5A2 */ s16  field_5A2;
    /* 0x5A4 */ s16  field_5A4;
    /* 0x5A6 */ byte pad_5A6[0x12];
    /* 0x5B8 */ s16  field_5B8;
    /* 0x5BA */ byte pad_5BA[2];
    /* 0x5BC */ s16  field_5BC;
} Actor510900Work;

/// Word-wise view of a `MATRIX` used to splat an identity rotation: five
/// aligned stores instead of nine halfword ones, each word holding two adjacent
/// `m[][]` entries. The same shape `Actor206100MatrixWords` has.
typedef struct Actor510900MatrixWords {
    /* 0x00 */ s32 m00_m01;
    /* 0x04 */ s32 m02_m10;
    /* 0x08 */ s32 m11_m12;
    /* 0x0C */ s32 m20_m21;
    /* 0x10 */ s16 m22;
} Actor510900MatrixWords;

/// 0x7C-byte `Task::idMap` block `func_actor_510900_8013AD90` allocates: two
/// linked `GpObj`s, each with its one-entry `GpRec18` table, laid out as the
/// head of `Actor510900Work` (`func_actor_510900_8013C430` unlinks both).
typedef struct Actor510900ChildWork {
    /* 0x00 */ GpObj   obj0;
    /* 0x20 */ GpRec18 rec20;
    /* 0x38 */ GpObj   obj38;
    /* 0x58 */ GpRec18 rec58;
    /* 0x70 */ byte    pad_70[0xC];
} Actor510900ChildWork;
STATIC_ASSERT_SIZEOF(Actor510900ChildWork, 0x7C);

typedef struct Actor510900 {
    /* 0x00 */ byte              pad_0[0x1C];
    /* 0x1C */ Actor510900Work*  field_1C;
    /* 0x20 */ GpEnemy*          field_20;
    /* 0x24 */ byte              pad_24[0x8];
    /* 0x2C */ Actor510900Obj2C* field_2C;
} Actor510900;

typedef struct Actor510900Ctx {
    /* 0x00 */ byte pad_0[0x8];
    /* 0x08 */ u16  field_8; // top nibble selects the sound bank
    /* 0x0A */ byte pad_A[0xA];
    /* 0x14 */ u8   field_14;
    /* 0x15 */ byte pad_15[0x37];
    /* 0x4C */ u8   field_4C;
} Actor510900Ctx;

/// 0x7D3 argument block. `field_4` is the animation the actor switches to,
/// biased by 0x1B into the id the handler stores in `Actor510900Work::field_586`
/// and reseeds animation slots 1..0x12 with; a non-zero `field_8` starts every
/// reseeded slot at blend 0x80 instead of snapping to the new pose.
typedef struct Actor510900AnimArgs {
    /* 0x00 */ byte pad_0[4];
    /* 0x04 */ u16  field_4;
    /* 0x06 */ byte pad_6[2];
    /* 0x08 */ s32  field_8;
} Actor510900AnimArgs;

/// Table the state 1 handler below picks `field_59C` from; a 4-bit
/// `Gp_LcgState` draw indexes at least sixteen `u16` entries.
extern u16 D_actor_510900_801679F0[];

/// Per-animation-id value `func_actor_510900_8013BB20` hands `func_800B4114`
/// as its fifth argument when it reseeds animation slots 1..0x12.
extern s16 D_actor_510900_80167B38[];

void func_actor_510900_801355B4(Actor510900Ctx* arg0, Actor510900* arg1);
void func_actor_510900_8013B608(Actor510900* arg0);
void func_actor_510900_8013B6A0(Actor510900Ctx* arg0, Actor510900* arg1);
void func_actor_510900_8013B870(Actor510900* arg0);
void func_actor_510900_8013B988(Actor510900* arg0);
s16  func_actor_510900_8013BE84(Actor510900* arg0);
void func_actor_510900_8013C380(Actor510900* arg0);
void func_actor_510900_8013C430(Actor510900* arg0);

#endif
