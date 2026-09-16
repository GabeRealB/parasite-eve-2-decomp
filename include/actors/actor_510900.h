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

/// 0x40-byte scratch `func_actor_510900_80138BF0` takes from `G_SCRATCH_HEAD`
/// to aim the head coordinate at the player. `view` is the head coordinate in
/// view space, `delta` the player offset from it, and `local` that offset
/// rotated into the body's frame and clamped before `Gp_OrientAlong`.
typedef struct Actor510900AimScratch {
    /* 0x00 */ MATRIX view;
    /* 0x20 */ VECTOR delta;
    /* 0x30 */ VECTOR local;
} Actor510900AimScratch;
STATIC_ASSERT_SIZEOF(Actor510900AimScratch, 0x40);

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
    /* 0x568 */ byte   pad_568[0x10];
    /* 0x578 */ s32    field_578;
    /* 0x57C */ s32    field_57C; ///< sound id stopped alongside field_580
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
    /* 0x59E */ byte pad_59E[2];
    /// Yaw the head coordinate is rebuilt from each frame: the actor's facing
    /// angle, stepped 0x1E at a time towards the direction `field_5A8` selects.
    /* 0x5A0 */ s16 field_5A0;
    /// Cleared by state 0 on the frame it restarts.
    /* 0x5A2 */ s16  field_5A2;
    /* 0x5A4 */ s16  field_5A4;
    /* 0x5A6 */ byte pad_5A6[2];
    /* 0x5A8 */ s16  field_5A8;
    /* 0x5AA */ s16  field_5AA;
    /* 0x5AC */ s16  field_5AC;
    /// Below 0x3E8 the turn target is taken one entry further along
    /// `D_actor_510900_80167B9C`.
    /* 0x5AE */ s16  field_5AE;
    /* 0x5B0 */ s16  field_5B0;
    /* 0x5B2 */ byte pad_5B2[6];
    /* 0x5B8 */ s16  field_5B8;
    /* 0x5BA */ s16  field_5BA;
    /* 0x5BC */ s16  field_5BC;
    /* 0x5BE */ byte pad_5BE[4];
    /// Written by the child task's frame handler from its `field_336` when
    /// that task's `field_334` is 2 or more.
    /* 0x5C2 */ s16 field_5C2;
} Actor510900Work;

/// `Task::idMap` of the child task `func_actor_510900_8013A85C` drives: an
/// animation context `Gp_AnimTickIndex` ticks slots 1..10 of, with a pair of
/// words past it. Below 2, `field_334` + 0xB is the game-flag nibble index
/// `field_336` is written to; otherwise `field_336` goes to the parent work's
/// `field_5C2`.
typedef struct Actor510900ChildAnim {
    /* 0x000 */ GpAnimCtx  anim;
    /* 0x014 */ GpAnimSlot slots[11];   ///< `func_800B3F84` arg4, reset 1..10
    /* 0x1CC */ byte       poses[0xB0]; ///< `func_800B3F84` arg3
    /* 0x27C */ MATRIX     colorMtx;    ///< handed to `TmdObject::field_20`
    /* 0x29C */ MATRIX     lightMtx;    ///< handed to `TmdObject::field_1C`
    /* 0x2BC */ GpObj      obj2BC;
    /* 0x2DC */ GpRec18    rec2DC;
    /* 0x2F4 */ GpObj      obj2F4;
    /* 0x314 */ GpRec18    rec314;
    /// Task the frame handler releases (state 2) once the camera has cut
    /// away from every view this actor is visible in.
    /* 0x32C */ struct _Task* field_32C;
    /* 0x330 */ byte          pad_330[2];
    /* 0x332 */ s16           field_332;
    /* 0x334 */ s16           field_334;
    /* 0x336 */ s16           field_336;
} Actor510900ChildAnim;
STATIC_ASSERT_SIZEOF(Actor510900ChildAnim, 0x338);

/// 0xD0-byte `Task::idMap` block `func_actor_510900_801397F0` allocates for its
/// child task: the child's colour and light matrices (handed to
/// `TmdObject::field_20` / `field_1C`), two linked `GpObj`s with their `GpRec18`
/// tables, and the timer/state trio `func_actor_510900_8013A100` runs its
/// teardown state machine on.
typedef struct Actor510900ChildFx {
    /* 0x00 */ MATRIX  colorMtx;
    /* 0x20 */ MATRIX  lightMtx;
    /* 0x40 */ GpObj   obj40;
    /* 0x60 */ GpRec18 rec60;
    /* 0x78 */ GpObj   obj78;
    /* 0x98 */ GpRec18 rec98;
    /* 0xB0 */ GpRec18 recB0;
    /* 0xC8 */ u16     field_C8; ///< frame counter, reset at every state change
    /* 0xCA */ s16     field_CA; ///< state: 0 fade in, 1 hold, 2 hit, 3 expire
    /* 0xCC */ s16     field_CC;
    /* 0xCE */ s16     field_CE;
} Actor510900ChildFx;
STATIC_ASSERT_SIZEOF(Actor510900ChildFx, 0xD0);

/// Spawn position of the child, indexed by its `Task::spawnArg1`.
extern SVECTOR D_actor_510900_80167CB8[];
/// Spawn rotation about Y, indexed the same way.
extern u16 D_actor_510900_80167CD0[];
/// Animation set table `func_800B3F84` installs in the context above.
extern GpAnimSet* D_actor_510900_80167CAC[];

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
    /* 0x70 */ Task*   field_70; // released (state 3) on a view change
    /* 0x74 */ s16     field_74; // row of `D_actor_510900_80167CEC`
    /* 0x76 */ s16     field_76; // countdown, decremented on a view change
    /* 0x78 */ byte    pad_78[0x4];
} Actor510900ChildWork;
STATIC_ASSERT_SIZEOF(Actor510900ChildWork, 0x7C);

/// Gameplay-resident block at `D_8011505C` the spawn handler below seeds: a
/// mode word, then a `GsCOORDINATE2` (set local to `Gfx_ViewWorldMtx`) followed
/// by a rotation and two distances.
typedef struct Actor510900CamCoord {
    /* 0x00 */ GsCOORDINATE2 coord;
    /* 0x50 */ SVECTOR       rot;
    /* 0x58 */ s32           field_58;
    /* 0x5C */ s32           field_5C;
} Actor510900CamCoord;

typedef struct Actor510900Cam {
    /* 0x00 */ s32                 field_0;
    /* 0x04 */ Actor510900CamCoord cam;
} Actor510900Cam;

extern Actor510900Cam D_8011505C;

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

/// Table the 0x14-animation state picks `field_59C` from, indexed by a 4-bit
/// `Gp_LcgState` draw the same way `D_actor_510900_801679F0` is.
extern u16 D_actor_510900_801679D0[];

/// `TaskDesc` table the state hands `Gp_SpawnEnemyFromTable` (entry 4).
extern TaskDesc D_actor_510900_80167A18[];

/// Per-animation-id value `func_actor_510900_8013BB20` hands `func_800B4114`
/// as its fifth argument when it reseeds animation slots 1..0x12.
extern s16 D_actor_510900_80167B38[];

/// The four cardinal yaws `func_actor_510900_801387F4` turns the actor's
/// coordinate towards, indexed by `Actor510900Work::field_5A8`.
extern u16 D_actor_510900_80167B9C[];

/// The three face normals `func_actor_510900_8013B524` copies into
/// `Gp_GridParams->field_4`, restoring the collision grid this actor edited.
extern SVECTOR D_actor_510900_80167BC4[3];

/// The twelve face corners `func_actor_510900_8013B524` copies into
/// `Gp_GridParams->field_8`.
extern SVECTOR D_actor_510900_80167BDC[12];

/// The three `GpGridFace` records `func_actor_510900_8013B524` copies into
/// `Gp_GridParams->field_C`.
extern GpGridFace D_actor_510900_80167C3C[3];

void func_actor_510900_801355B4(Actor510900Ctx* arg0, Actor510900* arg1);
void func_actor_510900_8013B608(Actor510900* arg0);
void func_actor_510900_8013B6A0(Actor510900Ctx* arg0, Actor510900* arg1);
void func_actor_510900_8013B870(Actor510900* arg0);
void func_actor_510900_8013B988(Actor510900* arg0);
s16  func_actor_510900_8013BE84(Actor510900* arg0);
void func_actor_510900_8013C380(Actor510900* arg0);
void func_actor_510900_8013C430(Actor510900* arg0);

#endif
