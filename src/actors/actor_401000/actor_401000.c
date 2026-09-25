#include "common.h"

#include <psyq/inline_c.h>
#include "gte.h"
#include <psyq/abs.h>

#include "actors/actors_shared_80169f74.h"
#include "gameplay/1A8.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3E9C.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"

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

/// Private work block of the actor 401000 task, hanging off `Task::work`.
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
    /// Heading `func_actor_401000_8013D814` takes from the root coordinate's
    /// Z axis once a placement record has been applied to it.
    /* 0x016 */ s16                yaw;
    /* 0x018 */ byte               pad_18[0x42];
    /* 0x05A */ u16                field_5A;
    /* 0x05C */ byte               pad_5C[0xC];
    /* 0x068 */ Actor401000Flags68 flags_68;
    /* 0x06C */ byte               pad_6C[0x828];
    /* 0x894 */ s32                field_894;
    /* 0x898 */ s16                field_898;
    /* 0x89A */ s16                field_89A;
    /// Clip the body slots are playing; `func_actor_401000_80132EF0` moves it
    /// to the requested `field_89E` when it applies a clip change.
    /* 0x89C */ s16 field_89C;
    /* 0x89E */ s16 field_89E;
    /// Frames since the last clip change: counted up every
    /// `func_actor_401000_80132EF0` tick and cleared when a change is applied.
    /* 0x8A0 */ u16 field_8A0;
    /* 0x8A2 */ s16 field_8A2;
    /* 0x8A4 */ s16 field_8A4;
    /* 0x8A6 */ s16 field_8A6;
    /* 0x8A8 */ s16 field_8A8;
    /// Playback rate of the blend slots, and the weight (out of 0x1000) the
    /// blend pose gets when `func_actor_401000_80132A84` mixes it into the
    /// body pose; both are seeded (0x30, 0x800) when a blend clip starts.
    /* 0x8AA */ u16  field_8AA;
    /* 0x8AC */ s16  field_8AC;
    /* 0x8AE */ s16  field_8AE;
    /* 0x8B0 */ s16  field_8B0;
    /* 0x8B2 */ byte pad_8B2[2];
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
    /// Contact records of the `field_8D0` node, also the enemy's `recs`;
    /// the movement helpers walk them twelve at a time.
    /* 0x8F0 */ GpRec18 field_8F0[12];
    /* 0xA10 */ GpObj   field_A10;
    /// Contact records of the `field_A10` node.
    /* 0xA30 */ GpRec18 field_A30[12];
    /* 0xB50 */ GpObj   field_B50;
    /// The single obstacle record the `field_B50` node is registered against.
    /* 0xB70 */ GpRec18 field_B70;
    /// Light matrix `func_actor_401000_80133274` binds to the model's
    /// `TmdObject::lightMtx` (the color matrix is `field_BA8`, which is the
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
    /* 0xBEA */ s16  field_BEA;
    /* 0xBEC */ s16  field_BEC;
    /* 0xBEE */ byte pad_BEE[2];
    /// Forward direction `func_actor_401000_801374D4` rebuilds from the wrapped
    /// turn toward the player: `Gfx_RotMatrixY` on the turn then its second
    /// column, normalised, and finally scaled by the `field_C0A` draw. The same
    /// slot `Actor401300Work` keeps at +0xC8C.
    /* 0xBF0 */ SVECTOR field_BF0;
    /// Position `func_actor_401000_8013D044` snaps the root coordinate to when
    /// a 0xB/0xD state transition arrives: written by the transition handler and
    /// loaded into `coord.t` with `flg` cleared so the local matrix is rebuilt.
    /* 0xBF8 */ SVECTOR field_BF8;
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
    /* 0xC04 */ s16 field_C04;
    /// Clip-phase latch `func_actor_401000_801365C8` runs the 8 / -1 / 0 march
    /// off: 8 flips to -1 once `field_8A2` reaches 0x18, -1 flips to 0 at 0x12,
    /// and 0 keys the 5-frame exit window. The same slot `Actor01900Work` keeps
    /// at +0xC26.
    /* 0xC06 */ s16 field_C06;
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
    /* 0xC2A */ byte pad_C2A[2];
    /// Ring of the last seven view-space positions `func_actor_401000_8013D044`
    /// records, one per step; `field_C7C` is the write cursor.
    /* 0xC2C */ SVECTOR field_C2C[7];
    /* 0xC64 */ byte    pad_C64[0x18];
    /// Cleared by `func_actor_401000_80133274` once both obstacle tables have
    /// been dropped; the write cursor `Actor401300Work` keeps at +0xD78.
    /* 0xC7C */ s16  field_C7C;
    /* 0xC7E */ byte pad_C7E[2];
} Actor401000Work;
STATIC_ASSERT_SIZEOF(Actor401000Work, 0xC80);

/// The actor's state handlers, indexed by `Actor401000Work::field_0`.
/// `func_actor_401000_8013D044` copies the table to its frame before
/// dispatching. Same shape as `Actor01900StateTable` / `Actor401300StateTable`.
typedef struct Actor401000StateTable {
    TaskFunc fn[34];
} Actor401000StateTable;
STATIC_ASSERT_SIZEOF(Actor401000StateTable, 0x88);

/// Hit-resolution scratch: knockback matrix, contact and damage reaction.
typedef struct Actor401000HitScratch {
    /* 0x00 */ MATRIX  m;
    /* 0x20 */ s32     dx;
    /* 0x24 */ s32     dy;
    /* 0x28 */ s32     dz;
    /* 0x2C */ s32     pad_2C;
    /* 0x30 */ SVECTOR dir;
    /* 0x38 */ SVECTOR hitPos;
    /* 0x40 */ s32     id;
    /* 0x44 */ s32     damage;
    /* 0x48 */ s32     dist;
    /* 0x4C */ s16     yaw;
    /* 0x4E */ s16     crit;
    /* 0x50 */ s16     effect;
    /* 0x52 */ s16     pad_52;
} Actor401000HitScratch;
STATIC_ASSERT_SIZEOF(Actor401000HitScratch, 0x54);

/// 0x18-byte scratch `func_actor_401000_8013D044` takes from `G_SCRATCH_HEAD`;
/// `pos` receives a model coordinate in view space. Same shape as
/// `Actor01900ViewScratch` / `Actor401300ViewScratch`.
typedef struct Actor401000ViewScratch {
    /* 0x00 */ byte    pad_0[0x10];
    /* 0x10 */ SVECTOR pos;
} Actor401000ViewScratch;
STATIC_ASSERT_SIZEOF(Actor401000ViewScratch, 0x18);

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
/// are matched against `GpAreaKey::stage` / `area`, and when a row
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

/// Scratch block `func_actor_401000_801323EC` takes from `G_SCRATCH_HEAD`: the
/// `GpDeltaScratch` filled by `func_800E0C10`, and `moved`, the value it
/// returns.
typedef struct Actor401000DeltaFlag {
    /* 0x00 */ GpDeltaScratch delta;
    /* 0x10 */ s32            moved;
} Actor401000DeltaFlag;
STATIC_ASSERT_SIZEOF(Actor401000DeltaFlag, 0x14);

/// Scratch block `func_actor_401000_80132824` takes from `G_SCRATCH_HEAD`:
/// `local` holds a root translation raised by 1000, which is rotated into `out`
/// for the player and into `from` for this actor; `hit` is the result of
/// `func_800E0308` on the two.
typedef struct Actor401000SightScratch {
    /* 0x00 */ SVECTOR out;
    /* 0x08 */ SVECTOR from;
    /* 0x10 */ SVECTOR local;
    /* 0x18 */ s32     hit;
} Actor401000SightScratch;
STATIC_ASSERT_SIZEOF(Actor401000SightScratch, 0x1C);

typedef struct Actor401000PushScratch {
    /* 0x00 */ SVECTOR offset;
    /* 0x08 */ SVECTOR pos;
    /* 0x10 */ s32     kind;
    /* 0x14 */ s32     len;
    /* 0x18 */ s16     i;
    /* 0x1A */ s16     hit;
    /* 0x1C */ s16     dist[12];
} Actor401000PushScratch;
STATIC_ASSERT_SIZEOF(Actor401000PushScratch, 0x34);

/// 0x10-byte `G_SCRATCH_HEAD` block `func_actor_401000_80134F98` carves off
/// for the offset from the actor to `Player_Status.coordMtx`, the wrapped turn
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
/// for the offset from the actor to `Player_Status.coordMtx`, the wrapped turn
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
/// the offset from the actor to `Player_Status.coordMtx` and the wrapped turn
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

/// Parameter pair `func_actor_401000_80133274` installs as `GpEnemy::param`
/// and reads `hpMax` out of as the actor's initial `field_40`.
extern GpPairSrcE D_actor_401000_8013E09C;

/// Three combat-parameter records `func_actor_401000_80133274` picks between
/// with `Task::spawnArg1 & 0xF`.
extern Actor401000SeedRec D_actor_401000_8013E0AC[3];

/// Animation table `func_actor_401000_80133274` writes to `Task::msgTable`.
extern s32 D_actor_401000_80154F90;

/// Overlay-data word `func_actor_401000_8013922C` points
/// `D_actor_401000_80154E88` at on entering its state.
extern s32  D_actor_401000_80154634;
extern s32* D_actor_401000_80154E88;

/// Message 0x3FF payload of `func_actor_401000_801383F0` and
/// `func_actor_401000_801385B0`: the animation argument the player task reads
/// when the actor's live-actor flag goes up.
extern GpAnimArg D_actor_401000_80154F1C;

/// Query buffer for message 0x3F8 in the grab state; only the last word is
/// seeded before dispatch. Same layout as `Actor356100Msg3F8`.
typedef struct Actor401000Msg3F8 {
    /* 0x00 */ byte pad_0[0x14];
    /* 0x14 */ s32  field_14;
} Actor401000Msg3F8;
STATIC_ASSERT_SIZEOF(Actor401000Msg3F8, 0x18);

extern Actor401000Msg3F8 D_actor_401000_80155038;

/// Animation blocks selected for the grab by the player-character flag.
extern s32 D_actor_401000_80154F00;
extern s32 D_actor_401000_80154F08;
extern s8  D_8007218A;

/// Free-running scroll the actor's forward draw accumulates into:
/// `func_actor_401000_801365C8` adds `field_C04` to it every frame, and the
/// walk state zeroes it on entry. The same slot `Actor01900` keeps in
/// `Actor01900_D172FC`.
extern u16 D_actor_401000_80155000;

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

/// Frame state `func_actor_401000_8013D044` switches its whole tail on: 0, 1
/// and 2 each run their own arm, and the display list is only advanced on the
/// 0 arm. `D_801153F2[1]` is the same pair's second byte, which forces state 6
/// on a state-0x18 actor.
extern u8 D_801153F2[2];

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

/// Handlers defined after the state table and the init that name them.
void func_actor_401000_8013DA78(Task* task);
void func_actor_401000_8013DB10(Task* arg0);
void func_actor_401000_8013DB6C(Task* arg0);
void func_actor_401000_8013DC14(Task* arg0);
void func_actor_401000_8013DCC0(Task* arg0);
void func_actor_401000_8013DD6C(Task* arg0);
void func_actor_401000_8013DE24(Task* arg0);
void func_actor_401000_8013DEC8(Task* arg0);
void func_actor_401000_8013DF6C(Task* arg0);

void func_8004BFF8(s16 angle, MATRIX* matrix);
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s16 arg2, s32 arg3, s32 arg4);

/// Integer part of the last delta `func_actor_401000_801323EC` resolved.
extern SVECTOR D_actor_401000_80155010;

/// Transition table the clip change seeks through: one byte per
/// (playing clip, requested clip) pair, 0x2D requested clips to a row.
extern s8 D_actor_401000_8015465C;

/// Builds `joint`'s absolute rotation in `out`: its own rotation, then each
/// ancestor pre-multiplied in turn (renormalised after every step) up to but
/// not including `stop`. Returns whether the walk reached `stop` rather than
/// the end of the chain.
static __inline__ s32 Actor401000_AccumulateRotation(GsCOORDINATE2* joint, MATRIX* out, GsCOORDINATE2* stop)
{
    MATRIX         matrix;
    GsCOORDINATE2* coord;

    coord = joint->sub;
    *out  = joint->coord;
    while (1) {
        if (coord == NULL) {
            return 0;
        }
        if (coord == stop) {
            return 1;
        }
        gte_SetRotMatrix(&coord->coord);
        MulRotMatrix(out);
        MatrixNormal(out, &matrix);
        *out  = matrix;
        coord = coord->sub;
    }
}

/// Turns the world-space rotation in `rotation` back into one relative to
/// `joint`'s parent: accumulates the chain above the parent up to the view
/// coordinate, transposes it and pre-multiplies. Nothing is done when the
/// parent is the view coordinate itself. Returns `joint`; the caller stores
/// through the returned pointer, which the matched code needs.
static __inline__ GsCOORDINATE2* Actor401000_LocalizeRotation(GsCOORDINATE2* joint, MATRIX* rotation)
{
    MATRIX         matrix;
    MATRIX         normal;
    MATRIX         transposed;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* view;

    coord = joint->sub;
    if (coord != &gGfxViewCoord) {
        view   = &gGfxViewCoord;
        matrix = coord->coord;
        while (1) {
            coord = coord->sub;
            if (coord == NULL) {
                break;
            }
            if (coord == view) {
                __asm__ volatile(
                    "lhu $12, 0(%0);"
                    "lhu $13, 6(%0);"
                    "lhu $14, 12(%0);"
                    "sh $12, 0(%1);"
                    "sh $13, 2(%1);"
                    "sh $14, 4(%1);"
                    "lhu $12, 2(%0);"
                    "lhu $13, 8(%0);"
                    "lhu $14, 14(%0);"
                    "sh $12, 6(%1);"
                    "sh $13, 8(%1);"
                    "sh $14, 10(%1);"
                    "lhu $12, 4(%0);"
                    "lhu $13, 10(%0);"
                    "lhu $14, 16(%0);"
                    "sh $12, 12(%1);"
                    "sh $13, 14(%1);"
                    "sh $14, 16(%1);"
                    : : "r"(&matrix), "r"(&transposed) : "$12", "$13", "$14", "memory");
                gte_SetRotMatrix(&transposed);
                MulRotMatrix(rotation);
                break;
            }
            gte_SetRotMatrix(&coord->coord);
            MulRotMatrix(&matrix);
            MatrixNormal(&matrix, &normal);
            matrix = normal;
        }
    }
    return joint;
}

/// Turns joint `coord` by `yaw` about the world Y axis: builds its world
/// rotation in a matrix carved off the scratchpad head, applies the turn,
/// converts the result back into the parent's frame, writes the 3x3 into the
/// joint and refreshes it. The actor turns two joints of its chain with it, by
/// two thirds and one half of the same clamped angle.
void func_actor_401000_801320E0(GsCOORDINATE2* coord, s16 yaw)
{
    MATRIX*        rotation;
    GsCOORDINATE2* out;

    *(MATRIX**)G_SCRATCH_HEAD -= 1;
    rotation                   = *(MATRIX**)G_SCRATCH_HEAD;
    Actor401000_AccumulateRotation(coord, rotation, &gGfxViewCoord);
    func_8004BFF8(yaw, rotation);
    out = Actor401000_LocalizeRotation(coord, rotation);
    __builtin_memcpy(out->coord.m, rotation->m, sizeof(out->coord.m));
    out->flg = 0;
    Gp_UpdateCoord(out);
    *(MATRIX**)G_SCRATCH_HEAD += 1;
}

/// `Actor401300_MoveForward`: the plain forward-step helper, the shape the
/// state-8 body `func_actor_401000_801388F4` inlines. The `Nonzero` variant
/// below is the same block with an `amount != 0` test and a `gteVec` copy
/// wrapped around it.
static __inline__ void Actor401000_MoveForward(GsCOORDINATE2* coord, s16 amount)
{
    SVECTOR* head;
    SVECTOR* vec;

    if (D_80072729 != 1) {
        head                       = *(SVECTOR**)G_SCRATCH_HEAD;
        vec                        = head - 1;
        *(SVECTOR**)G_SCRATCH_HEAD = vec;
        Gfx_MatrixCol2(&coord->coord, vec);
        VectorNormalSS(vec, vec);
        gte_lddp(amount);
        gte_ldsv(vec);
        gte_gpf12();
        gte_stsv(vec);
        coord->coord.t[0]          += head[-1].vx;
        coord->coord.t[1]          += vec->vy;
        coord->coord.t[2]          += vec->vz;
        coord->flg                  = 0;
        *(SVECTOR**)G_SCRATCH_HEAD += 1;
    }
}

/// `Actor401300_MoveForwardNonzero` and `Actor00100_MoveForwardNonzero`, down
/// to the `head[-1].vx` read-back; the step lands in `coord` directly rather
/// than being reported back through the caller's local.
static __inline__ void Actor401000_MoveForwardNonzero(GsCOORDINATE2* coord, s16 amount)
{
    SVECTOR* head;
    SVECTOR* vec;
    SVECTOR* gteVec;

    if (D_80072729 != 1) {
        head                       = *(SVECTOR**)G_SCRATCH_HEAD;
        vec                        = head - 1;
        *(SVECTOR**)G_SCRATCH_HEAD = vec;
        gteVec                     = vec;
        if (amount != 0) {
            SOFT_TOUCH_REG(vec);
            Gfx_MatrixCol2(&coord->coord, vec);
            VectorNormalSS(vec, vec);
            gte_lddp(amount);
            gte_ldsv(gteVec);
            gte_gpf12();
            gte_stsv(gteVec);
            coord->coord.t[0] += head[-1].vx;
            coord->coord.t[1] += vec->vy;
            coord->coord.t[2] += vec->vz;
            coord->flg         = 0;
        }
        *(SVECTOR**)G_SCRATCH_HEAD += 1;
    }
}

/// Moves `coord` in X and Z by the delta `func_800E0C10` resolves from the
/// first `count` records of `recs`, and keeps the integer part of the full
/// delta in `D_actor_401000_80155010`. A nonzero fractional part rounds both
/// the coordinate and the kept step one unit away from zero. Returns 1 when the
/// X or Z delta is nonzero.
s32 func_actor_401000_801323EC(GsCOORDINATE2* coord, GpRec18* recs, s16 count)
{
    void**                scratch;
    u8*                   head;
    Actor401000DeltaFlag* s;
    register void*        p asm("v1");
    s32                   val;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    p        = head - 0x14;
    s        = p;
    *scratch = p;
    s->moved = 0;
    if (func_800E0C10(recs, &s->delta, (s32)count, NULL) != 0) {
        coord->coord.t[0]          = coord->coord.t[0] + ((Actor401000DeltaFlag*)(head - 0x14))->delta.vx.h.hi;
        coord->coord.t[2]          = coord->coord.t[2] + s->delta.vz.h.hi;
        D_actor_401000_80155010.vx = ((Actor401000DeltaFlag*)(head - 0x14))->delta.vx.w >> 16;
        D_actor_401000_80155010.vy = s->delta.vy.w >> 16;
        D_actor_401000_80155010.vz = s->delta.vz.w >> 16;
        val                        = ((Actor401000DeltaFlag*)(head - 0x14))->delta.vx.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[0]++;
                D_actor_401000_80155010.vx++;
            } else {
                coord->coord.t[0]--;
                D_actor_401000_80155010.vx--;
            }
        }
        val = s->delta.vz.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[2]++;
                D_actor_401000_80155010.vz++;
            } else {
                coord->coord.t[2]--;
                D_actor_401000_80155010.vz--;
            }
        }
    }
    if (s->delta.vx.w != 0 || s->delta.vz.w != 0) {
        s->moved = 1;
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x14;
    return s->moved;
}

/// Tests whether stepping `coord` forward by `step` keeps the player out of
/// reach. The turn from the actor's facing to the player decides first: for a
/// forward step the player must be within 0x400 of dead ahead, for a backward
/// one outside it, or the answer is 1 at once. Otherwise the root is moved
/// `step` along its facing and the result is whether the player's root is at
/// least `range` + 0x96 away from that point.
s32 func_actor_401000_80132590(GsCOORDINATE2* coord, s16 range, s16 step)
{
    SVECTOR  v;
    SVECTOR  d;
    VECTOR   e;
    Task*    player;
    s16      angle;
    SVECTOR* pv;
    s32      x;

    player = gameGetPtrSlot(3);
    d.vx   = ((GpCoordXZ*)((TmdObject*)player->extra)->coords)->field_18 - ((GpCoordXZ*)coord)->field_18;
    d.vy   = (u16)((TmdObject*)player->extra)->coords->coord.t[1] - (u16)coord->coord.t[1];
    d.vz   = ((GpCoordXZ*)((TmdObject*)player->extra)->coords)->field_20 - ((GpCoordXZ*)coord)->field_20;
    angle  = ratan2(d.vx, d.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    if (angle < 0) {
    loop_neg:
        if (angle < -0x800) {
            angle += 0x1000;
            goto loop_neg;
        }
    } else {
    loop_pos:
        if (angle > 0x800) {
            angle -= 0x1000;
            goto loop_pos;
        }
    }
    x = angle << 16;
    if (step >= 0) {
        if (abs(x >> 16) > 0x400) {
            return 1;
        }
    } else {
        if (abs(x >> 16) < 0x400) {
            return 1;
        }
    }
    Gfx_MatrixCol2(&coord->coord, &v);
    pv = &v;
    VectorNormalSS(pv, pv);
    gte_lddp(step);
    gte_ldsv(pv);
    gte_gpf12();
    gte_stsv(pv);
    v.vx += (u16)coord->coord.t[0];
    v.vy += (u16)coord->coord.t[1];
    v.vz += (u16)coord->coord.t[2];
    e.vx  = ((TmdObject*)player->extra)->coords->coord.t[0] - v.vx;
    e.vy  = ((TmdObject*)player->extra)->coords->coord.t[1] - v.vy;
    e.vz  = ((TmdObject*)player->extra)->coords->coord.t[2] - v.vz;
    return SquareRoot0(e.vx * e.vx + e.vy * e.vy + e.vz * e.vz) >= range + 0x96;
}

/// Rotates the player's and this actor's root positions, each raised by 1000,
/// into world space and returns `func_800E0308` on the pair.
s32 func_actor_401000_80132824(Task* arg0)
{
    Task*                    player;
    u8*                      head;
    Actor401000SightScratch* s;
    SVECTOR*                 local;
    SVECTOR*                 v;
    SVECTOR*                 out;

    player                = gameGetPtrSlot(3);
    head                  = *(u8**)G_SCRATCH_HEAD;
    local                 = (SVECTOR*)(head - 0xC);
    s                     = (Actor401000SightScratch*)(head - 0x1C);
    s->local.vx           = ((TmdObject*)player->extra)->coords->coord.t[0];
    s->local.vy           = ((TmdObject*)player->extra)->coords->coord.t[1] - 1000;
    *(u8**)G_SCRATCH_HEAD = (u8*)s;
    s->local.vz           = ((TmdObject*)player->extra)->coords->coord.t[2];
    Gp_UpdateCoord(&gGfxViewCoord);
    v = local;
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(v);
    gte_rtv0();
    gte_stsv(&s->out);
    s->out.vx += gGfxViewCoord.workm.t[0];
    s->out.vy += gGfxViewCoord.workm.t[1];
    s->out.vz += gGfxViewCoord.workm.t[2];

    s->local.vx = ((TmdObject*)arg0->extra)->coords->coord.t[0];
    s->local.vy = ((TmdObject*)arg0->extra)->coords->coord.t[1] - 1000;
    s->local.vz = ((TmdObject*)arg0->extra)->coords->coord.t[2];
    Gp_UpdateCoord(&gGfxViewCoord);
    out = (SVECTOR*)(head - 0x14);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(v);
    gte_rtv0();
    gte_stsv(out);
    s->from.vx           += gGfxViewCoord.workm.t[0];
    s->from.vy           += gGfxViewCoord.workm.t[1];
    s->from.vz           += gGfxViewCoord.workm.t[2];
    s->hit                = func_800E0308(&s->out, out);
    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + 0x1C;
    return s->hit;
}

/// Ticks the body slots while a blend clip is active: slots 1-10 take the body
/// pose and the blend pose and write their mix, weighted by `field_8AC`, into
/// the model; slots 11-18 just advance the body clip. The body slots run three
/// sixteenths slower than `field_8A2`, the blend slots at `field_8AA`.
void func_actor_401000_80132A84(Task* arg0)
{
    GpAnimPose       pose;
    GpAnimPose       blendPose;
    GpAnimCtx*       anim;
    s16              weight;
    s16              i;
    Actor401000Work* work;

    work   = arg0->work;
    weight = work->field_8AC;
    anim   = &((Actor401000AnimWork*)work)->anim;
    for (i = 1; i < 0x13; i++) {
        if (i < 0xB) {
            ((Actor401000AnimWork*)work)->blendSlots[i].rate = (u8)work->field_8AA;
            ((Actor401000AnimWork*)work)->slots[i].rate      = (u8)(work->field_8A2 - 3);
            func_800B3448(anim, i, (s32)&pose, 0);
            func_800B3448(&((Actor401000AnimWork*)work)->blendAnim, i, (s32)&blendPose, 0);
            Gp_AnimWritePoseCopy(anim, i, &pose, &blendPose, weight, 0x1000 - weight);
        } else {
            ((Actor401000AnimWork*)work)->slots[i].rate = (u8)(work->field_8A2 - 3);
            Gp_AnimTickIndex(&((Actor401000AnimWork*)work)->anim, i);
        }
    }
}

/// Returns the sound event the current clip (`field_89E`) has reached at its
/// frame (`field_5A`), once per frame: the frame is latched in `field_8B4`, and
/// a frame already latched, or one that carries no event, returns 0.
s32 func_actor_401000_80132BB0(Actor401000Work* work)
{
    s32 id;
    s32 prev;

    switch (work->field_89E) {
        case 20:
        case 21:
            id = work->field_5A & 0x3FF;
            if (id == 7) {
                if (work->field_8B4 != id) {
                    work->field_8B4 = id;
                    return 0x400A0010;
                }
                work->field_8B4 = id;
            } else if (id == 0x10) {
                prev = work->field_8B4;
                if (prev != id) {
                    work->field_8B4 = id;
                    return 0x400A0011;
                }
                work->field_8B4 = prev;
            } else {
                work->field_8B4 = 0;
            }
            break;
        case 3:
            id = work->field_5A & 0x3FF;
            if (id == 0x1A) {
                if (work->field_8B4 != id) {
                    work->field_8B4 = id;
                    return 0x400A0004;
                }
                work->field_8B4 = id;
            } else if (id == 0x13) {
                prev = work->field_8B4;
                if (prev != id) {
                    work->field_8B4 = id;
                    return 0x400A0003;
                }
                work->field_8B4 = prev;
            } else {
                work->field_8B4 = 0;
            }
            break;
        case 2:
            id = work->field_5A & 0x3FF;
            if (id == 0x11) {
                if (work->field_8B4 != id) {
                    work->field_8B4 = id;
                    return 0x400A0002;
                }
                work->field_8B4 = id;
            } else if (id == 0x1B) {
                prev = work->field_8B4;
                if (prev != id) {
                    work->field_8B4 = id;
                    return 0x400A0001;
                }
                work->field_8B4 = prev;
            } else {
                work->field_8B4 = 0;
            }
            break;
        case 9:
            id = work->field_5A & 0x3FF;
            if (id == 4 && work->field_8B4 != id) {
                work->field_8B4 = id;
                return 0x400A0006;
            }
            prev            = work->field_5A & 0x3FF;
            work->field_8B4 = prev;
            break;
        case 11:
            id = work->field_5A & 0x3FF;
            if (id == 4 && work->field_8B4 != id) {
                work->field_8B4 = id;
                return 0x400A0005;
            }
            prev            = work->field_5A & 0x3FF;
            work->field_8B4 = prev;
            break;
        case 12:
            id = work->field_5A & 0x3FF;
            if (id == 7 && work->field_8B4 != id) {
                work->field_8B4 = id;
                return 0x400A0005;
            }
            prev            = work->field_5A & 0x3FF;
            work->field_8B4 = prev;
            break;
        case 4:
            id = work->field_5A & 0x3FF;
            if (id == 0xA) {
                if (work->field_8B4 != id) {
                    work->field_8B4 = id;
                    return 0x400A0004;
                }
            }
            id = work->field_5A & 0x3FF;
            if (id == 0x12) {
                if (work->field_8B4 != id) {
                    work->field_8B4 = id;
                    return 0x400A0002;
                }
            }
            prev            = work->field_5A & 0x3FF;
            work->field_8B4 = prev;
            break;
        case 5:
            id = work->field_5A & 0x3FF;
            if (id == 9 && work->field_8B4 != id) {
                work->field_8B4 = id;
                return 0x400A000D;
            }
            prev            = work->field_5A & 0x3FF;
            work->field_8B4 = prev;
            break;
        case 7:
            id = work->field_5A & 0x3FF;
            if (id == 0x16 && work->field_8B4 != id) {
                work->field_8B4 = id;
                return 0x400A0003;
            }
            prev            = work->field_5A & 0x3FF;
            work->field_8B4 = prev;
            break;
        case 6:
            id = work->field_5A & 0x3FF;
            if (id == 9) {
                if (work->field_8B4 != id) {
                    work->field_8B4 = id;
                    return 0x400A000D;
                }
            }
            id = work->field_5A & 0x3FF;
            if (id == 0x13) {
                if (work->field_8B4 != id) {
                    work->field_8B4 = id;
                    return 0x400A000C;
                }
            }
            prev            = work->field_5A & 0x3FF;
            work->field_8B4 = prev;
            break;
    }
    return 0;
}

/// The actor's per-frame animation driver. A pending clip change is applied
/// first: `field_898` 1 re-seeks every body slot from the playing clip to the
/// requested one through the transition table, 2 restarts them on the
/// requested clip, and either way the change is marked done (3). A requested
/// blend clip (`field_8A6` 2) restarts the blend slots. The body slots then
/// advance, mixed with the blend pose while `field_89A` is set, which ends once
/// the blend clip's first slot has run out. The head yaw `field_8B0` eases
/// toward `field_8AE` by at most 0x100 a frame and turns two joints of the
/// chain by two thirds and one half of it, clamped to 0x400 either way. Last,
/// the sound event the animation has reached is queued at the model's pan and
/// depth.
void func_actor_401000_80132EF0(Task* arg0)
{
    Actor401000Work* seekWork;
    Actor401000Work* resetWork;
    Actor401000Work* secondaryWork;
    Actor401000Work* tickWork;
    Actor401000Work* work;
    GpEnemy*         enemy;
    s32              animation;
    s32              index;
    u32              table;
    s16              state;
    s32              seekIndex;
    s32              resetIndex;
    s32              secondaryIndex;
    s32              tickIndex;
    s32              seekSlotIndex;
    s32              resetSlotIndex;
    s32              secondarySlotIndex;
    s32              tickSlotIndex;
    s32              targetAngle;
    s32              currentAngle;
    s32              targetAngleBits;
    s32              currentAngleBits;
    s16              angle;
    s32              clampedAngle;
    s16              signedTurn;
    s32              sound;
    s32              soundId;
    s32              pan;
    s8*              seekSlot;
    s8*              resetSlot;
    s8*              secondarySlot;
    s8*              tickSlot;

    work  = arg0->work;
    enemy = arg0->spawnArg2;
    state = work->field_898;
    if (state == 1) {
        // Keep the copy before the comparison so it fills the branch delay slot.
        seekWork = arg0->work;
        if (work->field_89C != (s16)work->field_89E) {
            seekIndex = 1;
            table     = (u32)&D_actor_401000_8015465C;
            // Slot i has stride 0x28; its rate is at work + 0x39 + i * 0x28.
            seekSlot = ((s8*)work + 0x28);
            do {
                seekSlotIndex  = seekIndex;
                seekSlot[0x39] = (u8)seekWork->field_8A2;
                animation      = (s16)seekWork->field_89E;
                seekSlot      += 0x28;
                index          = seekWork->field_89C * 0x2D;
                func_800B4114(&((Actor401000AnimWork*)seekWork)->anim, seekSlotIndex, animation, 0,
                              (s32) * (s8*)((animation + index) + table));
                seekIndex += 1;
            } while (seekIndex < 0x13);
            seekWork->field_89C = (s16)seekWork->field_89E;
        }
        work->field_898 = 3;
        work->field_8A0 = 0;
        work->field_8B4 = 0;
    } else if (state == 2) {
        resetWork = work;
        // Preserve the separate work pointer for the reset loop.
        TOUCH_REG(resetWork);
        resetIndex = 1;
        resetSlot  = ((s8*)work + 0x28);
        do {
            resetSlotIndex  = resetIndex;
            resetSlot[0x39] = (u8)resetWork->field_8A2;
            resetSlot      += 0x28;
            Gp_AnimResetSlot(&((Actor401000AnimWork*)resetWork)->anim, resetSlotIndex,
                             (s32)(s16)resetWork->field_89E);
            resetIndex += 1;
        } while (resetIndex < 0x13);
        resetWork->field_89C = (s16)resetWork->field_89E;
        work->field_898      = 3;
        work->field_8A0      = 0;
        work->field_8B4      = 0;
    }
    if (work->field_8A6 == 2) {
        secondaryWork            = arg0->work;
        secondaryIndex           = 1;
        secondarySlot            = ((s8*)secondaryWork + 0x28);
        secondaryWork->field_8AA = 0x30;
        secondaryWork->field_8AC = 0x800;
        do {
            secondarySlotIndex  = secondaryIndex;
            secondarySlot[0x39] = (u8)secondaryWork->field_8AA;
            secondarySlot      += 0x28;
            Gp_AnimResetSlot(&((Actor401000AnimWork*)secondaryWork)->blendAnim, secondarySlotIndex,
                             (s32)secondaryWork->field_8A8);
            secondaryIndex += 1;
        } while (secondaryIndex < 0x13);
        work->field_8A6 = 3;
    }
    work->field_8A0 = (u16)(work->field_8A0 + 1);
    if ((s16)work->field_89A == 0) {
        tickWork  = arg0->work;
        tickIndex = 1;
        tickSlot  = ((s8*)tickWork + 0x28);
        do {
            tickSlotIndex  = tickIndex;
            tickSlot[0x39] = (u8)tickWork->field_8A2;
            Gp_AnimTickIndex(&((Actor401000AnimWork*)tickWork)->anim, tickSlotIndex);
            tickSlot  += 0x28;
            tickIndex += 1;
        } while (tickIndex < 0x13);
    } else {
        func_actor_401000_80132A84(arg0);
        if (((Actor401000AnimWork*)work)->blendSlots[1].flags & 1) {
            work->field_89A = 0;
        }
    }
    targetAngle      = (s16)work->field_8AE;
    currentAngle     = (s16)work->field_8B0;
    targetAngleBits  = (u16)work->field_8AE;
    currentAngleBits = (u16)work->field_8B0;
    if (currentAngle < targetAngle) {
        if ((targetAngle - currentAngle) >= 0x101) {
            work->field_8B0 = currentAngleBits + 0x100;
        } else {
            goto atTargetAngle;
        }
    } else if ((currentAngle - targetAngle) >= 0x101) {
        work->field_8B0 = currentAngleBits - 0x100;
    } else {
    atTargetAngle:
        work->field_8B0 = targetAngleBits;
    }
    angle        = (s16)work->field_8B0;
    clampedAngle = (u16)work->field_8B0;
    if (angle != 0) {
        if (angle >= 0x401) {
            clampedAngle = 0x400;
        }
        if (angle < -0x400) {
            clampedAngle = -0x400;
        }
        signedTurn = (s16)clampedAngle * 2 / 3;
        func_actor_401000_801320E0(&((TmdObject*)arg0->extra)->coords[5], signedTurn);
        func_actor_401000_801320E0(&((TmdObject*)arg0->extra)->coords[2], (s16)clampedAngle / 2);
        ((TmdObject*)arg0->extra)->coords[5].flg = 0;
        ((TmdObject*)arg0->extra)->coords[4].flg = 0;
        ((TmdObject*)arg0->extra)->coords[3].flg = 0;
        ((TmdObject*)arg0->extra)->coords[2].flg = 0;
    }
    sound = func_actor_401000_80132BB0(work);
    if (sound != 0) {
        soundId = sound | (((u16)enemy->placeKey >> 0xC) << 8);
        pan     = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(soundId, (s32)pan,
                            (s32)(s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
    }
}

/// Points the model's light and color matrices at the work block's copies.
static __inline__ void Actor401000_BindMatrices(Task* actor)
{
    Actor401000Work* work;
    TmdObject*       obj;

    work          = actor->work;
    obj           = actor->extra;
    obj->lightMtx = &work->field_B88;
    obj->colorMtx = &work->field_BA8;
}

/// Rebuilds the root coordinate's scaled Y rotation and drops both obstacle
/// tables while the rotation scratch block is still held.
static __inline__ void Actor401000_InitPose(GsCOORDINATE2* coord, Actor401000Work* work)
{
    void*                  scratch_base;
    u8*                    head;
    u8*                    tail;
    Actor401000RotScratch* blk;
    GpRec18*               rec;
    s16                    ang;
    u16                    m22;

    scratch_base                             = PSX_SCRATCH;
    head                                     = scratch_base;
    head                                     = *(u8**)(head + 0x3FC);
    blk                                      = (Actor401000RotScratch*)(head - 0x34);
    *(Actor401000RotScratch**)G_SCRATCH_HEAD = blk;
    ang                                      = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    blk->angle                               = ang;
    Gfx_RotMatrixY(&blk->m, (s32)ang, 1);
    blk->scale.vz = 0x1194;
    blk->scale.vy = 0x1194;
    blk->scale.vx = 0x1194;
    ScaleMatrix(&blk->m, &blk->scale);
    coord->coord.m[0][0] = *(u16*)&((Actor401000RotScratch*)(head - 0x34))->m.m[0][0];
    coord->coord.m[0][1] = *(u16*)&blk->m.m[0][1];
    coord->coord.m[0][2] = *(u16*)&blk->m.m[0][2];
    coord->coord.m[1][0] = *(u16*)&blk->m.m[1][0];
    coord->coord.m[1][1] = *(u16*)&blk->m.m[1][1];
    coord->coord.m[1][2] = *(u16*)&blk->m.m[1][2];
    coord->coord.m[2][0] = *(u16*)&blk->m.m[2][0];
    coord->coord.m[2][1] = *(u16*)&blk->m.m[2][1];
    __asm__ volatile("lui %0, 0x1F80" : "=r"(tail));
    tail       = *(u8**)(tail + 0x3FC);
    m22        = *(u16*)&blk->m.m[2][2];
    rec        = work->field_A30;
    coord->flg = 0;
    tail       = tail + 0x34;
    __asm__ volatile("sw %0, 0x1F8003FC" ::"r"(tail) : "memory");
    coord->coord.m[2][2] = m22;
    work->field_C7C      = 0;
    Gp_ClearRec18Occupied(rec);
    Gp_ClearRec18Occupied(work->field_8F0);
}

/// Enemy init: allocates the 0xC80-byte work block, binds the model's light
/// and colour matrices to its copies, seeds both animation contexts and the
/// three `GpObj` nodes, then picks the opening clip from the low bits of
/// `GpEnemy::placeKey` and the `field_C10` parameter run from the spawn flags.
/// The tail rebuilds the root coordinate through `Actor401000_InitPose`.
void func_actor_401000_80133274(GpEnemy* enemy, Task* actor)
{
    SVECTOR          dir;
    VECTOR           pos;
    SVECTOR*         v;
    TmdObject*       obj;
    GsCOORDINATE2*   root;
    Actor401000Work* work;
    GpObj*           body;
    GpObj*           head;
    u16              kind;
    s16              clip;
    s32              variant;

    root        = ((TmdObject*)actor->extra)->coords;
    obj         = actor->extra;
    work        = memCalloc(0xC80, 0);
    actor->work = work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, actor);
        return;
    }
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    actor->exitCallback = func_actor_401000_8013DA78;
    Actor401000_BindMatrices(actor);
    enemy->field_4    = &((TmdObject*)actor->extra)->coords->coord;
    enemy->field_48   = 0;
    enemy->bodyPos.vx = 0;
    enemy->bodyPos.vy = 0;
    enemy->bodyPos.vz = 0;
    enemy->coord      = &((TmdObject*)actor->extra)->coords[2];
    Gp_LinkNode(&enemy->node);
    enemy->node.flags    = 1;
    enemy->reactionFlags = 0;
    enemy->hp            = (s16)D_actor_401000_8013E09C.hpMax;
    enemy->param         = &D_actor_401000_8013E09C;
    enemy->recs          = work->field_8F0;
    func_800B3F84(&((Actor401000AnimWork*)work)->anim, &D_actor_401000_80154E48, obj,
                  ((Actor401000AnimWork*)work)->pad_328, ((Actor401000AnimWork*)work)->slots);
    func_800B3F84(&((Actor401000AnimWork*)work)->blendAnim, &D_actor_401000_80154E48,
                  obj, ((Actor401000AnimWork*)work)->pad_764,
                  ((Actor401000AnimWork*)work)->blendSlots);
    work->field_898 = 2;
    work->field_89E = 2;
    work->field_89A = 0;
    work->field_8B0 = 0;
    work->field_8AE = 0;
    work->field_8A4 = 0x10;
    work->field_8A2 = 0x10;
    kind            = ((u16)enemy->placeKey >> 12) % 5;
    switch (kind) {
        case 0:
            clip = 0x11;
            break;
        case 1:
            clip = 0xF;
            break;
        case 2:
            clip = 0x10;
            break;
        case 3:
            clip = 0x12;
            break;
        case 4:
        default:
            clip = 0xE;
            break;
    }
    work->field_8A4 = clip;
    SCHED_BARRIER();
    func_actor_401000_80132EF0(actor);

    work->field_A10.ctx.recs = work->field_A30;
    work->field_A10.coord    = root;
    work->field_A10.pos.vx   = 0;
    work->field_A10.pos.vy   = -0xAC;
    work->field_A10.pos.vz   = 0;
    work->field_A10.key      = 0x30000;
    work->field_A10.radius   = 0x12C;
    work->field_A10.flags    = 1;
    Gp_LinkObj(2, &work->field_A10);
    work->field_BE8        = 0;
    work->field_A10.flags |= 0x4000;
    Gp_InitRec18Table(work->field_A10.ctx.recs, 0xC, 0);

    body           = &work->field_8D0;
    body->coord    = &((TmdObject*)actor->extra)->coords[2];
    body->ctx.recs = work->field_8F0;
    body->pos.vx   = 0;
    body->pos.vy   = 0;
    body->pos.vz   = 0;
    body->key      = 0x3000A;
    body->radius   = 0x1AE;
    body->flags    = 1;
    Gp_LinkObj(2, body);
    body->flags |= 0x8000;
    Gp_InitRec18Table(body->ctx.recs, 0xC, 0);
    work->field_8D0.key = 0x30000;

    dir.vx         = 0;
    dir.vy         = 0;
    dir.vz         = 0;
    head           = &work->field_B50;
    head->coord    = &((TmdObject*)actor->extra)->coords[6];
    head->ctx.recs = &work->field_B70;
    v              = &dir;
    head->pos.vx   = v->vx;
    head->pos.vy   = v->vy;
    head->pos.vz   = v->vz;
    head->radius   = 0x180;
    head->flags    = 1;
    Gp_LinkObj(3, head);
    Gp_InitRec18Table(head->ctx.recs, 1, 0);

    work->field_14     = 0;
    work->field_C[0].x = ((TmdObject*)actor->extra)->coords->coord.t[0];
    work->field_C[0].z = ((TmdObject*)actor->extra)->coords->coord.t[2];
    Gfx_MatrixCol2(&((TmdObject*)actor->extra)->coords->coord, v);
    dir.vy = 0;
    VectorNormalSS(v, v);
    gte_lddp(2000);
    gte_ldsv(v);
    gte_gpf12();
    gte_stsv(v);
    work->field_C[1].x = ((TmdObject*)actor->extra)->coords->coord.t[0] + dir.vx;
    work->field_C[1].z = ((TmdObject*)actor->extra)->coords->coord.t[2] + dir.vz;

    actor->msgTable = &D_actor_401000_80154F90;
    root->sub       = &gGfxViewCoord;
    root->flg       = 0;
    Gp_UpdateCoord(root);
    pos.vx = root->workm.t[0];
    pos.vy = root->workm.t[1];
    pos.vz = root->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);

    work->field_8B8.coord      = &((TmdObject*)actor->extra)->coords[1];
    work->field_8B8.spawnArgLo = 0x300;
    work->field_8B8.spawnArgHi = 2;
    variant                    = (actor->spawnArg1 >> 16);
    switch (variant & 0xF) {
        case 2:
            work->field_2 = -1;
            work->field_0 = 0;
            break;
        case 4:
            work->field_2 = -1;
            work->field_0 = 0x16;
            break;
        default:
            work->field_2 = -1;
            work->field_0 = 0x18;
            Tmd_AllocBuffers(obj);
            break;
    }
    switch (actor->spawnArg1 & 0xF) {
        case 2:
            work->field_C10 = D_actor_401000_8013E0AC[0].field_0;
            work->field_C12 = D_actor_401000_8013E0AC[0].field_2;
            work->field_C14 = D_actor_401000_8013E0AC[0].field_4;
            work->field_C16 = D_actor_401000_8013E0AC[0].field_6;
            break;
        case 1:
            work->field_C10 = D_actor_401000_8013E0AC[2].field_0;
            work->field_C12 = D_actor_401000_8013E0AC[2].field_2;
            work->field_C14 = D_actor_401000_8013E0AC[2].field_4;
            work->field_C16 = D_actor_401000_8013E0AC[2].field_6;
            break;
        case 0:
        default:
            work->field_C10 = D_actor_401000_8013E0AC[1].field_0;
            work->field_C12 = D_actor_401000_8013E0AC[1].field_2;
            work->field_C14 = D_actor_401000_8013E0AC[1].field_4;
            work->field_C16 = D_actor_401000_8013E0AC[1].field_6;
            break;
    }

    Actor401000_InitPose(((TmdObject*)actor->extra)->coords, work);

    actor->state++;
}

/// Spawn the effect a hit record `arg2` names at one of twelve model offsets
/// picked by the signed damage `arg1`: the `Gp_LcgState` draw's low bits
/// bucket `|arg1|` into below 0x200 / above 0x600 / positive / non-positive,
/// each selecting from its own run of `D_actor_401000_80154F30`. The chosen
/// offset goes into the work block's `field_8C0` and the `field_8B8` argument
/// record, which anchors it at the model's second coordinate part, scale
/// 0x300 and count 2 — the effect `func_800FDB18` then spawns hangs off the
/// part the vector's `pad` names. The 8-byte scratch the offset is built in is
/// carved off and given back around the call. Same body as
/// `Actor00100_Fn03340`, which keeps its record inline and scales by 0x100.
void func_actor_401000_80133940(Task* arg0, s16 arg1, s32 arg2)
{
    SVECTOR*         sc;
    s32              mag;
    Actor401000Work* work;

    sc   = (SVECTOR*)(*(u32*)G_SCRATCH_HEAD -= 8);
    mag  = (arg1 >= 0) ? arg1 : -arg1;
    work = arg0->work;
    if (mag < 0x200) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        switch ((s32)(Gp_LcgState >> 16) & 3) {
            case 0:
                *sc = D_actor_401000_80154F30[0];
                break;
            case 1:
                *sc = D_actor_401000_80154F30[1];
                break;
            case 2:
                *sc = D_actor_401000_80154F30[2];
                break;
            case 3:
                *sc = D_actor_401000_80154F30[3];
                break;
            default:
                *sc = D_actor_401000_80154F30[4];
                break;
        }
    } else if (mag > 0x600) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        switch ((s32)(Gp_LcgState >> 16) & 2) {
            case 0:
                *sc = D_actor_401000_80154F30[5];
                break;
            case 1:
                *sc = D_actor_401000_80154F30[6];
                break;
            default:
                *sc = D_actor_401000_80154F30[7];
                break;
        }
    } else if (arg1 > 0) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if ((Gp_LcgState >> 16) & 1) {
            *sc = D_actor_401000_80154F30[8];
        } else {
            *sc = D_actor_401000_80154F30[9];
        }
    } else {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if ((Gp_LcgState >> 16) & 1) {
            *sc = D_actor_401000_80154F30[10];
        } else {
            *sc = D_actor_401000_80154F30[11];
        }
    }
    work->field_8B8.coord      = &((TmdObject*)arg0->extra)->coords[1];
    work->field_8B8.spawnArgLo = 0x300;
    work->field_8B8.spawnArgHi = 2;
    work->field_8C0            = *sc;
    func_800FDB18(Gp_GetIdParam1(arg2) & 0xFFFF, &((TmdObject*)arg0->extra)->coords[sc->pad], &work->field_8C0, &work->field_8B8);
    *(u32*)G_SCRATCH_HEAD += 8;
}

/// Wrap `input` into the -0x800..0x800 turn range. Same body as
/// `Actor01900_NormalizeYaw` / `Actor401300_NormalizeYaw`.
static __inline__ s16 Actor401000_NormalizeYaw(s16 input)
{
    s16 value = input;

    if (input < 0) {
        while (1) {
            if (value >= -0x800) {
                break;
            }
            value += 0x1000;
        }
    } else {
        while (1) {
            if (value <= 0x800) {
                break;
            }
            value -= 0x1000;
        }
    }
    return value;
}

static __inline__ s32 Actor401000_FindHit(SVECTOR* pos, GpRec18* records)
{
    s16 i;

    for (i = 0; i < 12; i++) {
        if (!records[i].key)
            break;
        if ((records[i].key & 0xFFFF0000) == 0x20000) {
            pos->vx = records[i].point.vx;
            pos->vy = records[i].point.vy;
            pos->vz = records[i].point.vz;
            return records[i].key;
        }
    }
    return 0;
}

void func_actor_401000_80133D50(Task* arg0)
{
    PlayerStatus*          config = &Player_Status;
    Actor401000Work*       work;
    GpEnemy*               enemy;
    Actor401000HitScratch* head;
    Actor401000HitScratch* s;
    GsCOORDINATE2*         coord;
    Task*                  player;
    SVECTOR*               dir;
    s16                    z;
    s32                    yaw;
    s32                    dx;
    s32                    dy;
    s32                    dz;
    s32                    deathSound;
    s32                    deathPan;
    s32                    hitSound;
    s32                    hitPan;
    s32                    mag;
    s16                    state;
    s16                    effect;
    s16                    timer;
    u32                    damage;

    enemy = arg0->spawnArg2;
    work  = arg0->work;
    if (enemy->hp > 0) {
        head  = *(Actor401000HitScratch**)G_SCRATCH_HEAD;
        s     = (*(Actor401000HitScratch**)G_SCRATCH_HEAD = head - 1);
        s->id = Actor401000_FindHit(&head[-1].hitPos, work->field_8F0);
        if (s->id != 0) {
            if (s->id & 0x8000) {
                player       = gameGetPtrSlot(3);
                s->hitPos.vx = ((TmdObject*)player->extra)->coords->workm.t[0];
                s->hitPos.vy = ((TmdObject*)player->extra)->coords->workm.t[1];
                s->hitPos.vz = ((TmdObject*)player->extra)->coords->workm.t[2];
            }
            if (work->field_C28 == 1) {
                Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F1, 0, 0);
                work->field_C28 = 0;
                if (work->field_0 == 0xB || work->field_0 == 0xC || work->field_0 == 0xD || work->field_0 == 0xE) {
                    work->field_0 = 0x13;
                }
            }
            work->field_C24                        = 0;
            work->field_C26                        = 0;
            ((TmdObject*)arg0->extra)->coords->flg = 0;
            Gp_UpdateCoord(((TmdObject*)arg0->extra)->coords);
            s->dir.vx = ((TmdObject*)arg0->extra)->coords->workm.t[0];
            s->dir.vy = ((TmdObject*)arg0->extra)->coords->workm.t[1];
            s->dir.vz = ((TmdObject*)arg0->extra)->coords->workm.t[2];
            s->dir.vx = s->hitPos.vx - ((TmdObject*)arg0->extra)->coords->workm.t[0];
            s->dir.vy = s->hitPos.vy - ((TmdObject*)arg0->extra)->coords->workm.t[1];
            z         = s->hitPos.vz - ((TmdObject*)arg0->extra)->coords->workm.t[2];
            s->dir.vz = z;
            yaw       = ratan2(s->dir.vx, z);
            coord     = ((TmdObject*)arg0->extra)->coords;
            s->yaw    = yaw - ratan2(-coord->workm.m[2][0], coord->workm.m[2][2]);
            s->yaw    = Actor401000_NormalizeYaw(s->yaw);
            func_actor_401000_80133940(arg0, s->yaw, s->id);
            work->field_8B0 = 0;
            work->field_8AE = 0;
            s->effect       = -1;
            state           = work->field_0;
            if (state != 0x13 && state != 0x14 && state != 0x11 && state != 0x1F && state != 0x20 && state != 0xF && state != 0x10 && state != 4) {
                s->m = ((TmdObject*)arg0->extra)->coords->coord;
                Gfx_RotMatrixY(&s->m, s->yaw, 0);
                dir = &s->dir;
                Gfx_MatrixCol2(&s->m, dir);
                VectorNormalSS(dir, dir);
                if (work->field_BEC > 0) {
                    gte_lddp(-0x19);
                    gte_ldsv(dir);
                    gte_gpf12();
                    gte_stsv(dir);
                } else {
                    gte_lddp(-0x64);
                    gte_ldsv(dir);
                    gte_gpf12();
                    gte_stsv(dir);
                }
                ((TmdObject*)arg0->extra)->coords->coord.t[0] += s->dir.vx;
                ((TmdObject*)arg0->extra)->coords->coord.t[1] += s->dir.vy;
                ((TmdObject*)arg0->extra)->coords->coord.t[2] += s->dir.vz;
                ((TmdObject*)arg0->extra)->coords->flg         = 0;
            }
            dx        = config->coordMtx->t[0] - ((TmdObject*)arg0->extra)->coords->coord.t[0];
            s->dx     = dx;
            dy        = config->coordMtx->t[1] - ((TmdObject*)arg0->extra)->coords->coord.t[1];
            s->dy     = dy;
            dz        = config->coordMtx->t[2] - ((TmdObject*)arg0->extra)->coords->coord.t[2];
            s->dz     = dz;
            s->dist   = SquareRoot0(dx * dx + dy * dy + dz * dz);
            s->damage = Gp_ComputeDamage(s->id, s->dist, 0, 0);
            if (Gp_RollEnemyChance(enemy, s->id, 0) != 0) {
                s->crit    = 1;
                s->effect  = 0;
                s->damage *= 4;
            } else {
                s->crit = 0;
            }
            mag = s->yaw;
            if (mag < 0) {
                mag = -mag;
            }
            if (mag >= 0x501) {
                state = work->field_0;
                if (state != 0x13) {
                    if (state != 0x14 && state != 0x11 && state != 0x1F && state != 0x20 && state != 0xF && state != 0x10 && state != 4) {
                        damage    = s->damage * 2;
                        s->damage = damage;
                        if (damage != 0) {
                            s->effect = 4;
                        }
                    }
                }
            }
            func_800E2C78(enemy, s->id, s->damage, 0);
            enemy->hp -= s->damage;
            func_800DA6E8(&enemy->node, s->damage, 0);
            work->field_BEA += s->damage;
            effect           = s->effect;
            if (effect != -1) {
                Gp_SpawnEff(0x6009C, &((TmdObject*)arg0->extra)->coords[2], effect, NULL);
            }
            if (work->field_0 == 0x17) {
                SndEvt_EnqueueType7(0x51030008, 1);
            }
            if ((work->field_0 == 0xC || work->field_0 == 0xD || work->field_0 == 0xE) && config->hp > 0 && work->field_C28 == 1) {
                Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F1, 0, 0);
            }
            if (enemy->hp <= 0) {
                deathSound = ((enemy->placeKey >> 0xC) << 8) | 0x400A0008;
                deathPan   = (s8)Gp_GetObjPan((GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords);
                SndEvt_EnqueueType6(deathSound, deathPan, (s8)gpGetObjDepth((GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords));
            } else {
                hitSound = ((enemy->placeKey >> 0xC) << 8) | 0x400A0007;
                hitPan   = (s8)Gp_GetObjPan((GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords);
                SndEvt_EnqueueType6(hitSound, hitPan, (s8)gpGetObjDepth((GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords));
            }
            work->field_BE8 = Gp_GetIdParam2(s->id);
            switch (Gp_GetIdParam0(s->id) & 0xFFFF) {
                case 4:
                    state = work->field_0;
                    if (state != 0x13 && state != 0x14 && state != 0x1F && state != 0x20 && state != 4 && state != 0x11) {
                        if (work->field_0 == 0x10 && work->field_6 < 0x21) {
                            work->field_0 = 0x20;
                        } else if (work->field_0 == 0xF && work->field_6 < 0xC) {
                            work->field_0 = 0x1F;
                        } else {
                            mag = s->yaw;
                            if (mag < 0) {
                                mag = -mag;
                            }
                            work->field_0 = (mag < 0x400) ? 0x13 : 0x14;
                        }
                    }
                    break;
                case 0:
                case 5:
                case 6:
                case 7:
                    state = work->field_0;
                    if (state == 0x13 || state == 0x14 || state == 0xF || state == 0x10 || state == 4 || state == 0x11) {
                        if (work->field_89E == 0xB || work->field_89E == 0x17 || work->field_89E == 8 || work->field_89E == 0xA) {
                            work->field_89A = 1;
                            work->field_8A8 = 0xB;
                        } else {
                            work->field_89A = 1;
                            work->field_8A8 = 0x19;
                        }
                        work->field_8A6 = 2;
                    } else if (work->field_BEA >= 0x4C || s->crit == 1) {
                        if (work->field_0 == 0x10 && work->field_6 < 0x21) {
                            work->field_0 = 0x20;
                        } else if (work->field_0 == 0xF && work->field_6 < 0xC) {
                            work->field_0 = 0x1F;
                        } else {
                            mag = s->yaw;
                            if (mag < 0) {
                                mag = -mag;
                            }
                            work->field_0 = (mag < 0x400) ? 0x13 : 0x14;
                        }
                    } else {
                        work->field_8A8 = 0xD;
                        work->field_89A = 1;
                        work->field_8A6 = 2;
                    }
                    break;
                case 2:
                    Gp_SetObjFlag2(enemy, s->id, 0);
                    state = work->field_0;
                    if (state == 0x11 || state == 4) {
                        work->field_0 = 4;
                    } else if (work->field_0 == 0x10 && work->field_6 < 0x21) {
                        work->field_0 = 0x20;
                    } else if (work->field_0 == 0xF && work->field_6 < 0xC) {
                        work->field_0 = 0x1F;
                    } else {
                        mag = s->yaw;
                        if (mag < 0) {
                            mag = -mag;
                        }
                        work->field_0 = (mag < 0x400) ? 0x13 : 0x14;
                    }
                    break;
                case 3:
                    state = work->field_0;
                    if (state == 0x18 || state == 0x16 || state == 0x17) {
                        work->field_0 = 6;
                    }
                    Gp_SetObjFlag4(enemy, s->id, 0);
                    break;
                case 1:
                    enemy->reactionFlags &= 0xFE;
                    state                 = work->field_0;
                    if (state != 0x13 && state != 0x14 && state != 0x1F && state != 0x20 && state != 4 && state != 0x11) {
                        if (work->field_0 == 0x10 && work->field_6 < 0x21) {
                            work->field_0 = 0x20;
                        } else if (work->field_0 == 0xF && work->field_6 < 0xC) {
                            work->field_0 = 0x1F;
                        } else {
                            mag = s->yaw;
                            if (mag < 0) {
                                mag = -mag;
                            }
                            work->field_0 = (mag < 0x400) ? 0x13 : 0x14;
                        }
                    }
                    break;
                case 8:
                    state = work->field_0;
                    if (state != 0x13 && state != 0x14 && state != 0x1F && state != 0x20 && state != 4 && state != 0x11) {
                        mag = s->yaw;
                        if (mag < 0) {
                            mag = -mag;
                        }
                        if (mag < 0x501) {
                            if (work->field_0 == 0x10 && work->field_6 < 0x21) {
                                work->field_0 = 0x20;
                            } else if (work->field_0 == 0xF && work->field_6 < 0xC) {
                                work->field_0 = 0x1F;
                            } else {
                                mag = s->yaw;
                                if (mag < 0) {
                                    mag = -mag;
                                }
                                work->field_0 = (mag < 0x400) ? 0x13 : 0x14;
                            }
                        }
                    }
                    break;
                case 9:
                    state = work->field_0;
                    if (state != 0x13 && state != 0x14 && state != 0x1F && state != 0x20 && state != 4 && state != 0x11) {
                        if (work->field_0 == 0x10 && work->field_6 < 0x21) {
                            work->field_0 = 0x20;
                        } else if (work->field_0 == 0xF && work->field_6 < 0xC) {
                            work->field_0 = 0x1F;
                        } else {
                            mag = s->yaw;
                            if (mag < 0) {
                                mag = -mag;
                            }
                            work->field_0 = (mag < 0x400) ? 0x13 : 0x14;
                        }
                    }
                    break;
            }
            timer = 5;
        } else if (work->field_BEC > 0) {
            timer = (u16)work->field_BEC - 1;
        } else {
            work->field_BEA = 0;
            goto block_bec;
        }
        work->field_BEC = timer;
    block_bec:
        if (enemy->reactionFlags & 0xC) {
            s->damage = Gp_TickObjFlag4(enemy);
            if (Gp_ObjFlag4Expired(enemy) != 0) {
                enemy->reactionFlags &= 0xF3;
            }
            if (s->damage != 0) {
                enemy->hp -= s->damage;
                func_800DA6E8(&enemy->node, s->damage, 0);
                if (work->field_0 == 7 || work->field_0 == 0x1E || work->field_0 == 0xB || work->field_0 == 0x1B) {
                    work->field_0 = 5;
                } else if (work->field_0 == 4) {
                    work->field_2 = -1;
                } else {
                    if (work->field_0 == 0x13 || work->field_0 == 0x14 || work->field_0 == 0xF || work->field_0 == 0x10 || work->field_0 == 0x11) {
                        if (work->field_89E == 0xB || work->field_89E == 0x17 || work->field_89E == 8 || work->field_89E == 0xA) {
                            work->field_89A = 1;
                            work->field_8A8 = 0xB;
                        } else {
                            work->field_89A = 1;
                            work->field_8A8 = 0x19;
                        }
                    } else {
                        work->field_89A = 1;
                        work->field_8A8 = 0xD;
                    }
                    work->field_8A6 = 2;
                }
            }
        }
        if (enemy->hp <= 0) {
            if (s->id != 0) {
                if ((Gp_GetIdParam0(s->id) & 0xFFFF) == 4 || (Gp_GetIdParam0(s->id) & 0xFFFF) == 6) {
                    if ((u16)(work->field_89E - 2) < 2) {
                        work->field_0 = 0x21;
                    } else {
                        work->field_0 = 0x1D;
                    }
                } else {
                    state = work->field_0;
                    if (state != 0x13 && state != 0x14 && state != 4 && state != 0x11) {
                        if (work->field_0 == 0x10 && work->field_6 < 0x21) {
                            work->field_0 = 0x20;
                        } else if (work->field_0 == 0xF && work->field_6 < 0xC) {
                            work->field_0 = 0x1F;
                        } else {
                            mag = s->yaw;
                            if (mag < 0) {
                                mag = -mag;
                            }
                            work->field_0 = (mag < 0x400) ? 0x13 : 0x14;
                        }
                    }
                }
            } else {
                if (work->field_C28 == 1) {
                    Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F1, 2, 0);
                    work->field_C28 = 0;
                }
                state = work->field_0;
                if (state != 0x13 && state != 0x14 && state != 0x15 && state != 0x1D && state != 0 && state != 4 && state != 0x1F && state != 0x20 && state != 0x11) {
                    if (work->field_0 == 0x10 && work->field_6 < 0x21) {
                        work->field_0 = 0x20;
                    } else if (work->field_0 == 0xF && work->field_6 < 0xC) {
                        work->field_0 = 0x1F;
                    } else {
                        work->field_0 = 0x14;
                    }
                }
            }
        }
        *(u32*)G_SCRATCH_HEAD += 0x54;
    }
}

/// Enter the live-actor state: reinstate the model buffers, seed the
/// `field_898` / `field_8A2` animation pair, fold the current `field_89E`
/// state onto the 0x17/0x18 pair, then hold in `func_actor_401000_80132EF0`
/// until the clip's `field_5A` frame count passes 6 (state 0x17) or 9 (state
/// 0x18), or the `flags_68` word reports the actor gone. The un-flagged path
/// halves `field_8A2` down to the +-0x10 turntable step and retires the actor
/// once the enemy is spent. Same body as `func_actor_401300_80135DDC`, which
/// drops the frame-count loop's `flags_68` guard and its own 0x36 test.
void func_actor_401000_80134DB4(Task* arg0)
{
    Actor401000Work* work;
    GpEnemy*         enemy;
    TmdObject*       tmd;

    work  = arg0->work;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        tmd               = arg0->extra;
        enemy->node.flags = 0;
        tmd->flags        = 0;
        Tmd_AllocBuffers(tmd);
        work->field_898        = 2;
        work->field_8A2        = 0x10;
        work->field_A10.flags |= 0x4000;
        if (work->field_89E == 11 || work->field_89E == 23) {
            work->field_89E = 0x17;
        } else if (work->field_89E == 12 || work->field_89E == 25 || work->field_89E == 24) {
            work->field_89E = 0x18;
        }
        if ((u16)(work->field_89E - 0x17) >= 2) {
            work->field_89E = 0x17;
        }
        do {
            func_actor_401000_80132EF0(arg0);
            if (work->field_89E == 0x17 && (work->field_5A & 0x3FF) >= 6) {
                break;
            }
            if (work->field_89E == 0x18 && (work->field_5A & 0x3FF) >= 9) {
                break;
            }
        } while (!(work->flags_68.word & 0x102));
        work->field_8A2 = 0x20;
        return;
    }
    ((TmdObject*)arg0->extra)->coords->flg = 0;
    work->field_8A2                        = work->field_8A2 / 2;
    if (work->field_8A2 == 1) {
        work->field_8A2 = -0x10;
    }
    if (work->field_8A2 == -1) {
        work->field_8A2 = 0x10;
    }
    func_actor_401000_80132EF0(arg0);
    if (Gp_TickObjFlag2(enemy) == 1) {
        enemy->reactionFlags &= ~2;
        work->field_0         = 0x11;
    }
    if (enemy->hp <= 0) {
        work->field_0 = 0x11;
    }
}

/// Offset from `config`'s player position to `coord`'s, halfwords only. Same
/// body as `Actor401300_ConfigPositionDelta`.
static __inline__ void Actor401000_ConfigPositionDelta(PlayerStatus* config, GsCOORDINATE2* coord, SVECTOR* pos)
{
    pos->vx = config->coordMtx->t[0] - coord->coord.t[0];
    pos->vy = config->coordMtx->t[1] - coord->coord.t[1];
    pos->vz = config->coordMtx->t[2] - coord->coord.t[2];
}

/// Yaw from the actor's facing to the player, wrapped; `pos` receives the
/// offset. Same body as `Actor01900_PositionYaw` / `Actor401300_PositionYaw`.
static __inline__ s16 Actor401000_PositionYaw(Task* actor, SVECTOR* pos, PlayerStatus* config)
{
    GsCOORDINATE2* coord;
    s32            angle;

    Actor401000_ConfigPositionDelta(config, ((TmdObject*)actor->extra)->coords, pos);
    coord = ((TmdObject*)actor->extra)->coords;
    angle = ratan2(pos->vx, pos->vz);
    return Actor401000_NormalizeYaw(angle - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
}

/// Rebuild `coord`'s Y rotation from its current yaw, uniformly scaled by
/// `scale`. Same body as `Actor01900_RescaleYaw` / `Actor401300_RescaleYaw`.
static __inline__ void Actor401000_RescaleYaw(GsCOORDINATE2* coord, s16 scale)
{
    void**                 scratch;
    void*                  head;
    Actor401000RotScratch* blk;
    s16                    ang;
    u16                    m22;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    blk      = (Actor401000RotScratch*)((u8*)head - 0x34);
    *scratch = blk;

    ang        = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    blk->angle = ang;
    Gfx_RotMatrixY(&blk->m, ang, 1);
    blk->scale.vz = scale;
    blk->scale.vy = scale;
    blk->scale.vx = scale;
    ScaleMatrix(&blk->m, &blk->scale);

    coord->coord.m[0][0] = *(u16*)&((Actor401000RotScratch*)((u8*)head - 0x34))->m.m[0][0];
    coord->coord.m[0][1] = *(u16*)&blk->m.m[0][1];
    coord->coord.m[0][2] = *(u16*)&blk->m.m[0][2];
    coord->coord.m[1][0] = *(u16*)&blk->m.m[1][0];
    coord->coord.m[1][1] = *(u16*)&blk->m.m[1][1];
    coord->coord.m[1][2] = *(u16*)&blk->m.m[1][2];
    coord->coord.m[2][0] = *(u16*)&blk->m.m[2][0];
    coord->coord.m[2][1] = *(u16*)&blk->m.m[2][1];
    m22                  = *(u16*)&blk->m.m[2][2];
    *scratch             = (u8*)*scratch + 0x34;
    coord->flg           = 0;
    coord->coord.m[2][2] = m22;
}

/// Turn the actor toward the player, clamped to +-0x10 past its current facing,
/// then rebuild the root coordinate scaled by 0x1194. Same body as
/// `func_actor_401300_80136238`, minus that one's `field_8B6` state pair and
/// its message-id gate, which the 401000 sibling keeps in `field_C10`.
void func_actor_401000_80134F98(Task* arg0)
{
    Actor401000Work*       work;
    Actor401000AimScratch* aim;
    TmdObject*             obj;
    GsCOORDINATE2*         coord;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                     = arg0->extra;
        ((GpEnemy*)arg0->spawnArg2)->node.flags = 0;
        obj->flags                              = 0;
        Tmd_AllocBuffers(obj);
        work->field_898        = 1;
        work->field_8A2        = 0x10;
        work->field_89E        = 9;
        work->field_89A        = 0;
        work->field_B50.flags &= 0x7FFF;
        work->field_A10.flags |= 0x4000;
        func_actor_401000_80132EF0(arg0);
        work->field_8D0.radius = 0x1AE;
        Gp_ArmStateF0(1);
        return;
    }
    *(Actor401000AimScratch**)G_SCRATCH_HEAD -= 1;
    aim                                       = *(Actor401000AimScratch**)G_SCRATCH_HEAD;
    ((TmdObject*)arg0->extra)->coords->flg    = 0;
    if (work->flags_68.half & 1) {
        work->field_0 = 7;
    }
    aim->angle      = Actor401000_PositionYaw(arg0, &aim->delta, &Player_Status);
    work->field_8AE = aim->angle;
    if (aim->angle > 0x10) {
        aim->angle = 0x10;
    }
    if (aim->angle < -0x10) {
        aim->angle = -0x10;
    }
    coord       = ((TmdObject*)arg0->extra)->coords;
    aim->angle += ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->coords->coord, aim->angle, 1);
    Actor401000_RescaleYaw(((TmdObject*)arg0->extra)->coords, 0x1194);
    func_actor_401000_80132EF0(arg0);
    *(Actor401000AimScratch**)G_SCRATCH_HEAD += 1;
}

void func_actor_401000_801352DC(GpAreaKey* session, GsCOORDINATE2* coord)
{
    Actor401000HeightClamp* row;
    s32                     offset;
    s32                     lo;
    s16                     i;

    for (i = 0; i < 2; i++) {
        row = &D_actor_401000_80154FD0[i];
        if (session->stage == row->field_0 && session->area == row->field_2) {
            lo     = row->lo;
            offset = coord->coord.t[1];
            if (offset < lo) {
                coord->coord.t[1] = lo;
            } else if (row->hi < offset) {
                coord->coord.t[1] = row->hi;
            }
            coord->flg = 0;
            return;
        }
    }
}

/// Whether `D_actor_401000_80154FD0` has a row matching the session's
/// `GpAreaKey::stage` / `area` pair. The helper behind both
/// height-clamp probes of `func_actor_401000_80135374`; the second probe is
/// followed by the `func_actor_401000_801352DC` call itself, which walks the
/// same rows to clamp the root Y. Same helper as `Actor401300_HasHeightClamp`.
static __inline__ s32 Actor401000_HasHeightClamp(GpAreaKey* session)
{
    Actor401000HeightClamp* row;
    s16                     i;

    for (i = 0; i < 2; i++) {
        row = &D_actor_401000_80154FD0[i];
        if (session->stage == row->field_0 && session->area == row->field_2) {
            return 1;
        }
    }
    return 0;
}

/// Root-coordinate step, the 401000 twin of `func_actor_401300_80132C78`:
/// carve the 0x20-byte `Actor401000Delta` off `G_SCRATCH_HEAD`, fill its delta
/// from the `rec` obstacle record, clamp the Y step to ±0x12C while a
/// height-clamp row matches, hand the XZ step to the GTE normalisation once it
/// passes 0x96, and step the root coordinate by each component. Reports
/// whether anything moved.
///
/// Both the repeated clamp and the `clamped` temporary are load-bearing for
/// register allocation, not style. The first clamp only runs while a clamp row
/// matches and its in-range arm skips the second copy entirely, so folding the
/// two (or letting the add re-read `s->step.vy`) swaps `$s0`/`$s1`: the block
/// pointer against the `step` local. The temporary keeps one reference to the
/// block pointer out of the RTL, which is what tips that fight the other way.
s32 func_actor_401000_80135374(GsCOORDINATE2* coord, GpRec18* rec, s16 arg2, s16 arg3)
{
    Actor401000Delta* head;
    Actor401000Delta* s;
    s16               vy;
    s16               clamped;
    SVECTOR*          step;

    if (D_80072729 == 1) {
        return 0;
    }
    head                                = *(Actor401000Delta**)G_SCRATCH_HEAD;
    *(Actor401000Delta**)G_SCRATCH_HEAD = head - 1;
    s                                   = head - 1;
    s->moved                            = 0;
    if (func_800E0C10(rec, &s->delta, arg2, NULL) != 0) {
        s->step.vx = head[-1].delta.vx.w >> 16;
        s->step.vy = s->delta.vy.w >> 16;
        s->step.vz = s->delta.vz.w >> 16;
        if (Actor401000_HasHeightClamp(&gGameSession->at4.loc.view)) {
            vy = s->step.vy;
            if (((vy >= 0) ? vy : -vy) <= 0x12C) {
                goto addStep;
            }
            clamped    = (vy <= 0) ? -0x12C : 0x12C;
            s->step.vy = clamped;
        }
        vy = s->step.vy;
        if (((vy >= 0) ? vy : -vy) <= 0x12C) {
            goto addStep;
        }
        s->step.vy = (vy <= 0) ? -0x12C : 0x12C;
    addStep:
        coord->coord.t[1] += s->step.vy;
        s->len             = s->step.vx * s->step.vx + s->step.vz * s->step.vz;
        s->len             = SquareRoot0(s->len);
        step               = &s->step;
        if (s->len >= 0x96) {
            s->step.vy = 0;
            VectorNormalSS(step, step);
            gte_lddp(0x96);
            gte_ldsv(step);
            gte_gpf12();
            gte_stsv(step);
            coord->coord.t[0] += s->step.vx;
            coord->coord.t[2] += s->step.vz;
        } else {
            coord->coord.t[0] += s->step.vx;
            coord->coord.t[2] += s->step.vz;
        }
        if (s->delta.vx.w & 0xFFFF) {
            if (s->delta.vx.w > 0) {
                coord->coord.t[0]++;
            } else {
                coord->coord.t[0]--;
            }
        }
        if (s->delta.vz.w & 0xFFFF) {
            if (s->delta.vz.w > 0) {
                coord->coord.t[2]++;
            } else {
                coord->coord.t[2]--;
            }
        }
    }
    if (Actor401000_HasHeightClamp(&gGameSession->at4.loc.view)) {
        func_actor_401000_801352DC(&gGameSession->at4.loc.view, coord);
        coord->coord.t[1] += arg3;
    }
    if (s->delta.vx.w != 0 || s->delta.vz.w != 0) {
        s->moved = 1;
    }
    *(Actor401000Delta**)G_SCRATCH_HEAD += 1;
    return s->moved;
}

static __inline__ void Actor401000_CalcPush(SVECTOR* pos, GpRec18* rec, SVECTOR* out)
{
    VECTOR d;
    VECTOR n;
    s32    t;
    s32    pen;

    d.vx = pos->vx - rec->point.vx;
    d.vy = 0;
    d.vz = pos->vz - rec->point.vz;
    pen  = SquareRoot0(d.vx * d.vx + d.vz * d.vz);
    pen  = rec->depth - pen;
    if (pen <= 0) {
        t = 0;
    } else {
        t = pen;
    }
    pen  = t;
    d.vx = pos->vx - rec->point.vx;
    d.vy = pos->vy - rec->point.vy;
    d.vz = pos->vz - rec->point.vz;
    VectorNormal(&d, &n);
    ApplyTransposeMatrixLV(&Gp_GridParams->field_0->workm, &n, &d);
    out->vx = (pen * d.vx) >> 12;
    out->vy = 0;
    out->vz = (pen * d.vz) >> 12;
}

s32 func_actor_401000_80135704(Task* arg0, GpRec18* recs, s16 count)
{
    Actor401000PushScratch* head;
    Actor401000PushScratch* s;
    Actor401000PushScratch* blk;

    if (D_80072729 == 1 || gGameSession->viewReady == 1) {
        return 0;
    }
    ((TmdObject*)arg0->extra)->coords[1].flg  = 0;
    head                                      = *(Actor401000PushScratch**)G_SCRATCH_HEAD;
    blk                                       = head - 1;
    *(Actor401000PushScratch**)G_SCRATCH_HEAD = blk;
    s                                         = blk;
    Gp_UpdateCoord(&((TmdObject*)arg0->extra)->coords[1]);
    s->pos.vx = ((TmdObject*)arg0->extra)->coords[1].workm.t[0];
    s->pos.vy = ((TmdObject*)arg0->extra)->coords[1].workm.t[1];
    s->pos.vz = ((TmdObject*)arg0->extra)->coords[1].workm.t[2];
    s->hit    = 0;
    for (s->i = 0; s->i < count; s->i++) {
        if (recs[s->i].key == 0) {
            s->dist[s->i] = 0x7FFE;
            break;
        }
        s->kind = recs[s->i].key & 0xFFFF0000;
        if (s->kind == 0x10000 || s->kind == 0x30000) {
            s->hit = 1;
            Actor401000_CalcPush(&s->pos, &recs[s->i], &s->offset);
            s->len = s->offset.vx * s->offset.vx + s->offset.vz * s->offset.vz;
            s->len = SquareRoot0(s->len);
            if (s->len >= 0x6B) {
                s->offset.vy = 0;
                VectorNormalSS(&s->offset, &s->offset);
                gte_lddp(0x6B);
                gte_ldsv(&s->offset);
                gte_gpf12();
                gte_stsv(&s->offset);
                ((TmdObject*)arg0->extra)->coords->coord.t[0] += s->offset.vx >> 2;
                ((TmdObject*)arg0->extra)->coords->coord.t[2] += s->offset.vz >> 2;
            } else {
                ((TmdObject*)arg0->extra)->coords->coord.t[0] += s->offset.vx >> 2;
                ((TmdObject*)arg0->extra)->coords->coord.t[2] += s->offset.vz >> 2;
            }
            ((TmdObject*)arg0->extra)->coords->flg = 0;
        }
    }
    *(Actor401000PushScratch**)G_SCRATCH_HEAD += 1;
    return s->hit;
}

/// Nonzero when the XZ offset `d` lies outside radius `r`; squares in a scratch block.
static __inline__ s32 Actor401000_OutOfRange(SVECTOR* d, s16 r)
{
    u8*                      head;
    Actor401000RangeScratch* blk;
    s32                      ret;

    head                                          = *(u8**)G_SCRATCH_HEAD;
    ((Actor401000RangeScratch*)(head - 0xC))->dx  = d->vx;
    blk                                           = (Actor401000RangeScratch*)(head - 0xC);
    blk->dz                                       = d->vz;
    blk->r                                        = r;
    ((Actor401000RangeScratch*)(head - 0xC))->dx *= ((Actor401000RangeScratch*)(head - 0xC))->dx;
    *(Actor401000RangeScratch**)G_SCRATCH_HEAD    = blk;
    blk->dz                                      *= blk->dz;
    blk->r                                       *= blk->r;
    *(u8**)G_SCRATCH_HEAD                         = head;
    ret                                           = ((Actor401000RangeScratch*)(head - 0xC))->dx + blk->dz >= blk->r;
    return ret;
}

void func_actor_401000_80135AA4(Task* arg0)
{
    Actor401000ChaseScratch* head;
    Actor401000ChaseScratch* chase;
    Actor401000Work*         work;
    TmdObject*               obj;
    GsCOORDINATE2*           coord;
    s32                      angle;
    s32                      diff;
    s32                      kind;
    u16                      speed;

    kind = (arg0->spawnArg1 >> 16);
    work = arg0->work;
    if ((kind & 0xF0) == 0x10) {
        work->field_0 = 0x1E;
        return;
    }
    if (work->field_4 != 0) {
        obj                                     = arg0->extra;
        ((GpEnemy*)arg0->spawnArg2)->node.flags = 0;
        obj->flags                              = 0;
        Tmd_AllocBuffers(obj);
        work->field_8D0.radius = 0x1AE;
        work->field_898        = 1;
        SOFT_BARRIER();
        work->field_89E        = 3;
        speed                  = work->field_8A4;
        work->field_B50.flags &= 0x7FFF;
        SOFT_BARRIER();
        work->field_89A        = 0;
        work->field_8A2        = speed;
        work->field_A10.flags |= 0x4000;
        func_actor_401000_80132EF0(arg0);
        work->field_C24 = 0;
        work->field_6   = 0;
        work->field_8   = 0;
        return;
    }
    work->field_6 = (u16)work->field_6 + 1;
    work->field_8 = (u16)work->field_8 + 1;
    head          = *(Actor401000ChaseScratch**)G_SCRATCH_HEAD;
    chase         = (*(Actor401000ChaseScratch**)G_SCRATCH_HEAD = head - 1);
    Actor401000_ConfigPositionDelta(&Player_Status, ((TmdObject*)arg0->extra)->coords, &head[-1].delta);
    ((TmdObject*)arg0->extra)->coords->flg = 0;
    func_actor_401000_80132EF0(arg0);
    chase->pad_8 = ratan2(-((TmdObject*)gameGetPtrSlot(3)->extra)->coords->coord.m[2][0],
                          ((TmdObject*)gameGetPtrSlot(3)->extra)->coords->coord.m[2][2]);
    Actor401000_ConfigPositionDelta(&Player_Status, ((TmdObject*)arg0->extra)->coords, &chase->delta);
    chase->pad_A    = ratan2(chase->delta.vx, chase->delta.vz) + 0x800;
    chase->pad_A    = Actor401000_NormalizeYaw(chase->pad_A);
    coord           = ((TmdObject*)arg0->extra)->coords;
    chase->turn     = Actor401000_NormalizeYaw(ratan2(chase->delta.vx, chase->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
    work->field_8AE = chase->turn;
    diff            = chase->pad_A - chase->pad_8;
    if (ABS(diff) < 0x44) {
        if (work->field_C14 + work->field_C26 / 2 < work->field_6) {
            angle = chase->turn;
            if (angle < 0) {
                angle = -angle;
            }
            if (angle < 0x80) {
                if (Actor401000_OutOfRange(&chase->delta, 0x708) && func_actor_401000_80132824(arg0) != 1) {
                    work->field_0 = 0xA;
                }
            }
        }
    }
    if (func_actor_401000_80132824(arg0) != 1) {
        work->field_6   = (u16)work->field_6 + 1;
        coord           = ((TmdObject*)arg0->extra)->coords;
        chase->turn     = Actor401000_NormalizeYaw(ratan2(chase->delta.vx, chase->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
        work->field_8AE = chase->turn;
        if (chase->turn < 0x200) {
            if (!Actor401000_OutOfRange(&chase->delta, 0x44C) && work->field_C1B == 0) {
                work->field_0 = 0xB;
            }
        }
    } else {
        work->field_6   = 0;
        coord           = ((TmdObject*)arg0->extra)->coords;
        chase->turn     = Actor401000_NormalizeYaw(ratan2(chase->delta.vx, chase->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
        work->field_8AE = chase->turn;
        if (work->field_C08 == 1) {
            chase->turn = (u16)chase->turn + 0x300;
        } else {
            chase->turn = (u16)chase->turn - 0x300;
        }
        if (work->field_6 >= 0x169) {
            work->field_6   = 0;
            work->field_C08 = -(u16)work->field_C08;
        }
    }
    if (chase->turn > 0x40) {
        chase->turn = 0x40;
    }
    if (chase->turn < -0x40) {
        chase->turn = -0x40;
    }
    chase->turn += ratan2(-((TmdObject*)arg0->extra)->coords->coord.m[2][0], ((TmdObject*)arg0->extra)->coords->coord.m[2][2]);
    Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->coords->coord, chase->turn, 1);
    Actor401000_RescaleYaw(((TmdObject*)arg0->extra)->coords, 0x1194);
    ((TmdObject*)arg0->extra)->coords->flg = 0;
    if (work->field_89E == 3) {
        if (work->field_89A == 0) {
            if ((func_actor_401000_80132590(((TmdObject*)arg0->extra)->coords, 0x12C, ((work->field_8A4 + 2) * 0x78) / 0x12) << 0x10) != 0) {
                Actor401000_MoveForwardNonzero(((TmdObject*)arg0->extra)->coords, ((work->field_8A4 + 2) * 0x78) / 0x12);
            }
        } else {
            if ((func_actor_401000_80132590(((TmdObject*)arg0->extra)->coords, 0x12C, (s16)(((work->field_8A4 + 2) * 0x78) / 0x12 << 0xE >> 0x10)) << 0x10) != 0) {
                Actor401000_MoveForwardNonzero(((TmdObject*)arg0->extra)->coords, ((work->field_8A4 + 2) * 0x78) / 0x12 >> 2);
            }
        }
    } else if (work->flags_68.half & 1) {
        work->field_89E = 3;
        work->field_898 = 1;
    }
    if (func_actor_401000_80135374(((TmdObject*)arg0->extra)->coords, work->field_A30, 0xC, 0x4B) != 1 && func_actor_401000_801323EC(((TmdObject*)arg0->extra)->coords, work->field_8F0, 0xC) != 1) {
        func_actor_401000_80135704(arg0, work->field_8F0, 0xC);
    }
    ((TmdObject*)arg0->extra)->coords->flg = 0;
    Gp_UpdateCoord(((TmdObject*)arg0->extra)->coords);
    Gp_ClearRec18Occupied(work->field_8F0);
    if (work->field_C1B != 0) {
        work->field_C1B--;
    }
    if (work->field_8 >= 0x4C) {
        work->field_0 = 6;
    }
    *(Actor401000ChaseScratch**)G_SCRATCH_HEAD += 1;
}

/// Turn-aim state body, the 401000 twin of `Actor01900_Fn04D14`: take a 0x10
/// chase scratch off `G_SCRATCH_HEAD` and, on the live-actor flag, key the
/// two animation nodes, the frame counter and the `field_C06` clip phase.
/// Once `field_8` has counted 7 frames the arm aims at the player - the yaw
/// toward `gameGetPtrSlot(3)` goes in `pad_8`, the wrapped yaw toward
/// `Player_Status.coordMtx` in `pad_A` - and the root is turned by the facing
/// yaw plus a +-0x60 clamp of the turn's 1000 bias. The forward draw
/// `field_C04` is the doubled frame parameter (halved while `field_89A` is
/// up, forced to 2 while the frame counter runs), and the actor slides along
/// it when the `0x12C` probe reports the step is clear. `field_C06` walks 8 ->
/// -1 -> 0 as `field_8A2` passes 0x18 and 0x12, and the 0 arm runs the
/// five-frame exit window that re-aims once more and picks state 0xB when the
/// actor faces away from the player, else state 0x1A.
void func_actor_401000_801365C8(Task* arg0)
{
    Actor401000ChaseScratch* chase;
    Actor401000ChaseScratch* head;
    Actor401000Work*         work;
    TmdObject*               obj;
    GsCOORDINATE2*           coord;
    GsCOORDINATE2*           facing;
    s32                      turn;
    s32                      diffPos;
    s32                      diffNeg;
    s32                      yaw;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                     = arg0->extra;
        ((GpEnemy*)arg0->spawnArg2)->node.flags = 0;
        obj->flags                              = 0;
        Tmd_AllocBuffers(obj);
        work->field_8D0.radius = 0xD7;
        work->field_898        = 1;
        work->field_89E        = 3;
        work->field_89A        = 0;
        work->field_B50.flags &= 0x7FFF;
        work->field_A10.flags |= 0x4000;
        func_actor_401000_80132EF0(arg0);
        work->field_C06         = 8;
        work->field_6           = 0;
        work->field_8           = 0;
        D_actor_401000_80155000 = 0;
        work->field_C24++;
        return;
    }
    head                                       = *(Actor401000ChaseScratch**)G_SCRATCH_HEAD;
    *(Actor401000ChaseScratch**)G_SCRATCH_HEAD = head - 1;
    chase                                      = head - 1;
    ((TmdObject*)arg0->extra)->coords->flg     = 0;
    func_actor_401000_80132EF0(arg0);
    if (func_actor_401000_801323EC(((TmdObject*)arg0->extra)->coords, work->field_A30, 0xC) == 1) {
        work->field_8++;
    } else {
        func_actor_401000_80135704(arg0, work->field_8F0, 0xC);
    }
    Actor401000_ConfigPositionDelta(&Player_Status, ((TmdObject*)arg0->extra)->coords, &chase->delta);
    if (work->field_8 >= 7) {
        chase->pad_8  = ratan2(-((TmdObject*)(gameGetPtrSlot(3))->extra)->coords->coord.m[2][0],
                               ((TmdObject*)(gameGetPtrSlot(3))->extra)->coords->coord.m[2][2]);
        chase->pad_A  = ratan2(chase->delta.vx, chase->delta.vz) + 0x800;
        chase->pad_A  = Actor401000_NormalizeYaw(chase->pad_A);
        work->field_0 = 0x1A;
    }
    coord       = ((TmdObject*)arg0->extra)->coords;
    chase->turn = Actor401000_NormalizeYaw(ratan2(chase->delta.vx, chase->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
    turn        = chase->turn;
    if (turn >= 0) {
        diffPos = turn - 1000;
        if (ABS(diffPos) < 0x60) {
            chase->angle = chase->turn - 1000;
        } else if (diffPos > 0) {
            chase->angle = 0x60;
        } else {
            chase->angle = -0x60;
        }
    } else {
        diffNeg = turn + 1000;
        if (ABS(diffNeg) < 0x60) {
            chase->angle = chase->turn + 1000;
        } else if (diffNeg > 0) {
            chase->angle = 0x60;
        } else {
            chase->angle = -0x60;
        }
    }
    facing        = ((TmdObject*)arg0->extra)->coords;
    chase->angle += ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
    Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->coords->coord, chase->angle, 1);
    Actor401000_RescaleYaw(((TmdObject*)arg0->extra)->coords, 0x1194);
    coord                                  = ((TmdObject*)arg0->extra)->coords;
    work->field_8AE                        = Actor401000_NormalizeYaw(ratan2(chase->delta.vx, chase->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
    ((TmdObject*)arg0->extra)->coords->flg = 0;
    work->field_C04                        = work->field_8A2 * 8;
    if (work->field_89A != 0) {
        work->field_C04 = work->field_C04 >> 1;
    }
    if (work->field_8 != 0) {
        work->field_C04 = 2;
    }
    if ((func_actor_401000_80132590(((TmdObject*)arg0->extra)->coords, 0x12C, work->field_C04) << 0x10) != 0) {
        Actor401000_MoveForwardNonzero(((TmdObject*)arg0->extra)->coords, (u16)work->field_C04);
    }
    D_actor_401000_80155000 += (u16)work->field_C04;
    if (work->field_C06 == 8 && work->field_8A2 >= 0x18) {
        work->field_C06 = -1;
    }
    if (work->field_C06 == -1 && work->field_8A2 == 0x12) {
        work->field_C06 = 0;
        work->field_6   = 0;
    }
    if (work->field_C06 == 0) {
        if (++work->field_6 == 5) {
            chase->pad_8 = ratan2(-((TmdObject*)(gameGetPtrSlot(3))->extra)->coords->coord.m[2][0],
                                  ((TmdObject*)(gameGetPtrSlot(3))->extra)->coords->coord.m[2][2]);
            Actor401000_ConfigPositionDelta(&Player_Status, ((TmdObject*)arg0->extra)->coords, &chase->delta);
            chase->pad_A = ratan2(chase->delta.vx, chase->delta.vz) + 0x800;
            chase->pad_A = Actor401000_NormalizeYaw(chase->pad_A);
            yaw          = chase->pad_A - chase->pad_8;
            if (yaw < 0) {
                yaw = -yaw;
            }
            if (yaw > 0x400 && work->field_C1B == 0) {
                work->field_0 = 0xB;
            } else {
                work->field_0 = 0x1A;
                work->field_2 = -1;
            }
        }
    }
    work->field_8A2 += (u16)work->field_C06;
    if (work->field_C1B != 0) {
        work->field_C1B--;
    }
    *(Actor401000ChaseScratch**)G_SCRATCH_HEAD += 1;
}

/// Turn-entry body, the 401000 twin of `func_actor_401300_801376E4`: carve the
/// chase scratch off `G_SCRATCH_HEAD`, and while the live-actor flag is up
/// reset the display nodes and rebuild the actor's facing. The turn direction
/// comes off the wrapped yaw toward the player, the yaw itself out of the
/// root's own rotation, and the pair (`field_C00` / `field_C02`) is what the
/// per-frame arm then walks: the turn swings the facing 0x89 a frame until it
/// reaches the seeded yaw, `Gfx_RotMatrixY` rebuilds the rotation from it and
/// `Actor401000_RescaleYaw` re-scales the root by 0x1194. When the two have
/// met the actor re-arms (`field_0` 8 or 0xB) off `field_C24`, the obstacle
/// range and the `field_C1B` cooldown, and the arm is then slid forward along
/// its obstacle table. `field_C1B` counts down once per entry.
void func_actor_401000_80136E20(Task* arg0)
{
    Actor401000Work*         work;
    Actor401000ChaseScratch* aim;
    Actor401000ChaseScratch* head;
    TmdObject*               obj;
    GsCOORDINATE2*           coord;
    GsCOORDINATE2*           facing;

    work = arg0->work;
    if (work->field_4 != 0) {
        head                                       = *(Actor401000ChaseScratch**)G_SCRATCH_HEAD;
        obj                                        = arg0->extra;
        *(Actor401000ChaseScratch**)G_SCRATCH_HEAD = head - 1;
        aim                                        = head - 1;
        ((GpEnemy*)arg0->spawnArg2)->node.flags    = 0;
        obj->flags                                 = 0;
        Tmd_AllocBuffers(obj);
        work->field_8D0.radius = 0x1AE;
        work->field_898        = 1;
        work->field_8A2        = 0x10;
        work->field_89E        = 3;
        work->field_89A        = 0;
        work->field_8AE        = 0;
        work->field_B50.flags &= 0x7FFF;
        work->field_A10.flags |= 0x4000;
        func_actor_401000_80132EF0(arg0);
        Actor401000_ConfigPositionDelta(&Player_Status, ((TmdObject*)arg0->extra)->coords, &aim->delta);
        coord                                       = ((TmdObject*)arg0->extra)->coords;
        aim->turn                                   = Actor401000_NormalizeYaw(ratan2(head[-1].delta.vx, aim->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
        facing                                      = ((TmdObject*)arg0->extra)->coords;
        aim->angle                                  = ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
        work->field_C00                             = aim->angle;
        work->field_C02                             = aim->angle + (u16)aim->turn * 2;
        *(Actor401000ChaseScratch**)G_SCRATCH_HEAD += 1;
        return;
    }
    head                                       = *(Actor401000ChaseScratch**)G_SCRATCH_HEAD;
    *(Actor401000ChaseScratch**)G_SCRATCH_HEAD = head - 1;
    aim                                        = head - 1;
    func_actor_401000_80132EF0(arg0);
    Actor401000_ConfigPositionDelta(&Player_Status, ((TmdObject*)arg0->extra)->coords, &aim->delta);
    if (work->field_C00 == work->field_C02) {
        if (work->field_C24 < 2 || Actor401000_OutOfRange(&aim->delta, 0x384) || work->field_C1B != 0) {
            work->field_0 = 8;
        } else {
            work->field_0 = 0xB;
        }
    }
    if (work->field_C00 > work->field_C02) {
        work->field_C00 -= 0x89;
        if (work->field_C00 < work->field_C02) {
            work->field_C00 = work->field_C02;
        }
    }
    if (work->field_C00 < work->field_C02) {
        work->field_C00 += 0x89;
        if (work->field_C00 > work->field_C02) {
            work->field_C00 = work->field_C02;
        }
    }
    Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->coords->coord, work->field_C00, 1);
    Actor401000_RescaleYaw(((TmdObject*)arg0->extra)->coords, 0x1194);
    ((TmdObject*)arg0->extra)->coords->flg = 0;
    if (work->field_89A == 0) {
        if ((s16)func_actor_401000_80132590(((TmdObject*)arg0->extra)->coords, 0x12C, 0x28) != 0) {
            Actor401000_MoveForward(((TmdObject*)arg0->extra)->coords, 0x28);
        }
    } else {
        if ((s16)func_actor_401000_80132590(((TmdObject*)arg0->extra)->coords, 0x12C, 0x14) != 0) {
            Actor401000_MoveForward(((TmdObject*)arg0->extra)->coords, 0x14);
        }
    }
    if (func_actor_401000_801323EC(((TmdObject*)arg0->extra)->coords, work->field_A30, 0xC) != 1) {
        func_actor_401000_80135704(arg0, work->field_8F0, 0xC);
    }
    if (work->field_C1B != 0) {
        work->field_C1B--;
    }
    *(Actor401000ChaseScratch**)G_SCRATCH_HEAD += 1;
}

/// Turn-entry body, the 401000 twin of `func_actor_401300_80137D78`: carve the
/// aim scratch off `G_SCRATCH_HEAD`, and while the live-actor flag is up reset
/// the display nodes and rebuild the actor's facing. The turn direction
/// (`field_C08`) is drawn from `Gp_LcgState` on the first entry, and each entry
/// swings the facing toward the player by `field_C12` plus a 0x171 bias until
/// `field_C26` has been counted once. The forward direction `field_BF0` comes
/// out of the turn angle through `Gfx_RotMatrixY`, and the `field_C0A` draw
/// scales it onto the scratch vector; the actor is then slid along its obstacle
/// table, halving that draw while it overlaps. Counts the entry in `field_6`
/// and keys state 7 once 0x1E of them have run.
void func_actor_401000_801374D4(Task* arg0)
{
    Actor401000Work*       work;
    Actor401000AimScratch* aim;
    Actor401000AimScratch* head;
    TmdObject*             obj;
    GsCOORDINATE2*         coord;
    SVECTOR*               dir;
    MATRIX                 mat;
    u16                    angle;
    s32                    kind;

    kind = (arg0->spawnArg1 >> 16);
    work = arg0->work;
    if ((kind & 0xF0) == 0x10) {
        work->field_0 = 0x1E;
        return;
    }
    head                                     = *(Actor401000AimScratch**)G_SCRATCH_HEAD;
    *(Actor401000AimScratch**)G_SCRATCH_HEAD = head - 1;
    aim                                      = head - 1;
    if (work->field_4 != 0) {
        obj                                     = arg0->extra;
        ((GpEnemy*)arg0->spawnArg2)->node.flags = 0;
        obj->flags                              = 0;
        Tmd_AllocBuffers(obj);
        work->field_8D0.radius = 0xD7;
        work->field_6          = 0;
        work->field_B50.flags &= 0x7FFF;
        work->field_A10.flags |= 0x4000;
        Actor401000_ConfigPositionDelta(&Player_Status, ((TmdObject*)arg0->extra)->coords, &aim->delta);
        aim->angle = ratan2(head[-1].delta.vx, aim->delta.vz);
        if (work->field_C08 == 0) {
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            if ((Gp_LcgState >> 16) & 1) {
                work->field_C08 = 1;
            } else {
                work->field_C08 = -1;
            }
        }
        if (work->field_C08 == 1) {
            work->field_89E = 0x15;
            if (work->field_C26 == 0) {
                angle      = aim->angle + 0x171;
                aim->angle = work->field_C12 + angle;
            } else {
                aim->angle += work->field_C12;
            }
            work->field_C08 = -1;
        } else {
            work->field_89E = 0x14;
            if (work->field_C26 == 0) {
                angle      = aim->angle - 0x171;
                aim->angle = angle - work->field_C12;
            } else {
                aim->angle -= work->field_C12;
            }
            work->field_C08 = 1;
        }
        work->field_898 = 1;
        work->field_8A2 = 0xC;
        work->field_89A = 0;
        func_actor_401000_80132EF0(arg0);
        Gfx_RotMatrixY(&mat, aim->angle, 1);
        dir = &work->field_BF0;
        Gfx_MatrixCol2(&mat, dir);
        VectorNormalSS(dir, dir);
        work->field_C0A = 0xDE;
        work->field_C26++;
    }
    ((TmdObject*)arg0->extra)->coords->flg = 0;
    func_actor_401000_80132EF0(arg0);
    ((TmdObject*)arg0->extra)->coords->flg = 0;
    if (work->field_89A == 0) {
        gte_lddp(work->field_C0A);
        gte_ldsv(&work->field_BF0);
        gte_gpf12();
        gte_stsv(aim);
    } else {
        gte_lddp(work->field_C0A >> 1);
        gte_ldsv(&work->field_BF0);
        gte_gpf12();
        gte_stsv(aim);
    }
    if ((u32)((u16)work->field_6 - 0xC) < 0xAU) {
        coord              = ((TmdObject*)arg0->extra)->coords;
        coord->coord.t[0] += aim->delta.vx;
        coord              = ((TmdObject*)arg0->extra)->coords;
        coord->coord.t[2] += aim->delta.vz;
        if (func_actor_401000_801323EC(((TmdObject*)arg0->extra)->coords, work->field_A30, 0xC) == 1) {
            work->field_C0A = (u16)(work->field_C0A >> 1);
        }
    }
    if (++work->field_6 >= 0x1E) {
        work->field_0 = 7;
    }
    *(Actor401000AimScratch**)G_SCRATCH_HEAD += 1;
}

/// Offset from `coord` to the translation of `m`; `Actor401000_ConfigPositionDelta`
/// for a bare matrix. Same body as `Actor401300_MatrixPositionDelta`.
static __inline__ void Actor401000_MatrixPositionDelta(MATRIX* m, GsCOORDINATE2* coord, SVECTOR* pos)
{
    pos->vx = m->t[0] - coord->coord.t[0];
    pos->vy = m->t[1] - coord->coord.t[1];
    pos->vz = m->t[2] - coord->coord.t[2];
}

/// `Actor401000_PositionYaw` towards the translation of `m`. Same body as
/// `Actor401300_MatrixPositionYaw`.
static __inline__ s16 Actor401000_MatrixPositionYaw(Task* actor, SVECTOR* pos, MATRIX* m)
{
    GsCOORDINATE2* coord;
    s32            angle;

    Actor401000_MatrixPositionDelta(m, ((TmdObject*)actor->extra)->coords, pos);
    coord = ((TmdObject*)actor->extra)->coords;
    angle = ratan2(pos->vx, pos->vz);
    return Actor401000_NormalizeYaw(angle - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
}

void func_actor_401000_801378DC(Task* arg0)
{
    SVECTOR          delta;
    Actor401000Work* work;
    GpEnemy*         enemy;
    GameActor*       player;
    PlayerStatus*    config;
    GsCOORDINATE2*   coord;
    SVECTOR*         p;
    s16              angle;

    enemy  = arg0->spawnArg2;
    work   = arg0->work;
    player = (GameActor*)gameGetPtrSlot(3)->work;
    config = &Player_Status;
    if (work->field_4 != 0) {
        work->field_B50.flags &= 0x7FFF;
        work->field_A10.flags |= 0x4000;
        enemy->node.flags      = 0;
        work->field_898        = 1;
        work->field_8A2        = 0x10;
        work->field_89E        = 4;
        func_actor_401000_80132EF0(arg0);
        work->field_8AE                        = 0;
        work->field_8B0                        = 0;
        ((TmdObject*)arg0->extra)->coords->flg = 0;
        Gp_UpdateCoord(((TmdObject*)arg0->extra)->coords);
        work->field_C26 = 0;
        work->field_C28 = 0;
        work->field_C1B = 0xA;
        func_actor_401000_801323EC(((TmdObject*)arg0->extra)->coords, work->field_A30, 0xC);
        work->field_6 = 0;
        return;
    }
    if (++work->field_6 == 1) {
        func_actor_401000_801323EC(((TmdObject*)arg0->extra)->coords, work->field_A30, 0xC);
        work->field_BF8.vx                     = ((TmdObject*)arg0->extra)->coords->coord.t[0];
        work->field_BF8.vy                     = ((TmdObject*)arg0->extra)->coords->coord.t[1];
        work->field_BF8.vz                     = ((TmdObject*)arg0->extra)->coords->coord.t[2];
        work->field_8D0.radius                 = 0x1AE;
        ((TmdObject*)arg0->extra)->coords->flg = 0;
        Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->coords->coord, Actor401000_PositionYaw(arg0, &delta, config), 0);
        Actor401000_RescaleYaw(((TmdObject*)arg0->extra)->coords, 0x1194);
        delta.vx                               = ((TmdObject*)arg0->extra)->coords->coord.t[0] - config->coordMtx->t[0];
        delta.vy                               = 0;
        delta.vz                               = ((TmdObject*)arg0->extra)->coords->coord.t[2] - config->coordMtx->t[2];
        work->field_8AE                        = 0;
        work->field_8B0                        = 0;
        ((TmdObject*)arg0->extra)->coords->flg = 0;
        work->field_C26                        = 0;
        work->field_C28                        = 0;
        work->field_C1B                        = 0xA;
    }
    func_actor_401000_80132EF0(arg0);
    if ((work->field_5A & 0x3FF) == 0x10 && player->field_954 != 2) {
        angle = Actor401000_MatrixPositionYaw(arg0, &delta, D_80073B8C);
        if (abs(angle) < 0x10 && !Actor401000_OutOfRange(&delta, 0x44C)) {
            if (D_8007218A == 1) {
                D_actor_401000_80154F1C.animBlock.ptr = &D_actor_401000_80154F08;
            } else {
                D_actor_401000_80154F1C.animBlock.ptr = &D_actor_401000_80154F00;
            }
            D_actor_401000_80155038.field_14 = 8;
            if (Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F8, (s32)&D_actor_401000_80155038, 0) == 0) {
                work->field_0                   = 0xC;
                work->field_C28                 = 1;
                D_actor_401000_80154F1C.field_4 = 1;
                Gp_DispatchMsg(gameGetPtrSlot(3), 0x3FF, (s32)&D_actor_401000_80154F1C, 0);
            }
        }
    }
    if (work->field_89E == 4 && (work->flags_68.half & 1)) {
        work->field_0 = 7;
    }
    if ((u32)(work->field_5A & 0x3FF) >= 0x11) {
        p        = &delta;
        delta.vx = ((TmdObject*)arg0->extra)->coords->coord.t[0] - config->coordMtx->t[0];
        delta.vy = 0;
        delta.vz = ((TmdObject*)arg0->extra)->coords->coord.t[2] - config->coordMtx->t[2];
        if (!Actor401000_OutOfRange(p, 0x578)) {
            VectorNormalSS(p, p);
            gte_lddp(10);
            gte_ldsv(p);
            gte_gpf12();
            gte_stsv(p);
            coord                                  = ((TmdObject*)arg0->extra)->coords;
            coord->coord.t[0]                     += delta.vx;
            coord                                  = ((TmdObject*)arg0->extra)->coords;
            coord->coord.t[2]                     += delta.vz;
            ((TmdObject*)arg0->extra)->coords->flg = 0;
        }
        if (func_actor_401000_801323EC(((TmdObject*)arg0->extra)->coords, work->field_A30, 0xC) != 1) {
            func_actor_401000_80135704(arg0, work->field_8F0, 0xC);
        }
    }
}

void func_actor_401000_801380B8(Task* arg0)
{
    SVECTOR          dir;
    Actor401000Work* work;
    GpEnemy*         enemy;
    Task*            player;
    SVECTOR*         pdir;
    GpMsg3EE*        msg;

    work  = arg0->work;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        player                                   = gameGetPtrSlot(3);
        work->field_8D0.radius                   = 0x1AE;
        work->field_B50.flags                   &= 0x7FFF;
        work->field_A10.flags                   |= 0x4000;
        enemy->node.flags                        = 0;
        work->field_898                          = 1;
        work->field_8A2                          = 0x10;
        work->field_89E                          = 5;
        ((TmdObject*)player->extra)->coords->flg = 0;
        Gp_UpdateCoord(((TmdObject*)player->extra)->coords);
        msg          = &D_actor_401000_80155018;
        msg->field_0 = ((TmdObject*)player->extra)->coords->coord.t[0];
        msg->field_4 = ((TmdObject*)player->extra)->coords->coord.t[1];
        msg->field_8 = ((TmdObject*)player->extra)->coords->coord.t[2];
        pdir         = &dir;
        dir.vx       = ((GpCoordXZ*)((TmdObject*)arg0->extra)->coords)->field_18 - ((GpCoordXZ*)((TmdObject*)player->extra)->coords)->field_18;
        dir.vy       = 0;
        dir.vz       = ((GpCoordXZ*)((TmdObject*)arg0->extra)->coords)->field_20 - ((GpCoordXZ*)((TmdObject*)player->extra)->coords)->field_20;
        VectorNormalSS(pdir, pdir);
        gte_lddp(0x3E8);
        gte_ldsv(pdir);
        gte_gpf12();
        gte_stsv(pdir);
        ((TmdObject*)arg0->extra)->coords->coord.t[0] = ((TmdObject*)player->extra)->coords->coord.t[0] + dir.vx;
        ((TmdObject*)arg0->extra)->coords->coord.t[2] = ((TmdObject*)player->extra)->coords->coord.t[2] + dir.vz;
        ((TmdObject*)arg0->extra)->coords->flg        = 0;
        msg->field_10                                 = 0;
        msg->field_12                                 = ratan2(dir.vx, dir.vz);
        msg->field_14                                 = 0;
        Gp_DispatchMsg(player, 0x3E9, (s32)msg, 0);
        Gp_SpawnPadLerp(0xC, 8, 0x8F);
    }
    func_actor_401000_80132EF0(arg0);
    Gfx_RotMatrixX(&((TmdObject*)arg0->extra)->coords[2].coord, -0x80, 0);
    ((TmdObject*)arg0->extra)->coords[4].flg = 0;
    Gp_UpdateCoord(&((TmdObject*)arg0->extra)->coords[2]);
    Gfx_RotMatrixX(&((TmdObject*)arg0->extra)->coords[3].coord, -0x80, 0);
    ((TmdObject*)arg0->extra)->coords[5].flg = 0;
    Gp_UpdateCoord(&((TmdObject*)arg0->extra)->coords[3]);
    if (work->field_89E == 5 && (work->flags_68.half & 1)) {
        work->field_8B8.coord      = ((TmdObject*)arg0->extra)->coords + 5;
        work->field_8B8.spawnArgLo = 0x200;
        work->field_8B8.spawnArgHi = 2;
        func_800FDB18(Gp_GetIdParam1(0x1001) & 0xFFFF, ((TmdObject*)arg0->extra)->coords + 5, NULL, &work->field_8B8);
        work->field_0 = 0xD;
    }
}

void func_actor_401000_801383F0(Task* arg0)
{
    Actor401000Work* work;
    GpAnimArg*       msg;
    GpEnemy*         enemy;
    Task*            player;

    work  = arg0->work;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        work->field_8A2 = 0x10;
        work->field_89E = 6;
        work->field_898 = 2;
        msg             = &D_actor_401000_80154F1C;
        msg->field_4    = 2;
        Gp_DispatchMsg(gameGetPtrSlot(3), 0x3FF, (s32)msg, 0);
        player = gameGetPtrSlot(3);
        Gp_DispatchMsg(player, 0x3F9, Gp_PackObjPair(enemy, 0), 0);
        Gp_SpawnPadLerp(5, 0xFF, 8);
    }
    if (work->flags_68.half & 1) {
        work->field_8B8.coord      = ((TmdObject*)arg0->extra)->coords + 1;
        work->field_8B8.spawnArgLo = 0x100;
        work->field_8B8.spawnArgHi = 2;
        func_800FDB18(Gp_GetIdParam1(0x1001) & 0xFFFF, ((TmdObject*)arg0->extra)->coords + 5, NULL, &work->field_8B8);
        work->field_0 = 0xE;
    }
    work->field_894 = work->field_5A & 0x3FF;
    func_actor_401000_80132EF0(arg0);
    Gfx_RotMatrixX(&((TmdObject*)arg0->extra)->coords[2].coord, -0x80, 0);
    ((TmdObject*)arg0->extra)->coords[4].flg = 0;
    Gp_UpdateCoord(&((TmdObject*)arg0->extra)->coords[3]);
    Gfx_RotMatrixX(&((TmdObject*)arg0->extra)->coords[3].coord, -0x80, 0);
    ((TmdObject*)arg0->extra)->coords[5].flg = 0;
    Gp_UpdateCoord(&((TmdObject*)arg0->extra)->coords[2]);
}

void func_actor_401000_801385B0(Task* arg0)
{
    Actor401000Work* work;
    GpEnemy*         enemy;
    GpAnimArg*       msg;
    PlayerStatus*    cfg;
    u8               kind;

    work  = arg0->work;
    enemy = arg0->spawnArg2;
    cfg   = &Player_Status;
    if (work->field_4 != 0) {
        work->field_8A2 = 0x10;
        work->field_89E = 7;
        work->field_898 = 2;
        func_actor_401000_80132EF0(arg0);
        msg          = &D_actor_401000_80154F1C;
        msg->field_4 = 3;
        if (cfg->hp > 0) {
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x3FF, (s32)msg, 0);
        }
        work->field_C0C        = -0x78;
        work->field_6          = 0;
        work->field_A10.flags |= 0x4000;
        return;
    }
    if (Gp_DispatchMsg(gameGetPtrSlot(3), 0x3ED, 0, 0) == 0 && cfg->hp > 0 && work->field_C28 == 1) {
        Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F1, 0, 0);
        work->field_C28 = 0;
    }
    if ((u32)((work->field_5A & 0x3FF) - 0x10) < 7U) {
        if ((s16)func_actor_401000_80132590(((TmdObject*)arg0->extra)->coords, 0x12C, work->field_C0C) != 0) {
            Actor401000_MoveForwardNonzero(((TmdObject*)arg0->extra)->coords, (u16)work->field_C0C);
        }
        if (func_actor_401000_801323EC(((TmdObject*)arg0->extra)->coords, work->field_A30, 0xC) == 1) {
            work->field_C0C = (s16)(u16)work->field_C0C / 2;
        }
        ((TmdObject*)arg0->extra)->coords->flg = 0;
    }
    func_actor_401000_80132EF0(arg0);
    if (work->flags_68.half & 1) {
        kind = enemy->node.targeted;
        if (kind == 1) {
            if (func_actor_401000_80132824(arg0) == kind) {
                work->field_0 = 6;
            } else {
                work->field_0 = 0xA;
            }
        } else {
            work->field_0 = 6;
        }
        if (cfg->hp > 0 && work->field_C28 == 1) {
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F1, 0, 0);
            work->field_C28 = 0;
        }
    }
}

/// State 8 body, the 401000 twin of `func_actor_401300_80138CF8`: on the
/// live-actor flag, reset the two animation nodes, the root coordinate and the
/// model's facing, then slide the root along both obstacle tables and take one
/// forward step while the 0x12C probe is still in range. The tail keys the
/// actor's next state (`field_0`) off `GpEnemy.hp` / `.reactionFlags` whenever
/// the work block's pending-request bit is up.
void func_actor_401000_801388F4(Task* arg0)
{
    Actor401000Work* work;
    GpEnemy*         enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        ((TmdObject*)arg0->extra)->flags = 0;
        work->field_8D0.radius           = 0x1AE;
        work->field_B50.flags           &= 0x7FFF;
        work->field_A10.flags           |= 0x4000;
        enemy->node.flags                = 0;
        work->field_898                  = 1;
        work->field_89E                  = 0xA;
        work->field_89A                  = 0;
        work->field_8A2                  = 0x10;
        work->field_8B0                  = 0;
        work->field_8AE                  = 0;
        if (enemy->hp < 0) {
            Gp_SetStateF0Byte3(1);
        }
        work->field_8D0.flags |= 0x4000;
    }
    func_actor_401000_80132EF0(arg0);
    func_actor_401000_801323EC(((TmdObject*)arg0->extra)->coords, work->field_8F0, 0xC);
    func_actor_401000_801323EC(((TmdObject*)arg0->extra)->coords, work->field_A30, 0xC);
    if (work->field_89E == 0xA && (s16)func_actor_401000_80132590(((TmdObject*)arg0->extra)->coords, 0x12C, -0x57) != 0) {
        Actor401000_MoveForward(((TmdObject*)arg0->extra)->coords, -0x57);
    }
    ((TmdObject*)arg0->extra)->coords->flg = 0;
    if (work->flags_68.half & 1) {
        if (work->field_89E == 0xA) {
            work->field_89E = 0xB;
            work->field_898 = 2;
            func_actor_401000_80132EF0(arg0);
        }
        if ((work->flags_68.half & 1) && work->field_89E == 0xB) {
            work->field_8D0.flags |= 0x4000;
            if (enemy->hp > 0) {
                if (enemy->reactionFlags & 2) {
                    work->field_0 = 4;
                } else {
                    work->field_0 = 0x11;
                }
            } else {
                work->field_0 = 0x15;
            }
        }
    }
}

/// State 9 body, the 401000 twin of `func_actor_401300_80140300` and
/// `Actor01900_Fn09BE8`: on the live-actor flag, reset the two animation nodes,
/// the root coordinate and the model's facing, then hand the root to the
/// obstacle helper once per record table. The tail keys the actor's next state
/// (`field_0`) off `GpEnemy.hp` / `.reactionFlags` whenever the work block's
/// pending-request bit is up.
void func_actor_401000_80138BB4(Task* arg0)
{
    Actor401000Work* work;
    GpEnemy*         enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        ((TmdObject*)arg0->extra)->flags = 0;
        work->field_8D0.radius           = 0x1AE;
        work->field_B50.flags           &= 0x7FFF;
        work->field_A10.flags           |= 0x4000;
        enemy->node.flags                = 0;
        work->field_898                  = 1;
        work->field_89E                  = 0xC;
        work->field_8A2                  = 0x10;
        work->field_8B0                  = 0;
        work->field_8AE                  = 0;
        if (enemy->hp < 0) {
            Gp_SetStateF0Byte3(1);
        }
        work->field_8D0.flags |= 0x4000;
    }
    func_actor_401000_80132EF0(arg0);
    func_actor_401000_801323EC(((TmdObject*)arg0->extra)->coords, work->field_8F0, 0xC);
    func_actor_401000_801323EC(((TmdObject*)arg0->extra)->coords, work->field_A30, 0xC);
    ((TmdObject*)arg0->extra)->coords->flg = 0;
    if (work->flags_68.half & 1) {
        work->field_8D0.flags |= 0x4000;
        if (enemy->hp <= 0) {
            work->field_0 = 0x15;
        } else if (enemy->reactionFlags & 2) {
            work->field_0 = 4;
        } else {
            work->field_0 = 0x11;
        }
    }
}

/// Emergence tick: `field_6` counts up from the state's 0x18 seed and the
/// deltas dispatch the one-shot actions — release state F0 at 0x18, the
/// light-mode pair at 0x1D / 0x29, the 0x600A5 spawn at 0x1D, and the
/// `TmdObject.flags` writes at 0x2F / 0x3F. From 0x1A on the tail rebuilds
/// the actor's root coordinate: a Y rotation taken from the model root's
/// facing, scaled by 0x1194 less 0xB per tick past 0x14, written back through
/// `coord.m` with `flg` cleared so the local matrix is recomputed.
/// Same body as `Actor01900_Fn06904`, minus that one's 0x13 release argument
/// and with `field_A10.flags |= 0x4000` in place of its `&= 0xBFFF`.
void func_actor_401000_80138D08(Task* arg0)
{
    Actor401000Work*       work;
    GpEnemy*               enemy;
    TmdObject*             obj;
    GsCOORDINATE2*         coord;
    Actor401000RotScratch* blk;
    u8*                    head;
    u8*                    tail;
    void*                  scratch_base;
    s16                    temp_v0;
    s16                    ang;
    s16                    cur;
    s32                    k;
    s32                    sy;
    u16                    temp_v1;
    u16                    m22;

    work  = arg0->work;
    obj   = arg0->extra;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        obj->flags            = 0;
        work->field_B50.flags = (u16)(work->field_B50.flags & 0x7FFF);
        work->field_A10.flags = (u16)(work->field_A10.flags | 0x4000);
        enemy->node.flags     = 1;
        work->field_6         = 0;
    }
    temp_v1      = (u16)work->field_6;
    scratch_base = PSX_SCRATCH;
    if (work->field_6 < 0x401) {
        work->field_6 = (s16)(temp_v1 + 1);
        temp_v0       = temp_v1 - 0x18;
        switch (temp_v0) {
            case 0:
                Gp_ReleaseStateF0Add(arg0, 0xA);
                break;
            case 5:
                Gp_SetLightMode(enemy, 1);
                Gp_SpawnEff(0x600A5, ((TmdObject*)arg0->extra)->coords + 2, 3, NULL);
                break;
            case 23:
                ((TmdObject*)arg0->extra)->flags = 2;
                break;
            case 17:
                Gp_SetLightMode(enemy, 2);
                break;
            case 39:
                ((TmdObject*)arg0->extra)->flags = 0x80;
                break;
        }
        cur = work->field_6;
        if (cur >= 0x1A) {
            k                                        = 0x1194;
            head                                     = scratch_base;
            head                                     = *(u8**)(head + 0x3FC);
            coord                                    = ((TmdObject*)arg0->extra)->coords;
            blk                                      = (Actor401000RotScratch*)(head - 0x34);
            sy                                       = k - (cur - 0x14) * 0xB;
            *(Actor401000RotScratch**)G_SCRATCH_HEAD = blk;
            ang                                      = ratan2((s32)-coord->coord.m[2][0], (s32)coord->coord.m[2][2]);
            blk->angle                               = ang;
            Gfx_RotMatrixY(&blk->m, (s32)ang, 1);
            blk->scale.vx = k;
            blk->scale.vy = (s32)(s16)sy;
            blk->scale.vz = k;
            ScaleMatrix(&blk->m, &((Actor401000RotScratch*)(head - 0x34))->scale);
            coord->coord.m[0][0] = *(u16*)&((Actor401000RotScratch*)(head - 0x34))->m.m[0][0];
            coord->coord.m[0][1] = *(u16*)&blk->m.m[0][1];
            coord->coord.m[0][2] = *(u16*)&blk->m.m[0][2];
            coord->coord.m[1][0] = *(u16*)&blk->m.m[1][0];
            coord->coord.m[1][1] = *(u16*)&blk->m.m[1][1];
            coord->coord.m[1][2] = *(u16*)&blk->m.m[1][2];
            coord->coord.m[2][0] = *(u16*)&blk->m.m[2][0];
            coord->coord.m[2][1] = *(u16*)&blk->m.m[2][1];
            __asm__ volatile("lui %0, 0x1F80" : "=r"(tail));
            tail       = *(u8**)(tail + 0x3FC);
            m22        = *(u16*)&blk->m.m[2][2];
            coord->flg = 0;
            tail       = tail + 0x34;
            __asm__ volatile("sw %0, 0x1F8003FC" ::"r"(tail) : "memory");
            coord->coord.m[2][2] = m22;
        }
    }
}

/// Per-frame tick of the 0xE/0xF animation pair, the same shape as
/// `func_actor_401000_8013922C`. The live-actor arm allocates the model
/// buffers, restores the saved pose matrix `field_BA8` over the live
/// `field_BC8`, and restarts the 0xE / 0x898 animation slots; the body is then
/// gated on the `field_6` countdown and a 0-15 `Gp_LcgState` draw. The XZ
/// offset to `D_80073B8C` is probed against `field_C16`, and an armed
/// `Gp_StateF0` bit 0x50000, each dropping the actor to state 6. The tail runs
/// `func_actor_401000_80132EF0` and swaps `field_89E` between 0xE and 0xF on
/// `flags_68` bits 1 and 2, re-running the tick after each swap.
/// Same body as `func_actor_401300_80139520`, with the pose matrix in place of
/// that one's `field_C48` / `field_C68` pair and a `field_C16` radius in place
/// of its literal 3000.
void func_actor_401000_80138F50(Task* arg0)
{
    Actor401000Work* work;
    GpEnemy*         enemy;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    SVECTOR          delta;
    SVECTOR*         d;

    work  = arg0->work;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        obj        = arg0->extra;
        obj->flags = 0;
        Tmd_AllocBuffers(obj);
        work->field_8D0.radius = 0x1AE;
        work->field_B50.flags &= 0x7FFF;
        work->field_A10.flags |= 0x4000;
        enemy->node.flags      = 0;
        work->field_6          = 0;
        work->field_BC8        = work->field_BA8;
        work->field_89E        = 0xE;
        work->field_898        = 1;
        work->field_8A2        = work->field_8A4;
    }
    if (work->field_6 > 0x960) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if (!((Gp_LcgState >> 16) & 0xF)) {
            return;
        }
    } else {
        work->field_6 = (u16)work->field_6 + 1;
    }
    coord    = ((TmdObject*)arg0->extra)->coords;
    d        = &delta;
    delta.vx = D_80073B8C->t[0] - coord->coord.t[0];
    d->vy    = D_80073B8C->t[1] - coord->coord.t[1];
    d->vz    = D_80073B8C->t[2] - coord->coord.t[2];
    if (!Actor401000_OutOfRange(d, work->field_C16)) {
        work->field_0 = 6;
    }
    if (*(u32*)&Gp_StateF0 & 0x50000) {
        Gp_ArmStateF0(1);
        work->field_0 = 6;
    }
    func_actor_401000_80132EF0(arg0);
    if (work->field_89E == 0xE && (work->flags_68.half & 2)) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if ((Gp_LcgState >> 16) & 1) {
            work->field_89E = 0xF;
            work->field_898 = 1;
            func_actor_401000_80132EF0(arg0);
        }
    }
    if (work->field_89E == 0xF && (work->flags_68.half & 1)) {
        work->field_89E = 0xE;
        work->field_898 = 1;
        func_actor_401000_80132EF0(arg0);
    }
}

void func_actor_401000_8013922C(Task* arg0)
{
    Actor401000Work* work;
    GpEnemy*         enemy;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    SVECTOR          delta;
    SVECTOR*         d;
    s32              sound;
    s32              pan;

    work  = arg0->work;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        obj                     = arg0->extra;
        D_actor_401000_80154E88 = &D_actor_401000_80154634;
        work->field_89E         = 0x10;
        work->field_898         = 2;
        obj->flags              = 0;
        Tmd_AllocBuffers(obj);
        work->field_8D0.radius = 0x1AE;
        work->field_B50.flags &= 0x7FFF;
        work->field_A10.flags |= 0x4000;
        enemy->node.flags      = 0;
        work->field_8B0        = 0;
        work->field_8A2        = 0x10;
        work->field_8AE        = 0;
        work->field_6          = 0;
    } else if (work->field_6 == 0) {
        sound = ((enemy->placeKey >> 0xC) << 8) | 0x51030008;
        pan   = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
        work->field_6 = 1;
    }
    func_actor_401000_80132EF0(arg0);
    if ((work->field_5A & 0x3FF) == 4 && work->field_8B4 != (work->field_5A & 0x3FF)) {
        work->field_8B8.coord      = ((TmdObject*)arg0->extra)->coords + 1;
        work->field_8B8.spawnArgLo = 0x100;
        work->field_8B8.spawnArgHi = 2;
        func_800FDB18((u16)Gp_GetIdParam1(0x1001), ((TmdObject*)arg0->extra)->coords + 5, NULL, &work->field_8B8);
    }
    work->field_8B4 = work->field_5A & 0x3FF;
    coord           = ((TmdObject*)arg0->extra)->coords;
    d               = &delta;
    delta.vx        = D_80073B8C->t[0] - coord->coord.t[0];
    d->vy           = D_80073B8C->t[1] - coord->coord.t[1];
    d->vz           = D_80073B8C->t[2] - coord->coord.t[2];
    if (!Actor401000_OutOfRange(d, work->field_C16)) {
        SndEvt_EnqueueType7(0x51030008, 1);
        Gp_ArmStateF0(1);
        work->field_0 = 6;
    }
    if (*(u32*)&Gp_StateF0 & 0x50000) {
        Gp_ArmStateF0(1);
        work->field_0 = 6;
    }
}

/// Walk the actor along its `field_C` waypoint pair: `field_14` picks the
/// waypoint the offset is taken from and flips once the actor closes inside
/// 0xA0 of it, or after 0x15 frames in `field_6`, and the wrapped yaw toward
/// that waypoint is clamped to +-0x20, added back to the facing yaw and the
/// root rotation rescaled by 0x1194. The `func_actor_401000_80132590` probe
/// takes one 0xA step forward, the obstacle walk runs against `field_A30`
/// (plus `field_8F0` through `func_actor_401000_80135704` when the spawn
/// sub-type is 0x10), and each arm counts `field_6` up while the yaw stays
/// inside 0x80.
/// The tail drops the actor to state 6 on the `Player_Status` range checks and
/// the `Gp_StateF0` bits, and the live-actor arm restarts the 0x1AE clip.
void func_actor_401000_801394EC(Task* arg0)
{
    Actor401000Work*        work;
    Actor401000TurnScratch* turn;
    TmdObject*              obj;
    GpRec18*                rec;
    GsCOORDINATE2*          coord;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                     = arg0->extra;
        ((GpEnemy*)arg0->spawnArg2)->node.flags = 0;
        obj->flags                              = 0;
        Tmd_AllocBuffers(obj);
        work->field_8D0.radius = 0x1AE;
        work->field_898        = 1;
        work->field_8A2        = 0x10;
        work->field_89E        = 2;
        work->field_89A        = 0;
        work->field_B50.flags &= 0x7FFF;
        work->field_A10.flags |= 0x4000;
        func_actor_401000_80132EF0(arg0);
        work->field_6 = 0;
        if ((arg0->spawnArg1 >> 16) == 0x10) {
            work->field_8D0.flags |= 0x4000;
        }
    } else {
        *(Actor401000TurnScratch**)G_SCRATCH_HEAD -= 1;
        turn                                       = *(Actor401000TurnScratch**)G_SCRATCH_HEAD;
        turn->delta.vx                             = work->field_C[work->field_14].x - ((TmdObject*)arg0->extra)->coords->coord.t[0];
        turn->delta.vy                             = 0;
        turn->delta.vz                             = work->field_C[work->field_14].z - ((TmdObject*)arg0->extra)->coords->coord.t[2];
        if (!Actor401000_OutOfRange(&turn->delta, 0xA0) || work->field_6 >= 0x15) {
            if (work->field_14 == 0) {
                work->field_14 = 1;
            } else {
                work->field_14 = 0;
            }
            work->field_6 = 0;
        }
        func_actor_401000_80132EF0(arg0);
        coord           = ((TmdObject*)arg0->extra)->coords;
        turn->angle     = Actor401000_NormalizeYaw(ratan2(turn->delta.vx, turn->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
        work->field_8AE = turn->angle;
        if (turn->angle > 0x20) {
            turn->angle = 0x20;
        }
        if (turn->angle < -0x20) {
            turn->angle = -0x20;
        }
        turn->angle += ratan2(-((TmdObject*)arg0->extra)->coords->coord.m[2][0], ((TmdObject*)arg0->extra)->coords->coord.m[2][2]);
        Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->coords->coord, turn->angle, 1);
        Actor401000_RescaleYaw(((TmdObject*)arg0->extra)->coords, 0x1194);
        if (work->field_89A == 0 && (func_actor_401000_80132590(((TmdObject*)arg0->extra)->coords, 0x12C, 0xA) << 16) != 0) {
            Actor401000_MoveForward(((TmdObject*)arg0->extra)->coords, 0xA);
        }
        if ((arg0->spawnArg1 >> 16) != 0x10) {
            if (func_actor_401000_801323EC(((TmdObject*)arg0->extra)->coords, work->field_A30, 0xC) == 1) {
                if (ABS(work->field_8AE) < 0x80) {
                    work->field_6 = (u16)work->field_6 + 1;
                }
            }
        } else {
            rec = work->field_8F0;
            if (func_actor_401000_801323EC(((TmdObject*)arg0->extra)->coords, rec, 0xC) != 1 && func_actor_401000_801323EC(((TmdObject*)arg0->extra)->coords, work->field_A30, 0xC) != 1) {
                func_actor_401000_80135704(arg0, rec, 0xC);
            } else {
                if (ABS(work->field_8AE) < 0x80) {
                    work->field_6 = (u16)work->field_6 + 1;
                }
            }
        }
        ((TmdObject*)arg0->extra)->coords->flg = 0;
        if (func_actor_401000_80132824(arg0) != 1) {
            Actor401000_ConfigPositionDelta(&Player_Status, ((TmdObject*)arg0->extra)->coords, &turn->delta);
            if (!Actor401000_OutOfRange(&turn->delta, work->field_C16)) {
                work->field_0 = 6;
            } else if (!Actor401000_OutOfRange(&turn->delta, 0xFA0)) {
                coord       = ((TmdObject*)arg0->extra)->coords;
                turn->angle = Actor401000_NormalizeYaw(ratan2(turn->delta.vx, turn->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
                if (ABS(turn->angle) < 0x300) {
                    work->field_0 = 6;
                }
            }
        }
        if (*(u32*)&Gp_StateF0 & 0xD0000) {
            work->field_0 = 6;
        }
        *(Actor401000TurnScratch**)G_SCRATCH_HEAD += 1;
    }
}

/// Walk the actor at the player: on the live-actor flag it restarts the
/// 0x12 clip and clears the spawn pose, then takes a 0xC-byte `G_SCRATCH_HEAD`
/// turn block, aims it at `Player_Status.coordMtx` through
/// `Actor401000_PositionYaw`, clamps the turn to +-0x40 and adds the facing
/// yaw back in before rebuilding the root coordinate. The obstacle walk
/// `func_actor_401000_801323EC` runs against `field_A30` and hands
/// `field_8F0` to `func_actor_401000_80135704` when it reports a hit, the
/// `func_actor_401000_80132590` probe takes one forward step out of
/// `field_C04`, and that same countdown then runs down by 0xA a frame. The
/// tail drops the actor to state 9 on the `flags_68` bit or once the
/// countdown is spent.
void func_actor_401000_80139D10(Task* arg0)
{
    Actor401000Work*        work;
    GpEnemy*                enemy;
    TmdObject*              obj;
    GsCOORDINATE2*          coord;
    Actor401000TurnScratch* turn;

    work = arg0->work;
    if (work->field_4 != 0) {
        enemy           = arg0->spawnArg2;
        obj             = arg0->extra;
        work->field_89E = 0x12;
        work->field_898 = 1;
        obj->flags      = 0;
        Tmd_AllocBuffers(obj);
        work->field_8D0.radius = 0x1AE;
        work->field_B50.flags &= 0x7FFF;
        work->field_A10.flags |= 0x4000;
        enemy->node.flags      = 0;
        work->field_8B0        = 0;
        work->field_8A2        = 0x1E;
    }
    *(Actor401000TurnScratch**)G_SCRATCH_HEAD -= 1;
    turn                                       = *(Actor401000TurnScratch**)G_SCRATCH_HEAD;
    turn->angle                                = Actor401000_PositionYaw(arg0, &turn->delta, &Player_Status);
    work->field_8AE                            = turn->angle;
    if (turn->angle > 0x40) {
        turn->angle = 0x40;
    }
    if (turn->angle < -0x40) {
        turn->angle = -0x40;
    }
    coord        = ((TmdObject*)arg0->extra)->coords;
    turn->angle += ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->coords->coord, turn->angle, 1);
    if (func_actor_401000_801323EC(((TmdObject*)arg0->extra)->coords, work->field_A30, 0xC) != 1) {
        func_actor_401000_80135704(arg0, work->field_8F0, 0xC);
    }
    if ((func_actor_401000_80132590(((TmdObject*)arg0->extra)->coords, 0x12C, work->field_C04) << 0x10) != 0) {
        Actor401000_MoveForwardNonzero(((TmdObject*)arg0->extra)->coords, (u16)work->field_C04);
    }
    if (work->field_C04 > 0) {
        work->field_C04 = (u16)work->field_C04 - 0xA;
        if (work->field_C04 < 0) {
            work->field_C04 = 0;
        }
    }
    func_actor_401000_80132EF0(arg0);
    if ((work->flags_68.half & 1) || work->field_C04 == 0) {
        work->field_0 = 9;
    }
    *(Actor401000TurnScratch**)G_SCRATCH_HEAD += 1;
}

/// Aim step toward the player, the `func_actor_401300_8013A5C0` twin. Unlike
/// `func_actor_401000_80134F98` the wrapped turn is halved and clamped to
/// +-0x80 instead of +-0x10, so the actor turns at half speed and only the
/// 0x16/2 -> 0x16/0x11 spawn pair keys the follow-up; the spawn arm writes
/// `field_8D0.field_1C` first and leaves `field_6` alone, and the exit turn
/// reads the sign of `field_8AE` with the `0x4B0` arm first.
void func_actor_401000_8013A0C8(Task* arg0)
{
    Actor401000Work*       work;
    Actor401000AimScratch* aim;
    TmdObject*             obj;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                     = arg0->extra;
        ((GpEnemy*)arg0->spawnArg2)->node.flags = 0;
        obj->flags                              = 0;
        Tmd_AllocBuffers(obj);
        work->field_8D0.radius = 0x1AE;
        work->field_898        = 1;
        work->field_8A2        = 0x16;
        work->field_89E        = 2;
        work->field_89A        = 0;
        work->field_B50.flags &= 0x7FFF;
        work->field_A10.flags |= 0x4000;
        func_actor_401000_80132EF0(arg0);
        return;
    }
    func_actor_401000_80132EF0(arg0);
    *(Actor401000AimScratch**)G_SCRATCH_HEAD -= 1;
    aim                                       = *(Actor401000AimScratch**)G_SCRATCH_HEAD;
    aim->angle                                = Actor401000_PositionYaw(arg0, &aim->delta, &Player_Status);
    work->field_8AE                           = aim->angle;
    if (ABS(aim->angle) < 0x81 && work->field_89E == 2) {
        work->field_8A2 = 0x16;
        work->field_89E = 0x11;
        work->field_898 = 1;
        work->field_6   = 0;
        func_actor_401000_80132EF0(arg0);
    }
    if (aim->angle >= 0x81) {
        aim->angle = 0x80;
    }
    if (aim->angle < -0x80) {
        aim->angle = -0x80;
    } else {
        aim->angle = aim->angle >> 1;
    }
    aim->angle += ratan2(-((TmdObject*)arg0->extra)->coords->coord.m[2][0], ((TmdObject*)arg0->extra)->coords->coord.m[2][2]);
    Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->coords->coord, aim->angle, 1);
    Actor401000_RescaleYaw(((TmdObject*)arg0->extra)->coords, 0x1194);
    ((TmdObject*)arg0->extra)->coords->flg = 0;
    if (work->field_89E == 0x11) {
        work->field_6++;
        if ((func_actor_401000_80132590(((TmdObject*)arg0->extra)->coords, 0x12C, -0x10) << 16) != 0) {
            Actor401000_MoveForward(((TmdObject*)arg0->extra)->coords, -0x10);
        }
        if (func_actor_401000_801323EC(((TmdObject*)arg0->extra)->coords, work->field_A30, 0xC) != 1) {
            func_actor_401000_80135704(arg0, work->field_8F0, 0xC);
        }
        ((TmdObject*)arg0->extra)->coords->flg = 0;
        if ((s16)work->field_6 >= 0x13) {
            if (work->field_8AE <= 0) {
                Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->coords->coord, 0x4B0, 0);
            } else {
                Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->coords->coord, -0x4B0, 0);
            }
            work->field_0 = 7;
        }
    }
    *(Actor401000AimScratch**)G_SCRATCH_HEAD += 1;
}

/// Aim step toward the player: the same body as `func_actor_401000_80134F98`
/// with three differences. The wrapped turn is clamped to zero-or-negative
/// rather than +-0x10, so the actor only ever rotates one way; the animation
/// slot is 0x13 instead of 9 and `field_8D0.field_1C` is written before the
/// other state words; and the spawn arm clears the `field_6` latch on its way
/// out instead of arming state F0.
void func_actor_401000_8013A5F0(Task* arg0)
{
    Actor401000Work*       work;
    Actor401000AimScratch* aim;
    TmdObject*             obj;
    GsCOORDINATE2*         coord;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                     = arg0->extra;
        ((GpEnemy*)arg0->spawnArg2)->node.flags = 0;
        obj->flags                              = 0;
        Tmd_AllocBuffers(obj);
        work->field_8D0.radius = 0x1AE;
        work->field_898        = 1;
        work->field_8A2        = 0x10;
        work->field_89E        = 0x13;
        work->field_89A        = 0;
        work->field_B50.flags &= 0x7FFF;
        work->field_A10.flags |= 0x4000;
        func_actor_401000_80132EF0(arg0);
        work->field_6 = 0;
        return;
    }
    work->field_6                            += 1;
    *(Actor401000AimScratch**)G_SCRATCH_HEAD -= 1;
    aim                                       = *(Actor401000AimScratch**)G_SCRATCH_HEAD;
    ((TmdObject*)arg0->extra)->coords->flg    = 0;
    if (work->flags_68.half & 1) {
        work->field_0 = 7;
    }
    aim->angle      = Actor401000_PositionYaw(arg0, &aim->delta, &Player_Status);
    work->field_8AE = aim->angle;
    if (aim->angle > 0) {
        aim->angle = 0;
    }
    if (aim->angle < 0) {
        aim->angle = 0;
    }
    coord       = ((TmdObject*)arg0->extra)->coords;
    aim->angle += ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->coords->coord, aim->angle, 1);
    Actor401000_RescaleYaw(((TmdObject*)arg0->extra)->coords, 0x1194);
    func_actor_401000_80132EF0(arg0);
    *(Actor401000AimScratch**)G_SCRATCH_HEAD += 1;
}

/// Turn the actor toward the player in two stages: while the `field_6`
/// countdown is under 0x32 the five part coordinates are reset to fixed
/// pitches, and afterwards each one unwinds by shifting its pitch down a step
/// every four frames; the turn itself is clamped to +-0x24 and drops the actor
/// to state 7 once it lines up. The `field_8AE` slot it drives is what
/// `func_actor_401000_8013A5F0` writes whole; here it slides toward the target
/// by at most 0x28 a frame. Same body as `func_actor_401300_8013AE48`, with the
/// spawn arm arming the 0x13 clip, `field_8D0.field_1C` written before the other
/// state words, and `field_A10.flags |= 0x4000` in place of the sibling's
/// `&= 0xBFFF`.
void func_actor_401000_8013A930(Task* arg0)
{
    Actor401000Work*       work;
    Actor401000AimScratch* aim;
    TmdObject*             obj;
    GsCOORDINATE2*         coord;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                     = arg0->extra;
        ((GpEnemy*)arg0->spawnArg2)->node.flags = 0;
        obj->flags                              = 0;
        Tmd_AllocBuffers(obj);
        work->field_8D0.radius = 0x1AE;
        work->field_898        = 2;
        work->field_8A2        = 8;
        work->field_89E        = 0x13;
        work->field_89A        = 0;
        work->field_B50.flags &= 0x7FFF;
        work->field_A10.flags |= 0x4000;
        func_actor_401000_80132EF0(arg0);
        func_actor_401000_80132EF0(arg0);
        work->field_6   = 0;
        work->field_8B0 = 0;
        return;
    }
    work->field_6++;
    *(Actor401000AimScratch**)G_SCRATCH_HEAD -= 1;
    aim                                       = *(Actor401000AimScratch**)G_SCRATCH_HEAD;
    aim->angle                                = Actor401000_PositionYaw(arg0, &aim->delta, &Player_Status);
    if (work->field_8AE < aim->angle) {
        if (aim->angle - work->field_8AE > 0x28) {
            work->field_8AE += 0x28;
        } else {
            work->field_8AE = aim->angle;
        }
    } else if (work->field_8AE - aim->angle > 0x28) {
        work->field_8AE -= 0x28;
    } else {
        work->field_8AE = aim->angle;
    }
    coord      = ((TmdObject*)arg0->extra)->coords;
    aim->angle = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->coords->coord, aim->angle, 1);
    Actor401000_RescaleYaw(((TmdObject*)arg0->extra)->coords, 0x1194);
    func_actor_401000_80132EF0(arg0);
    if (work->field_6 < 0x32) {
        Gfx_RotMatrixX(&((TmdObject*)arg0->extra)->coords[1].coord, 0x40, 0);
        ((TmdObject*)arg0->extra)->coords[1].flg = 0;
        Gp_UpdateCoord(&((TmdObject*)arg0->extra)->coords[1]);
        Gfx_RotMatrixX(&((TmdObject*)arg0->extra)->coords[2].coord, 0x80, 0);
        ((TmdObject*)arg0->extra)->coords[2].flg = 0;
        Gp_UpdateCoord(&((TmdObject*)arg0->extra)->coords[2]);
        Gfx_RotMatrixX(&((TmdObject*)arg0->extra)->coords[3].coord, 0x80, 0);
        ((TmdObject*)arg0->extra)->coords[3].flg = 0;
        Gp_UpdateCoord(&((TmdObject*)arg0->extra)->coords[3]);
        Gfx_RotMatrixX(&((TmdObject*)arg0->extra)->coords[4].coord, 0x80, 0);
        ((TmdObject*)arg0->extra)->coords[4].flg = 0;
        Gp_UpdateCoord(&((TmdObject*)arg0->extra)->coords[4]);
        Gfx_RotMatrixX(&((TmdObject*)arg0->extra)->coords[5].coord, 0x100, 0);
        ((TmdObject*)arg0->extra)->coords[4].flg = 0;
        Gp_UpdateCoord(&((TmdObject*)arg0->extra)->coords[4]);
    } else {
        Gfx_RotMatrixX(&((TmdObject*)arg0->extra)->coords[1].coord, 0x40 >> ((work->field_6 - 0x31) / 4), 0);
        ((TmdObject*)arg0->extra)->coords[1].flg = 0;
        Gp_UpdateCoord(&((TmdObject*)arg0->extra)->coords[1]);
        Gfx_RotMatrixX(&((TmdObject*)arg0->extra)->coords[2].coord, 0x80 >> ((work->field_6 - 0x30) / 4), 0);
        ((TmdObject*)arg0->extra)->coords[2].flg = 0;
        Gp_UpdateCoord(&((TmdObject*)arg0->extra)->coords[2]);
        Gfx_RotMatrixX(&((TmdObject*)arg0->extra)->coords[3].coord, 0x80 >> ((work->field_6 - 0x2F) / 4), 0);
        ((TmdObject*)arg0->extra)->coords[3].flg = 0;
        Gp_UpdateCoord(&((TmdObject*)arg0->extra)->coords[3]);
        Gfx_RotMatrixX(&((TmdObject*)arg0->extra)->coords[4].coord, 0x80 >> ((work->field_6 - 0x2E) / 4), 0);
        ((TmdObject*)arg0->extra)->coords[4].flg = 0;
        Gp_UpdateCoord(&((TmdObject*)arg0->extra)->coords[4]);
        Gfx_RotMatrixX(&((TmdObject*)arg0->extra)->coords[5].coord, 0x100 >> ((work->field_6 - 0x31) / 4), 0);
        ((TmdObject*)arg0->extra)->coords[4].flg = 0;
        Gp_UpdateCoord(&((TmdObject*)arg0->extra)->coords[4]);
        aim->angle = Actor401000_PositionYaw(arg0, &aim->delta, &Player_Status);
        if (aim->angle > 0x24) {
            aim->angle = 0x24;
        } else if (aim->angle < -0x24) {
            aim->angle = -0x24;
        }
        if (ABS(aim->angle) < 0x24 || work->field_6 >= 0x4F) {
            work->field_0 = 7;
        }
        coord       = ((TmdObject*)arg0->extra)->coords;
        aim->angle += ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
        Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->coords->coord, aim->angle, 1);
        Actor401000_RescaleYaw(((TmdObject*)arg0->extra)->coords, 0x1194);
        ((TmdObject*)arg0->extra)->coords->flg = 0;
    }
    *(Actor401000AimScratch**)G_SCRATCH_HEAD += 1;
}

/// Tint a freshly spawned effect model from the enemy's area record: read the
/// session's location key, let `Gp_SyncAreaKeyIndex` fill its table index, and
/// copy the room's texture page / CLUT row into the model's `TmdObject`. The
/// same body `func_actor_302600_80165A6C` writes out inline and
/// `Actor401300_TintEffect` keeps as a helper; it has to be a helper here too
/// rather than four copies in the caller, because each inline expansion gets
/// its own `raw` / `model` / `idx` pseudos and the copies then share `$s0`
/// block by block, while four uses of one variable rank `model` (96 refs/insn
/// over 104) above `idx` (24 over 28) in `global.c` and hand it `$s0`.
static __inline__ void Actor401000_TintEffect(GpEffWork* eff, GpEnemy* enemy)
{
    GpAreaKey    key;
    GpAreaKey*   sessionKey;
    u8           areaByte0;
    GpAreaRec*   rec;
    GpAreaPlace* entry;
    TmdObject*   model;
    s32          idx;
    u32          raw;

    if (eff != NULL) {
        sessionKey = (GpAreaKey*)&gGameSession->at4.loc;
        raw        = enemy->placeKey;
        model      = (TmdObject*)eff->task->extra;
        key.stage  = sessionKey->stage;
        key.area   = sessionKey->area;
        key.room   = sessionKey->room;
        areaByte0  = gGameSession->at4.loc.view;
        idx        = raw >> 12;
        key.view   = areaByte0;
        Gp_SyncAreaKeyIndex(&key);
        rec = Gp_GetNestedAreaRec(&key);
        /* offset + base, not `&rec->field_0[idx]`: the ROM adds the scaled
           index onto the table (`addu s0, s0, v0`). */
        entry        = (GpAreaPlace*)((idx << 4) + (s32)rec->field_0);
        model->tpage = entry->tpage;
        model->clut  = entry->clut;
        if (model->buffer != NULL) {
            tmdProcessStream(model);
            tmdProcessStream(model);
        }
    }
}

/// Clip-0x2D body: on the live-actor flag it resets the effect node and the
/// spawn offset, then walks the animation latch `field_6` from 0 to 0x3D and
/// spawns one effect per key frame, each tinted by `Actor401000_TintEffect`.
/// At 0x3D the actor returns to state 0. The 401000 twin of
/// `func_actor_401300_8013B6E8`: same five clips, three of them at the same
/// node offsets (`+1`, `+9`, `+12`, `+1`, `+3` off the root coordinate) and the
/// same 0x64/0/0 spawn vector, but it reads the offset from the work block
/// rather than a stack `SVECTOR` and has no `field_D20` guard on the tail.
void func_actor_401000_8013B1E4(Task* arg0)
{
    Actor401000Work* work;
    GpEnemy*         enemy;
    u16              next;

    work  = arg0->work;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        ((TmdObject*)arg0->extra)->flags = 0x80;
        work->field_8D0.radius           = 0x1AE;
        work->field_A10.flags           |= 0x4000;
        enemy->node.flags                = 1;
        work->field_8AE                  = 0;
        work->field_6                    = 0;
        work->field_8C0.vx               = 0x64;
        work->field_8C0.vz               = 0;
        work->field_8C0.vy               = 0;
        Gp_SpawnEff(0x60030, ((TmdObject*)arg0->extra)->coords + 1, 0x10300, &work->field_8C0);
        Gp_ReleaseStateF0Add(arg0, 0xA);
    }
    next          = work->field_6 + 1;
    work->field_6 = next;
    if ((s16)next == 3) {
        D_80114B78[0]      = &D_actor_401000_80143EB4;
        work->field_8C0.vz = 0x64;
        work->field_8C0.vy = 0;
        work->field_8C0.vx = 0;
        Actor401000_TintEffect(Gp_SpawnEff(0xA0005, ((TmdObject*)arg0->extra)->coords + 9, 0x200, &work->field_8C0), enemy);
    }
    if ((s16)work->field_6 == 5) {
        D_80114B78[0]      = &D_actor_401000_80144830;
        work->field_8C0.vy = 0;
        work->field_8C0.vx = 0;
        Actor401000_TintEffect(Gp_SpawnEff(0xA0000 | 5, ((TmdObject*)arg0->extra)->coords + 12, 0x200, &work->field_8C0), enemy);
    }
    if ((s16)work->field_6 == 7) {
        D_80114B78[0] = &D_actor_401000_80146190;
        Actor401000_TintEffect(Gp_SpawnEff(0xA0005, ((TmdObject*)arg0->extra)->coords + 1, 0x200, NULL), enemy);
    }
    if ((s16)work->field_6 == 8) {
        D_80114B78[0] = &D_actor_401000_8014599C;
        Actor401000_TintEffect(Gp_SpawnEff(0xA0005, ((TmdObject*)arg0->extra)->coords + 3, 0x200, NULL), enemy);
    }
    if ((s16)work->field_6 >= 0x3D) {
        work->field_0 = 0;
    }
}

/// Rebuild `coord`'s Y rotation from its current yaw at unit scale, the same
/// body as `Actor401300_ResetYaw` / `Actor01900_ResetYaw` (`coord->coord.m` is
/// splatted back from the rotation scratch block and `flg` cleared so the local
/// matrix is recomputed). The state-0x1A body below walks it down the model
/// root's node run from `+2` to `+10`.
static __inline__ void Actor401000_ResetYaw(GsCOORDINATE2* coord)
{
    void*                  head;
    Actor401000RotScratch* blk;
    s16                    ang;

    head                    = *(void**)G_SCRATCH_HEAD;
    blk                     = (Actor401000RotScratch*)((u8*)head - 0x34);
    *(void**)G_SCRATCH_HEAD = blk;

    ang        = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    blk->angle = ang;
    Gfx_RotMatrixY(&blk->m, ang, 1);
    blk->scale.vz = 1;
    blk->scale.vy = 1;
    blk->scale.vx = 1;
    ScaleMatrix(&blk->m, &blk->scale);

    coord->coord.m[0][0]    = *(u16*)&blk->m.m[0][0];
    coord->coord.m[0][1]    = *(u16*)&blk->m.m[0][1];
    coord->coord.m[0][2]    = *(u16*)&blk->m.m[0][2];
    coord->coord.m[1][0]    = *(u16*)&blk->m.m[1][0];
    coord->coord.m[1][1]    = *(u16*)&blk->m.m[1][1];
    coord->coord.m[1][2]    = *(u16*)&blk->m.m[1][2];
    coord->coord.m[2][0]    = *(u16*)&blk->m.m[2][0];
    coord->coord.m[2][1]    = *(u16*)&blk->m.m[2][1];
    coord->coord.m[2][2]    = *(u16*)&blk->m.m[2][2];
    coord->flg              = 0;
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x34;
}

/// State-2 clip body and its 0x1A successor, the 401000 twin of
/// `func_actor_401300_8013BB30` and `Actor01900_Fn0892C`. On the live-actor flag
/// it arms the effect node, seeds the 0x8C0 spawn offset and the animation
/// slots, and spawns clip 0x60030. `field_6` then counts up under `field_89E`:
/// the state-2 arm waits 0x10 frames on `flags_68` bit 2 before switching to
/// 0x1A, runs the `0x12C`/0xA range probe and the `field_A30` obstacle slide,
/// and spawns the three tinted key-frame effects at counts 3, 5 and 6; the
/// state-0x1A arm gates on `flags_68` bit 0x100, dispatches the one-shot actions
/// off `field_6 - 0x19`, and from 0x1A on rebuilds the root coordinate through
/// `ratan2` at scale `0x1194 - (field_6 - 0x14) * 0xB`. Both arms end in
/// `func_actor_401000_80132EF0` and `Actor401000_ResetYaw` on nodes 2..10.
void func_actor_401000_8013B61C(Task* arg0)
{
    Actor401000Work*       work;
    GpEnemy*               enemy;
    GsCOORDINATE2*         coord;
    Actor401000RotScratch* blk;
    u8*                    head;
    u8*                    tail;
    void*                  scratch_base;
    u16                    next;
    s16                    ang;
    s16                    cur;
    s32                    k;
    s32                    sy;
    u16                    m22;

    work         = arg0->work;
    enemy        = arg0->spawnArg2;
    scratch_base = PSX_SCRATCH;
    if (work->field_4 != 0) {
        work->field_8D0.radius = 0x1AE;
        work->field_A10.flags |= 0x4000;
        enemy->node.flags      = 1;
        work->field_8C0.vx     = 0x64;
        work->field_89E        = 2;
        work->field_898        = 1;
        work->field_8AE        = 0;
        work->field_6          = 0;
        work->field_8C0.vz     = 0;
        work->field_8C0.vy     = 0;
        work->field_8A2        = 0x10;
        Gp_SpawnEff(0x60030, ((TmdObject*)arg0->extra)->coords + 1, 0x10300, &work->field_8C0);
        work->field_6 = 0;
    }
    next          = work->field_6 + 1;
    work->field_6 = next;
    switch (work->field_89E) {
        case 2:
            if ((s16)next >= 0x10 && (work->flags_68.half & 2)) {
                work->field_89E = 0x1A;
                work->field_898 = 2;
                work->field_8A2 = 0x10;
                work->field_89A = 0;
            }
            if ((s16)func_actor_401000_80132590(((TmdObject*)arg0->extra)->coords, 0x12C, 0xA) != 0) {
                Actor401000_MoveForward(((TmdObject*)arg0->extra)->coords, 0xA);
            }
            func_actor_401000_801323EC(((TmdObject*)arg0->extra)->coords, work->field_A30, 0xC);
            if ((s16)work->field_6 == 3) {
                D_80114B78[0]      = &D_actor_401000_80143EB4;
                work->field_8C0.vz = 0x64;
                work->field_8C0.vy = 0;
                work->field_8C0.vx = 0;
                Actor401000_TintEffect(Gp_SpawnEff(0xA0005, ((TmdObject*)arg0->extra)->coords + 9, 0x200, &work->field_8C0), enemy);
            }
            if ((s16)work->field_6 == 5) {
                D_80114B78[0] = &D_actor_401000_80146190;
                Actor401000_TintEffect(Gp_SpawnEff(0xA0000 | 5, ((TmdObject*)arg0->extra)->coords + 1, 0x200, NULL), enemy);
            }
            if ((s16)work->field_6 == 6) {
                D_80114B78[0] = &D_actor_401000_8014599C;
                Actor401000_TintEffect(Gp_SpawnEff(0xA0005, ((TmdObject*)arg0->extra)->coords + 3, 0x200, NULL), enemy);
            }
            break;
        case 0x1A:
            if (!(work->flags_68.half & 0x100)) {
                work->field_6 = 0;
            }
            switch ((s16)(work->field_6 - 0x19)) {
                case 0:
                    Gp_ReleaseStateF0Add(arg0, 0xA);
                    break;
                case 5:
                    Gp_SetLightMode(enemy, 1);
                    Gp_SpawnEff(0x600A5, ((TmdObject*)arg0->extra)->coords + 2, 2, NULL);
                    break;
                case 23:
                    ((TmdObject*)arg0->extra)->flags = 2;
                    break;
                case 17:
                    Gp_SetLightMode(enemy, 2);
                    break;
                case 39:
                    ((TmdObject*)arg0->extra)->flags = 0x80;
                    work->field_0                    = 0;
                    break;
            }
            cur = work->field_6;
            if (cur >= 0x1A) {
                k                                        = 0x1194;
                head                                     = scratch_base;
                head                                     = *(u8**)(head + 0x3FC);
                coord                                    = ((TmdObject*)arg0->extra)->coords;
                blk                                      = (Actor401000RotScratch*)(head - 0x34);
                sy                                       = k - (cur - 0x14) * 0xB;
                *(Actor401000RotScratch**)G_SCRATCH_HEAD = blk;
                ang                                      = ratan2((s32)-coord->coord.m[2][0], (s32)coord->coord.m[2][2]);
                blk->angle                               = ang;
                Gfx_RotMatrixY(&blk->m, (s32)ang, 1);
                blk->scale.vx = k;
                blk->scale.vy = (s32)(s16)sy;
                blk->scale.vz = k;
                ScaleMatrix(&blk->m, &((Actor401000RotScratch*)(head - 0x34))->scale);
                coord->coord.m[0][0] = *(u16*)&((Actor401000RotScratch*)(head - 0x34))->m.m[0][0];
                coord->coord.m[0][1] = *(u16*)&blk->m.m[0][1];
                coord->coord.m[0][2] = *(u16*)&blk->m.m[0][2];
                coord->coord.m[1][0] = *(u16*)&blk->m.m[1][0];
                coord->coord.m[1][1] = *(u16*)&blk->m.m[1][1];
                coord->coord.m[1][2] = *(u16*)&blk->m.m[1][2];
                coord->coord.m[2][0] = *(u16*)&blk->m.m[2][0];
                coord->coord.m[2][1] = *(u16*)&blk->m.m[2][1];
                __asm__ volatile("lui %0, 0x1F80" : "=r"(tail));
                tail       = *(u8**)(tail + 0x3FC);
                m22        = *(u16*)&blk->m.m[2][2];
                coord->flg = 0;
                tail       = tail + 0x34;
                __asm__ volatile("sw %0, 0x1F8003FC" ::"r"(tail) : "memory");
                coord->coord.m[2][2] = m22;
            }
            break;
    }
    func_actor_401000_80132EF0(arg0);
    Actor401000_ResetYaw(((TmdObject*)arg0->extra)->coords + 2);
    Actor401000_ResetYaw(((TmdObject*)arg0->extra)->coords + 3);
    Actor401000_ResetYaw(((TmdObject*)arg0->extra)->coords + 4);
    Actor401000_ResetYaw(((TmdObject*)arg0->extra)->coords + 5);
    Actor401000_ResetYaw(((TmdObject*)arg0->extra)->coords + 6);
    Actor401000_ResetYaw(((TmdObject*)arg0->extra)->coords + 7);
    Actor401000_ResetYaw(((TmdObject*)arg0->extra)->coords + 8);
    Actor401000_ResetYaw(((TmdObject*)arg0->extra)->coords + 9);
    Actor401000_ResetYaw(((TmdObject*)arg0->extra)->coords + 10);
}

/// State-2 aim body, the 401000 twin of `func_actor_401300_8013CBAC` and
/// `Actor01900_Fn042BC`: on the live-actor flag it resets the effect node, forks
/// the first clip and seeds the animation slots, then walks both obstacle tables
/// and aims the actor at the player with `Gfx_RotMatrixY` / `Actor401000_RescaleYaw`.
/// `field_6` and `field_8` then count up under the `func_actor_401000_80132824`
/// clip test: the still-aiming arm re-wraps the turn, drops the actor to state
/// 0xB once the 0x44C range check fails inside 0x200 and re-arms at 0x1B past
/// 0x5B frames, while the settled arm draws a turn direction from `Gp_LcgState`
/// and flips it every 0xF1 frames. The tail takes one forward step off the
/// 0x12C probe, or re-arms the clip on the `flags_68` bit. `field_C1B` counts
/// down once per entry.
void func_actor_401000_8013C46C(Task* arg0)
{
    Actor401000Work*       work;
    TmdObject*             obj;
    GsCOORDINATE2*         coord;
    GsCOORDINATE2*         facing;
    Actor401000AimScratch* head;
    Actor401000AimScratch* s;
    s16                    yaw;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                     = arg0->extra;
        ((GpEnemy*)arg0->spawnArg2)->node.flags = 0;
        obj->flags                              = 0;
        Tmd_AllocBuffers(obj);
        work->field_8D0.radius = 0x1AE;
        work->field_898        = 1;
        work->field_8A2        = 0x24;
        work->field_89E        = 2;
        work->field_89A        = 0;
        work->field_C24        = 0;
        work->field_B50.flags &= 0x7FFF;
        work->field_A10.flags |= 0x4000;
        Gp_ArmStateF0(1);
        work->field_6 = 0;
        work->field_8 = 0;
        if ((arg0->spawnArg1 >> 16) == 0x10) {
            work->field_8D0.flags |= 0x4000;
        }
    }
    work->field_6++;
    work->field_8++;
    head                                     = *(Actor401000AimScratch**)G_SCRATCH_HEAD;
    *(Actor401000AimScratch**)G_SCRATCH_HEAD = head - 1;
    s                                        = head - 1;
    if (func_actor_401000_801323EC(((TmdObject*)arg0->extra)->coords, work->field_A30, 0xC) != 1) {
        if (func_actor_401000_801323EC(((TmdObject*)arg0->extra)->coords, work->field_8F0, 0xC) != 1) {
            func_actor_401000_80135704(arg0, work->field_8F0, 0xC);
        }
    }
    Actor401000_ConfigPositionDelta(&Player_Status, ((TmdObject*)arg0->extra)->coords, &s->delta);
    ((TmdObject*)arg0->extra)->coords->flg = 0;
    func_actor_401000_80132EF0(arg0);
    s->pad_8 = ratan2(-((TmdObject*)(gameGetPtrSlot(3))->extra)->coords->coord.m[2][0],
                      ((TmdObject*)(gameGetPtrSlot(3))->extra)->coords->coord.m[2][2]);
    Actor401000_ConfigPositionDelta(&Player_Status, ((TmdObject*)arg0->extra)->coords, &s->delta);
    yaw             = ratan2(s->delta.vx, s->delta.vz) + 0x800;
    s->pad_A        = yaw;
    s->pad_A        = Actor401000_NormalizeYaw(yaw);
    coord           = ((TmdObject*)arg0->extra)->coords;
    s->angle        = Actor401000_NormalizeYaw(ratan2(s->delta.vx, s->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
    work->field_8AE = s->angle;
    if (func_actor_401000_80132824(arg0) != 1) {
        work->field_6   = 0;
        coord           = ((TmdObject*)arg0->extra)->coords;
        s->angle        = Actor401000_NormalizeYaw(ratan2(s->delta.vx, s->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
        work->field_8AE = s->angle;
        if (s->angle < 0x200) {
            if (!Actor401000_OutOfRange(&s->delta, 0x44C) && work->field_C1B == 0) {
                work->field_0 = 0xB;
            }
        }
        if (work->field_8 >= 0x5B) {
            work->field_0 = 0x1B;
        }
    } else {
        work->field_8   = 0;
        coord           = ((TmdObject*)arg0->extra)->coords;
        s->angle        = Actor401000_NormalizeYaw(ratan2(s->delta.vx, s->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
        work->field_8AE = s->angle;
        if (work->field_C08 == 0) {
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            if ((Gp_LcgState >> 16) & 1) {
                work->field_C08 = -1;
            } else {
                work->field_C08 = 1;
            }
        }
        if (work->field_C08 == 1) {
            s->angle += 0x300;
        } else {
            s->angle -= 0x300;
        }
        if (work->field_6 >= 0xF1) {
            work->field_6   = 0;
            work->field_C08 = -work->field_C08;
        }
    }
    if (s->angle > 0x20) {
        s->angle = 0x20;
    }
    if (s->angle < -0x20) {
        s->angle = -0x20;
    }
    facing    = ((TmdObject*)arg0->extra)->coords;
    s->angle += ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
    Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->coords->coord, s->angle, 1);
    Actor401000_RescaleYaw(((TmdObject*)arg0->extra)->coords, 0x1194);
    ((TmdObject*)arg0->extra)->coords->flg = 0;
    if (work->field_89E == 2) {
        if (work->field_89A == 0) {
            if ((s16)func_actor_401000_80132590(((TmdObject*)arg0->extra)->coords, 0x12C, 0x16) != 0) {
                Actor401000_MoveForward(((TmdObject*)arg0->extra)->coords, 0x16);
            }
        } else {
            if ((s16)func_actor_401000_80132590(((TmdObject*)arg0->extra)->coords, 0x12C, 5) != 0) {
                Actor401000_MoveForward(((TmdObject*)arg0->extra)->coords, 5);
            }
        }
    } else if (work->flags_68.half & 1) {
        work->field_89E = 2;
        work->field_898 = 1;
    }
    if (work->field_C1B != 0) {
        work->field_C1B--;
    }
    *(Actor401000AimScratch**)G_SCRATCH_HEAD += 1;
}

/// State 9 clip-0xB body, the 401000 twin of `func_actor_401000_80138BB4` and
/// `func_actor_401000_8013CEF0`: on the live-actor flag it resets the two
/// animation nodes and the root coordinate like the state 9 body, but keys the
/// node pair off clip 0xB / slot 2 and tests the request bit `0x100` rather
/// than bit 0. Same tail: `GpEnemy.hp` / `.reactionFlags` pick the next
/// `field_0` whenever the request bit is up.
void func_actor_401000_8013CD9C(Task* arg0)
{
    Actor401000Work* work;
    GpEnemy*         enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        ((TmdObject*)arg0->extra)->flags = 0;
        work->field_8D0.radius           = 0x1AE;
        work->field_B50.flags           &= 0x7FFF;
        work->field_A10.flags           |= 0x4000;
        enemy->node.flags                = 0;
        work->field_898                  = 2;
        work->field_89E                  = 0xB;
        work->field_8A2                  = 0x10;
        work->field_8B0                  = 0;
        work->field_8AE                  = 0;
        if (enemy->hp < 0) {
            Gp_SetStateF0Byte3(1);
        }
        work->field_8D0.flags |= 0x4000;
    }
    func_actor_401000_80132EF0(arg0);
    func_actor_401000_801323EC(((TmdObject*)arg0->extra)->coords, work->field_8F0, 0xC);
    func_actor_401000_801323EC(((TmdObject*)arg0->extra)->coords, work->field_A30, 0xC);
    ((TmdObject*)arg0->extra)->coords->flg = 0;
    if (work->flags_68.half & 0x100) {
        work->field_8D0.flags |= 0x4000;
        if (enemy->hp <= 0) {
            work->field_0 = 0x15;
        } else if (enemy->reactionFlags & 2) {
            work->field_0 = 4;
        } else {
            work->field_0 = 0x11;
        }
    }
}

/// State 10 body, the 401000 twin of `func_actor_401000_80138BB4` and
/// `func_actor_401300_8014046C`: on the live-actor flag it resets the two
/// animation nodes and the root coordinate like the state 9 body, but keys the
/// node pair off clip 0x19 / slot 2 and tests the request bit `0x100` rather
/// than bit 0. Same tail: `GpEnemy.hp` / `.reactionFlags` pick the next
/// `field_0` whenever the request bit is up.
void func_actor_401000_8013CEF0(Task* arg0)
{
    Actor401000Work* work;
    GpEnemy*         enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        ((TmdObject*)arg0->extra)->flags = 0;
        work->field_8D0.radius           = 0x1AE;
        work->field_B50.flags           &= 0x7FFF;
        work->field_A10.flags           |= 0x4000;
        enemy->node.flags                = 0;
        work->field_898                  = 2;
        work->field_89E                  = 0x19;
        work->field_8A2                  = 0x10;
        work->field_8B0                  = 0;
        work->field_8AE                  = 0;
        if (enemy->hp < 0) {
            Gp_SetStateF0Byte3(1);
        }
        work->field_8D0.flags |= 0x4000;
    }
    func_actor_401000_80132EF0(arg0);
    func_actor_401000_801323EC(((TmdObject*)arg0->extra)->coords, work->field_8F0, 0xC);
    func_actor_401000_801323EC(((TmdObject*)arg0->extra)->coords, work->field_A30, 0xC);
    ((TmdObject*)arg0->extra)->coords->flg = 0;
    if (work->flags_68.half & 0x100) {
        work->field_8D0.flags |= 0x4000;
        if (enemy->hp <= 0) {
            work->field_0 = 0x15;
        } else if (enemy->reactionFlags & 2) {
            work->field_0 = 4;
        } else {
            work->field_0 = 0x11;
        }
    }
}

/// Walks `p` up its parent chain to `gGfxViewCoord`, transforming `out` by each
/// coordinate; `out` is left unchanged if the chain ends before the view. Same
/// body as `Actor401300_TransformToView` / `Actor01900_TransformToView`.
static __inline__ void Actor401000_TransformToView(GsCOORDINATE2* p, SVECTOR* out)
{
    SVECTOR        sv;
    VECTOR         vec;
    s32            flag;
    SVECTOR*       svp   = &sv;
    GsCOORDINATE2* view  = &gGfxViewCoord;
    VECTOR*        vecp  = &vec;
    s32*           flagp = &flag;
    sv.vx                = out->vx;
    sv.vy                = out->vy;
    sv.vz                = out->vz;
loop:
    if (p->sub != NULL) {
        if (p != view) {
            gte_SetTransMatrix(&p->coord);
            gte_SetRotMatrix(&p->coord);
            gte_ldv0(svp);
            gte_rtv0tr();
            gte_stlvnl(vecp);
            gte_stflg(flagp);
            sv.vx = vec.vx;
            sv.vy = vec.vy;
            sv.vz = vec.vz;
            p     = p->sub;
            goto loop;
        }
        out->vx = sv.vx;
        out->vy = sv.vy;
        out->vz = sv.vz;
    }
}

/// The actor's state handlers, indexed by `Actor401000Work::field_0`. Copied to
/// the frame by `func_actor_401000_8013D044` before the dispatch, so the
/// handler may overwrite the live table entry.
static const Actor401000StateTable D_actor_401000_80131FF4 = { {
    func_actor_401000_8013DB10,
    func_actor_401000_8013DB6C,
    func_actor_401000_8013DC14,
    func_actor_401000_8013DCC0,
    func_actor_401000_80134DB4,
    func_actor_401000_8013DD6C,
    func_actor_401000_80134F98,
    func_actor_401000_80135AA4,
    func_actor_401000_801365C8,
    func_actor_401000_80136E20,
    func_actor_401000_801374D4,
    func_actor_401000_801378DC,
    func_actor_401000_801380B8,
    func_actor_401000_801383F0,
    func_actor_401000_801385B0,
    func_actor_401000_8013DE24,
    func_actor_401000_8013DEC8,
    func_actor_401000_8013DF6C,
    NULL,
    func_actor_401000_801388F4,
    func_actor_401000_80138BB4,
    func_actor_401000_80138D08,
    func_actor_401000_80138F50,
    func_actor_401000_8013922C,
    func_actor_401000_801394EC,
    func_actor_401000_8013A0C8,
    func_actor_401000_80139D10,
    func_actor_401000_8013A5F0,
    func_actor_401000_8013A930,
    func_actor_401000_8013B1E4,
    func_actor_401000_8013C46C,
    func_actor_401000_8013CD9C,
    func_actor_401000_8013CEF0,
    func_actor_401000_8013B61C,
} };

/// The actor's per-frame tick, the 401000 twin of `func_actor_401300_801405DC`:
/// copy the state table to the frame, advance the root coordinate and hand it
/// to `Gp_UpdateActorColor`, then run the `Gp_StateF0.field_4` arm. Arms 1 and 2 only
/// drop the two obstacle records (2 also opening the `field_C` draw to 0x80)
/// and return; arm 0 falls through into the common tail, which counts
/// `field_BE8` down into `func_actor_401000_80133D50`, carries a new
/// `field_0` into `field_2`/`field_4` (snapping the root to `field_BF8` on a
/// 0xB/0xD transition), dispatches through the table, re-flags `field_8D0`,
/// then appends the view-space position to the `field_C2C` ring and publishes
/// it as the enemy's `field_1C` while the `field_89E` clip is 0x14/0x15.
void func_actor_401000_8013D044(GpEnemy* enemy, Task* actor)
{
    VECTOR                  pos;
    Actor401000StateTable   states;
    Actor401000Work*        work;
    Actor401000ViewScratch* scratch;
    Actor401000ViewScratch* head;
    s32                     state;

    work   = actor->work;
    states = D_actor_401000_80131FF4;

    ((TmdObject*)actor->extra)->coords->flg = 0;
    Gp_UpdateCoord(((TmdObject*)actor->extra)->coords);
    pos.vx = ((TmdObject*)actor->extra)->coords->workm.t[0];
    pos.vy = ((TmdObject*)actor->extra)->coords->workm.t[1];
    pos.vz = ((TmdObject*)actor->extra)->coords->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);

    switch (Gp_StateF0.field_4) {
        case 0:
            state = work->field_0;
            if ((state != 0) && (state != 0x15) && (state != 0x1D) && (state != 0x21)) {
                ((TmdObject*)actor->extra)->flags = 0;
                Gp_DrawEffGroundQuad((VECTOR3*)((TmdObject*)actor->extra)->coords->workm.t, 0x180, Gp_State1C->groundShade);
                state = work->field_0;
            }
            if ((state == 0x21) && (work->field_89E == 2)) {
                Gp_DrawEffGroundQuad((VECTOR3*)((TmdObject*)actor->extra)->coords->workm.t, 0x180, Gp_State1C->groundShade);
            }
            break;
        case 1:
            state = work->field_0;
            if ((state != 0) && (state != 0x15) && (state != 0x1D) && (state != 0x21)) {
                ((TmdObject*)actor->extra)->flags = 0;
                Gp_DrawEffGroundQuad((VECTOR3*)((TmdObject*)actor->extra)->coords->workm.t, 0x180, Gp_State1C->groundShade);
                state = work->field_0;
            }
            if ((state == 0x21) && (work->field_89E == 2)) {
                Gp_DrawEffGroundQuad((VECTOR3*)((TmdObject*)actor->extra)->coords->workm.t, 0x180, Gp_State1C->groundShade);
            }
            Gp_ClearRec18Occupied(work->field_A30);
            Gp_ClearRec18Occupied(work->field_8F0);
            return;
        case 2:
            ((TmdObject*)actor->extra)->flags = 0x80;
            Gp_ClearRec18Occupied(work->field_A30);
            Gp_ClearRec18Occupied(work->field_8F0);
            return;
    }

    head                                      = *(Actor401000ViewScratch**)G_SCRATCH_HEAD;
    *(Actor401000ViewScratch**)G_SCRATCH_HEAD = head - 1;
    scratch                                   = head - 1;

    if (work->field_BE8 > 0) {
        work->field_BE8 = (s16)((u16)work->field_BE8 - 1);
    } else {
        func_actor_401000_80133D50(actor);
    }
    if (work->field_2 != work->field_0) {
        if ((work->field_2 == 0xB) || (work->field_2 == 0xD)) {
            ((TmdObject*)actor->extra)->coords->coord.t[0] = work->field_BF8.vx;
            ((TmdObject*)actor->extra)->coords->coord.t[1] = work->field_BF8.vy;
            ((TmdObject*)actor->extra)->coords->coord.t[2] = work->field_BF8.vz;
            ((TmdObject*)actor->extra)->coords->flg        = 0;
            Gp_UpdateCoord(((TmdObject*)actor->extra)->coords);
        }
        work->field_4 = 1;
    } else {
        work->field_4 = 0;
    }
    work->field_2 = (u16)work->field_0;
    states.fn[work->field_0](actor);

    state = work->field_0;
    if ((state == 0x15) || (state == 0) || (state == 0x1D) || (state == 0x21)) {
        work->field_8D0.flags &= 0x7FFF;
    } else {
        work->field_8D0.flags |= 0x8000;
    }
    Gp_ClearRec18Occupied(work->field_A30);
    Gp_ClearRec18Occupied(work->field_8F0);

    if ((D_801153F2[1] == 1) && (work->field_0 == 0x18)) {
        work->field_0 = 6;
    }

    scratch->pos.vx = 0;
    scratch->pos.vy = 0;
    scratch->pos.vz = 0;
    Actor401000_TransformToView(((TmdObject*)actor->extra)->coords + 2, &scratch->pos);

    work->field_C2C[work->field_C7C].vx = scratch->pos.vx;
    work->field_C2C[work->field_C7C].vy = scratch->pos.vy;
    work->field_C2C[work->field_C7C].vz = scratch->pos.vz;

    *(u8**)G_SCRATCH_HEAD += 0x18;
    work->field_C7C        = (u16)work->field_C7C + 1;
    if (work->field_C7C == 7) {
        work->field_C7C = 0;
    }
    if ((u32)((u16)work->field_89E - 0x14) < 2U) {
        enemy->bodyPos.vx = work->field_C2C[work->field_C7C].vx;
        enemy->bodyPos.vy = work->field_C2C[work->field_C7C].vy;
        enemy->bodyPos.vz = work->field_C2C[work->field_C7C].vz;
    } else {
        enemy->bodyPos.vx = scratch->pos.vx;
        enemy->bodyPos.vy = scratch->pos.vy;
        enemy->bodyPos.vz = scratch->pos.vz;
    }
    enemy->coord = &gGfxViewCoord;
}

void func_actor_401000_8013D68C(void)
{
}

/// The task's handlers, indexed by `Task::state` in
/// `func_actor_401000_8013E038`: the first allocates and sets up the work
/// block, the second runs the per-state logic every frame, and the third tears
/// the enemy down.
static const GpEnemyTaskFuncTable3 D_actor_401000_8013207C = { {
    func_actor_401000_80133274,
    func_actor_401000_8013D044,
    Gp_DestroyEnemy,
} };

s32 func_actor_401000_8013D694(Task* arg0, s32 arg1, Actor401000Msg* arg2)
{
    Actor401000Work* work = arg0->work;

    switch (arg2->field_4) {
        case 0:
            work->field_89E = 0x22;
            break;
        case 1:
            work->field_89E = 0x23;
            break;
        case 2:
            work->field_89E = 0x24;
            break;
        case 3:
            work->field_89E = 0x25;
            break;
        case 4:
            work->field_89E = 0x27;
            break;
    }
    work->field_0 = 0x11;
    work->field_2 = -1;
    return 0;
}

/// Applies one of four model settings chosen by `arg2`: 0 sets the model's
/// flags to 0x80 and reallocates its buffers, 1 clears the flags and
/// reallocates them, 2 and 3 set bit 2 (3 clearing the rest first). Case 1
/// moves the actor to state 0x18, the others to state 0. Always returns 0.
s32 func_actor_401000_8013D704(Task* task, s32 arg1, s32 arg2)
{
    TmdObject*       obj;
    Actor401000Work* work;

    obj  = (TmdObject*)task->extra;
    work = (Actor401000Work*)task->work;
    switch (arg2) {
        case 0:
            obj->flags = 0x80;
            Tmd_AllocBuffers(obj);
            work->field_0 = 0;
            break;
        case 1:
            obj->flags = 0;
            Tmd_AllocBuffers(obj);
            work->field_0 = 0x18;
            break;
        case 2:
            obj->flags   |= 4;
            work->field_0 = 0;
            break;
        case 3:
            obj->flags    = 0;
            work->field_0 = 0;
            obj->flags   |= 4;
            break;
    }
    return 0;
}

/// Returns 1 while the actor's enemy still has HP. Once it is down, returns 0
/// if the model carries bit 0x80 or lacks bit 2, and 1 otherwise.
s32 func_actor_401000_8013D7C4(Task* task)
{
    s32 ret;
    u16 flags;
    s32 mask2;
    s32 mask80;

    if (((GpEnemy*)task->spawnArg2)->hp > 0) {
        return 1;
    }

    flags   = ((TmdObject*)task->extra)->flags;
    mask80  = flags;
    mask80 &= 0x80;
    mask2   = flags & 2;
    if (mask80 != 0) {
        return 0;
    }

    ret = 0;
    if (mask2 == 0) {
        ret = 1;
        SOFT_BARRIER();
    }
    return ret;
}

/// Places the actor's model from `placement`: the translation goes into the
/// root coordinate, then the X, Y and Z rotations are applied in that order.
/// The heading of the rotated Z axis is stored as the work block's `yaw`.
/// Returns 1.
s32 func_actor_401000_8013D814(Task* task, s32 arg1, ActorShared80169f74Placement* placement)
{
    GsCOORDINATE2*   coord;
    s32              mx;
    s32              mz;
    Actor401000Work* work;

    work                                          = (Actor401000Work*)task->work;
    ((TmdObject*)task->extra)->coords->coord.t[0] = placement->pos.vx;
    ((TmdObject*)task->extra)->coords->coord.t[1] = placement->pos.vy;
    ((TmdObject*)task->extra)->coords->coord.t[2] = placement->pos.vz;
    Gfx_RotMatrixX(&((TmdObject*)task->extra)->coords->coord, placement->rot.vx, 1);
    Gfx_RotMatrixY(&((TmdObject*)task->extra)->coords->coord, placement->rot.vy, 0);
    Gfx_RotMatrixZ(&((TmdObject*)task->extra)->coords->coord, placement->rot.vz, 0);
    ((TmdObject*)task->extra)->coords->flg = 0;
    coord                                  = ((TmdObject*)task->extra)->coords;
    mx                                     = coord->coord.m[2][0];
    mz                                     = coord->coord.m[2][2];
    work->yaw                              = ratan2(-mx, mz);
    return 1;
}

/// Leaves state 0xD: to 0xE while the player has HP left, to 0x16 once it is
/// gone. Any other state is left alone.
s32 func_actor_401000_8013D914(Task* task)
{
    Actor401000Work* work = (Actor401000Work*)task->work;
    PlayerStatus*    cfg  = &Player_Status;

    if (work->field_0 == 0xD) {
        if (cfg->hp > 0) {
            work->field_0 = 0xE;
        } else {
            work->field_0 = 0x16;
        }
    }
    return 1;
}

/// Message 0x301 / 0x1002 handler: copies the payload's three leading bytes
/// into `field_C18`, then keys the actor's state off the message id and sub-id
/// (0x301/1 to 0x17, 0x1002/0 to 0, 0x1002/2 to 0x1C). The 0x1002/2 arm also
/// drops the model root to `(-0x595, 0, -0x5B1)` and turns it to -0x400.
/// Returns 1 when a state was set.
s32 func_actor_401000_8013D958(Task* arg0, s32 arg1, u16* arg2)
{
    u16              room;
    u16              state;
    u16              state2;
    Actor401000Work* work;

    work               = arg0->work;
    work->field_C18[0] = ((u8*)arg2)[0];
    work->field_C18[1] = ((u8*)arg2)[1];
    work->field_C18[2] = ((u8*)arg2)[2];
    room               = arg2[0];
    if (room == 0x301) {
        state = arg2[1];
        if (state == 1) {
            work->field_0 = 0x17;
            return 1;
        }
        return 0;
    }
    if (room == 0x1002) {
        state2 = arg2[1];
        switch (state2) {
            case 0:
                work->field_0 = 0;
                return 1;
            case 2:
                work->field_0                                 = 0x1C;
                ((TmdObject*)arg0->extra)->coords->coord.t[0] = -0x595;
                ((TmdObject*)arg0->extra)->coords->coord.t[1] = 0;
                ((TmdObject*)arg0->extra)->coords->coord.t[2] = -0x5B1;
                Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->coords->coord, -0x400, 1);
                ((TmdObject*)arg0->extra)->coords->flg = 0;
                return 1;
            default:
                return 0;
        }
    } else {
        return 0;
    }
}

/// `Task::exitCallback` teardown: kills the two helper tasks, unlinks the three
/// display nodes and drops the enemy's `recs`, then destroys the enemy.
void func_actor_401000_8013DA78(Task* task)
{
    Actor401000Work* work;
    GpEnemy*         enemy;

    work  = (Actor401000Work*)task->work;
    enemy = (GpEnemy*)task->spawnArg2;
    if (work != NULL) {
        if (work->field_C1C != NULL) {
            taskKill(work->field_C1C);
        }
        if (work->field_C20 != NULL) {
            taskKill(work->field_C20);
        }
        Gp_UnlinkObj(&work->field_B50);
        Gp_UnlinkObj(&work->field_8D0);
        Gp_UnlinkObj(&work->field_A10);
        enemy->recs = 0;
    }
    Gp_DestroyEnemy(enemy, task);
}

void func_actor_401000_8013DB10(Task* arg0)
{
    TmdObject*       obj;
    Actor401000Work* work;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                     = arg0->extra;
        ((GpEnemy*)arg0->spawnArg2)->node.flags = 1;
        obj->flags                              = (u16)(obj->flags | 0x80);
        work->field_B50.flags                   = (u16)(work->field_B50.flags & 0x7FFF);
        work->field_A10.flags                   = (u16)(work->field_A10.flags | 0x4000);
    }
}

void func_actor_401000_8013DB6C(Task* arg0)
{
    TmdObject*       obj;
    Actor401000Work* work;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                     = arg0->extra;
        ((GpEnemy*)arg0->spawnArg2)->node.flags = 0;
        obj->flags                              = 0;
        Tmd_AllocBuffers(obj);
        work->field_898       = 2;
        work->field_8A2       = 0x10;
        work->field_89E       = 2;
        work->field_89A       = 0;
        work->field_B50.flags = (u16)(work->field_B50.flags & 0x7FFF);
        work->field_A10.flags = (u16)(work->field_A10.flags | 0x4000);
        func_actor_401000_80132EF0(arg0);
    } else {
        ((TmdObject*)arg0->extra)->coords->flg = 0;
        func_actor_401000_80132EF0(arg0);
    }
}

void func_actor_401000_8013DC14(Task* arg0)
{
    TmdObject*       obj;
    Actor401000Work* work;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                     = arg0->extra;
        ((GpEnemy*)arg0->spawnArg2)->node.flags = 0;
        obj->flags                              = 0;
        Tmd_AllocBuffers(obj);
        work->field_898       = 2;
        work->field_8A2       = 0x10;
        work->field_89E       = 3;
        work->field_89A       = 0;
        work->field_B50.flags = (u16)(work->field_B50.flags & 0x7FFF);
        work->field_A10.flags = (u16)(work->field_A10.flags | 0x4000);
        func_actor_401000_80132EF0(arg0);
    } else {
        ((TmdObject*)arg0->extra)->coords->flg = 0;
        func_actor_401000_80132EF0(arg0);
    }
}

void func_actor_401000_8013DCC0(Task* arg0)
{
    TmdObject*       obj;
    Actor401000Work* work;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                     = arg0->extra;
        ((GpEnemy*)arg0->spawnArg2)->node.flags = 0;
        obj->flags                              = 0;
        Tmd_AllocBuffers(obj);
        work->field_898       = 2;
        work->field_8A2       = 0x10;
        work->field_89E       = 0xB;
        work->field_89A       = 0;
        work->field_B50.flags = (u16)(work->field_B50.flags & 0x7FFF);
        work->field_A10.flags = (u16)(work->field_A10.flags | 0x4000);
        func_actor_401000_80132EF0(arg0);
    } else {
        ((TmdObject*)arg0->extra)->coords->flg = 0;
        func_actor_401000_80132EF0(arg0);
    }
}

void func_actor_401000_8013DD6C(Task* arg0)
{
    TmdObject*       obj;
    Actor401000Work* work;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                     = arg0->extra;
        ((GpEnemy*)arg0->spawnArg2)->node.flags = 0;
        obj->flags                              = 0;
        Tmd_AllocBuffers(obj);
        work->field_898       = 2;
        work->field_8A2       = 0x12;
        work->field_89E       = 0xD;
        work->field_89A       = 0;
        work->field_B50.flags = (u16)(work->field_B50.flags & 0x7FFF);
        work->field_A10.flags = (u16)(work->field_A10.flags | 0x4000);
    }
    ((TmdObject*)arg0->extra)->coords->flg = 0;
    func_actor_401000_80132EF0(arg0);
    if (work->flags_68.half & 1) {
        work->field_0 = 7;
    }
}

void func_actor_401000_8013DE24(Task* arg0)
{
    Actor401000Work* work;
    GpEnemy*         enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        ((TmdObject*)arg0->extra)->flags = 0;
        work->field_8D0.radius           = 0x1AE;
        work->field_B50.flags           &= 0x7FFF;
        work->field_A10.flags           |= 0x4000;
        enemy->node.flags                = 0;
        work->field_898                  = 2;
        work->field_89E                  = 8;
        work->field_8B0                  = 0;
        work->field_8AE                  = 0;
        work->field_8A2                  = work->field_8A4;
    }
    func_actor_401000_80132EF0(arg0);
    if (work->flags_68.half & 1) {
        work->field_0 = 7;
    }
}

void func_actor_401000_8013DEC8(Task* arg0)
{
    Actor401000Work* work;
    GpEnemy*         enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        ((TmdObject*)arg0->extra)->flags = 0;
        work->field_8D0.radius           = 0x1AE;
        work->field_B50.flags           &= 0x7FFF;
        work->field_A10.flags           |= 0x4000;
        enemy->node.flags                = 0;
        work->field_898                  = 2;
        work->field_89E                  = 0x16;
        work->field_8B0                  = 0;
        work->field_8AE                  = 0;
        work->field_8A2                  = work->field_8A4;
    }
    func_actor_401000_80132EF0(arg0);
    if (work->flags_68.half & 1) {
        work->field_0 = 7;
    }
}

/// Idle state: on entry the `field_6` countdown is seeded from `field_C10` plus
/// a 0-15 draw from `Gp_LcgState`. When it runs out, clips 0xB/0x17 move the
/// actor to state 0xF and clips 0xC/0x18/0x19 to state 0x10; a spent enemy HP
/// moves it to state 0x15 whatever else happened.
void func_actor_401000_8013DF6C(Task* arg0)
{
    Actor401000Work* work;
    GpEnemy*         enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
        work->field_6 = work->field_C10 + ((Gp_LcgState >> 16) & 0xF);
    }
    if (--work->field_6 < 0) {
        switch (work->field_89E) {
            case 0xB:
            case 0x17:
                work->field_0 = 0xF;
                break;
            case 0xC:
            case 0x18:
            case 0x19:
                work->field_0 = 0x10;
                break;
        }
    }
    if (enemy->hp <= 0) {
        work->field_0 = 0x15;
    }
}

/// Runs the actor's handler for the task's current state, copying the
/// three-entry table onto the stack first.
void func_actor_401000_8013E038(Task* task)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_401000_8013207C;
    sp.funcs[task->state](task->spawnArg2, task);
}
