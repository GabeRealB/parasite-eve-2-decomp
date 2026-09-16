#ifndef ACTOR_402200_H
#define ACTOR_402200_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/1BC.h"
#include "gameplay/3FB8.h"

/// The actor's per-part attach coordinates, 0x50 apart, hanging off the
/// display object's 0x08 slot (`TmdObject::field_8`, the trailing per-part
/// `GsCOORDINATE2` array). This overlay's code reaches the root and the fourth
/// part: `func_actor_402200_80137444` parks `&field_F0` in the work block's
/// 0x65C slot, and the shared ground-quad body `ActorsShared8013806c` takes the
/// shadow quad's horizontal position from `field_F0` and its height from
/// `field_0`.
typedef struct Actor402200Coord {
    /* 0x000 */ GsCOORDINATE2 field_0;
    /* 0x050 */ byte          pad_50[0xA0];
    /* 0x0F0 */ GsCOORDINATE2 field_F0;
} Actor402200Coord;

/// Display object hung off `Actor402200::field_2C`; `field_8` is the per-part
/// coordinate array above.
typedef struct Actor402200Obj2C {
    /* 0x00 */ byte              pad_0[8];
    /* 0x08 */ Actor402200Coord* field_8;
    /* 0x0C */ s16               field_C;
    /* 0x0E */ byte              pad_E[0xE];
    /* 0x1C */ MATRIX*           field_1C;
    /* 0x20 */ MATRIX*           field_20;
    /* 0x24 */ byte              pad_24[8];
    /* 0x2C */ s32               field_2C;
} Actor402200Obj2C;

/// One 0x10-byte entry of the box table `Actor402200Work::field_6B4`, the same
/// entry the shared scan `ActorsShared80132d78` walks as its own
/// `ActorShared80132d78Region`.
typedef struct Actor402200Region {
    /* 0x0 */ s16 field_0;
    /* 0x2 */ s16 field_2;
    /* 0x4 */ s16 field_4;
    /* 0x6 */ s16 field_6;
    /* 0x8 */ s16 field_8;
    /* 0xA */ s16 field_A;
    /* 0xC */ s16 field_C;
    /* 0xE */ s16 field_E;
} Actor402200Region;
STATIC_ASSERT_SIZEOF(Actor402200Region, 0x10);

/// One 8-byte entry of the spawn's placement run `D_actor_402200_80153C78`,
/// terminated by a zero `field_0`: when the session's stage (`field_2`) and
/// room (`field_4`) match, `field_0` indexes the box tables and `field_6` is
/// the box count stored to `Actor402200Work::field_6FA`.
typedef struct Actor402200Spot {
    /* 0x0 */ s16 field_0;
    /* 0x2 */ s16 field_2;
    /* 0x4 */ s16 field_4;
    /* 0x6 */ u16 field_6;
} Actor402200Spot;
STATIC_ASSERT_SIZEOF(Actor402200Spot, 0x8);

/// Per-instance work block the overlay's setup `func_actor_402200_80137444`
/// allocates with `Mem_Calloc(0x71C)` and parks in the 0x1C slot below (the
/// task's `Task::idMap`, which is not a `TaskIdMap` here).
///
/// `field_6E2` is the ground-shadow shade the shared ground-quad body
/// `ActorsShared8013806c` hands to `Gp_DrawEffGroundQuad`, which draws nothing
/// for a negative value: the frame code turns the calloc'd zero into -1 the
/// first time it runs, so an actor that never raises the shade casts no shadow.
///
/// `field_6F4` is the actor's phase, written and read as a signed halfword:
/// the frame handler clears it on entry, `ActorsShared801381e0` raises it to 1
/// while the remaining-enemy count is positive, and the handler branches on
/// 0 / 1 thereafter.
///
/// `field_6EA` is a pending tint request: `func_actor_402200_80137FB0` reads it
/// once a frame and, while it is 1 or 2, hands the display object the matching
/// translate (0, 0, 0x400 or full 0xFFF) and clears it, so each request is
/// consumed on the frame it is raised.
///
/// `field_718` arms a one-shot vocal cue and `field_71A` is its frame counter.
/// While the flag is clear the body does nothing; once it is set the counter
/// runs up, plays the actor's cue at 0x14, and at 0x5F asks the scene for
/// message 0x3ED - clearing the flag and sending 0x3F1 instead if the scene
/// refuses it.
/// `field_3C` is the animation slot the cue body `func_actor_402200_80135BE0`
/// hands to `Gp_AnimGetRec`: the second of the 0x28-byte slots the actor work
/// blocks lay out from 0x14, the same one the other actor overlays' cue bodies
/// play from. `field_6CA` latches the record's two cue bits (`0x30`) for the
/// next frame, and `field_712` is the running entry index into the overlay's
/// cue-id table `D_actor_402200_80138420` - zero disarms the body, and while it
/// is set the two adjacent words `[field_712 * 2 - 1]` and `[field_712 * 2]`
/// are the cue ids it plays.
typedef struct Actor402200Work {
    /* 0x000 */ byte       pad_0[0x14];
    /* 0x014 */ GpAnimSlot field_14;
    /* 0x03C */ GpAnimSlot field_3C;
    /* 0x064 */ byte       pad_64[0x2A8];
    /* 0x30C */ byte       field_30C[0x130];
    /* 0x43C */ MATRIX     field_43C;
    /* 0x45C */ MATRIX     field_45C;
    /* 0x47C */ byte       field_47C[8];
    /* 0x484 */ void*      field_484;
    /* 0x488 */ GpRec18*   field_488;
    /* 0x48C */ s16        field_48C;
    /* 0x48E */ s16        field_48E;
    /// Halfword the attack sequences park alongside the timers: state 0 stores
    /// -0xA7 when `field_6D2` is clear and 0x109 when it is set. The branch
    /// sequence `func_actor_402200_80135630` stores the same pair, so it is the
    /// same slot set's vertical placement.
    /* 0x490 */ s16  field_490;
    /* 0x492 */ byte pad_492[2];
    /// Hit descriptor the flinch handler `func_actor_402200_80131F54` and the
    /// hurt states `func_actor_402200_80133AEC` / `func_actor_402200_80134194`
    /// store on the frame a hit lands: the damage amount `field_716` with the
    /// tag bits 0x30000 OR'd in.
    /* 0x494 */ s32 field_494;
    /// Halfword the attack sequences arm to 0x15E next to `field_490`.
    /* 0x498 */ s16 field_498;
    /// Hit-pending flags, raised together with `field_494`: bit 0x8000 is the
    /// flag the hit handler clears when it consumes the descriptor. The shared
    /// body `ActorsShared80137a20` tests the same bit through its own work
    /// block, and `func_actor_402200_80131F54` clears `field_6C6` as it raises
    /// it.
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
    /// Flag word the attack sequences raise: bit 0x4000 is set by state 0 of
    /// both `func_actor_402200_80135630` and `func_actor_402200_80135A24`,
    /// alongside clearing bit 0x4000 of the 0x502 word below.
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
    /// Head of the actor's first `GpRec18` table; `func_actor_402200_801329A4`
    /// branches on its `field_4` before clearing it.
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
    /// Head of a second `GpRec18` table, cleared by state 5 of
    /// `func_actor_402200_801329A4`.
    /* 0x644 */ GpRec18 field_644;
    /// `func_800FDB18` argument record for the hit spark: the fourth part's
    /// coordinate, 0x500, 2.
    /* 0x65C */ GpEffArg field_65C;
    /* 0x664 */ s32      field_664;
    /* 0x668 */ s32      field_668;
    /* 0x66C */ s32      field_66C;
    /* 0x670 */ byte     pad_670[0x34];
    /* 0x6A4 */ s32      field_6A4;
    /* 0x6A8 */ s32      field_6A8;
    /* 0x6AC */ s32      field_6AC;
    /* 0x6B0 */ byte     pad_6B0[4];
    /// Box table the shared scan `ActorsShared80132d78` walks, `field_6FA`
    /// entries of 0x10 bytes each.
    /* 0x6B4 */ Actor402200Region* field_6B4;
    /* 0x6B8 */ s32                field_6B8;
    /// Sound event id the sequence body `func_actor_402200_8013539C` queues: the
    /// overlay's cue word `D_actor_402200_80138468` with the `GpEnemy` work id's
    /// high nibble in bits 8-11, the same construction the cue body
    /// `func_actor_402200_80135BE0` uses on `D_actor_402200_80138420`. Stored
    /// back to the block and re-read from there as the first argument of
    /// `SndEvt_EnqueueType6`.
    /* 0x6BC */ s32 field_6BC;
    /// Animation id the frame code reseeds slots 1..0x12 with; the reseed body
    /// `func_actor_402200_80137EEC` also indexes the blend table
    /// `D_actor_402200_801383AC` with it.
    /* 0x6C0 */ s16 field_6C0;
    /// Animation id the slots were last reseeded with, so the reseed runs once
    /// per change rather than every frame.
    /* 0x6C2 */ s16 field_6C2;
    /// Frames the current animation has been ticking; the reseed clears it and
    /// the tick path walks it up by one a frame.
    /* 0x6C4 */ s16 field_6C4;
    /// Flinch countdown: `func_actor_402200_80131F54` arms it from
    /// `Gp_GetIdParam2` when a hit lands and ticks it down a frame at a time,
    /// raising `field_494`/`field_49A` on the frame it runs out. While it is
    /// non-zero a hit is already being flinched, so the sequence bodies arm
    /// the pair immediately only when it is zero.
    /* 0x6C6 */ s16 field_6C6;
    /// Cleared on the frame the sequence body `func_actor_402200_8013539C`
    /// reseeds the animation.
    /* 0x6C8 */ s16 field_6C8;
    /* 0x6CA */ u16 field_6CA;
    /// Set to 4 when the sequence restarts in mode 2, cleared otherwise.
    /* 0x6CC */ s16 field_6CC;
    /// State `func_actor_402200_8013539C` advances: 0 reseeds the animation at
    /// `field_6C0` and arms the cue, 1 waits for `field_6C4` to reach 0x37 and
    /// then drops the state back to 0 so the reseed runs again.
    /// The attack sequence `func_actor_402200_801354B0` runs the same shape
    /// over three states: its state 0 picks between slot sets 9 and 0xA on
    /// `field_6D2` and parks the state on the matching one, and states 1 / 2
    /// each wait out their own `field_6C4` threshold (0x50 and 0x3B) before
    /// dropping back to 0.
    /* 0x6CE */ s16  field_6CE;
    /* 0x6D0 */ byte pad_6D0[2];
    /// Which-side flag the target body `func_actor_402200_80131F54` raises from
    /// a dot product of the offset to the actor it is tracking: 1 when the
    /// product comes out zero, 0 otherwise. The sequence bodies branch on it -
    /// `func_actor_402200_801354B0` picks between slot sets 9 and 0xA, and
    /// `func_actor_402200_80135630` / `func_actor_402200_80135A24` between 0xD
    /// and the set at `field_6C0`.
    /* 0x6D2 */ s16 field_6D2;
    /// Countdown `func_actor_402200_801347F4` rolls from the `Gp_LcgState` LCG
    /// (0x4B..0x6A) when it reseeds the animation, and ticks down a frame at a
    /// time until it runs out and the cue fires.
    /* 0x6D4 */ u16 field_6D4;
    /* 0x6D6 */ s16 field_6D6;
    /* 0x6D8 */ s16 field_6D8;
    /// Timer pair the reseed arms alongside `field_6DE`.
    /* 0x6DA */ s16 field_6DA;
    /* 0x6DC */ s16 field_6DC;
    /// Third timer the reseed arms; written last of the three.
    /* 0x6DE */ s16 field_6DE;
    /// Fourth timer `func_actor_402200_801347F4` clears alongside the trio
    /// above when its countdown runs out.
    /* 0x6E0 */ s16 field_6E0;
    /* 0x6E2 */ s16 field_6E2;
    /* 0x6E4 */ s16 field_6E4;
    /* 0x6E6 */ s16 field_6E6;
    /* 0x6E8 */ s16 field_6E8;
    /* 0x6EA */ s16 field_6EA;
    /// Sequence mode `func_actor_402200_8013539C` tests: the reseed arms the
    /// cue unless it is already 1, and a restart that finds it 1 flips it to 2.
    /* 0x6EC */ s16 field_6EC;
    /* 0x6EE */ s16 field_6EE;
    /// Latch the attack sequences park the slot set in: state 0 stores 1 or 2
    /// next to `field_6C0`, and state 1 reads it back to pick the frame count
    /// it waits for (0x2C for the 0x11 animation, 0x19 otherwise).
    /* 0x6F0 */ s16 field_6F0;
    /// Pair `func_actor_402200_80135A24` parks at 2 while it runs, cleared when
    /// its countdown runs out.
    /* 0x6F2 */ s16  field_6F2;
    /* 0x6F4 */ s16  field_6F4;
    /* 0x6F6 */ byte pad_6F6[4];
    /// Entry count of the box table at `field_6B4`, read as a signed halfword;
    /// a non-positive count disarms the scan.
    /* 0x6FA */ s16 field_6FA;
    /// Screen x / y and quartered depth of the two points
    /// `func_actor_402200_80135D5C` projects.
    /* 0x6FC */ s16 field_6FC[2];
    /* 0x700 */ s16 field_700[2];
    /* 0x704 */ s16 field_704[2];
    /// Index of the box the scan last reported a hit on.
    /* 0x708 */ s16 field_708;
    /* 0x70A */ s16 field_70A;
    /* 0x70C */ s16 field_70C;
    /* 0x70E */ s16 field_70E;
    /* 0x710 */ s16 field_710;
    /* 0x712 */ s16 field_712;
    /// Second per-state latch, read and written as a signed halfword: the
    /// attack sequences raise it to 1 in state 0 and state 1 bumps it to 2 on
    /// the frame it still equals the state.
    /* 0x714 */ s16 field_714;
    /// Damage amount the hit handlers OR into `field_494`; read as a signed
    /// halfword on the frame the hit lands.
    /* 0x716 */ s16 field_716;
    /* 0x718 */ s16 field_718;
    /* 0x71A */ s16 field_71A;
} Actor402200Work;
STATIC_ASSERT_SIZEOF(Actor402200Work, 0x71C);

/// Actor context handed to this overlay's callbacks: `field_1C` is the work
/// block above, `field_20` the `GpEnemy` the spawner left in the task's
/// `Task::spawnArg2` slot, and `field_2C` the display object. Same shape as the
/// other actor overlays' contexts.
typedef struct Actor402200 {
    /* 0x00 */ byte              pad_0[0x1C];
    /* 0x1C */ Actor402200Work*  field_1C;
    /* 0x20 */ GpEnemy*          field_20;
    /* 0x24 */ void*             field_24;
    /* 0x28 */ byte              pad_28[4];
    /* 0x2C */ Actor402200Obj2C* field_2C;
    /// Sequence state the overlay's body parks for the frame dispatcher, the
    /// same slot the other actor contexts keep at 0x30.
    /* 0x30 */ s32 field_30;
} Actor402200;

/// 0x18-byte block `func_actor_402200_80138208` takes from `G_SCRATCH_HEAD`
/// while projecting the actor's origin through its attach coordinate and
/// working out the ordering-table depth. `vec` is the zeroed origin the GTE
/// reads; the rest are the projection's results, and their order is the one
/// `rtps` writes them in: screen xy, depth cue, `FLAG`, and the average
/// screen z `otz`. Same block the other actor overlays spell
/// `<overlay>ProjectScratch`.
typedef struct Actor402200ProjectScratch {
    /* 0x00 */ SVECTOR vec;
    /* 0x08 */ s32     sxy;
    /* 0x0C */ s32     dp;
    /* 0x10 */ s32     flag;
    /* 0x14 */ s32     otz;
} Actor402200ProjectScratch;
STATIC_ASSERT_SIZEOF(Actor402200ProjectScratch, 0x18);

/// 0x18-byte block `func_actor_402200_80132E34` takes from `G_SCRATCH_HEAD`
/// to place the actor relative to the player: `in` is the offset rotated
/// through the player's root coordinate into `out`.
typedef struct Actor402200OffsetScratch {
    /* 0x00 */ VECTOR  out;
    /* 0x10 */ SVECTOR in;
} Actor402200OffsetScratch;
STATIC_ASSERT_SIZEOF(Actor402200OffsetScratch, 0x18);

/// 0x48-byte block `func_actor_402200_80135D5C` takes from `G_SCRATCH_HEAD`
/// to aim the actor: `m` is the root's world matrix brought local to the
/// fourth part, `out` the GTE's rotated offset, and `pts` the two world points
/// (root-based aim point, fourth-part offset) projected through `GsWSMATRIX`
/// into `sxy` and the quartered screen z `otz`.
typedef struct Actor402200AimScratch {
    /* 0x00 */ MATRIX  m;
    /* 0x20 */ VECTOR  out;
    /* 0x30 */ SVECTOR pts[2];
    /* 0x40 */ s32     sxy;
    /* 0x44 */ s32     otz;
} Actor402200AimScratch;
STATIC_ASSERT_SIZEOF(Actor402200AimScratch, 0x48);

/// Per-animation-id value `func_actor_402200_80137EEC` hands `func_800B4114`
/// as its fifth argument when it reseeds animation slots 1..0x12.
extern s16 D_actor_402200_801383AC[];

/// The game's shared 32-bit LCG state: every draw is
/// `Gp_LcgState = Gp_LcgState * 5 + 0x71357911`, read back from the global,
/// with the caller taking the bits it wants out of the high half.
extern u32 Gp_LcgState;

/// One 4-byte entry of `D_actor_402200_801383D8`: the first entry whose
/// `frame` is not below the animation frame `Actor402200Work::field_6C4`
/// supplies `value` for `field_6C8`.
typedef struct Actor402200FrameStep {
    /* 0x0 */ s16 frame;
    /* 0x2 */ u16 value;
} Actor402200FrameStep;
STATIC_ASSERT_SIZEOF(Actor402200FrameStep, 4);

extern Actor402200FrameStep D_actor_402200_801383D8[];

/// 0x30-byte block `func_actor_402200_80131F54` takes from `G_SCRATCH_HEAD`:
/// `delta` receives the `func_800E0C10` push-back and is then reused for the
/// offset to the player, and `ofs` is the spark offset handed to
/// `func_800FDB18`.
typedef struct Actor402200HitScratch {
    /* 0x00 */ GpDeltaScratch delta;
    /* 0x10 */ byte           pad_10[0x10];
    /* 0x20 */ SVECTOR        ofs;
    /* 0x28 */ byte           pad_28[8];
} Actor402200HitScratch;
STATIC_ASSERT_SIZEOF(Actor402200HitScratch, 0x30);

/// Reacts to the damage just taken; see its definition.
void func_actor_402200_801324E8(Actor402200* arg0, s32 arg1);

/// Aims the actor at the player; see its definition.
void func_actor_402200_80135D5C(Actor402200* arg0);

/// Parks the actor's target position off the player; see its definition.
void func_actor_402200_80132E34(Actor402200* arg0);

#endif
