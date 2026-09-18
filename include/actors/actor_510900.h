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

/// `G_SCRATCH_HEAD` seen as a struct field rather than a bare pointer. The
/// struct store carries `MEM_IN_STRUCT_P`, which is what keeps GCC 2.8.1's
/// first scheduling pass from hoisting a later struct load above the push in
/// `func_actor_510900_80138D38`.
typedef struct Actor510900ScratchStack {
    u32 sp;
} Actor510900ScratchStack;

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

/// 0x10-byte scratch `func_actor_510900_80138F44` takes from `G_SCRATCH_HEAD`
/// to rebuild the collision face this actor occupies. `center` starts as the
/// fixed local offset of the body's footprint and becomes that offset rotated
/// into world space, translated by the actor coordinate and clamped to the
/// grid; `rotated` receives each `rtv0` result in turn.
typedef struct Actor510900GridScratch {
    /* 0x00 */ SVECTOR center;
    /* 0x08 */ SVECTOR rotated;
} Actor510900GridScratch;
STATIC_ASSERT_SIZEOF(Actor510900GridScratch, 0x10);

/// 0x24-byte scratch `func_actor_510900_80134284` takes from `G_SCRATCH_HEAD`
/// to draw one frame of the debris trail. `vec0` is the effect coordinate's
/// `workm.t[]` before the per-frame drift is added and `vec1` the same after,
/// so the two `RTPS` projections give the ends of the trail `LINE_F2`.
/// `otz0` / `otz1` receive `gte_stszotz` for each end and their mean picks the
/// OT bucket; `flag` is the shared `gte_stflg` the projections are dropped on.
typedef struct Actor510900TrailScratch {
    /* 0x00 */ SVECTOR vec0;
    /* 0x08 */ SVECTOR vec1;
    /* 0x10 */ s32     otz0;
    /* 0x14 */ s32     otz1;
    /* 0x18 */ s32     flag;
    /* 0x1C */ DVECTOR sxy0;
    /* 0x20 */ DVECTOR sxy1;
} Actor510900TrailScratch;
STATIC_ASSERT_SIZEOF(Actor510900TrailScratch, 0x24);

/// 0x1C-byte scratch `func_actor_510900_80134C90` takes from `G_SCRATCH_HEAD`
/// to draw one frame of the muzzle flash. `vec` is the effect coordinate's
/// `workm.t[]` truncated to s16 and pushed through `GsWSMATRIX` by a single
/// `RTPS`; `flag` is its `gte_stflg`, `otz` its `gte_stszotz` (biased by 1 so
/// it can also be the divisor) and `sxy` its `gte_stsxy`. `dx` / `dy` are the
/// rotated half-extents `(size * 39 / otz) * rsin/rcos(angle) >> 12` that
/// offset `sxy` into the four corners of the billboard `POLY_FT4`.
typedef struct Actor510900QuadScratch {
    /* 0x00 */ SVECTOR vec;
    /* 0x08 */ s32     otz;
    /* 0x0C */ s32     flag;
    /* 0x10 */ s32     dx;
    /* 0x14 */ s32     dy;
    /* 0x18 */ DVECTOR sxy;
} Actor510900QuadScratch;
STATIC_ASSERT_SIZEOF(Actor510900QuadScratch, 0x1C);

/// One VRAM CLUT coordinate per frame of the muzzle-flash sprite, packed the
/// way `getClut` takes them. `D_actor_510900_8013C48C` holds twelve, one for
/// each frame `D_80111E48` supplies the texture window for.
typedef struct Actor510900SprClut {
    /* 0x0 */ u16 clutX;
    /* 0x2 */ u16 clutY;
} Actor510900SprClut;
STATIC_ASSERT_SIZEOF(Actor510900SprClut, 4);

typedef struct Actor510900Obj2C {
    /* 0x00 */ byte              pad_0[8];
    /* 0x08 */ Actor510900Coord* field_8;
    /* 0x0C */ s16               field_C;  // model flag word, as TmdObject::flags
    /* 0x0E */ byte              pad_E[0xE];
    /* 0x1C */ MATRIX*           field_1C; // light matrix, as TmdObject::lightMtx
    /* 0x20 */ MATRIX*           field_20; // colour matrix, as TmdObject::colorMtx
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
    /* 0x43C */ MATRIX field_43C; ///< colour matrix, handed to TmdObject::colorMtx
    /* 0x45C */ MATRIX field_45C; ///< light matrix, handed to TmdObject::lightMtx
    /* 0x47C */ GpObj  obj47C;
    /// `obj47C`'s collision table (`Gp_InitRec18Table` seeds 3 records) and the
    /// byte address the context's `field_54` points at.
    /* 0x49C */ GpRec18 rec49C[3];
    /* 0x4E4 */ GpObj   obj4E4;
    /* 0x504 */ GpObj   obj504;
    /// Shared collision table of `obj4E4` and `obj504`; only `obj4E4`'s
    /// `Gp_InitRec18Table` seeds it.
    /* 0x524 */ GpRec18        rec524[1];
    /* 0x53C */ GsCOORDINATE2* field_53C; ///< `&coord[3]`, as the context's `field_18`
    /* 0x540 */ s16            field_540;
    /* 0x542 */ s16            field_542;
    /* 0x544 */ MATRIX         field_544;
    /* 0x564 */ s32*           field_564; // 0x34 receives field_594 when it changes
                                          /// Task of the second enemy the spawn creates from `D_actor_510900_80167A18`;
                                          /// `obj4E4` hangs off its model's first coordinate.
    /* 0x568 */ Task* field_568;
    /// Task of the third enemy spawned from the same table.
    /* 0x56C */ Task* field_56C;
    /// Residual head rotation, stepped 0x20 at a time towards zero each frame
    /// by `func_actor_510900_80138D38` while it yaws the head coordinate.
    /* 0x570 */ SVECTOR field_570;
    /* 0x578 */ s32     field_578;
    /* 0x57C */ s32     field_57C; ///< sound id stopped alongside field_580
    /* 0x580 */ s32     field_580; ///< last sound id queued
    /* 0x584 */ s16     field_584;
    /// Animation id the 0x7D3 handler reseeds slots 1..0x12 with; the handler
    /// stores `Actor510900AnimArgs::field_4 + 0x1B` here.
    /* 0x586 */ s16 field_586;
    /// Animation id the slots were last reseeded with; `func_actor_510900_8013BB20`
    /// reseeds 1..0x12 whenever this differs from `field_586`.
    /* 0x588 */ s16 field_588;
    /// Blend weight the 0x7D3 handler is handed, cleared once the reseed is done.
    /* 0x58A */ s16 field_58A;
    /* 0x58C */ s16 field_58C;
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
    /* 0x59C */ s16 field_59C;
    /* 0x59E */ s16 field_59E;
    /// Yaw the head coordinate is rebuilt from each frame: the actor's facing
    /// angle, stepped 0x1E at a time towards the direction `field_5A8` selects.
    /* 0x5A0 */ s16 field_5A0;
    /// Cleared by state 0 on the frame it restarts.
    /* 0x5A2 */ s16 field_5A2;
    /* 0x5A4 */ s16 field_5A4;
    /// Distance travelled around the patrol square, advanced by `field_5A2`
    /// each frame and clamped to 0xC8..0xB66C. Its quotient by the side length
    /// picks the corner below, the remainder the offset along that side.
    /* 0x5A6 */ u16 field_5A6;
    /* 0x5A8 */ s16 field_5A8;
    /* 0x5AA */ s16 field_5AA;
    /* 0x5AC */ s16 field_5AC;
    /// Below 0x3E8 the turn target is taken one entry further along
    /// `D_actor_510900_80167B9C`.
    /* 0x5AE */ s16 field_5AE;
    /* 0x5B0 */ s16 field_5B0;
    /// Latch that sends the wind-up out to state 8 instead of back to state 1;
    /// state 3 clears it on the way through.
    /* 0x5B2 */ s16 field_5B2;
    /// Handler phase latch: state 0 sets it to 1 on entry, state 2 to 2 once
    /// the 0x50 blend has passed.
    /* 0x5B4 */ s16 field_5B4;
    /// Sub-state of the state-3 handler's disc load: 1 queues the file, 2 waits
    /// for `CdCmd_IsIdle` and plays the cue, 0 is idle.
    /* 0x5B6 */ s16 field_5B6;
    /* 0x5B8 */ s16 field_5B8;
    /* 0x5BA */ s16 field_5BA;
    /* 0x5BC */ s16 field_5BC;
    /// Grab request the child task's state 0 watches: when it equals the
    /// child's `field_334 + 1` and `field_5C0` is 1 the grab lands, and the
    /// handler takes the request back by writing -1.
    /* 0x5BE */ s16 field_5BE;
    /* 0x5C0 */ s16 field_5C0;
    /// Written by the child task's frame handler from its `field_336` when
    /// that task's `field_334` is 2 or more.
    /* 0x5C2 */ s16 field_5C2;
    /// Phase the child task's state machine reads: 1 starts it, 2 makes the
    /// grab land on the node's occupancy tag rather than 0.
    /* 0x5C4 */ s16  field_5C4;
    /* 0x5C6 */ byte pad_5C6[2];
} Actor510900Work;
STATIC_ASSERT_SIZEOF(Actor510900Work, 0x5C8);

/// Animation view of `Actor510900Work`'s prefix. `func_800B3F84` is handed the
/// block as a `GpAnimCtx`, the nineteen `GpAnimSlot`s that live at 0x14 and the
/// pose buffer that follows them at 0x30C -- the same bytes the child-task
/// views (`Actor510900ChildAnim`, `Actor510900ChildWork`) label as `GpObj`s,
/// which is why the handlers reach slot 1 as `&work->obj38.prev`.
typedef struct Actor510900Anim {
    /* 0x000 */ GpAnimCtx  context;
    /* 0x014 */ GpAnimSlot slots[0x13];
    /* 0x30C */ byte       poses[0x130];
} Actor510900Anim;
STATIC_ASSERT_SIZEOF(Actor510900Anim, 0x43C);

/// `Task::work` of the child task `func_actor_510900_8013A85C` drives: an
/// animation context `Gp_AnimTickIndex` ticks slots 1..10 of, with a pair of
/// words past it. Below 2, `field_334` + 0xB is the game-flag nibble index
/// `field_336` is written to; otherwise `field_336` goes to the parent work's
/// `field_5C2`.
typedef struct Actor510900ChildAnim {
    /* 0x000 */ GpAnimCtx  anim;
    /* 0x014 */ GpAnimSlot slots[11];   ///< `func_800B3F84` arg4, reset 1..10
    /* 0x1CC */ byte       poses[0xB0]; ///< `func_800B3F84` arg3
    /* 0x27C */ MATRIX     colorMtx;    ///< handed to `TmdObject::colorMtx`
    /* 0x29C */ MATRIX     lightMtx;    ///< handed to `TmdObject::lightMtx`
    /* 0x2BC */ GpObj      obj2BC;
    /* 0x2DC */ GpRec18    rec2DC;
    /* 0x2F4 */ GpObj      obj2F4;
    /* 0x314 */ GpRec18    rec314;
    /// Task the frame handler releases (state 2) once the camera has cut
    /// away from every view this actor is visible in.
    /* 0x32C */ struct Task* field_32C;
    /// State the frame handler below dispatches on: 0 waits for the grab, 1
    /// runs the `field_332` countdown, 2 is done.
    /* 0x330 */ s16 field_330;
    /* 0x332 */ s16 field_332;
    /* 0x334 */ s16 field_334;
    /* 0x336 */ s16 field_336;
} Actor510900ChildAnim;
STATIC_ASSERT_SIZEOF(Actor510900ChildAnim, 0x338);

/// 0x18 scratch block `func_actor_510900_8013A9BC` takes from `G_SCRATCH_HEAD`
/// for the frame it starts the grab on; only the trailing `SVECTOR` is used,
/// as the spawn argument of both effects.
typedef struct Actor510900GrabScratch {
    /* 0x00 */ byte    pad_0[0x10];
    /* 0x10 */ SVECTOR rot;
} Actor510900GrabScratch;
STATIC_ASSERT_SIZEOF(Actor510900GrabScratch, 0x18);

/// 0xD0-byte `Task::work` block `func_actor_510900_801397F0` allocates for its
/// child task: the child's colour and light matrices (handed to
/// `TmdObject::colorMtx` / `field_1C`), two linked `GpObj`s with their `GpRec18`
/// tables, and the timer/state trio `func_actor_510900_8013A100` runs its
/// teardown state machine on.
typedef struct Actor510900ChildFx {
    /* 0x00 */ MATRIX       colorMtx;
    /* 0x20 */ MATRIX       lightMtx;
    /* 0x40 */ GpObj        obj40;
    /* 0x60 */ GpRec18      rec60;
    /* 0x78 */ GpObj        obj78;
    /* 0x98 */ GpActorD4Rec d4rec;
    /* 0xB0 */ GpRec18      recB0;
    /* 0xC8 */ u16          field_C8; ///< frame counter, reset at every state change
    /* 0xCA */ s16          field_CA; ///< state: 0 fade in, 1 hold, 2 hit, 3 expire
    /* 0xCC */ s16          field_CC;
    /* 0xCE */ s16          field_CE;
} Actor510900ChildFx;
STATIC_ASSERT_SIZEOF(Actor510900ChildFx, 0xD0);

/// 0x38-byte scratch `func_actor_510900_801397F0` takes from `G_SCRATCH_HEAD`
/// to place the child effect: `rot` is first the local spawn offset and then
/// the yaw the model is turned by, `pos` that offset rotated into world space
/// and afterwards the player's XZ delta the distance is measured from, and
/// `mtx` the rotation `RotMatrix` builds and composes into the coordinate.
typedef struct Actor510900ChildFxScratch {
    /* 0x00 */ SVECTOR rot;
    /* 0x08 */ VECTOR  pos;
    /* 0x18 */ MATRIX  mtx;
} Actor510900ChildFxScratch;
STATIC_ASSERT_SIZEOF(Actor510900ChildFxScratch, 0x38);

/// 0x28-byte scratch `func_actor_510900_80139C10` takes from `G_SCRATCH_HEAD`
/// every frame the child effect turns: `rot` is the yaw it spins by (and then
/// the offset the trail effect is spawned along), `mtx` the rotation
/// `RotMatrix` builds from it and composes into the coordinate.
typedef struct Actor510900ChildFxTickScratch {
    /* 0x00 */ SVECTOR rot;
    /* 0x08 */ MATRIX  mtx;
} Actor510900ChildFxTickScratch;
STATIC_ASSERT_SIZEOF(Actor510900ChildFxTickScratch, 0x28);

/// Payload of the 0x3F8 query the hit handler asks the player before it takes
/// the hold; `field_14` is the range it asks for. The same shape as
/// `Actor400600Msg3F8`.
typedef struct Actor510900Msg3F8 {
    /* 0x00 */ byte pad_0[0x14];
    /* 0x14 */ s32  field_14;
} Actor510900Msg3F8;
STATIC_ASSERT_SIZEOF(Actor510900Msg3F8, 0x18);

/// 0x2C-byte scratch from `G_SCRATCH_HEAD` used by `func_actor_510900_8013A310`:
/// the 0x3F8 query buffer followed by the `GpAnimArg` it sends as message 0x3FF.
typedef struct Actor510900HitScratch {
    /* 0x00 */ Actor510900Msg3F8 query;
    /* 0x18 */ GpAnimArg         anim;
} Actor510900HitScratch;
STATIC_ASSERT_SIZEOF(Actor510900HitScratch, 0x2C);

/// The pair `Gp_PackPair` packs entry 4 of for the 0x3F9 message the hold sends.
extern GpU16Pair D_actor_510900_80167968;

/// Animation-set table handed to the player as the 0x3FF payload's `field_0`.
extern GpAnimSet* D_actor_510900_80167B2C[];

/// Spawn position of the child, indexed by its `Task::spawnArg1`.
extern SVECTOR D_actor_510900_80167CB8[];
/// Spawn rotation about Y, indexed the same way.
extern u16 D_actor_510900_80167CD0[];
/// Animation set table `func_800B3F84` installs in the context above.
extern GpAnimSet* D_actor_510900_80167CAC[];

/// `Actor510900ChildFx::field_CE` per 1000 units of distance between the child
/// and the player, clamped to the last entry.
extern u16 D_actor_510900_80167C94[12];

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

/// 0x7C-byte `Task::work` block `func_actor_510900_8013AD90` allocates: two
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
    /* 0x78 */ s16     field_78; // state handed to `field_70` when the grab lands
    /* 0x7A */ byte    pad_7A[0x2];
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
    /* 0x00 */ byte              pad_0[0x18];
    /* 0x18 */ TaskFunc          exitCallback;
    /* 0x1C */ Actor510900Work*  field_1C;
    /* 0x20 */ GpEnemy*          field_20;
    /* 0x24 */ void*             field_24;
    /* 0x28 */ byte              pad_28[0x4];
    /* 0x2C */ Actor510900Obj2C* field_2C;
    /* 0x30 */ s32               state;
} Actor510900;

typedef struct Actor510900Ctx {
    /* 0x00 */ byte           pad_0[0x4];
    /* 0x04 */ MATRIX*        field_4;
    /* 0x08 */ u16            field_8; // top nibble selects the sound bank
    /* 0x0A */ byte           pad_A[0x6];
    /* 0x10 */ GpLinkNode     node;
    /* 0x18 */ GsCOORDINATE2* field_18;
    /* 0x1C */ s32            field_1C;
    /* 0x20 */ s32            field_20;
    /* 0x24 */ s32            field_24;
    /* 0x28 */ byte           pad_28[0x18];
    /* 0x40 */ u16            field_40; ///< HP, seeded from the pair source's `field_4`
    /* 0x42 */ byte           pad_42[0x6];
    /* 0x48 */ u8             field_48;
    /* 0x49 */ byte           pad_49[0x3];
    /* 0x4C */ u8             field_4C;
    /* 0x4D */ byte           pad_4D[0x3];
    /* 0x50 */ GpPairSrcE*    field_50;
    /* 0x54 */ s32            field_54; ///< byte address of `Actor510900Work::rec49C`
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

/// Per-cycle threshold the patrol walk in `func_actor_510900_80136B70` compares
/// a 4-bit `Gp_LcgState` draw against, indexed by the `field_59C` cycle counter;
/// a draw above the entry ends the walk.
extern s16 D_actor_510900_80167A10[];

/// `TaskDesc` table the state hands `Gp_SpawnEnemyFromTable` (entry 4).
extern TaskDesc D_actor_510900_80167A18[];

/// Per-animation-id value `func_actor_510900_8013BB20` hands `func_800B4114`
/// as its fifth argument when it reseeds animation slots 1..0x12.
extern s16 D_actor_510900_80167B38[];

/// One corner of the square `func_actor_510900_80138978` walks the actor
/// around, in world x/z.
typedef struct Actor510900PatrolCorner {
    /* 0x0 */ s16 x;
    /* 0x2 */ s16 z;
} Actor510900PatrolCorner;
STATIC_ASSERT_SIZEOF(Actor510900PatrolCorner, 0x4);

/// The unit direction the side leaving that corner runs in.
typedef struct Actor510900PatrolStep {
    /* 0x0 */ s8 x;
    /* 0x1 */ s8 z;
} Actor510900PatrolStep;
STATIC_ASSERT_SIZEOF(Actor510900PatrolStep, 0x2);

/// The four corners of the patrol square, indexed by `Actor510900Work::field_5A8`.
extern Actor510900PatrolCorner D_actor_510900_80167B84[4];

/// The direction of each of its four sides, indexed the same way.
extern Actor510900PatrolStep D_actor_510900_80167B94[4];

/// The four cardinal yaws `func_actor_510900_801387F4` turns the actor's
/// coordinate towards, indexed by `Actor510900Work::field_5A8`.
/// `func_800FDB18` argument record the state-3 handler refreshes every sixth
/// frame from the player's model coordinates.
extern GpEffArg D_actor_510900_80167B7C;

extern u16 D_actor_510900_80167B9C[];

/// One of the four world-space x/z boxes `func_actor_510900_8013864C` tests the
/// player against; the index of the box containing them is latched into
/// `Actor510900Work::field_5AA`.
typedef struct Actor510900PatrolBox {
    /* 0x0 */ s16 minX;
    /* 0x2 */ s16 maxX;
    /* 0x4 */ s16 minZ;
    /* 0x6 */ s16 maxZ;
} Actor510900PatrolBox;
STATIC_ASSERT_SIZEOF(Actor510900PatrolBox, 0x8);

/// The four boxes, in the same order as the patrol square's corners.
extern Actor510900PatrolBox D_actor_510900_80167BA4[4];

/// The three face normals `func_actor_510900_8013B524` copies into
/// `Gp_GridParams->field_4`, restoring the collision grid this actor edited.
extern SVECTOR D_actor_510900_80167BC4[3];

/// The twelve face corners `func_actor_510900_8013B524` copies into
/// `Gp_GridParams->field_8`.
extern SVECTOR D_actor_510900_80167BDC[12];

/// The three `GpGridFace` records `func_actor_510900_8013B524` copies into
/// `Gp_GridParams->field_C`.
extern GpGridFace D_actor_510900_80167C3C[3];

/// The extra face normal `func_actor_510900_8013B424` installs as
/// `Gp_GridParams->field_4[3]` while the actor's own face is in the grid.
extern SVECTOR D_actor_510900_80167C60;

/// The four face corners that face uses, copied into
/// `Gp_GridParams->field_8[12..15]`.
extern SVECTOR D_actor_510900_80167C68[4];

/// The `GpGridFace` record for that face, copied into
/// `Gp_GridParams->field_C[3]`.
extern GpGridFace D_actor_510900_80167C88;

void func_actor_510900_801355B4(Actor510900Ctx* arg0, Actor510900* arg1);
void func_actor_510900_8013B608(Actor510900* arg0);
void func_actor_510900_8013B6A0(Actor510900Ctx* arg0, Actor510900* arg1);
void func_actor_510900_8013B870(Actor510900* arg0);
void func_actor_510900_8013B988(Actor510900* arg0);
void func_actor_510900_8013BA58(Actor510900* arg0);
s16  func_actor_510900_8013BE84(Actor510900* arg0);
void func_actor_510900_8013C380(Actor510900* arg0);
void func_actor_510900_8013C430(Actor510900* arg0);

#endif
