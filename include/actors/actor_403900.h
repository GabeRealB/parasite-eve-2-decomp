#ifndef ACTOR_403900_H
#define ACTOR_403900_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3FB8.h"

/// The actor's per-part attach coordinates, 0x50 apart, hanging off the
/// display object's 0x08 slot (`TmdObject::field_8`, the trailing per-part
/// `GsCOORDINATE2` array). This overlay's code reaches the root and the fourth
/// part: `func_actor_403900_80137444` parks `&field_F0` in the work block's
/// 0x65C slot, and the sound helpers take the root as the `GpObj38*` they
/// project.
typedef struct Actor403900Coord {
    /* 0x000 */ GsCOORDINATE2 field_0;
    /* 0x050 */ byte          pad_50[0xA0];
    /* 0x0F0 */ GsCOORDINATE2 field_F0;
    /* 0x140 */ GsCOORDINATE2 field_140;
} Actor403900Coord;

/// One 0x10-byte entry of the box table `Actor403900Work::field_6B4`, the same
/// entry the shared scan `ActorsShared80132d78` walks as its own
/// `ActorShared80132d78Region`.
typedef struct Actor403900Region {
    /* 0x0 */ s16 field_0;
    /* 0x2 */ s16 field_2;
    /* 0x4 */ s16 field_4;
    /* 0x6 */ s16 field_6;
    /* 0x8 */ s16 field_8;
    /* 0xA */ s16 field_A;
    /* 0xC */ s16 field_C;
    /* 0xE */ s16 field_E;
} Actor403900Region;
STATIC_ASSERT_SIZEOF(Actor403900Region, 0x10);

/// One 8-byte entry of the spawn's placement run `D_actor_403900_80153C7C`,
/// terminated by a zero `field_0`: when the session's stage (`field_2`) and
/// room (`field_4`) match, `field_0` indexes the box tables and `field_6` is
/// the box count stored to `Actor403900Work::field_6FA`.
typedef struct Actor403900Spot {
    /* 0x0 */ s16 field_0;
    /* 0x2 */ s16 field_2;
    /* 0x4 */ s16 field_4;
    /* 0x6 */ u16 field_6;
} Actor403900Spot;
STATIC_ASSERT_SIZEOF(Actor403900Spot, 0x8);

/// Per-instance work block the actor's task holds in the 0x1C slot of
/// `Actor403900`, allocated with `Mem_Calloc(0x71C)` by the spawn handler
/// `func_actor_403900_80137444`; the same shape the other enemy overlays give
/// their work blocks.
///
/// `field_6C0` is the animation id the cue body's state 0 arms, `field_6CE`
/// is the cue state itself, and `field_6D4` is the countdown state 1 ticks
/// down: it is rolled from the `Gp_LcgState` LCG when the animation is
/// reseeded, and the frame it runs out queues the actor's cue.
///
/// `field_6C6` is the flinch countdown the hit handler arms when a hit lands.
/// While it is non-zero a hit is already being flinched, so the cue body
/// raises the hit descriptor `field_494` and the pending flags `field_49A`
/// only when it is zero - the same pair the hurt states store directly.
typedef struct Actor403900Work {
    /* 0x000 */ byte       pad_0[0x14];
    /* 0x014 */ GpAnimSlot field_14;
    /* 0x03C */ byte       pad_3C[0x2D0];
    /* 0x30C */ byte       field_30C[0x130];
    /* 0x43C */ MATRIX     field_43C;
    /* 0x45C */ MATRIX     field_45C;
    /* 0x47C */ byte       field_47C[8];
    /* 0x484 */ void*      field_484;
    /* 0x488 */ GpRec18*   field_488;
    /* 0x48C */ s16        field_48C;
    /* 0x48E */ s16        field_48E;
    /* 0x490 */ s16        field_490;
    /* 0x492 */ byte       pad_492[2];
    /// Hit descriptor: the damage amount `field_716` with the tag bits
    /// 0x30000 OR'd in.
    /* 0x494 */ s32 field_494;
    /* 0x498 */ s16 field_498;
    /// Hit-pending flags, raised together with `field_494`; bit 0x8000 is the
    /// flag the hit handler clears when it consumes the descriptor.
    /* 0x49A */ u16      field_49A;
    /* 0x49C */ GpRec18  field_49C[3];
    /* 0x4E4 */ byte     field_4E4[8];
    /* 0x4EC */ void*    field_4EC;
    /* 0x4F0 */ GpRec18* field_4F0;
    /* 0x4F4 */ s16      field_4F4;
    /* 0x4F6 */ s16      field_4F6;
    /* 0x4F8 */ s16      field_4F8;
    /* 0x4FA */ byte     pad_4FA[2];
    /* 0x4FC */ s32      field_4FC;
    /* 0x500 */ s16      field_500;
    /* 0x502 */ u16      field_502;
    /* 0x504 */ GpRec18  field_504[4];
    /* 0x564 */ byte     field_564[8];
    /* 0x56C */ void*    field_56C;
    /* 0x570 */ GpRec18* field_570;
    /* 0x574 */ s16      field_574;
    /* 0x576 */ s16      field_576;
    /* 0x578 */ s16      field_578;
    /* 0x57A */ byte     pad_57A[2];
    /* 0x57C */ s32      field_57C;
    /* 0x580 */ s16      field_580;
    /* 0x582 */ u16      field_582;
    /* 0x584 */ GpRec18  field_584;
    /* 0x59C */ byte     field_59C[8];
    /* 0x5A4 */ void*    field_5A4;
    /* 0x5A8 */ void*    field_5A8;
    /* 0x5AC */ s16      field_5AC;
    /* 0x5AE */ s16      field_5AE;
    /* 0x5B0 */ s16      field_5B0;
    /* 0x5B2 */ byte     pad_5B2[2];
    /* 0x5B4 */ s32      field_5B4;
    /* 0x5B8 */ s16      field_5B8;
    /// Flag word the settle states 3 and 4 clear bit 0x4000 of, beside the
    /// matching word at 0x5DA.
    /* 0x5BA */ u16      field_5BA;
    /* 0x5BC */ byte     field_5BC[8];
    /* 0x5C4 */ void*    field_5C4;
    /* 0x5C8 */ GpRec18* field_5C8;
    /* 0x5CC */ s16      field_5CC;
    /* 0x5CE */ s16      field_5CE;
    /* 0x5D0 */ s16      field_5D0;
    /* 0x5D2 */ byte     pad_5D2[2];
    /* 0x5D4 */ s32      field_5D4;
    /* 0x5D8 */ s16      field_5D8;
    /// Second flag word, cleared by state 3 only.
    /* 0x5DA */ u16      field_5DA;
    /* 0x5DC */ s16      field_5DC;
    /* 0x5DE */ s16      field_5DE;
    /* 0x5E0 */ s16      field_5E0;
    /* 0x5E2 */ byte     pad_5E2[2];
    /* 0x5E4 */ s16      field_5E4;
    /* 0x5E6 */ s16      field_5E6;
    /* 0x5E8 */ s16      field_5E8;
    /* 0x5EA */ byte     pad_5EA[2];
    /* 0x5EC */ s16      field_5EC;
    /* 0x5EE */ s16      field_5EE;
    /* 0x5F0 */ GpRec18* field_5F0;
    /// Head of the actor's first collision-record table; the settle states
    /// branch on its `field_4` before handing the table to
    /// `Gp_ClearRec18Occupied`.
    /* 0x5F4 */ GpRec18  field_5F4;
    /* 0x60C */ byte     field_60C[8];
    /* 0x614 */ void*    field_614;
    /* 0x618 */ void*    field_618;
    /* 0x61C */ s16      field_61C;
    /* 0x61E */ s16      field_61E;
    /* 0x620 */ s16      field_620;
    /* 0x622 */ byte     pad_622[2];
    /* 0x624 */ s32      field_624;
    /* 0x628 */ s16      field_628;
    /* 0x62A */ u16      field_62A;
    /* 0x62C */ s16      field_62C;
    /* 0x62E */ s16      field_62E;
    /* 0x630 */ s16      field_630;
    /* 0x632 */ byte     pad_632[2];
    /* 0x634 */ s16      field_634;
    /* 0x636 */ s16      field_636;
    /* 0x638 */ s16      field_638;
    /* 0x63A */ byte     pad_63A[2];
    /* 0x63C */ s16      field_63C;
    /* 0x63E */ s16      field_63E;
    /* 0x640 */ GpRec18* field_640;
    /// Head of a second collision-record table, cleared by state 5.
    /* 0x644 */ GpRec18 field_644;
    /// `func_800B3F84` argument record for the hit spark: the fourth part's
    /// coordinate, 0x500, 2.
    /* 0x65C */ GpEffArg field_65C;
    /// The root coordinate's position the push-back restores when a record
    /// table reports a full block (`func_800E0C10` case 2).
    /* 0x664 */ s32  field_664;
    /* 0x668 */ s32  field_668;
    /* 0x66C */ s32  field_66C;
    /* 0x670 */ byte pad_670[0x34];
    /// World position the grab places the player at, written by
    /// `func_actor_403900_80132E34` and read back by the 0x3E9 message.
    /* 0x6A4 */ s32  field_6A4;
    /* 0x6A8 */ s32  field_6A8;
    /* 0x6AC */ s32  field_6AC;
    /* 0x6B0 */ byte pad_6B0[4];
    /// Box table the shared scan `ActorsShared80132d78` walks, `field_6FA`
    /// entries of 0x10 bytes each.
    /* 0x6B4 */ Actor403900Region* field_6B4;
    /* 0x6B8 */ s32                field_6B8;
    /// Sound event id the cue body queues: the overlay's cue word
    /// `D_actor_403900_8013846C` with the `GpEnemy` work id's high nibble in
    /// bits 8-11. Stored back to the block and re-read from there as the first
    /// argument of `SndEvt_EnqueueType6`.
    /* 0x6BC */ s32 field_6BC;
    /// Animation id the cue reseeds at.
    /* 0x6C0 */ s16 field_6C0;
    /// Animation id the slots were last reseeded with, so the reseed runs once
    /// per change rather than every frame.
    /* 0x6C2 */ s16 field_6C2;
    /* 0x6C4 */ s16 field_6C4;
    /// Flinch countdown, armed by the hit handler and ticked down a frame at a
    /// time; see the block comment.
    /* 0x6C6 */ s16 field_6C6;
    /// Cleared on the frame the wait state rolls a new countdown.
    /* 0x6C8 */ s16 field_6C8;
    /* 0x6CA */ s16 field_6CA;
    /// Cleared when the cue expires.
    /* 0x6CC */ s16 field_6CC;
    /// Cue state: 0 arms the animation and the countdown, 1 waits the
    /// countdown out and fires the cue, then drops back to 0.
    /* 0x6CE */ s16 field_6CE;
    /* 0x6D0 */ s16 field_6D0;
    /* 0x6D2 */ s16 field_6D2;
    /// Countdown the reseed rolls from the LCG (0x2D..0x4C) and state 1 ticks
    /// down; read back as an unsigned halfword and sign-extended through a
    /// 16-bit shift, so it runs out on the frame it passes 0xFFFF.
    /* 0x6D4 */ u16 field_6D4;
    /* 0x6D6 */ s16 field_6D6;
    /* 0x6D8 */ s16 field_6D8;
    /// Timer pair the reseed arms alongside `field_6DE`.
    /* 0x6DA */ s16 field_6DA;
    /* 0x6DC */ s16 field_6DC;
    /// Third timer the reseed arms; written last of the three.
    /* 0x6DE */ s16 field_6DE;
    /// Fourth timer, cleared alongside the trio above when the countdown runs
    /// out.
    /* 0x6E0 */ s16 field_6E0;
    /// Ground-shadow shade the shared ground-quad body hands to
    /// `Gp_DrawEffGroundQuad`, which draws nothing for a negative value: the
    /// spawn turns the calloc'd zero into -1 for the full variant, so an actor
    /// that never raises the shade casts no shadow.
    /* 0x6E2 */ s16 field_6E2;
    /// Latch that sends the wait state straight to state 2 with no countdown;
    /// cleared again on the frame it is taken.
    /* 0x6E4 */ s16 field_6E4;
    /* 0x6E6 */ s16 field_6E6;
    /// Latch that sends the wait state to an LCG-drawn state 3-5 offset and
    /// reposts the actor's target instead of rolling a countdown.
    /* 0x6E8 */ s16 field_6E8;
    /* 0x6EA */ s16 field_6EA;
    /// Sequence mode, cleared by the wait state.
    /* 0x6EC */ s16 field_6EC;
    /// Set by the wait state when it takes the `field_6E8` branch, cleared
    /// otherwise.
    /* 0x6EE */ s16 field_6EE;
    /* 0x6F0 */ s16 field_6F0;
    /* 0x6F2 */ s16 field_6F2;
    /* 0x6F4 */ s16 field_6F4;
    /* 0x6F6 */ s16 field_6F6;
    /* 0x6F8 */ s16 field_6F8;
    /// Entry count of the box table at `field_6B4`, read as a signed halfword;
    /// a non-positive count disarms the scan.
    /* 0x6FA */ s16 field_6FA;
    /* 0x6FC */ s16 field_6FC[2];
    /* 0x700 */ s16 field_700[2];
    /* 0x704 */ s16 field_704[2];
    /* 0x708 */ s16 field_708;
    /* 0x70A */ s16 field_70A;
    /// Escalation counter the settle states walk up to 8; the wait state's
    /// countdown is scaled by `16 - field_70C`, so a higher count is a
    /// shorter wait.
    /* 0x70C */ s16 field_70C;
    /// Which settle state last ran (1, 2 or 3); state 2 picks the next state
    /// from it.
    /* 0x70E */ s16 field_70E;
    /// Run length of repeats of the same settle state; state 2 biases its LCG
    /// draw by it and the settle states walk it back down.
    /* 0x710 */ s16 field_710;
    /// Running entry index into the overlay's cue-bank table; the spawn seeds
    /// it from the per-stage array `D_actor_403900_8015409C` and a zero
    /// disarms the cue request.
    /* 0x712 */ s16 field_712;
    /* 0x714 */ s16 field_714;
    /// Damage amount the hit handlers OR into `field_494`.
    /* 0x716 */ s16 field_716;
    /* 0x718 */ s16 field_718;
    /* 0x71A */ s16 field_71A;
} Actor403900Work;
STATIC_ASSERT_SIZEOF(Actor403900Work, 0x71C);

/// Display object hung off `Actor403900::field_2C`; `field_8` is the actor's
/// per-part attach coordinate array, which the sound helpers take as the
/// `GpObj38*` light object they project.
typedef struct Actor403900Obj2C {
    /* 0x00 */ byte              pad_0[8];
    /* 0x08 */ Actor403900Coord* field_8;
    /* 0x0C */ s16               field_C;
    /* 0x0E */ byte              pad_E[0xE];
    /* 0x1C */ MATRIX*           field_1C;
    /* 0x20 */ MATRIX*           field_20;
    /* 0x24 */ byte              pad_24[8];
    /* 0x2C */ s32               field_2C;
} Actor403900Obj2C;

/// 0x30-byte block `func_actor_403900_80131F54` takes from `G_SCRATCH_HEAD`:
/// `delta` receives the `func_800E0C10` push-back and is then reused for the
/// offset to the player, and `ofs` is the spark offset handed to
/// `func_800FDB18`.
typedef struct Actor403900HitScratch {
    /* 0x00 */ GpDeltaScratch delta;
    /* 0x10 */ byte           pad_10[0x10];
    /* 0x20 */ SVECTOR        ofs;
    /* 0x28 */ byte           pad_28[8];
} Actor403900HitScratch;
STATIC_ASSERT_SIZEOF(Actor403900HitScratch, 0x30);

/// 0x18-byte block `func_actor_403900_80134194` takes from `G_SCRATCH_HEAD`
/// to place the actor relative to the box it is working off: `in` is the box
/// heading rotated into the attach coordinate, and `out` holds the offset to
/// the player the range and aim checks then measure.
typedef struct Actor403900OffsetScratch {
    /* 0x00 */ VECTOR  out;
    /* 0x10 */ SVECTOR in;
} Actor403900OffsetScratch;
STATIC_ASSERT_SIZEOF(Actor403900OffsetScratch, 0x18);

/// 0x48-byte block `func_actor_403900_80135D5C` takes from `G_SCRATCH_HEAD`
/// to aim the actor: `m` is the root's world matrix brought local to the
/// fourth part, `out` the GTE's rotated offset, and `pts` the two world points
/// (root-based aim point, fourth-part offset) projected through `GsWSMATRIX`
/// into `sxy` and the quartered screen z `otz`.
typedef struct Actor403900AimScratch {
    /* 0x00 */ MATRIX  m;
    /* 0x20 */ VECTOR  out;
    /* 0x30 */ SVECTOR pts[2];
    /* 0x40 */ s32     sxy;
    /* 0x44 */ s32     otz;
} Actor403900AimScratch;
STATIC_ASSERT_SIZEOF(Actor403900AimScratch, 0x48);

/// Actor context handed to this overlay's callbacks: `field_1C` is the work
/// block above, `field_20` the `GpEnemy` the spawner left in the task's
/// `Task::spawnArg2` slot, and `field_2C` the display object. Same shape as
/// the other actor overlays' contexts.
typedef struct Actor403900 {
    /* 0x00 */ byte              pad_0[0x1C];
    /* 0x1C */ Actor403900Work*  field_1C;
    /* 0x20 */ GpEnemy*          field_20;
    /* 0x24 */ void*             field_24;
    /* 0x28 */ byte              pad_28[4];
    /* 0x2C */ Actor403900Obj2C* field_2C;
    /// Sequence state the overlay's body parks for the frame dispatcher, the
    /// same slot the other actor contexts keep at 0x30.
    /* 0x30 */ s32 field_30;
} Actor403900;

/// The game's shared 32-bit LCG state: every draw is
/// `Gp_LcgState = Gp_LcgState * 5 + 0x71357911`, read back from the global,
/// with the caller taking the bits it wants out of the high half.
extern u32 Gp_LcgState;

/// One 4-byte entry of `D_actor_403900_801383DC`: the first entry whose
/// `frame` is not below the animation frame `Actor403900Work::field_6C4`
/// supplies `value` for `field_6C8`.
typedef struct Actor403900FrameStep {
    /* 0x0 */ s16 frame;
    /* 0x2 */ u16 value;
} Actor403900FrameStep;
STATIC_ASSERT_SIZEOF(Actor403900FrameStep, 4);

extern Actor403900FrameStep D_actor_403900_801383DC[];

/// Payload of the 0x3F8 query `func_actor_403900_8013314C` sends the player
/// before it grabs; `field_14` is the range it asks for.
typedef struct Actor403900Msg3F8 {
    /* 0x00 */ byte pad_0[0x14];
    /* 0x14 */ s32  field_14;
} Actor403900Msg3F8;
STATIC_ASSERT_SIZEOF(Actor403900Msg3F8, 0x18);

/// Payload of message 0x3E9: the world position and rotation the player is
/// placed at.
typedef struct Actor403900Msg3E9 {
    /* 0x00 */ VECTOR  pos;
    /* 0x10 */ SVECTOR rot;
} Actor403900Msg3E9;
STATIC_ASSERT_SIZEOF(Actor403900Msg3E9, 0x18);

/// 0x5C-byte block `func_actor_403900_8013314C` takes from `G_SCRATCH_HEAD`:
/// the 0x3F8 query, the `GpAnimArg` sent as message 0x3FF, the 0x3E9
/// placement, and the offset `in` rotated through the actor's root into `out`
/// (`in` is also the rotation handed to `RotMatrix` and `func_800FDB18`).
typedef struct Actor403900GrabScratch {
    /* 0x00 */ Actor403900Msg3F8 query;
    /* 0x18 */ GpAnimArg         anim;
    /* 0x2C */ Actor403900Msg3E9 place;
    /* 0x44 */ VECTOR            out;
    /* 0x54 */ SVECTOR           in;
} Actor403900GrabScratch;
STATIC_ASSERT_SIZEOF(Actor403900GrabScratch, 0x5C);

/// Runs the one-shot vocal cue armed by `field_718`; see its definition.
void func_actor_403900_801380DC(Actor403900* arg0);

/// Parks the actor's target position off the player; see its definition.
void func_actor_403900_80132E34(Actor403900* arg0);

/// Aims the actor at the player; see its definition.
void func_actor_403900_80135D5C(Actor403900* arg0);

/// Reacts to the damage just taken; see its definition.
void func_actor_403900_801324E8(Actor403900* arg0, s32 arg1);

#endif /* ACTOR_403900_H */
