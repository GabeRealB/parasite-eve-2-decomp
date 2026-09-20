#ifndef ACTOR_421600_H
#define ACTOR_421600_H

#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3FB8.h"
#include "main/task.h"
#include "main/tmd.h"

/// The actor id word at 0xE90, read two ways: `func_actor_421600_8013848C`
/// and `func_actor_421600_8013E9D8` mask the whole word to 24 bits and compare
/// it with 0x11402, while `func_actor_421600_8013947C` tests its third byte
/// alone against 2.
typedef union Actor421600IdWord {
    /* 0x0 */ s32 word;
    /* 0x0 */ u8  bytes[4];
} Actor421600IdWord;
STATIC_ASSERT_SIZEOF(Actor421600IdWord, 0x4);

/// One halfword of an `Actor421600Msg`, which `func_actor_421600_80132A00`
/// also reads as the raw byte triple it copies into `field_E90`.
typedef union Actor421600MsgWord {
    /* 0x00 */ u16 word;
    /* 0x00 */ u8  bytes[2];
} Actor421600MsgWord;

/// Payload of the messages `func_actor_421600_80132A00` dispatches on:
/// `field_0` is the opcode (0x109 drives the state machine, 0x1402 the state
/// jumps) and `field_2` the sub-command. Same four bytes as `Actor00100Msg`.
typedef struct Actor421600Msg {
    /* 0x00 */ Actor421600MsgWord field_0;
    /* 0x02 */ Actor421600MsgWord field_2;
} Actor421600Msg;
STATIC_ASSERT_SIZEOF(Actor421600Msg, 0x4);

typedef struct Actor421600Waypoint {
    /* 0x0 */ s16 x;
    /* 0x2 */ s16 z;
} Actor421600Waypoint;
STATIC_ASSERT_SIZEOF(Actor421600Waypoint, 0x4);

/// Per-actor state block for the `actor_421600` overlay's enemy.
///
/// `func_actor_421600_80134AD4` allocates it with `memCalloc(0xEB0, 0)` and
/// stores it in the `Task::work` slot (0x1C), so the size below is the
/// allocation rather than a guess: this actor reuses that pointer field for its
/// own work block and it is *not* a `TaskIdMap` here. Reach it with
/// `(Actor421600Work*)task->work`.
///
/// Only the fields the decompiled code touches are named so far: the three
/// `GpObj` display nodes `func_actor_421600_8013E668` hands back to
/// `Gp_UnlinkObj`, the two child tasks it kills, and the halfword
/// `func_actor_421600_8013E654` writes. The display nodes sit 0x24 later than
/// the 0x8C8/0xA08/0xB48 triple on actor 01900/401800, with the same 0x140
/// stride. `field_4` is the live-actor flag `func_actor_421600_8013E858`
/// tests, and `field_B6C.flags` is the mask it writes. `field_0` / `field_68`
/// and the 0x828 motion halfwords are the same cluster `Actor00100_Fn0B730`
/// uses; `field_8EC.field_1C` is the 0x908 store. `field_B8C` is the
/// `GpRec18` table `func_actor_421600_8013285C` walks after the 0x20-byte
/// `field_B6C` node, matching `Actor00100Obj.field_20` after `objs[2]`.
/// `field_E90` is a word here (not the `s16` actor 444000 keeps at the same
/// offset); `func_actor_421600_8013E9D8` masks it to 24 bits and compares that
/// with 0x11402 to pick the state it writes to `field_0`.
typedef struct Actor421600Work {
    /* 0x000 */ s16 field_0;
    /// Companion halfword the message handler `func_actor_421600_80132A00`
    /// clears to -1 whenever it reseeds `field_0`; same pairing as actor 00100's
    /// `field_0` / `field_2`.
    /* 0x002 */ s16 field_2;
    /* 0x004 */ s16 field_4;
    /// Frame counter `func_actor_421600_8013848C` bumps each tick and waits on
    /// to read 0xF; same slot actor 00100 keeps its own tick in.
    /* 0x006 */ u16 field_6;
    /// Retry counter `func_actor_421600_80138D24` bumps while the 0xB8C walk
    /// reports a hit and reads with `(s16)` in its 9..0x18 frame window; the
    /// live-actor edge clears it.
    /* 0x008 */ u16                 field_8;
    /* 0x00A */ byte                pad_A[2];
    /* 0x00C */ Actor421600Waypoint field_C[2];
    /* 0x014 */ s16                 field_14;
    /* 0x016 */ byte                pad_16[0x44];
    /// Animation step counter masked to 0x3FF by the state handlers; the
    /// model-shrink tails wait for it to read 0xC.
    /* 0x05A */ u16  field_5A;
    /* 0x05C */ byte pad_5C[0xC];
    /* 0x068 */ u16  field_68;
    /* 0x06A */ byte pad_6A[0x7BE];
    /* 0x828 */ u16  field_828;
    /// Motion mode every tick in this overlay tests against 0; read as a
    /// signed halfword wherever it branches (`lh` in `func_actor_421600_8013B4C4`
    /// and the six other readers), so it is an `s16` rather than the `u16`
    /// the store-only callers would suggest.
    /* 0x82A */ s16  field_82A;
    /* 0x82C */ byte pad_82C[2];
    /* 0x82E */ s16  field_82E;
    /* 0x830 */ byte pad_830[2];
    /* 0x832 */ u16  field_832;
    /// Source `func_actor_421600_8013848C` copies into `field_832`; the anim
    /// view above still reaches it through its own padding.
    /* 0x834 */ u16  field_834;
    /* 0x836 */ byte pad_836[4];
    /// Clip id `func_actor_421600_80133B30` copies into the blend slots.
    /* 0x83A */ u16  field_83A;
    /* 0x83C */ byte pad_83C[2];
    /* 0x83E */ u16  field_83E;
    /* 0x840 */ u16  field_840;
    /* 0x842 */ byte pad_842[2];
    /* 0x844 */ s16  field_844;
    /* 0x846 */ byte pad_846[0x4A];
    /// Argument record `func_actor_421600_801350BC` fills for `func_800FDB18`:
    /// the model part coordinate `sc->pad` names, scale 0x100 and count 2.
    /// Same slot actor 00100 keeps at its own 0x890.
    /* 0x890 */ GpEffArg field_890;
    /// Hit position `func_actor_421600_801350BC` copies out of its scratch
    /// vector and hands to `func_800FDB18` as the effect rotation.
    /* 0x898 */ SVECTOR field_898;
    /// Cleared alongside `field_6` on the live-actor edge of the shrink tick
    /// `func_actor_421600_801366F4`, the same place actor 00100 clears its own
    /// 0x8DC byte.
    /* 0x8A0 */ s8   field_8A0;
    /* 0x8A1 */ byte pad_8A1[3];
    /// World X and Z `func_actor_421600_8013848C` takes off the gte-rotated
    /// vec (`field_8A4` from its `vx`, `field_8AC` from its `vz`), around the
    /// zeroed `field_8A8` actor 00100 keeps at its own 0x8DC.
    /* 0x8A4 */ s32  field_8A4;
    /* 0x8A8 */ s32  field_8A8;
    /* 0x8AC */ s32  field_8AC;
    /* 0x8B0 */ byte pad_8B0[4];
    /// Pose id / blend flag pair `func_actor_421600_8013848C` sets to 7 and 1;
    /// actor 00100 has the same pair at 0x8E8 / 0x8EA.
    /* 0x8B4 */ s16   field_8B4;
    /* 0x8B6 */ s8    field_8B6;
    /* 0x8B7 */ byte  pad_8B7[0x35];
    /* 0x8EC */ GpObj field_8EC;
    /// `GpRec18` table paired with `field_8EC`, the same 0x20-byte stride
    /// `field_B8C` keeps after `field_B6C`.
    /* 0x90C */ GpRec18 field_90C;
    /* 0x924 */ byte    pad_924[0x108];
    /* 0xA2C */ GpObj   field_A2C;
    /// `GpRec18` table paired with `field_A2C`, the middle of the three the
    /// death tick `func_actor_421600_801392A8` walks (0x90C / 0xA4C / 0xB8C).
    /* 0xA4C */ GpRec18 field_A4C;
    /* 0xA64 */ byte    pad_A64[0x108];
    /* 0xB6C */ GpObj   field_B6C;
    /* 0xB8C */ GpRec18 field_B8C;
    /* 0xBA4 */ byte    pad_BA4[0x108];
    /* 0xCAC */ GpObj   field_CAC;
    /// Capsule carried by the fourth collision node. Its second endpoint's
    /// Z offset at 0xCD8 is 0x2BC at spawn and -0x320 in the movement tick.
    /* 0xCCC */ GpActorD4Rec field_CCC;
    /// The 12 0x18-byte slots `func_actor_421600_80138D24` scans for one whose
    /// `key` reads 0x100000, stopping at the first empty one. A cursor into
    /// the same run sits at 0xCE0, which `func_actor_421600_80134AD4` points at
    /// `field_CE4` itself.
    /* 0xCE4 */ GpRec18           field_CE4[12];
    /* 0xE04 */ MATRIX            field_E04;
    /* 0xE24 */ MATRIX            field_E24;
    /* 0xE44 */ byte              pad_E44[0x34];
    /* 0xE78 */ s16               field_E78;
    /* 0xE7A */ byte              pad_E7A[2];
    /* 0xE7C */ s32               field_E7C;
    /* 0xE80 */ s32               field_E80;
    /* 0xE84 */ s32               field_E84;
    /* 0xE88 */ s32               field_E88;
    /* 0xE8C */ s32               field_E8C;
    /* 0xE90 */ Actor421600IdWord field_E90;
    /* 0xE94 */ Task*             field_E94;
    /* 0xE98 */ Task*             field_E98;
    /// One-shot "already reported" latch `func_actor_421600_80132A00` clears
    /// and dispatches 0x3F1 to slot 3 on, the same handshake actor 00100 keeps
    /// at its own 0xE9C.
    /* 0xE9C */ s16 field_E9C;
    /// Distance `func_actor_421600_8013848C` clamps to 0xFA0 after the gte
    /// rotation.
    /* 0xE9E */ s16  field_E9E;
    /* 0xEA0 */ byte pad_EA0[2];
    /* 0xEA2 */ u16  field_EA2;
    /// Halfword the idle tick `func_actor_421600_8013A404` reseeds `field_6`
    /// from, adding the low nibble of an `Gp_LcgState` draw while `field_4` is
    /// set.
    /* 0xEA4 */ u16 field_EA4;
    /* 0xEA6 */ u16 field_EA6;
    /// Halfword pair `func_actor_421600_80132A00` forwards under the 0x109
    /// message, the same one-step lag its sibling actor 00100 keeps at
    /// 0xC24 / 0xC26.
    /* 0xEA8 */ u16  field_EA8;
    /* 0xEAA */ u16  field_EAA;
    /* 0xEAC */ s16  field_EAC;
    /* 0xEAE */ byte pad_EAE[2];
} Actor421600Work;
STATIC_ASSERT_SIZEOF(Actor421600Work, 0xEB0);

/// Animation view of the same work block, as `func_actor_421600_80133B30`
/// reads it: the pose context at 0x1C and its blend twin at 0x420, each
/// followed by 0x28-byte `GpAnimSlot`s, plus the two clip ids the loop copies
/// into them. The pads stand in for the rest of the block -- a slot array
/// cannot span the fields `Actor421600Work` names at 0x5A / 0x68, and 0x420 is
/// not a whole number of slots past 0x30.
typedef struct Actor421600AnimWork {
    /* 0x000 */ s16        field_0;
    /* 0x002 */ byte       pad_2[0x1A];
    /* 0x01C */ GpAnimCtx  anim;
    /* 0x030 */ GpAnimSlot slots[25];
    /* 0x418 */ byte       pad_418[8];
    /* 0x420 */ GpAnimCtx  blendAnim;
    /* 0x434 */ GpAnimSlot blendSlots[25];
    /* 0x81C */ byte       pad_81C[0xC];
    /* 0x828 */ u16        field_828;
    /* 0x82A */ s16        field_82A;
    /* 0x82C */ s16        field_82C;
    /* 0x82E */ s16        field_82E;
    /* 0x830 */ u16        field_830;
    /* 0x832 */ u16        field_832;
    /* 0x834 */ u16        field_834;
    /* 0x836 */ s16        field_836;
    /* 0x838 */ s16        field_838;
    /* 0x83A */ u16        field_83A;
    /* 0x83C */ s16        field_83C;
    /* 0x83E */ u16        field_83E;
    /* 0x840 */ u16        field_840;
    /* 0x842 */ s16        field_842;
    /* 0x844 */ s16        field_844;
    /* 0x846 */ byte       pad_846[2];
    /* 0x848 */ s32        field_848[18];
} Actor421600AnimWork;
STATIC_ASSERT_SIZEOF(Actor421600AnimWork, 0x890);

/// 0x34-byte block taken from the scratchpad head by the shrink tick
/// `func_actor_421600_801366F4`: a `MATRIX`, the per-axis scale `VECTOR`
/// `ScaleMatrix` folds into it (1.0 / the shrinking factor / 1.0), and the yaw
/// stored just before `Gfx_RotMatrixY` rebuilds the rotation. Same shape as
/// `ActorShared80135a60Scratch`, whose body is the uniform-scale twin.
typedef struct Actor421600ShrinkScratch {
    /* 0x00 */ MATRIX m;
    /* 0x20 */ VECTOR scale;
    /* 0x30 */ s16    angle;
    /* 0x32 */ s16    pad_32;
} Actor421600ShrinkScratch;
STATIC_ASSERT_SIZEOF(Actor421600ShrinkScratch, 0x34);

/// 0xC-byte block taken from the scratchpad head by the zone-aim tick
/// `func_actor_421600_8013B4C4`: the XZ direction it measures the actor's
/// facing against, plus the wrapped heading it derives from that direction and
/// re-reads after clamping `field_83E` to +/-0x80, before `Gfx_RotMatrixY`.
typedef struct Actor421600SeekScratch {
    /* 0x0 */ SVECTOR vec;
    /* 0x8 */ s16     angle;
    /* 0xA */ s16     pad_A;
} Actor421600SeekScratch;
STATIC_ASSERT_SIZEOF(Actor421600SeekScratch, 0xC);

typedef struct Actor421600TurnScratch {
    /* 0x00 */ SVECTOR vec;
    /* 0x08 */ s16     delta;
    /* 0x0A */ s16     yaw;
    /* 0x0C */ s16     steps;
    /* 0x0E */ s16     pad;
} Actor421600TurnScratch;
STATIC_ASSERT_SIZEOF(Actor421600TurnScratch, 0x10);

/// 0xC-byte scratch from `G_SCRATCH_HEAD` used by `func_actor_421600_80133444`
/// to hold the XZ offset of a `GsCOORDINATE2` from the centre of its circular
/// arena (`field_0` / `field_4`) together with the radius `field_8` is tested
/// against: the function squares all three and pulls the coordinate onto a
/// 700-unit ring when `field_0 + field_4` reaches `field_8`. Same slot shape as
/// the gameplay `GpAngleScratch` (3 words at `head - 0xC`); the fields there
/// are wrap angles, so this overlay keeps its own name for it.
typedef struct {
    /* 0x0 */ s32 field_0;
    /* 0x4 */ s32 field_4;
    /* 0x8 */ s32 field_8;
} Actor421600ArenaScratch;
STATIC_ASSERT_SIZEOF(Actor421600ArenaScratch, 0xC);

/// Per-task actor context: `field_1C` is the work block above (the same
/// pointer `Task::work` holds), `field_20` the `GpEnemy` in
/// `Task::spawnArg2`, and `field_2C` the actor's `TmdObject`. Same shape as
/// `Actor403000` / `Actor401800`.
typedef struct Actor421600 {
    /* 0x00 */ byte             pad_0[0x18];
    /* 0x18 */ TaskFunc         exitCallback;
    /* 0x1C */ Actor421600Work* field_1C;
    /* 0x20 */ GpEnemy*         field_20;
    /* 0x24 */ void*            field_24;
    /* 0x28 */ byte             pad_28[4];
    /* 0x2C */ TmdObject*       field_2C;
    /* 0x30 */ s32              state;
    /* 0x34 */ s16              spawnArg1Lo;
    /* 0x36 */ s16              field_36;
} Actor421600;

typedef struct Actor421600ParamRow {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
    /* 0x4 */ u16 field_4;
    /* 0x6 */ u16 field_6;
} Actor421600ParamRow;
STATIC_ASSERT_SIZEOF(Actor421600ParamRow, 0x8);

extern Actor421600ParamRow D_actor_421600_8013EF48[];
extern GpPairSrcE          D_actor_421600_8013EF38;
extern u8                  D_actor_421600_80151028[];
extern void*               D_actor_421600_80151118;

/// Shared gameplay mode record. This overlay reads the unsigned halfword at
/// +2 before releasing the actor's state; the leading byte is the mode other
/// actors use to gate their updates.
typedef struct Actor421600ModeState {
    /* 0x00 */ u8   mode;
    /* 0x01 */ byte pad_1;
    /* 0x02 */ u16  field_2;
} Actor421600ModeState;
STATIC_ASSERT_SIZEOF(Actor421600ModeState, 0x4);

extern Actor421600ModeState D_801153F4;

/// The overlay's pose table: 8-byte records of three halfwords at 0x0/0x2/0x4
/// plus padding, i.e. `SVECTOR`s. Indexed by the low signed halfword of the
/// caller's id. `actor_403000` keeps a table of the same shape at 0x80158CE0
/// and reaches it with a body the shared-body index groups with this one; a
/// body that reads its own overlay's data cannot be promoted, so each carrier
/// keeps a plain-C copy -- `src/actors/actor_403000/actor_403000.c` for the
/// other.
extern SVECTOR D_actor_421600_80151158[];

/// Hit-position table `func_actor_421600_801350BC` picks one of twelve entries
/// from by damage magnitude: the same 8-byte `SVECTOR` records as the pose
/// table above, at the 0x801510B8 end of the same trailing data run.
extern SVECTOR D_actor_421600_801510B8[];

/// 4-byte table indexed by `(arg0 > 0) + ((arg1 < 1) << 1)`.
extern s8 D_actor_421600_801511D0[];

/// Effect callback the spawning code points `D_80114B78` at before each
/// `Gp_SpawnEff` on a death-tick frame, so the spawn knows which tick follows.
/// Same role as actor 401300's `D_actor_401300_80147894` pair; the four
/// addresses sit in this overlay's trailing model/animation bank, so they are
/// objects rather than named tables.
extern char D_actor_421600_80143EF4;
extern char D_actor_421600_801443C8;
extern char D_actor_421600_80145124;
extern char D_actor_421600_80145604;

/// Global effect-model callback slot the spawn helpers read; a one-element
/// array so the store is absolute (see actor 401300's header for the same
/// declaration).
extern void* D_80114B78[1];

/// `Task::exitCallback` teardown: kill the two helper tasks, unlink the three
/// display nodes, drop the enemy's `recs` slot, then `Gp_DestroyEnemy`.
void func_actor_421600_8013E668(Task* task);

/// Copy the `vx`/`vy`/`vz` of record `arg1` of the pose table into `arg0`.
void func_actor_421600_8013E7F8(SVECTOR* arg0, s32 arg1);

s8 func_actor_421600_8013E830(s32 arg0, s32 arg1);

void func_actor_421600_8013E858(Actor421600* arg0);

/// Write the actor's start state: `field_0` becomes 2, or 5 when the id word
/// `field_E90` masks down to 0x11402. Also runs the shared spawn/teardown tail
/// that `func_actor_421600_8013EAAC` / `_8013EB7C` share.
void func_actor_421600_8013E9D8(Actor421600* arg0);

void func_actor_421600_80134604(Actor421600* arg0);

void func_actor_421600_80134AD4(GpEnemy* enemy, Actor421600* actor);

/// Moves an interior coordinate to the nearest padded X or Z edge; returns
/// 1 when moved, or 0 when already outside. `func_actor_421600_8013947C`
/// runs it while `field_E90`'s third byte is not 2.
s32 func_actor_421600_80133334(GsCOORDINATE2* arg0);

s32 func_actor_421600_8013285C(GsCOORDINATE2* coord, GpRec18* movement, s16 arg2);

/// `func_actor_421600_8013285C`'s sibling: same `arg0` (rotated into the
/// scratch `MATRIX`) and same 0x18-stride `GpRec18` table in `arg1`, walked for
/// the first `arg2` records. `arg3` is zeroed up front and holds the
/// displacement of whichever record applied. `func_actor_421600_801392A8`
/// walks the 0x90C and 0xA4C tables with it and tests the return.
s32 func_actor_421600_80132310(GsCOORDINATE2* arg0, GpRec18* arg1, s16 arg2, SVECTOR* arg3);

/// Camera-target matrix the actor measures its offset from; see
/// `D_80073B8C[0]->t[]` in the other enemy overlays.
extern MATRIX* D_80073B8C;

/// Non-1 while the model is being aimed rather than left alone; the guards
/// `func_actor_421600_80138D24` runs around its gte scale are the same test
/// `Actor00100_MoveForward` makes before touching a coordinate.
extern u8 D_80072729;

extern u32 Gp_LcgState;

/// 4x4 zone table `func_actor_421600_8013A404` samples with the X and Z
/// buckets of the actor's position, cell `x | z * 4`; the sample is compared
/// against 0xB to pick between the 6 and 0x24 states.
extern s8 D_actor_421600_801511C0[16];

void func_actor_421600_8013EAAC(Actor421600* arg0);
void func_actor_421600_8013EB7C(Actor421600* arg0);

/// Idle yaw `func_actor_421600_80132A00` stamps onto the enemy's `field_40`
/// on every state message, the same slot actor 00100 keeps at 0x8013EF3C.
extern u16 D_actor_421600_8013EF3C;

/// Progress counter the same handler compares against 4 / 5 / 2 / 0 to pick
/// the arena corner the actor is dropped into. Written by
/// `func_actor_421600_80134AD4` at spawn.
extern s16 D_actor_421600_80151268;

/// State-machine message handler: 0x109 drives `field_0` directly, 0x1402
/// teleports the actor to the corner its `field_8` mode and the progress
/// counter select and reseeds the state.
s32 func_actor_421600_80132A00(Actor421600* arg0, s32 arg1, Actor421600Msg* arg2);

/// Signed transition durations, indexed by old animation * 25 + new animation.
extern s8 D_actor_421600_80150DB4[];

void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);
s32  func_actor_421600_80133CAC(Actor421600* arg0, Actor421600Work* work);

#endif
