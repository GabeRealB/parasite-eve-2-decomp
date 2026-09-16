#ifndef ACTOR_401000_H
#define ACTOR_401000_H

#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3FB8.h"
#include "main/task.h"
#include "main/tmd.h"

#include <psyq/inline_c.h>

/// An XZ pair: `field_C[0]` is the actor's spawn square and `field_C[1]` one
/// step along its facing, both rebuilt by `func_actor_401000_80133274`. Same
/// shape as `Actor401300Waypoint` / `Actor01900Waypoint`.
typedef struct Actor401000Waypoint {
    /* 0x0 */ s16 x;
    /* 0x2 */ s16 z;
} Actor401000Waypoint;

/// One 0xC-byte combat-parameter record `func_actor_401000_80133274` selects
/// with `Task::spawnArg1 & 0xF` and copies into `Actor401000Work.field_C10`
/// through `field_C16`; the four halfwords are the frame bias, the turn step,
/// a third parameter and the radius, and the tail is not copied.
typedef struct Actor401000SeedRec {
    /* 0x0 */ s16  field_0;
    /* 0x2 */ s16  field_2;
    /* 0x4 */ s16  field_4;
    /* 0x6 */ s16  field_6;
    /* 0x8 */ byte pad_8[4];
} Actor401000SeedRec;
STATIC_ASSERT_SIZEOF(Actor401000SeedRec, 0xC);

/// Animation view of `Actor401000Work`'s prefix. `func_800B3F84` is handed the
/// context, the pose buffer just past its slot array, and the array itself;
/// the work block's own fields at 0x898 and up are not repeated here. Same
/// shape as `Actor401300AnimWork`, 4 bytes earlier.
typedef struct Actor401000AnimWork {
    /* 0x000 */ byte       pad_0[0x1C];
    /* 0x01C */ GpAnimCtx  anim;
    /* 0x030 */ GpAnimSlot slots[19];
    /* 0x328 */ byte       pad_328[0x130];
    /* 0x458 */ GpAnimCtx  blendAnim;
    /* 0x46C */ GpAnimSlot blendSlots[19];
    /* 0x764 */ byte       pad_764[0x134];
} Actor401000AnimWork;
STATIC_ASSERT_SIZEOF(Actor401000AnimWork, 0x898);

/// Status flags at `Actor401000Work` + 0x68, read through two widths: the
/// guards in this overlay test bit 0 or bit 0x100 as a halfword, while
/// `func_actor_401000_80134DB4` tests bits 0x102 as a word, so both views are
/// modelled explicitly rather than casting at the use site. The same shape as
/// `Actor341700Flags` / `Actor342400Flags` / `Actor400500HitFlags`.
typedef union Actor401000Flags68 {
    /* 0x0 */ u32 word;
    /* 0x0 */ u16 half;
} Actor401000Flags68;
STATIC_ASSERT_SIZEOF(Actor401000Flags68, 0x4);

/// Private work block of the actor 401000 task, hanging off `Task::idMap`.
///
/// Only the fields the decompiled code touches are named, so the struct is
/// deliberately open-ended: the three `GpObj` display nodes the teardown hands
/// back to `Gp_UnlinkObj`, and the two child tasks it kills. The nodes sit
/// 8 bytes later than the 0x8C8/0xA08/0xB48 triple on actor 01900/401800, with
/// the same 0x140 stride.
/// `field_4` is the live-actor flag `func_actor_401000_8013DB10` tests, and
/// `field_B50.flags` / `field_A10.flags` are the two masks it writes. The
/// halfwords at 0x898..0x8A2 are the same animation-state slots
/// `Actor01900_Fn0A7C0` writes; `func_actor_401000_8013DB6C` is that body
/// with `field_A10.flags |= 0x4000` in place of the sibling's `&= 0xBFFF`.
typedef struct Actor401000Work {
    /* 0x000 */ s16 field_0;
    /* 0x002 */ s16 field_2;
    /* 0x004 */ s16 field_4;
    /// One-shot latch `func_actor_401000_8013922C` raises once the actor's
    /// spawn sound has been queued; the same slot `Actor401300Work` keeps at
    /// +0x6.
    /* 0x006 */ s16 field_6;
    /// Frame counter `func_actor_401000_8013C46C` runs the 0x5B re-arm and the
    /// 0xF1 turn flip off; the same slot `Actor401300Work` keeps at +0x8.
    /* 0x008 */ s16  field_8;
    /* 0x00A */ byte pad_A[2];
    /// Spawn square and one step along the facing, both narrowed to 16 bits by
    /// `func_actor_401000_80133274`'s normalised heading.
    /* 0x00C */ Actor401000Waypoint field_C[2];
    /* 0x014 */ s16                 field_14;
    /* 0x016 */ byte                pad_16[0x44];
    /* 0x05A */ u16                 field_5A;
    /* 0x05C */ byte                pad_5C[0xC];
    /* 0x068 */ Actor401000Flags68  flags_68;
    /* 0x06C */ byte                pad_6C[0x828];
    /* 0x894 */ s32                 field_894;
    /* 0x898 */ s16                 field_898;
    /* 0x89A */ s16                 field_89A;
    /* 0x89C */ byte                pad_89C[2];
    /* 0x89E */ s16                 field_89E;
    /* 0x8A0 */ byte                pad_8A0[2];
    /* 0x8A2 */ s16                 field_8A2;
    /* 0x8A4 */ s16                 field_8A4;
    /* 0x8A6 */ byte                pad_8A6[8];
    /* 0x8AE */ s16                 field_8AE;
    /* 0x8B0 */ s16                 field_8B0;
    /* 0x8B2 */ byte                pad_8B2[2];
    /// Last animation state `func_actor_401000_8013922C` acted on; the same
    /// de-duplication slot `Actor401300Work` keeps at +0x8BC.
    /* 0x8B4 */ s32      field_8B4;
    /* 0x8B8 */ GpEffArg field_8B8;
    /// Offset the actor's state-3/5/7/8 effects spawn at, passed as the
    /// `Gp_SpawnEff` position: the same local `SVECTOR` `Actor401300` keeps on
    /// the stack for the 3013B6E8 triple, materialised into the work block
    /// here because every one of the four spawns reads it.
    /* 0x8C0 */ SVECTOR field_8C0;
    /* 0x8C8 */ byte    pad_8C8[8];
    /* 0x8D0 */ GpObj   field_8D0;
    /// The two obstacle-record tables `func_actor_401000_801323EC` slides the
    /// root coordinate against; the same pair `Actor01900Work` keeps at
    /// +0x8E8 / +0xA28 and `func_actor_401300_801323B0` walks at +0x990/+0xAD0.
    /* 0x8F0 */ byte  field_8F0[0x120];
    /* 0xA10 */ GpObj field_A10;
    /* 0xA30 */ byte  field_A30[0x120];
    /* 0xB50 */ GpObj field_B50;
    /// The single obstacle record the `field_B50` node is registered against.
    /* 0xB70 */ GpRec18 field_B70;
    /// Light matrix `func_actor_401000_80133274` binds to the model's
    /// `TmdObject::field_1C` (the color matrix is `field_BA8`, which is the
    /// same pair `Actor401300Work` keeps at +0xC28 / +0xC48).
    /* 0xB88 */ MATRIX field_B88;
    /// Saved at 0xBA8 and copied over 0xBC8 when
    /// `func_actor_401000_80138F50` enters its state; the same pair
    /// `Actor401300Work` keeps at +0xC48 / +0xC68.
    /* 0xBA8 */ MATRIX field_BA8;
    /* 0xBC8 */ MATRIX field_BC8;
    /// Cleared by `func_actor_401000_80133274` right after the `field_A10`
    /// node is linked; the same slot `Actor401300Work` keeps at +0xC88.
    /* 0xBE8 */ s16  field_BE8;
    /* 0xBEA */ byte pad_BEA[6];
    /// Forward direction `func_actor_401000_801374D4` rebuilds from the wrapped
    /// turn toward the player: `Gfx_RotMatrixY` on the turn then its second
    /// column, normalised, and finally scaled by the `field_C0A` draw. The same
    /// slot `Actor401300Work` keeps at +0xC8C.
    /* 0xBF0 */ SVECTOR field_BF0;
    /* 0xBF8 */ byte    pad_BF8[0x8];
    /// Turn angle `func_actor_401000_80136E20` rebuilds the facing from, and
    /// the yaw it is driven to: each entry nudges `field_C00` by 0x89 toward
    /// `field_C02` and stops once they meet, and `Gfx_RotMatrixY` /
    /// `Actor401000_RescaleYaw` turn that angle into the root rotation. The
    /// same pair `Actor401300Work` keeps at +0xC94 / +0xC96.
    /* 0xC00 */ s16 field_C00;
    /* 0xC02 */ s16 field_C02;
    /// Turn countdown `func_actor_401000_80139D10` runs while it walks the
    /// actor at the player: the `func_actor_401000_80132590` probe reads it
    /// signed, the step helper and the countdown itself through a `(u16)`.
    /* 0xC04 */ s16  field_C04;
    /* 0xC06 */ byte pad_C06[2];
    /// Turn direction `func_actor_401000_801374D4` toggles as it enters: 0 (the
    /// unseeded state) draws a sign from `Gp_LcgState`, and each entry flips it
    /// to the other side. Selects the `field_89E` clip and the `field_C12` sign.
    /// The same slot `Actor401300Work` keeps at +0xC9C.
    /* 0xC08 */ s16 field_C08;
    /// Turn length `func_actor_401000_801374D4` rebuilds the forward direction
    /// with: seeded to 0xDE, taken signed by the `gte_lddp` draw and halved
    /// while the actor overlaps an obstacle record. The same slot
    /// `Actor401300Work` keeps at +0xC9E.
    /* 0xC0A */ s16 field_C0A;
    /// Forward step `func_actor_401000_801385B0` walks the root by, feeding the
    /// same `MoveForwardNonzero` helper `Actor401300Work` keeps at +0xC98.
    /// Set to -0x78 when the live-actor flag goes up, halved while the actor
    /// overlaps an obstacle record. The three reads widen it differently: the
    /// `func_actor_401000_80132590` probe takes the signed value, while the
    /// step helper and the halving read it back through a `(u16)`.
    /* 0xC0C */ s16  field_C0C;
    /* 0xC0E */ byte pad_C0E[2];
    /// Frame-length bias `func_actor_401000_8013DF6C` reseeds the `field_6`
    /// countdown from, plus a 0-15 `Gp_LcgState` draw. The 401300 sibling keeps
    /// the same bias at +0xCA0, and the countdown `Actor01900` runs off +0xC10
    /// is the same slot.
    /* 0xC10 */ u16 field_C10;
    /// Turn step `func_actor_401000_801374D4` adds to (or subtracts from) the
    /// wrapped facing each entry; the same slot `Actor401300Work` keeps at
    /// +0xCA2 and `Actor01900Work` at +0xC14.
    /* 0xC12 */ s16 field_C12;
    /// Third of the four halfwords `func_actor_401000_80133274` copies out of
    /// the `spawnArg1`-selected record; not read anywhere yet.
    /* 0xC14 */ s16 field_C14;
    /// Radius `func_actor_401000_8013922C` and `func_actor_401000_80138F50`
    /// test the actor's distance from `D_80073B8C` against.
    /* 0xC16 */ u16 field_C16;
    /// The three bytes `func_actor_401000_8013D958` copies out of the front of
    /// the message payload; the same triple `Actor01900Work` keeps at +0xC34.
    /* 0xC18 */ u8 field_C18[3];
    /// Turn cooldown `func_actor_401000_80136E20` spends an entry on: while it
    /// is up the actor keeps the state-8 arm instead of the 0xB one, and each
    /// entry it is up it counts down by one. The same slot `Actor401300Work`
    /// keeps at +0xC1F.
    /* 0xC1B */ u8 field_C1B;
    /// The two helper tasks killed before the nodes are unlinked; the same
    /// pair `Actor01900Work` keeps at +0xC38 / +0xC3C.
    /* 0xC1C */ Task* field_C1C;
    /* 0xC20 */ Task* field_C20;
    /// Counter `func_actor_401000_80136E20` gates the turn-entry obstacle
    /// probe on: below 2 the actor keeps the state-8 arm whatever the range
    /// check says. The same slot `Actor401300Work` keeps at +0xD1C.
    /* 0xC24 */ s16 field_C24;
    /// Wraps counter `func_actor_401000_801374D4` counts the turn entries with:
    /// nonzero picks the un-biased `field_C12` arm, and each entry increments
    /// it. The same slot `Actor401300Work` keeps at +0xD1E.
    /* 0xC26 */ s16 field_C26;
    /// Latch `func_actor_401000_801385B0` clears after sending the closing
    /// 0x3F1 message, gating on it being 1 the same way the 0x3ED probe does.
    /// The same slot `Actor00100Work` keeps at +0xC28.
    /* 0xC28 */ s16  field_C28;
    /* 0xC2A */ byte pad_C2A[0x52];
    /// Cleared by `func_actor_401000_80133274` once both obstacle tables have
    /// been dropped; the write cursor `Actor401300Work` keeps at +0xD78.
    /* 0xC7C */ s16  field_C7C;
    /* 0xC7E */ byte pad_C7E[2];
} Actor401000Work;
STATIC_ASSERT_SIZEOF(Actor401000Work, 0xC80);

/// Per-task actor context: `field_1C` is the work block above (the same
/// pointer `Task::idMap` holds), `field_20` the `GpEnemy` in
/// `Task::spawnArg2`, and `field_2C` the actor's `TmdObject`. Same shape as
/// `Actor01900`.
typedef struct Actor401000 {
    /* 0x00 */ byte             pad_0[0x1C];
    /* 0x1C */ Actor401000Work* field_1C;
    /* 0x20 */ GpEnemy*         field_20;
    /* 0x24 */ byte             pad_24[8];
    /* 0x2C */ TmdObject*       field_2C;
    /* 0x30 */ byte             pad_30[6];
    /// High halfword of `Task::spawnArg1`; `func_actor_401000_801374D4` returns
    /// early on the 0x1 arm before it touches the scratch block.
    /* 0x36 */ s16 field_36;
} Actor401000;

/// Message payload of `func_actor_401000_8013D694`, the actor's animation
/// request handler: `field_4` is the requested clip index, which the handler
/// maps onto `Actor401000Work.field_89E` (0x22-0x25, 0x27) and then restarts
/// the state halfwords. Only the two leading words of the argument block are
/// read, so the struct covers just those.
typedef struct Actor401000Msg {
    /* 0x0 */ s32 field_0;
    /* 0x4 */ s32 field_4;
} Actor401000Msg;

/// Height-clamp row `func_actor_401000_801352DC` scans: `field_0` / `field_2`
/// are matched against `GameSessionFrom4::field_3` / `field_2`, and when a row
/// matches the coordinate's Y is clamped to [`lo`, `hi`] and its `flg` is
/// cleared so the local matrix is rebuilt. Two rows
/// (`(1, 3, -0x12C, 0)`, `(5, 0x1D, 0, 0x12C)`). Same shape as
/// `Actor401300HeightClamp` / `Actor01900HeightClamp`.
typedef struct Actor401000HeightClamp {
    /* 0x0 */ s16  field_0;
    /* 0x2 */ s16  field_2;
    /* 0x4 */ s16  lo;
    /* 0x6 */ s16  hi;
    /* 0x8 */ byte pad_8[8];
} Actor401000HeightClamp;
STATIC_ASSERT_SIZEOF(Actor401000HeightClamp, 0x10);

/// 0x20-byte scratch from `G_SCRATCH_HEAD` used by `func_actor_401000_80135374`.
/// Same shape as `Actor401300Delta` / `Actor01900Delta`: the `GpDeltaScratch`
/// filled by `func_800E0C10`, its integer `step` (Y clamped to ±0x12C while a
/// height-clamp row matches, XZ normalised through the GTE past 0x96), the XZ
/// length `len`, and `moved`, the return value.
typedef struct Actor401000Delta {
    /* 0x00 */ GpDeltaScratch delta;
    /* 0x10 */ SVECTOR        step;
    /* 0x18 */ s32            len;
    /* 0x1C */ s32            moved;
} Actor401000Delta;
STATIC_ASSERT_SIZEOF(Actor401000Delta, 0x20);

/// 0x10-byte `G_SCRATCH_HEAD` block `func_actor_401000_80134F98` carves off
/// for the offset from the actor to `Wip_SysConfig.field_4`, the wrapped turn
/// toward it and the facing yaw. Same shape as `Actor01900AimScratch` /
/// `Actor401300AimScratch`.
typedef struct Actor401000AimScratch {
    /* 0x0 */ SVECTOR delta;
    /* 0x8 */ s16     pad_8;
    /* 0xA */ s16     pad_A;
    /* 0xC */ s16     angle;
    /* 0xE */ s16     pad_E;
} Actor401000AimScratch;
STATIC_ASSERT_SIZEOF(Actor401000AimScratch, 0x10);

/// 0x10-byte `G_SCRATCH_HEAD` block `func_actor_401000_80136E20` carves off
/// for the offset from the actor to `Wip_SysConfig.field_4`, the wrapped turn
/// toward it and the facing yaw. Same shape as `Actor401000AimScratch`, but
/// with the turn and the yaw as two separate halfwords.
typedef struct Actor401000ChaseScratch {
    /* 0x0 */ SVECTOR delta;
    /* 0x8 */ s16     pad_8;
    /* 0xA */ s16     pad_A;
    /* 0xC */ s16     turn;
    /* 0xE */ s16     angle;
} Actor401000ChaseScratch;
STATIC_ASSERT_SIZEOF(Actor401000ChaseScratch, 0x10);

/// 0xC-byte `G_SCRATCH_HEAD` block `func_actor_401000_80139D10` carves off for
/// the offset from the actor to `Wip_SysConfig.field_4` and the wrapped turn
/// toward it. Same shape as `Actor401300TurnScratch` / `Actor01900TurnScratch`.
typedef struct Actor401000TurnScratch {
    /* 0x0 */ SVECTOR delta;
    /* 0x8 */ s16     angle;
    /* 0xA */ s16     pad_A;
} Actor401000TurnScratch;
STATIC_ASSERT_SIZEOF(Actor401000TurnScratch, 0xC);

/// 0x34-byte `G_SCRATCH_HEAD` block `func_actor_401000_80134F98` carves off to
/// rebuild the root coordinate: a Y rotation, the uniform scale `ScaleMatrix`
/// takes, and the angle it was built from. Same shape as
/// `Actor401300RotScratch`.
typedef struct Actor401000RotScratch {
    /* 0x00 */ MATRIX m;
    /* 0x20 */ VECTOR scale;
    /* 0x30 */ s16    angle;
    /* 0x32 */ s16    pad_32;
} Actor401000RotScratch;
STATIC_ASSERT_SIZEOF(Actor401000RotScratch, 0x34);

/// 0xC-byte scratch `func_actor_401000_8013922C` carves off `G_SCRATCH_HEAD`
/// to hold the actor's XZ offset from `D_80073B8C` and the radius it is
/// measured against, each squared before the comparison. Same shape as
/// `Actor01900RangeScratch` / `Actor401300RangeScratch`.
typedef struct Actor401000RangeScratch {
    /* 0x0 */ s32 dx;
    /* 0x4 */ s32 dz;
    /* 0x8 */ s32 r;
} Actor401000RangeScratch;
STATIC_ASSERT_SIZEOF(Actor401000RangeScratch, 0xC);

extern MATRIX* D_80073B8C;

/// Animation bank `func_actor_401000_80133274` hands to both `func_800B3F84`
/// calls; the same `s32` the 401300 sibling keeps in `D_actor_401300_80158838`.
extern s32 D_actor_401000_80154E48;

/// Parameter pair `func_actor_401000_80133274` installs as `GpEnemy::field_50`
/// and reads `field_4` out of as the actor's initial `field_40`.
extern GpPairSrcE D_actor_401000_8013E09C;

/// Three combat-parameter records `func_actor_401000_80133274` picks between
/// with `Task::spawnArg1 & 0xF`.
extern Actor401000SeedRec D_actor_401000_8013E0AC[3];

/// Animation table `func_actor_401000_80133274` writes to `Task::field_24`.
extern s32 D_actor_401000_80154F90;

/// Linear congruential generator state `func_actor_401000_8013DF6C` advances
/// with the same `(x * 5 + 0x71357911) >> 16` draw the 401300 sibling uses.
extern u32 Gp_LcgState;

/// Overlay-data word `func_actor_401000_8013922C` points
/// `D_actor_401000_80154E88` at on entering its state.
extern s32  D_actor_401000_80154634;
extern s32* D_actor_401000_80154E88;

/// Message 0x3FF payload of `func_actor_401000_801383F0` and
/// `func_actor_401000_801385B0`: the animation argument the player task reads
/// when the actor's live-actor flag goes up.
extern GpAnimArg D_actor_401000_80154F1C;

/// Twelve `SVECTOR` hit positions `func_actor_401000_80133940` picks from by
/// damage magnitude. The fourth halfword (`pad`, unused by the effect) is the
/// model part index the spawned effect anchors to. Same table as the
/// `Actor00100_D1B9F4` one `Actor00100_Fn03340` reads.
extern SVECTOR D_actor_401000_80154F30[12];

/// The two `Actor401000HeightClamp` rows `func_actor_401000_801352DC` and
/// `func_actor_401000_80135374` walk.
extern Actor401000HeightClamp D_actor_401000_80154FD0[];

/// Message 0x3E9 payload of `func_actor_401000_801380B8`: the player task's
/// world position, then the yaw from the actor to it, handed straight to the
/// slot-3 handler. The 401000 twin of the block `func_actor_401300_80138800`
/// keeps inline at `Actor401300Work.field_CD4` / `.field_CE4`.
extern GpMsg3EE D_actor_401000_80155018;

/// Gameplay slot `Gp_SpawnEff` effects read their model data from; set before
/// each spawn in `func_actor_401000_8013B1E4`.
extern void* D_80114B78[1];

/// Overlay effect model data `func_actor_401000_8013B1E4` points
/// `D_80114B78` at before spawning, one per animation-latch key frame
/// (`field_6` 3, 5, 7, 8).
extern char D_actor_401000_80143EB4;
extern char D_actor_401000_80144830;
extern char D_actor_401000_8014599C;
extern char D_actor_401000_80146190;

/// 0 = movement running, 1 = frozen; the same flag byte `Actor01900_StepForward`
/// and `Actor00100_MoveForward` test.
extern u8 D_80072729;

void func_actor_401000_80132EF0(Actor401000* arg0);

/// Range probe `func_actor_401000_801385B0` runs against the actor root: the
/// same helper as `func_actor_401300_8013267C`, with the step amount in the
/// third argument instead of the second.
s32 func_actor_401000_80132590(GsCOORDINATE2* coord, s16 arg1, s16 arg2);

/// Returns the actor's current animation/clip kind, matched against
/// `GpEnemy.node.field_5` by `func_actor_401000_801385B0`.
s32 func_actor_401000_80132824(Actor401000* arg0);

/// Walk a `GpRec18` table and push `coord` back out of the obstacles it
/// overlaps, returning the record's `field_10`. The same helper as
/// `func_actor_401300_801323B0`, whose second argument is the sibling's
/// `field_990` run.
s32 func_actor_401000_801323EC(GsCOORDINATE2* coord, GpRec18* rec, s32 arg2);

s32 func_actor_401000_8013D694(Actor401000* arg0, s32 arg1, Actor401000Msg* arg2);

/// Message 0x301 / 0x1002 handler: copy the payload's three leading bytes onto
/// the work block's animation slots, then key the actor's state (`field_0`) off
/// the message id and sub-id. The 0x1002/2 arm also drops the model root to
/// `(-0x595, 0, -0x5B1)` and rebuilds its yaw. Same body as
/// `Actor01900_Fn0A5A4`, minus that one's 0x301/0 arm.
s32 func_actor_401000_8013D958(Actor401000* arg0, s32 arg1, u16* arg2);

/// `Task::exitCallback` teardown: kill the two helper tasks, unlink the three
/// display nodes, drop the enemy's `field_54` slot, then `Gp_DestroyEnemy`.
void func_actor_401000_8013DA78(Task* task);
void func_actor_401000_8013DB10(Actor401000* arg0);
void func_actor_401000_8013DB6C(Actor401000* arg0);
void func_actor_401000_8013DC14(Actor401000* arg0);
void func_actor_401000_8013DCC0(Actor401000* arg0);
void func_actor_401000_8013DD6C(Actor401000* arg0);
void func_actor_401000_8013DEC8(Actor401000* arg0);

/// Per-frame animation tick: while the live-actor flag is up, reload the
/// `field_6` countdown from `field_C10` plus a 0-15 draw from `Gp_LcgState`.
/// On underflow the `field_89E` state picks the next clip — 0xB/0x17 select
/// 0xF and 0xC/0x18/0x19 select 0x10, every other state leaving the actor as
/// it was — and a spent enemy HP forces 0x15 whatever came out. The same body
/// as `func_actor_401300_80141DF4`, which reads the state from `field_8A2`
/// with case groups 0xB/0x17 and 0xC/0x18/0x22.
void func_actor_401000_8013DF6C(Actor401000* arg0);

#endif // ACTOR_401000_H
