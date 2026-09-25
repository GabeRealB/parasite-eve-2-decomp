#include "common.h"

#include <psyq/libgte.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "actors/actor.h"
#include "actors/actors_shared_80164954.h"
#include "gameplay/1A8.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
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

/// The actor id word at 0xE90, read two ways: `func_actor_421600_8013848C`
/// and `func_actor_421600_8013E9D8` mask the whole word to 24 bits and compare
/// it with 0x11402, while `func_actor_421600_8013947C` tests its third byte
/// alone against 2.
typedef union Actor421600IdWord {
    /* 0x0 */ s32 word;
    /* 0x0 */ u8  bytes[4];
} Actor421600IdWord;
STATIC_ASSERT_SIZEOF(Actor421600IdWord, 0x4);

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
    /// Yaw the placement handler `func_actor_421600_8013E52C` reads back off
    /// the root coordinate after writing it.
    /* 0x016 */ s16  field_16;
    /* 0x018 */ byte pad_18[0x42];
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
    /* 0x834 */ u16 field_834;
    /* 0x836 */ s16 field_836;
    /* 0x838 */ s16 field_838;
    /// Clip id `func_actor_421600_80133B30` copies into the blend slots.
    /* 0x83A */ u16  field_83A;
    /* 0x83C */ byte pad_83C[2];
    /* 0x83E */ u16  field_83E;
    /* 0x840 */ u16  field_840;
    /* 0x842 */ byte pad_842[2];
    /* 0x844 */ s16  field_844;
    /* 0x846 */ byte pad_846[2];
    /// Record last handled by the per-frame effect dispatch, one entry per
    /// animation slot and wiped as one block when no case claims a record. This
    /// overlay only ever uses entry 1, against the record word at 0x5A.
    /* 0x848 */ s32 field_848[18];
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
    /* 0x8B4 */ s16  field_8B4;
    /* 0x8B6 */ s8   field_8B6;
    /* 0x8B7 */ byte pad_8B7;
    /// Player position and rotation sent together as message 0x3E9.
    /* 0x8B8 */ VECTOR  field_8B8;
    /* 0x8C8 */ SVECTOR field_8C8;
    /// Reply buffer for message 0x3F8; field_8E4 selects query mode 8.
    /* 0x8D0 */ byte  field_8D0[0x14];
    /* 0x8E4 */ s32   field_8E4;
    /* 0x8E8 */ u8    field_8E8;
    /* 0x8E9 */ u8    field_8E9;
    /* 0x8EA */ s16   field_8EA;
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
    /* 0xE44 */ byte              pad_E44[0x20];
    /* 0xE64 */ s16               field_E64;
    /* 0xE66 */ u16               field_E66;
    /* 0xE68 */ byte              pad_E68[8];
    /* 0xE70 */ s16               field_E70;
    /* 0xE72 */ s16               field_E72;
    /* 0xE74 */ s16               field_E74;
    /* 0xE76 */ byte              pad_E76[2];
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

typedef struct Actor421600DamageScratch {
    /* 0x00 */ s32  field_0;
    /* 0x04 */ s32  field_4;
    /* 0x08 */ s32  field_8;
    /* 0x0C */ byte pad_C[4];
    /* 0x10 */ s16  field_10;
    /* 0x12 */ s16  field_12;
    /* 0x14 */ s16  field_14;
    /* 0x16 */ byte pad_16[2];
    /* 0x18 */ s16  field_18;
    /* 0x1A */ s16  field_1A;
    /* 0x1C */ s16  field_1C;
    /* 0x1E */ byte pad_1E[2];
    /* 0x20 */ s32  field_20;
    /* 0x24 */ s32  field_24;
    /* 0x28 */ s32  field_28;
    /* 0x2C */ s16  field_2C;
    /* 0x2E */ s16  field_2E;
} Actor421600DamageScratch;
STATIC_ASSERT_SIZEOF(Actor421600DamageScratch, 0x30);

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

typedef struct Actor421600AvoidScratch {
    /* 0x00 */ MATRIX   m;
    /* 0x20 */ SVECTOR  dir;
    /* 0x28 */ SVECTOR3 eye;
    /* 0x2E */ byte     pad_2E[0x2];
    /* 0x30 */ s32      kind;
    /* 0x34 */ s32      flags;
    /* 0x38 */ s16      angle[8];
    /* 0x48 */ s8       ok[8];
    /* 0x50 */ s16      face;
    /* 0x52 */ s16      diff;
    /* 0x54 */ u8       i;
    /* 0x55 */ u8       j;
    /* 0x56 */ u8       count;
    /* 0x57 */ u8       blocked;
} Actor421600AvoidScratch;
STATIC_ASSERT_SIZEOF(Actor421600AvoidScratch, 0x58);

extern SVECTOR D_actor_421600_80151260;

/// The attack tick takes 0x14 bytes from G_SCRATCH_HEAD for its direction,
/// wrapped angles, arena zone and player contact reply.
typedef struct Actor421600AttackScratch {
    /* 0x00 */ SVECTOR vec;
    /* 0x08 */ s16     playerYaw;
    /* 0x0A */ s16     yaw;
    /* 0x0C */ s16     delta;
    /* 0x0E */ s16     zone;
    /* 0x10 */ s16     aim;
    /* 0x12 */ s16     reply;
} Actor421600AttackScratch;
STATIC_ASSERT_SIZEOF(Actor421600AttackScratch, 0x14);

/// Waypoint steering scratch with a zone-table index at the tail.
typedef struct Actor421600RouteScratch {
    /* 0x00 */ SVECTOR vec;
    /* 0x08 */ SVECTOR target;
    /* 0x10 */ MATRIX  matrix;
    /* 0x30 */ s16     delta;
    /* 0x32 */ s16     original;
    /* 0x34 */ s16     yaw;
    /* 0x36 */ s16     playerYaw;
    /* 0x38 */ s16     zone;
    /* 0x3A */ s16     pad;
} Actor421600RouteScratch;
STATIC_ASSERT_SIZEOF(Actor421600RouteScratch, 0x3C);

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

/// Two XZ waypoint pairs for each place-key mode, indexed by zone.
extern s32 D_actor_421600_801511D4[][8];

/// The records closing four of the overlay's model streams, which the
/// death-tick frames point `D_80114B78` at before each `Gp_SpawnEff`.
extern TmdSource D_actor_421600_80143EF4;
extern TmdSource D_actor_421600_801443C8;
extern TmdSource D_actor_421600_80145124;
extern TmdSource D_actor_421600_80145604;

/// Global effect-model callback slot the spawn helpers read; a one-element
/// array so the store is absolute (see actor 401300's header for the same
/// declaration).
extern void* D_80114B78[1];

/// Psy-Q `RotMatrixY` (it sits right after `RotMatrixX`).
void func_8004BFF8(s16 angle, MATRIX* matrix);

/// 4x4 zone table `func_actor_421600_8013A404` samples with the X and Z
/// buckets of the actor's position, cell `x | z * 4`; the sample is compared
/// against 0xB to pick between the 6 and 0x24 states.
extern s8 D_actor_421600_801511C0[16];

/// Animation tables selected by the attack tick for front and rear contact.
extern s32 D_actor_421600_80151090[];
extern s32 D_actor_421600_801510A4[];

/// Idle yaw `func_actor_421600_80132A00` stamps onto the enemy's `field_40`
/// on every state message, the same slot actor 00100 keeps at 0x8013EF3C.
extern u16 D_actor_421600_8013EF3C;

/// Progress counter the same handler compares against 4 / 5 / 2 / 0 to pick
/// the arena corner the actor is dropped into. Written by
/// `func_actor_421600_80134AD4` at spawn.
extern s16 D_actor_421600_80151268;

/// Signed transition durations, indexed by old animation * 25 + new animation.
extern s8 D_actor_421600_80150DB4[];

void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/// Per-frame scratch: the view-space body position and its arena zone.
typedef struct Actor421600UpdateScratch {
    /* 0x00 */ VECTOR  unused;
    /* 0x10 */ SVECTOR pos;
    /* 0x18 */ s16     zone;
    /* 0x1A */ s16     pad;
} Actor421600UpdateScratch;
STATIC_ASSERT_SIZEOF(Actor421600UpdateScratch, 0x1C);

typedef struct Actor421600StateTable {
    TaskFunc fn[40];
} Actor421600StateTable;
STATIC_ASSERT_SIZEOF(Actor421600StateTable, 0xA0);

/// Command fields filled from the player's current animation block.
typedef struct Actor421600AnimCommand {
    /* 0x00 */ s32 entries[4];
    /* 0x10 */ s32 field_10;
    /* 0x14 */ s32 field_14;
} Actor421600AnimCommand;

typedef struct Actor421600AnimWord {
    s32 value;
} Actor421600AnimWord;

extern GpAnimBlk*          Gp_PlayerAnimBlkTbl[];
extern u16                 Gp_WeaponIdBase[];
extern s8                  D_8007218A;
extern u8                  D_80073BA9;
extern u16                 D_801876A8, D_801876AA;
extern Actor421600AnimWord D_actor_421600_801510A0;

/// The fallback command's +0x14 endpoint overlaps the next table's first word.
/// Keep a separate C view of the endpoint so its address is materialized at
/// the store, independently of comparisons against the table base.
extern Actor421600AnimWord Actor421600FallbackEnd __asm__("D_actor_421600_801510A4");

void func_actor_421600_8013E668(Task* task);
void func_actor_421600_8013E858(Task* arg0);
void func_actor_421600_8013E8AC(Task* arg0);
void func_actor_421600_8013E9D8(Task* arg0);
void func_actor_421600_8013EAAC(Task* arg0);
void func_actor_421600_8013EB7C(Task* arg0);
void func_actor_421600_8013EC28(Task* arg0);
void func_actor_421600_8013ED24(Task* arg0);
void func_actor_421600_8013EE0C(Task* arg0);

static __inline__ Actor421600UpdateScratch* Actor421600_AllocUpdateScratch(Actor421600UpdateScratch** head)
{
    Actor421600UpdateScratch* p                     = SCRATCH_HEAD_AT(head, Actor421600UpdateScratch) - 1;
    SCRATCH_HEAD_AT(head, Actor421600UpdateScratch) = p;
    return p;
}
static __inline__ s32 Actor421600_HasPlayerContact(GpRec18* records)
{
    s16 i;
    for (i = 0; i < 12; i++) {
        if (records[i].key == 0)
            break;
        if ((records[i].key & 0xFFFF0000) == 0x10000)
            return 1;
    }
    return 0;
}

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);

/// Set `coord`'s rotation to its view-space orientation turned by `yaw`,
/// expressed back in its parent's frame, and refresh the coordinate. The work
/// matrix is borrowed from the scratchpad stack.
void func_actor_421600_80132004(GsCOORDINATE2* coord, s16 yaw)
{
    MATRIX*        rotation;
    GsCOORDINATE2* out;

    SCRATCH_PUSH(MATRIX);
    rotation = SCRATCH_HEAD(MATRIX);
    actorAccumulateRotation(coord, rotation, &gGfxViewCoord);
    func_8004BFF8(yaw, rotation);
    out = actorLocalizeRotation(coord, rotation);
    __builtin_memcpy(out->coord.m, rotation->m, sizeof(out->coord.m));
    out->flg = 0;
    Gp_UpdateCoord(out);
    SCRATCH_POP(MATRIX);
}

/// Push `coord` away from the obstacle records in `recs` (the first `count`,
/// stopping at an empty key). Every kind-0x10000 or 0x30000 record contributes
/// its bearing from the coordinate's view-space origin; bearings closer than
/// 0x400 to another cancel each other, and each remaining one moves the
/// coordinate 10 units along it in the XZ plane. `pos` receives the total
/// displacement. Returns whether a kind-0x10000 record was among them. Does
/// nothing, returning 0, while `gGameSession->viewReady` or `Mc_SaveData.field_5C1` is 1.
s32 func_actor_421600_80132310(GsCOORDINATE2* coord, GpRec18* recs, s16 count, SVECTOR* pos)
{
    u8*                  head;
    OverlayAvoidScratch* s;
    s16                  diff;
    s16                  t;
    s32                  mag;

    if (gGameSession->viewReady == 1 || Mc_SaveData.field_5C1 == 1) {
        return 0;
    }

    head             = SCRATCH_HEAD(u8);
    SCRATCH_HEAD(u8) = head - sizeof(OverlayAvoidScratch);
    s                = (OverlayAvoidScratch*)SCRATCH_HEAD(u8);
    s->blocked       = 0;
    pos->vz          = 0;
    pos->vy          = 0;
    pos->vx          = 0;

    Gfx_MatrixCol1(&coord->workm, (SVECTOR*)(head - 0x34));
    VectorNormalSS((SVECTOR*)(head - 0x34), (SVECTOR*)(head - 0x34));

    if (ABS(s->dir.vz) < 0x818) {
        s->face = ratan2(-coord->workm.m[2][0], coord->workm.m[2][2]);
    } else {
        s->face = -ratan2(-coord->workm.m[0][2], coord->workm.m[1][2]);
    }

    s->eye.vx = *(u16*)&coord->workm.t[0];
    s->eye.vy = *(u16*)&coord->workm.t[1];
    s->eye.vz = *(u16*)&coord->workm.t[2];
    s->count  = 0;

    for (s->i = 0; s->i < count; s->i++) {
        if (recs[s->i].key == 0) {
            break;
        }
        s->kind = recs[s->i].key & 0xFFFF0000;
        switch (s->kind) {
            case 0x10000:
                s->blocked = 1;
            case 0x30000:
                break;
            default:
                continue;
        }

        if (ABS(s->dir.vz) < 0x818) {
            s->angle[s->count] = overlayBearingXZ((SVECTOR3*)&recs[s->i].point, &s->eye);
        } else {
            s->angle[s->count] = overlayBearingXY((SVECTOR3*)&recs[s->i].point, &s->eye);
        }
        s->ok[s->count] = 1;
        s->count++;
        if (s->count >= 8) {
            break;
        }
    }

    for (s->i = 0; s->i < s->count; s->i++) {
        for (s->j = s->i + 1; s->j < s->count; s->j++) {
            diff = (u16)s->angle[s->i] - (u16)s->angle[s->j];
            t    = diff;
            if (diff < 0) {
            wrapUp:
                if (t < -0x800) {
                    t += 0x1000;
                    goto wrapUp;
                }
            } else {
            wrapDown:
                if (t > 0x800) {
                    t -= 0x1000;
                    goto wrapDown;
                }
            }
            mag     = t;
            s->diff = mag;
            SOFT_BARRIER();
            if (mag < 0) {
                mag = -mag;
            }
            if (mag >= 0x401) {
                s->ok[s->i] = 0;
                s->ok[s->j] = 0;
            }
        }
        if (s->ok[s->i] != 0) {
            diff = ((u16)s->angle[s->i] - (u16)s->face) +
                   ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
            s->diff = diff;
            Gfx_RotMatrixY(&s->m, diff, 1);
            Gfx_MatrixCol2(&s->m, &s->dir);
            VectorNormalSS(&s->dir, &s->dir);
            gte_lddp(-10);
            gte_ldsv(&s->dir);
            gte_gpf12();
            gte_stsv(&s->dir);
            pos->vx           += s->dir.vx;
            pos->vz           += s->dir.vz;
            coord->coord.t[0] += s->dir.vx;
            coord->coord.t[2] += s->dir.vz;
        }
    }

    SCRATCH_POP_BYTES(sizeof(OverlayAvoidScratch));
    return s->blocked != 0;
}

/// Step `coord` by the movement the first `arg2` records of `movement` resolve
/// to, and latch the integer part of that delta in `D_actor_421600_80151260`.
/// A nonzero fractional X or Z part rounds the coordinate and the latched step
/// one unit further from zero. Returns 1 when the X or Z delta is nonzero.
s32 func_actor_421600_8013285C(GsCOORDINATE2* coord, GpRec18* movement, s16 arg2)
{
    void**            scratch;
    u8*               head;
    OverlayDeltaFlag* s;
    register void*    p asm("v1");
    s32               val;

    scratch                        = SCRATCH_HEAD_ADDR;
    head                           = SCRATCH_HEAD_AT(scratch, void);
    p                              = head - 0x14;
    s                              = p;
    SCRATCH_HEAD_AT(scratch, void) = p;
    s->moved                       = 0;
    if (func_800E0C10(movement, &s->delta, (s32)arg2, NULL) != 0) {
        coord->coord.t[0]          = coord->coord.t[0] + ((OverlayDeltaFlag*)(head - 0x14))->delta.vx.h.hi;
        coord->coord.t[2]          = coord->coord.t[2] + s->delta.vz.h.hi;
        D_actor_421600_80151260.vx = ((OverlayDeltaFlag*)(head - 0x14))->delta.vx.w >> 16;
        D_actor_421600_80151260.vy = s->delta.vy.w >> 16;
        D_actor_421600_80151260.vz = s->delta.vz.w >> 16;
        val                        = ((OverlayDeltaFlag*)(head - 0x14))->delta.vx.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[0]++;
                D_actor_421600_80151260.vx++;
            } else {
                coord->coord.t[0]--;
                D_actor_421600_80151260.vx--;
            }
        }
        val = s->delta.vz.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[2]++;
                D_actor_421600_80151260.vz++;
            } else {
                coord->coord.t[2]--;
                D_actor_421600_80151260.vz--;
            }
        }
    }
    if (s->delta.vx.w != 0 || s->delta.vz.w != 0) {
        s->moved = 1;
    }
    SCRATCH_POP_BYTES(0x14);
    return s->moved;
}

/// Message handler. Message 0x109 nudges the state machine by its sub-command
/// (1 copies `field_EA8` into `field_EAA`, 3 moves state 1 on to 2). Any other
/// message has its opcode and the low byte of its sub-command latched into
/// `field_E90`; for 0x1402 the enemy's hit points are restored and, by
/// sub-command, the placement mode in `placeKey` and the progress counter
/// `D_actor_421600_80151268`, the actor is dropped at a fixed spot with a new
/// state. Returns 1 when the message was handled.
s32 func_actor_421600_80132A00(Task* arg0, s32 arg1, GpCmdArg* arg2)
{
    Actor421600Work* work;
    GpEnemy*         enemy;
    s32              angle;
    s16              mode;

    work  = arg0->work;
    enemy = arg0->spawnArg2;

    if (arg2->from.key == 0x109) {
        switch (arg2->command) {
            case 1:
                work->field_EAA = work->field_EA8;
                break;
            case 2:
                if (work->field_0 == 0x26) {
                    work->field_0 = 0x26;
                }
                break;
            case 3:
                if (work->field_0 == 1) {
                    work->field_0 = 2;
                }
                break;
        }
        return 1;
    }

    work->field_E90.bytes[0] = arg2->from.loc.stage;
    work->field_E90.bytes[1] = arg2->from.loc.area;
    work->field_E90.bytes[2] = (u8)arg2->command;

    if (arg2->from.key != 0x1402) {
        return 0;
    }

    switch (arg2->command) {
        case 0:
            enemy->hp = D_actor_421600_8013EF3C;
            if ((enemy->placeKey >> 12) == 0) {
                work->field_0 = 2;
            }
            return 1;

        case 1:
            mode      = enemy->placeKey >> 12;
            enemy->hp = D_actor_421600_8013EF3C;
            switch (mode) {
                case 0:
                    if (D_actor_421600_80151268 < 4) {
                        goto negstate;
                    }
                    if (work->field_0 != 0) {
                        goto tail;
                    }
                    ((TmdObject*)arg0->extra)->coords->coord.t[0] = 0x1057;
                    ((TmdObject*)arg0->extra)->coords->coord.t[2] = -0x11A3;
                    Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->coords->coord, -0x400, 1);
                    ((TmdObject*)arg0->extra)->coords->flg = 0;
                    Gp_UpdateCoord(((TmdObject*)arg0->extra)->coords);
                    work->field_0 = 0x20;
                    work->field_2 = -1;
                    goto tail;
                case 1:
                    if (D_actor_421600_80151268 < 5) {
                        goto negstate;
                    }
                    if (work->field_0 != 0) {
                        goto tail;
                    }
                    ((TmdObject*)arg0->extra)->coords->coord.t[0] = 0x1467;
                    ((TmdObject*)arg0->extra)->coords->coord.t[2] = 0x4B9;
                    Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->coords->coord, 0x7BC, 1);
                    ((TmdObject*)arg0->extra)->coords->flg = 0;
                    Gp_UpdateCoord(((TmdObject*)arg0->extra)->coords);
                    work->field_0 = 0x20;
                    work->field_2 = -1;
                    goto tail;
            }
            goto tail;
        negstate:
            work->field_0 = 0;
            work->field_2 = -1;
        tail:
            Gp_SetLightMode(enemy, 0);
            enemy->reactionFlags = 0;
            enemy->hp            = D_actor_421600_8013EF3C;
            return 1;

        case 2:
            switch (enemy->placeKey >> 12) {
                case 0:
                    if (D_actor_421600_80151268 <= 0) {
                        goto blockDE0;
                    }
                    ((TmdObject*)arg0->extra)->coords->coord.t[0] = -0xD40;
                    ((TmdObject*)arg0->extra)->coords->coord.t[2] = 0x104F;
                    Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->coords->coord, -0x76C, 1);
                    ((TmdObject*)arg0->extra)->coords->flg = 0;
                    Gp_UpdateCoord(((TmdObject*)arg0->extra)->coords);
                    Gp_SetLightMode(enemy, 0);
                    enemy->reactionFlags = 0;
                    enemy->hp            = D_actor_421600_8013EF3C;
                    work->field_0        = 6;
                    goto blockDE0;
                case 1:
                    if (D_actor_421600_80151268 < 2) {
                        goto blockDE0;
                    }
                    ((TmdObject*)arg0->extra)->coords->coord.t[0] = 0x138C;
                    ((TmdObject*)arg0->extra)->coords->coord.t[2] = 0x4B2;
                    Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->coords->coord, 0x7BC, 1);
                    ((TmdObject*)arg0->extra)->coords->flg = 0;
                    Gp_UpdateCoord(((TmdObject*)arg0->extra)->coords);
                    Gp_SetLightMode(enemy, 0);
                    enemy->reactionFlags = 0;
                    enemy->hp            = D_actor_421600_8013EF3C;
                    work->field_0        = 6;
                    goto blockDE0;
                default:
                    goto blockDE0;
            }
        blockDE0:
            if (D_actor_421600_80151268 == 0) {
                Gp_DispatchMsg(gameGetPtrSlot(7), 0x13F4, 0, 0);
            }
            return 1;

        case 3:
            if (work->field_E9C == 1) {
                work->field_E9C = 0;
                Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F1, 2, 0);
            }
            if (work->field_0 != 0x14 && work->field_0 != 0x11 && work->field_0 != 0x15 &&
                work->field_0 != 0x16 && work->field_0 != 0 && work->field_0 != 8) {
                work->field_0 = 5;
                work->field_2 = -1;
            }
            return 1;

        case 9:
            work->field_0 = 0;
            work->field_2 = -1;
            return 1;

        default:
            return 0;
    }
}

/// Draw a beam between model parts `firstJoint` and `secondJoint`: take both
/// ends into view space at height `height`, widen them by `width` into a quad
/// and emit it as a textured `POLY_FT4` shaded `shade`. Nothing is drawn when
/// the two parts are the same.
void func_actor_421600_80132EC0(Task* actor, s16 firstJoint, s16 secondJoint, s16 width, s16 height, u8 shade)
{
    ActorBeamScratch* s;
    s16               angle;
    GsCOORDINATE2*    secondCoord;
    GsCOORDINATE2*    firstCoord;
    s32               offset0;
    s32               offset1;
    s32               offset2;
    s32               offset3;
    s32               halfX;
    s32               halfZ;
    GsCOORDINATE2*    coords;
    GsCOORDINATE2*    view;
    POLY_FT4*         poly;

    coords      = ((TmdObject*)actor->extra)->coords;
    firstCoord  = coords + firstJoint;
    secondCoord = coords + secondJoint;
    if (firstJoint != secondJoint) {
        s = (ActorBeamScratch*)SCRATCH_PUSH_BYTES(sizeof(ActorBeamScratch));
        Gp_UpdateCoord(firstCoord);
        Gp_UpdateCoord(secondCoord);
        Gp_WorldToLocal(&Gfx_ViewWorldMtx, &firstCoord->workm, &s->firstMatrix);
        Gp_WorldToLocal(&Gfx_ViewWorldMtx, &secondCoord->workm, &s->secondMatrix);
        s->first.vy   = height;
        s->second.vy  = height;
        s->first.vx   = s->firstMatrix.t[0];
        s->first.vz   = s->firstMatrix.t[2];
        s->second.vx  = s->secondMatrix.t[0];
        s->second.vz  = s->secondMatrix.t[2];
        angle         = ratan2(s->second.vx - s->first.vx, s->second.vz - s->first.vz);
        halfX         = (s->first.vx - s->second.vx) / 2;
        halfZ         = (s->first.vz - s->second.vz) / 2;
        offset0       = rcos(angle) * width;
        s->corner0.vy = height;
        s->corner0.vx = halfX + (s->first.vx - (offset0 >> 0xC));
        s->corner0.vz = halfZ + (s->first.vz + ((s32)(rsin(angle) * width) >> 0xC));
        offset1       = rcos(angle) * width;
        s->corner1.vy = height;
        s->corner1.vx = halfX + (s->first.vx + (offset1 >> 0xC));
        s->corner1.vz = halfZ + (s->first.vz - ((s32)(rsin(angle) * width) >> 0xC));
        offset2       = rcos(angle) * width;
        s->corner2.vy = height;
        s->corner2.vx = (s->second.vx - (offset2 >> 0xC)) - halfX;
        s->corner2.vz = (s->second.vz + ((s32)(rsin(angle) * width) >> 0xC)) - halfZ;
        offset3       = rcos(angle) * width;
        s->corner3.vy = height;
        s->corner3.vx = (s->second.vx + (offset3 >> 0xC)) - halfX;
        s->corner3.vz = (s->second.vz - ((s32)(rsin(angle) * width) >> 0xC)) - halfZ;
        view          = (GsCOORDINATE2*)((u8*)&Gfx_ViewWorldMtx - OFFSET_OF(GsCOORDINATE2, workm));
        view->flg     = 0;
        Gp_UpdateCoord(view);
        gte_SetRotMatrix(&Gfx_ViewWorldMtx);
        gte_SetTransMatrix(&Gfx_ViewWorldMtx);
        s->depth = RotTransPers4(&s->corner0, &s->corner1, &s->corner2, &s->corner3, &s->screen0, &s->screen1,
                                 &s->screen2, &s->screen3, &s->perspective, &s->flags);
        if (s->flags >= 0) {
            poly           = gGpuPrimCursor;
            gGpuPrimCursor = (u8*)poly + 0x28;
            setlen(poly, 9);
            poly->code       = 0x2E;
            *(s32*)&poly->x0 = s->screen0;
            *(s32*)&poly->x1 = s->screen1;
            *(s32*)&poly->x2 = s->screen2;
            *(s32*)&poly->x3 = s->screen3;
            setUV4(poly, 0xC0, 0x98, 0xF7, 0x98, 0xC0, 0xCF, 0xF7, 0xCF);
            poly->tpage = 0x48;
            poly->clut  = 0x4283;
            setRGB0(poly, shade, shade, shade);
            addPrim((u32*)((((u32)(s->depth << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (u32)gGpuCurrentOt), poly);
        }
        SCRATCH_POP_BYTES(sizeof(ActorBeamScratch));
    }
}

/// Moves an interior coordinate to the nearest padded X or Z edge.
/// Returns 1 when moved, or 0 when already outside the rectangle.
s32 func_actor_421600_80133334(GsCOORDINATE2* arg0)
{
    s16 dx;
    s16 dz;
    s32 adx;
    s32 adz;
    s32 x;
    s32 z;
    s32 z2;

    x = arg0->coord.t[0];
    if ((x >= -0xC4D) && (x < 0xD16)) {
        z = arg0->coord.t[2];
        if (z < 0xC4E) {
            if (z >= -0xC4D) {
                if ((0xD16 - x) > (x + 0xC4E)) {
                    dx = -(((GpCoordPos*)arg0)->x + 0xCE4);
                } else {
                    dx = 0xDAC - ((GpCoordPos*)arg0)->x;
                }
                z2 = arg0->coord.t[2];
                if ((0xC4E - z2) > (z2 + 0xC4E)) {
                    dz = -(((GpCoordPos*)arg0)->z + 0xCE4);
                } else {
                    dz = 0xCE4 - ((GpCoordPos*)arg0)->z;
                }
                adx = ABS(dx);
                adz = ABS(dz);
                if (adz < adx) {
                    arg0->coord.t[2] += dz;
                } else {
                    arg0->coord.t[0] += dx;
                }
                arg0->flg = 0;
                return 1;
            }
        }
    }
    return 0;
}

void func_actor_421600_80133444(GsCOORDINATE2* arg0)
{
    SVECTOR              vec;
    SVECTOR*             dir;
    OverlayRangeScratch* blk;
    u8*                  head;
    s32                  outside;
    u32                  spad_a;
    u32                  spad_b;

    if ((u32)(arg0->coord.t[0] - 0x1F5) < 0x3E7) {
        if (arg0->coord.t[2] < 0x1F4) {
            if (arg0->coord.t[2] < -0x1F4) {
                head                           = SCRATCH_HEAD(void);
                blk                            = (OverlayRangeScratch*)(head - 0xC);
                spad_a                         = (u32)PSX_SCRATCH;
                *(void**)((u8*)spad_a + 0x3FC) = blk;
                vec.vx                         = (u16)arg0->coord.t[0] - 0x3E8;
                vec.vy                         = 0;
                vec.vz                         = (u16)arg0->coord.t[2] + 1;
                blk->dx                        = vec.vx;
                dir                            = &vec;
                blk->dz                        = dir->vz;
                blk->r                         = 0x2D0;
                blk->dx                        = blk->dx * blk->dx;
                blk->dz                        = blk->dz * blk->dz;
                blk->r                         = blk->r * blk->r;
                spad_b                         = (u32)PSX_SCRATCH + 0x3F8;
                *(void**)((u8*)spad_b + 0x4)   = head;
                outside                        = blk->dx + blk->dz >= blk->r;
                if (outside != 0) {
                    return;
                }
                VectorNormalSS(dir, dir);
                gte_lddp(0x2BC);
                gte_ldsv(dir);
                gte_gpf12();
                gte_stsv(dir);
                arg0->coord.t[0] = vec.vx + 0x3E8;
                arg0->coord.t[2] = vec.vz;
                arg0->flg        = 0;
            }
        }
    }
}

s32 func_actor_421600_801335BC(GsCOORDINATE2* coord, GpRec18* recs, s16 count, SVECTOR* pos)
{
    u8*                      head;
    Actor421600AvoidScratch* s;
    s16                      diff;
    s16                      t;
    s32                      mag;

    if (Mc_SaveData.field_5C1 == 1 || gGameSession->viewReady == 1) {
        return 0;
    }

    head             = SCRATCH_HEAD(u8);
    SCRATCH_HEAD(u8) = head - sizeof(Actor421600AvoidScratch);
    s                = (Actor421600AvoidScratch*)SCRATCH_HEAD(u8);

    s->blocked = 0;
    pos->vz    = 0;
    pos->vy    = 0;
    pos->vx    = 0;

    Gfx_MatrixCol1(&coord->workm, (SVECTOR*)(head - 0x38));
    VectorNormalSS((SVECTOR*)(head - 0x38), (SVECTOR*)(head - 0x38));

    if (ABS(s->dir.vz) < 0x818) {
        s->face = ratan2(-coord->workm.m[2][0], coord->workm.m[2][2]);
    } else {
        s->face = -ratan2(-coord->workm.m[0][2], coord->workm.m[1][2]);
    }

    s->eye.vx = *(u16*)&coord->workm.t[0];
    s->eye.vy = *(u16*)&coord->workm.t[1];
    s->eye.vz = *(u16*)&coord->workm.t[2];
    s->count  = 0;

    for (s->i = 0; s->i < count; s->i++) {
        if (recs[s->i].key == 0) {
            break;
        }
        s->kind  = recs[s->i].key & 0xFFFF0000;
        s->flags = recs[s->i].key & 0x80;
        switch (s->kind) {
            case 0x10000:
                if (s->flags == 0) {
                    s->blocked = 1;
                }
            case 0x30000:
                break;
            default:
                continue;
        }

        if (ABS(s->dir.vz) < 0x818) {
            s->angle[s->count] = overlayBearingXZ((SVECTOR3*)&recs[s->i].point, &s->eye);
        } else {
            s->angle[s->count] = overlayBearingXY((SVECTOR3*)&recs[s->i].point, &s->eye);
        }
        s->ok[s->count] = 1;
        s->count++;
        if (s->count >= 8) {
            break;
        }
    }

    for (s->i = 0; s->i < s->count; s->i++) {
        for (s->j = s->i + 1; s->j < s->count; s->j++) {
            diff = (u16)s->angle[s->i] - (u16)s->angle[s->j];
            t    = diff;
            if (diff < 0) {
            wrapUp:
                if (t < -0x800) {
                    t += 0x1000;
                    goto wrapUp;
                }
            } else {
            wrapDown:
                if (t > 0x800) {
                    t -= 0x1000;
                    goto wrapDown;
                }
            }
            mag     = t;
            s->diff = mag;
            SOFT_BARRIER();
            if (mag < 0) {
                mag = -mag;
            }
            if (mag >= 0x401) {
                s->ok[s->i] = 0;
                s->ok[s->j] = 0;
            }
        }
        if (s->ok[s->i] != 0) {
            diff = ((u16)s->angle[s->i] - (u16)s->face) +
                   ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
            s->diff = diff;
            Gfx_RotMatrixY(&s->m, diff, 1);
            Gfx_MatrixCol2(&s->m, &s->dir);
            VectorNormalSS(&s->dir, &s->dir);
            gte_lddp(-10);
            gte_ldsv(&s->dir);
            gte_gpf12();
            gte_stsv(&s->dir);
            pos->vx           += s->dir.vx;
            pos->vz           += s->dir.vz;
            coord->coord.t[0] += s->dir.vx;
            coord->coord.t[2] += s->dir.vz;
        }
    }

    SCRATCH_POP_BYTES(sizeof(Actor421600AvoidScratch));
    return s->blocked != 0;
}

void func_actor_421600_80133B30(Task* arg0)
{
    GpAnimPose           pose;
    GpAnimPose           blendPose;
    Actor421600AnimWork* work;
    s32                  blend;
    s32                  invBlend;
    s16                  index;
    s16                  next;

    index = 1;
    work  = (Actor421600AnimWork*)arg0->work;
    do {
        switch (index) {
            case 1:
                blend = 0xC00;
                break;
            case 2:
                blend = 0x800;
                break;
            case 3:
            case 4:
            case 5:
                blend = 0x5DE;
                break;
            default:
                blend = 0xBD0;
                break;
        }
        invBlend = 0x1000 - blend;
        if (index < 0xB) {
            work->blendSlots[index].rate = (u8)work->field_83A;
            work->slots[index].rate      = (s8)(work->field_832 - 3);
            func_800B3448(&work->anim, index, (s32)&pose, 0);
            func_800B3448(&work->blendAnim, index, (s32)&blendPose, 0);
            Gp_AnimWritePoseCopy(&work->anim, index, &pose, &blendPose, blend, invBlend);
        } else {
            work->slots[index].rate = (s8)(work->field_832 - 3);
            Gp_AnimTickIndex(&work->anim, index);
        }
        next  = index + 1;
        index = next;
    } while (next < 0x12);
}

/// Per-frame effect dispatch keyed on `field_82E` and the low ten bits of
/// `field_5A`. Each recognised frame is handled once: `field_848[1]` remembers
/// the frame last handled, and meeting it again only clears `clear`. A handled
/// frame spawns its effects while the room effect mode is 2 and returns a
/// request word; otherwise the result is 0, after wiping `field_848` when no
/// case claimed the frame.
///
/// `steer` is a matching carrier (see `CSE_STEER`); it has no effect.
s32 func_actor_421600_80133CAC(Task* arg0, Actor421600Work* work)
{
    SVECTOR offset;
    u32     prev;
    s32     clear = 1;
    s32     steer;

    switch (work->field_82E) {
        case 0:
            if ((work->field_5A & 0x3FF) == 9) {
                prev = work->field_848[1];
                if (prev != 9) {
                    work->field_848[1] = 9;
                    offset.vz          = 0;
                    offset.vx          = 0;
                    offset.vy          = 0x258;
                    if (Gp_State1C->roomEffectMode == 2) {
                        Gp_SpawnEff(0x60054, &((TmdObject*)arg0->extra)->coords[17], 0x80002280, &offset);
                    }
                    offset.vz = 0;
                    offset.vx = 0;
                    offset.vy = 0x2BC;
                    if (Gp_State1C->roomEffectMode == 2) {
                        Gp_SpawnEff(0x60054, &((TmdObject*)arg0->extra)->coords[9], 0x80002120, &offset);
                    }
                    return 0x40010002;
                }
                work->field_848[1] = prev;
                clear              = 0;
            }
            if ((work->field_5A & 0x3FF) == 6) {
                prev = work->field_848[1];
                if (prev != 6) {
                    work->field_848[1] = 6;
                    offset.vz          = 0;
                    offset.vx          = 0;
                    offset.vy          = 0x258;
                    if (Gp_State1C->roomEffectMode == 2) {
                        Gp_SpawnEff(0x60054, &((TmdObject*)arg0->extra)->coords[14], 0x80002220, &offset);
                    }
                    offset.vz = 0;
                    offset.vx = 0;
                    offset.vy = 0x2BC;
                    if (Gp_State1C->roomEffectMode == 2) {
                        Gp_SpawnEff(0x60054, &((TmdObject*)arg0->extra)->coords[7], 0x80002120, &offset);
                    }
                    return 0x40010001;
                }
                work->field_848[1] = prev;
                clear              = 0;
            }
            break;
        case 10:
            if ((work->field_5A & 0x3FF) == 10) {
                prev = work->field_848[1];
                if (prev != 10) {
                    work->field_848[1] = 10;
                    offset.vz          = 0;
                    offset.vx          = 0;
                    offset.vy          = 0x0;
                    if (Gp_State1C->roomEffectMode == 2) {
                        Gp_SpawnEff(0x60054, &((TmdObject*)arg0->extra)->coords[0], 0x80004A00, &offset);
                    }
                    return 0x40010005;
                }
                work->field_848[1] = prev;
                clear              = 0;
            }
            break;
        case 3:
            if ((work->field_5A & 0x3FF) == 12) {
                prev = work->field_848[1];
                if (prev != 12) {
                    work->field_848[1] = 12;
                    return 0x40010004;
                }
                work->field_848[1] = prev;
                clear              = 0;
            }
            if ((work->field_5A & 0x3FF) == 8) {
                prev = work->field_848[1];
                if (prev != 8) {
                    work->field_848[1] = 8;
                    return 0x40010003;
                }
                work->field_848[1] = prev;
                clear              = 0;
            }
            break;
        case 6:
            if ((work->field_5A & 0x3FF) == 6) {
                prev = work->field_848[1];
                if (prev != 6) {
                    work->field_848[1] = 6;
                    offset.vz          = 0;
                    offset.vx          = 0;
                    offset.vy          = 0x2BC;
                    if (Gp_State1C->roomEffectMode == 2) {
                        Gp_SpawnEff(0x60054, &((TmdObject*)arg0->extra)->coords[9], 0x80003200, &offset);
                    }
                    offset.vz = 0;
                    offset.vx = 0;
                    offset.vy = 0x2BC;
                    if (Gp_State1C->roomEffectMode == 2) {
                        Gp_SpawnEff(0x60054, &((TmdObject*)arg0->extra)->coords[7], 0x80003200, &offset);
                    }
                    return 0x40010011;
                }
                work->field_848[1] = prev;
                clear              = 0;
            }
            if ((work->field_5A & 0x3FF) == 12) {
                prev = work->field_848[1];
                if (prev != 12) {
                    work->field_848[1] = 12;
                    offset.vz          = 0;
                    offset.vx          = 0;
                    offset.vy          = 0x258;
                    if (Gp_State1C->roomEffectMode == 2) {
                        Gp_SpawnEff(0x60054, &((TmdObject*)arg0->extra)->coords[17], 0x80004480, &offset);
                    }
                    offset.vz = 0;
                    offset.vx = 0;
                    offset.vy = 0x258;
                    if (Gp_State1C->roomEffectMode == 2) {
                        Gp_SpawnEff(0x60054, &((TmdObject*)arg0->extra)->coords[14], 0x80004480, &offset);
                    }
                    return 0x40010011;
                }
                work->field_848[1] = prev;
                clear              = 0;
            }
            if ((work->field_5A & 0x3FF) == 13) {
                prev = work->field_848[1];
                if (prev != 13) {

                    work->field_848[1] = 13;
                    offset.vz          = 0;
                    offset.vx          = 0;
                    offset.vy          = 0x2BC;
                    if (Gp_State1C->roomEffectMode == 2) {
                        Gp_SpawnEff(0x60054, &((TmdObject*)arg0->extra)->coords[9], 0x80002200, &offset);
                    }
                    CSE_STEER(steer);
                    offset.vz = 0;
                    offset.vx = 0;
                    offset.vy = 0x2BC;
                    if (steer == 0 && Gp_State1C->roomEffectMode == 2) {
                        Gp_SpawnEff(0x60054, &((TmdObject*)arg0->extra)->coords[7], 0x80002240, &offset);
                    }
                    CSE_STEER(steer);
                    offset.vz = 0;
                    offset.vx = 0;
                    offset.vy = 0x258;
                    if (steer == 0 && Gp_State1C->roomEffectMode == 2) {
                        Gp_SpawnEff(0x60054, &((TmdObject*)arg0->extra)->coords[17], 0x80003300, &offset);
                    }

                    offset.vz = 0;
                    offset.vx = 0;
                    offset.vy = 0x258;
                    if (Gp_State1C->roomEffectMode == 2) {
                        Gp_SpawnEff(0x60054, &((TmdObject*)arg0->extra)->coords[14], 0x80003340, &offset);
                    }
                    return 0x40010011;
                }
                work->field_848[1] = prev;
                clear              = 0;
            }
            break;
        case 21:
            if ((work->field_5A & 0x3FF) == 6) {
                prev = work->field_848[1];
                if (prev != 6) {
                    work->field_848[1] = 6;
                    offset.vz          = 0;
                    offset.vx          = 0;
                    offset.vy          = 0x2BC;
                    if (Gp_State1C->roomEffectMode == 2) {
                        Gp_SpawnEff(0x60054, &((TmdObject*)arg0->extra)->coords[9], 0x80003200, &offset);
                    }
                    offset.vz = 0;
                    offset.vx = 0;
                    offset.vy = 0x2BC;
                    if (Gp_State1C->roomEffectMode == 2) {
                        Gp_SpawnEff(0x60054, &((TmdObject*)arg0->extra)->coords[7], 0x80003200, &offset);
                    }
                    return 0x40010001;
                }
                work->field_848[1] = prev;
                clear              = 0;
            }
            if ((work->field_5A & 0x3FF) == 9) {
                prev = work->field_848[1];
                if (prev != 9) {
                    work->field_848[1] = 9;
                    offset.vz          = 0;
                    offset.vx          = 0;
                    offset.vy          = 0x2BC;
                    if (Gp_State1C->roomEffectMode == 2) {
                        Gp_SpawnEff(0x60054, &((TmdObject*)arg0->extra)->coords[9], 0x80003200, &offset);
                    }
                    offset.vz = 0;
                    offset.vx = 0;
                    offset.vy = 0x258;
                    if (Gp_State1C->roomEffectMode == 2) {
                        Gp_SpawnEff(0x60054, &((TmdObject*)arg0->extra)->coords[17], 0x80003200, &offset);
                    }
                    return 0x40010001;
                }
                work->field_848[1] = prev;
                clear              = 0;
            }
            if ((work->field_5A & 0x3FF) == 14) {
                prev = work->field_848[1];
                if (prev != 14) {
                    work->field_848[1] = 14;
                    offset.vz          = 0;
                    offset.vx          = 0;
                    offset.vy          = 0x258;
                    if (Gp_State1C->roomEffectMode == 2) {
                        Gp_SpawnEff(0x60054, &((TmdObject*)arg0->extra)->coords[14], 0x80003200, &offset);
                    }
                    offset.vz = 0;
                    offset.vx = 0;
                    offset.vy = 0x258;
                    if (Gp_State1C->roomEffectMode == 2) {
                        Gp_SpawnEff(0x60054, &((TmdObject*)arg0->extra)->coords[17], 0x80003200, &offset);
                    }
                    return 0x40010002;
                }
                work->field_848[1] = prev;
                clear              = 0;
            }
            break;
        case 20:
            if ((work->field_5A & 0x3FF) == 6) {
                prev = work->field_848[1];
                if (prev != 6) {
                    work->field_848[1] = 6;
                    offset.vz          = 0;
                    offset.vx          = 0;
                    offset.vy          = 0x2BC;
                    if (Gp_State1C->roomEffectMode == 2) {
                        Gp_SpawnEff(0x60054, &((TmdObject*)arg0->extra)->coords[9], 0x80003200, &offset);
                    }
                    offset.vz = 0;
                    offset.vx = 0;
                    offset.vy = 0x2BC;
                    if (Gp_State1C->roomEffectMode == 2) {
                        Gp_SpawnEff(0x60054, &((TmdObject*)arg0->extra)->coords[7], 0x80003200, &offset);
                    }
                    return 0x40010001;
                }
                work->field_848[1] = prev;
                clear              = 0;
            }
            if ((work->field_5A & 0x3FF) == 10) {
                prev = work->field_848[1];
                if (prev != 10) {
                    work->field_848[1] = 10;
                    offset.vz          = 0;
                    offset.vx          = 0;
                    offset.vy          = 0x2BC;
                    if (Gp_State1C->roomEffectMode == 2) {
                        Gp_SpawnEff(0x60054, &((TmdObject*)arg0->extra)->coords[7], 0x80003200, &offset);
                    }
                    offset.vz = 0;
                    offset.vx = 0;
                    offset.vy = 0x258;
                    if (Gp_State1C->roomEffectMode == 2) {
                        Gp_SpawnEff(0x60054, &((TmdObject*)arg0->extra)->coords[14], 0x80003200, &offset);
                    }
                    return 0x40010001;
                }
                work->field_848[1] = prev;
                clear              = 0;
            }
            if ((work->field_5A & 0x3FF) == 14) {
                prev = work->field_848[1];
                if (prev != 14) {
                    work->field_848[1] = 14;
                    offset.vz          = 0;
                    offset.vx          = 0;
                    offset.vy          = 0x258;
                    if (Gp_State1C->roomEffectMode == 2) {
                        Gp_SpawnEff(0x60054, &((TmdObject*)arg0->extra)->coords[14], 0x80003200, &offset);
                    }
                    offset.vz = 0;
                    offset.vx = 0;
                    offset.vy = 0x258;
                    if (Gp_State1C->roomEffectMode == 2) {
                        Gp_SpawnEff(0x60054, &((TmdObject*)arg0->extra)->coords[17], 0x80003200, &offset);
                    }
                    return 0x40010002;
                }
                work->field_848[1] = prev;
                clear              = 0;
            }
            break;
    }
    if (clear == 1) {
        Mem_Set(work->field_848, 0, 0x48);
    }
    return 0;
}

void func_actor_421600_80134604(Task* arg0)
{
    s32                  index;
    u32                  table;
    Actor421600AnimWork* seekWork;
    Actor421600AnimWork* resetWork;
    Actor421600AnimWork* turnWork;
    Actor421600AnimWork* secondaryWork;
    Actor421600AnimWork* tickWork;
    Actor421600AnimWork* work;
    s32                  targetAngle;
    s32                  animation;
    s32                  updatedTurn;
    s16                  currentTurn;
    s16                  thirdAngle;
    s16                  state;
    s32                  currentAngle;
    s16                  angle;
    s32                  seekSlotIndex;
    s32                  resetSlotIndex;
    s32                  secondarySlotIndex;
    s32                  tickSlotIndex;
    s32                  signedTurn;
    s32                  sound;
    s32                  resetIndex;
    s32                  secondaryIndex;
    s32                  tickIndex;
    s32                  seekIndex;
    s32                  delta;
    s8*                  tickSlot;
    s8*                  seekSlot;
    s8*                  resetSlot;
    s8*                  secondarySlot;
    s32                  pan;
    s32                  currentAngleBits;
    u16                  originalTurn;
    s32                  targetAngleBits;
    u16                  updatedTurnBits;
    s32                  clampedAngle;
    s32                  targetTurn;

    work  = (Actor421600AnimWork*)arg0->work;
    state = (s16)work->field_828;
    if (state == 1) {
        if (work->field_82C != (s16)work->field_82E) {
            seekWork = work;
            TOUCH_REG(seekWork);
            seekIndex = 1;
            table     = (u32)&D_actor_421600_80150DB4;
            seekSlot  = (s8*)&work->anim.slots;
            do {
                seekSlotIndex  = seekIndex;
                seekSlot[0x39] = (u8)seekWork->field_832;
                animation      = (s16)seekWork->field_82E;
                seekSlot      += 0x28;
                index          = seekWork->field_82C * 0x19;
                func_800B4114(&seekWork->anim, seekSlotIndex, animation, 0, (s32) * (s8*)((animation + index) + table));
                seekIndex += 1;
            } while (seekIndex < 0x12);
            seekWork->field_82C = (s16)seekWork->field_82E;
        }
        work->field_828 = 3;
        work->field_830 = 0;
        Mem_Set(work->field_848, 0U, 0x48U);
    } else if (state == 2) {
        resetWork = work;
        TOUCH_REG(resetWork);
        resetIndex = 1;
        resetSlot  = (s8*)&work->anim.slots;
        do {
            resetSlotIndex  = resetIndex;
            resetSlot[0x39] = (u8)resetWork->field_832;
            resetSlot      += 0x28;
            Gp_AnimResetSlot(&resetWork->anim, resetSlotIndex, (s32)(s16)resetWork->field_82E);
            resetIndex += 1;
        } while (resetIndex < 0x12);
        resetWork->field_82C = (s16)resetWork->field_82E;
        work->field_828      = 3;
        work->field_830      = 0U;
        Mem_Set(work->field_848, 0U, 0x48U);
    }
    if (work->field_836 == 2) {
        secondaryWork            = (Actor421600AnimWork*)arg0->work;
        secondaryIndex           = 1;
        secondarySlot            = (s8*)&secondaryWork->anim.slots;
        secondaryWork->field_83A = 0x20;
        secondaryWork->field_83C = 0x800;
        do {
            secondarySlotIndex  = secondaryIndex;
            secondarySlot[0x39] = (u8)secondaryWork->field_83A;
            secondarySlot      += 0x28;
            Gp_AnimResetSlot(&secondaryWork->blendAnim, secondarySlotIndex, (s32)secondaryWork->field_838);
            secondaryIndex += 1;
        } while (secondaryIndex < 0x12);
        work->field_836 = 3;
    }
    work->field_830 = (u16)(work->field_830 + 1);
    if ((s16)work->field_82A == 0) {
        tickWork  = (Actor421600AnimWork*)arg0->work;
        tickIndex = 1;
        tickSlot  = (s8*)&tickWork->anim.slots;
        do {
            tickSlotIndex  = tickIndex;
            tickSlot[0x39] = (u8)tickWork->field_832;
            Gp_AnimTickIndex(&tickWork->anim, tickSlotIndex);
            tickSlot  += 0x28;
            tickIndex += 1;
        } while (tickIndex < 0x12);
    } else {
        func_actor_421600_80133B30(arg0);
        if (work->blendSlots[1].flags & 0x100) {
            work->field_82A = 0;
        }
    }
    targetAngle      = (s16)work->field_840;
    currentAngle     = (s16)work->field_844;
    targetAngleBits  = work->field_840;
    currentAngleBits = (u16)work->field_844;
    if (currentAngle < targetAngle) {
        if ((targetAngle - currentAngle) >= 0x72) {
            work->field_844 = currentAngleBits + 0x71;
        } else {
            goto block_26;
        }
    } else if ((currentAngle - targetAngle) >= 0x72) {
        work->field_844 = currentAngleBits - 0x71;
    } else {
    block_26:
        work->field_844 = targetAngleBits;
    }
    angle        = (s16)work->field_844;
    clampedAngle = (u16)work->field_844;
    if (angle != 0) {
        if (angle >= 0x501) {
            clampedAngle = 0x500;
        }
        if (angle < -0x500) {
            clampedAngle = -0x500;
        }
        thirdAngle = (s16)clampedAngle / 3;
        func_actor_421600_80132004(&((TmdObject*)arg0->extra)->coords[2], thirdAngle);
        ((TmdObject*)arg0->extra)->coords[2].flg = 0;
        func_actor_421600_80132004(&((TmdObject*)arg0->extra)->coords[3], thirdAngle);
        ((TmdObject*)arg0->extra)->coords[3].flg = 0;
        func_actor_421600_80132004(&((TmdObject*)arg0->extra)->coords[4], (s16)clampedAngle / 2);
        ((TmdObject*)arg0->extra)->coords[4].flg = 0;
    }
    if (((s16)work->field_82E == 0) && (work->field_0 == 0x26)) {
        Gfx_RotMatrixX(&((TmdObject*)arg0->extra)->coords[4].coord, 0x280, 0);
        ((TmdObject*)arg0->extra)->coords[4].flg = 0;
        Gp_UpdateCoord(&((TmdObject*)arg0->extra)->coords[4]);
    }
    turnWork     = (Actor421600AnimWork*)arg0->work;
    targetTurn   = turnWork->field_83E;
    originalTurn = targetTurn;
    if ((s16)targetTurn >= 0x201) {
        targetTurn = 0x200;
    }
    if ((s16)originalTurn < -0x200) {
        targetTurn = -0x200;
    }
    signedTurn  = (s16)targetTurn;
    currentTurn = turnWork->field_842;
    if (currentTurn < signedTurn) {
        if ((signedTurn - currentTurn) >= 0xD) {
            turnWork->field_842 = (s16)((u16)turnWork->field_842 + 0xC);
        } else {
            turnWork->field_842 = (s16)targetTurn;
        }
    }
    updatedTurn     = turnWork->field_842;
    updatedTurnBits = (u16)turnWork->field_842;
    if ((s16)targetTurn < updatedTurn) {
        delta = updatedTurn - (s16)targetTurn;
        if (delta < 0) {
            delta = -delta;
        }
        if (delta >= 0xD) {
            turnWork->field_842 = (s16)(updatedTurnBits - 0xC);
        } else {
            turnWork->field_842 = (s16)targetTurn;
        }
    }
    func_actor_421600_80132004(&((TmdObject*)arg0->extra)->coords[10], (s16)((s32)(u16)turnWork->field_842 * -1));
    ((TmdObject*)arg0->extra)->coords[10].flg = 0;
    sound                                     = func_actor_421600_80133CAC(arg0, (Actor421600Work*)work);
    if (sound != 0) {
        pan = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(sound, (s32)pan, (s32)(s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
    }
}

static __inline__ void Actor421600_BindMatrices(Task* actor)
{
    Actor421600Work* work;
    TmdObject*       obj;
    work          = actor->work;
    obj           = actor->extra;
    obj->lightMtx = &work->field_E04;
    obj->colorMtx = &work->field_E24;
}

void func_actor_421600_80134AD4(GpEnemy* enemy, Task* actor)
{
    SVECTOR          dir;
    s32              kind;
    SVECTOR*         v;
    VECTOR           pos;
    TmdObject*       obj;
    GsCOORDINATE2*   root;
    Actor421600Work* mapped;
    Actor421600Work* work;
    GpObj*           body;
    GpObj*           head;
    s16              extent;
    s32              linkKind;
    GpObj*           linkObj;
    root        = ((TmdObject*)actor->extra)->coords;
    obj         = actor->extra;
    work        = memCalloc(0xEB0, 0);
    actor->work = work;
    if (work == 0) {
        Gp_DestroyEnemy(enemy, actor);
        return;
    }
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    actor->exitCallback = func_actor_421600_8013E668;
    Actor421600_BindMatrices(actor);
    enemy->field_4    = &((TmdObject*)actor->extra)->coords[0].coord;
    enemy->field_48   = 0;
    enemy->bodyPos.vx = 0;
    enemy->bodyPos.vy = 0;
    enemy->bodyPos.vz = 0;
    enemy->coord      = &((TmdObject*)actor->extra)->coords[2];
    Gp_LinkNode(&enemy->node);
    enemy->node.state.b.flags = 1;
    enemy->reactionFlags      = 0;
    enemy->hp                 = (s16)D_actor_421600_8013EF38.hpMax;
    enemy->param              = &D_actor_421600_8013EF38;
    enemy->recs               = &work->field_90C;
    func_800B3F84(&((Actor421600AnimWork*)work)->anim, D_actor_421600_80151028, obj, &((Actor421600AnimWork*)work)->slots[18], ((Actor421600AnimWork*)work)->slots);
    func_800B3F84(&((Actor421600AnimWork*)work)->blendAnim, D_actor_421600_80151028, obj, &((Actor421600AnimWork*)work)->blendSlots[18], ((Actor421600AnimWork*)work)->blendSlots);
    work->field_828 = 2;
    work->field_82A = 0;
    work->field_82E = 1;
    work->field_844 = 0;
    work->field_840 = 0;
    work->field_834 = 0x10;
    work->field_832 = 0x10;
    func_actor_421600_80134604(actor);
    work->field_B6C.ctx.recs = &work->field_B8C;
    work->field_B6C.coord    = root;
    work->field_B6C.pos.vx   = 0;
    work->field_B6C.pos.vy   = -0x11C;
    work->field_B6C.pos.vz   = 0;
    work->field_B6C.key      = 0x30001;
    work->field_B6C.radius   = 0x12C;
    work->field_B6C.flags    = 1;
    Gp_LinkObj(2, &work->field_B6C);
    work->field_CCC.end0.vy = -0x180;
    work->field_CCC.end1.vy = -0x180;
    extent                  = 0x2BC;
    linkKind                = 2;
    linkObj                 = &work->field_CAC;
    __asm__("" : "+r"(linkKind), "+r"(linkObj) : "m"(work->field_CCC.end0.vy), "m"(work->field_CCC.end1.vy));
    __asm__("" : "+r"(extent) : "m"(work->field_CCC.end0.vy), "m"(work->field_CCC.end1.vy));
    work->field_CCC.end0Radius   = 0x12C;
    mapped                       = work;
    mapped->field_CCC.end1Radius = 0x12C;
    __asm__("" : "+r"(extent), "+m"(mapped->field_CCC.end1Radius) : "r"(work->field_CE4));
    mapped->field_CCC.end1.vz   = extent;
    mapped->field_CAC.ctx.d4rec = &mapped->field_CCC;
    mapped->field_CCC.end0.vx   = 0;
    mapped->field_CCC.end0.vz   = 0;
    mapped->field_CCC.end1.vx   = 0;
    mapped->field_CCC.recs      = work->field_CE4;
    (&mapped->field_CAC)->coord = root;
    mapped->field_CAC.pos.vx    = 0;
    mapped->field_CAC.pos.vy    = 0;
    mapped->field_CAC.pos.vz    = 0;
    mapped->field_CAC.key       = 0x30001;
    mapped->field_CAC.radius    = 0;
    mapped->field_CAC.flags     = 3;
    mapped->field_B6C.flags     = mapped->field_B6C.flags | 0x4000;
    Gp_LinkObj(linkKind, linkObj);
    mapped->field_CAC.flags = mapped->field_CAC.flags | 0x4000;
    Gp_InitRec18Table(mapped->field_CE4, 0xC, 0);
    Gp_InitRec18Table(mapped->field_B6C.ctx.recs, 0xC, 0);
    body           = &mapped->field_8EC;
    body->coord    = &((TmdObject*)actor->extra)->coords[2];
    body->ctx.recs = &mapped->field_90C;
    body->pos.vx   = 0;
    body->pos.vy   = 0;
    body->pos.vz   = 0;
    body->key      = 0x30001;
    body->radius   = 0x19C;
    body->flags    = 1;
    Gp_LinkObj(2, body);
    body->flags |= 0x8000;
    Gp_InitRec18Table(body->ctx.recs, 0xC, 0);
    head           = &mapped->field_A2C;
    head->coord    = &((TmdObject*)actor->extra)->coords[10];
    head->ctx.recs = &mapped->field_A4C;
    head->pos.vx   = 0;
    head->pos.vy   = 0;
    head->pos.vz   = 0;
    head->key      = 0x30001;
    head->radius   = 0x100;
    head->flags    = 1;
    Gp_LinkObj(2, head);
    head->flags |= 0x8000;
    Gp_InitRec18Table(head->ctx.recs, 0xC, 0);
    mapped->field_A2C.pos.vx = 0;
    mapped->field_A2C.pos.vy = 0;
    mapped->field_A2C.pos.vz = -0x100;
    mapped->field_14         = 0;
    mapped->field_C[0].x     = ((TmdObject*)actor->extra)->coords->coord.t[0];
    mapped->field_C[0].z     = ((TmdObject*)actor->extra)->coords->coord.t[2];
    Gfx_MatrixCol2(&((TmdObject*)actor->extra)->coords->coord, &dir);
    dir.vy = 0;
    v      = &dir;
    VectorNormalSS(v, v);
    gte_lddp(5000);
    gte_ldsv(v);
    gte_gpf12();
    gte_stsv(v);
    mapped->field_C[1].x = ((TmdObject*)actor->extra)->coords->coord.t[0] + dir.vx;
    mapped->field_C[1].z = ((TmdObject*)actor->extra)->coords->coord.t[2] + dir.vz;
    mapped->field_E88    = 3;
    mapped->field_E7C    = 0;
    mapped->field_E80    = 1;
    mapped->field_E84    = 0;
    mapped->field_E8C    = 1;
    actor->msgTable      = &D_actor_421600_80151118;
    root->sub            = &gGfxViewCoord;
    root->flg            = 0;
    Gp_UpdateCoord(root);
    pos.vx = root->workm.t[0];
    pos.vy = root->workm.t[1];
    pos.vz = root->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);
    kind = actor->spawnArg1 >> 16;
    switch (kind & 0xF) {
        case 1:
            mapped->field_2 = -1;
            mapped->field_0 = 0;
            break;

        case 2:
            mapped->field_2 = -1;
            mapped->field_0 = 0x21;
            break;

        case 0:

        default:
            mapped->field_2 = -1;
            mapped->field_0 = 0x18;
            Tmd_AllocBuffers(obj);
            break;
    }

    switch (actor->spawnArg1 & 0xF) {
        case 2:
            mapped->field_EA2 = D_actor_421600_8013EF48[0].field_2;
            mapped->field_EA4 = D_actor_421600_8013EF48[0].field_0;
            mapped->field_EA6 = D_actor_421600_8013EF48[0].field_4;
            mapped->field_EA8 = D_actor_421600_8013EF48[0].field_6;
            break;

        case 1:
            mapped->field_EA2 = D_actor_421600_8013EF48[2].field_2;
            mapped->field_EA4 = D_actor_421600_8013EF48[2].field_0;
            mapped->field_EA6 = D_actor_421600_8013EF48[2].field_4;
            mapped->field_EA8 = D_actor_421600_8013EF48[2].field_6;
            break;

        case 0:

        default:
            mapped->field_EA2 = D_actor_421600_8013EF48[1].field_2;
            mapped->field_EA4 = D_actor_421600_8013EF48[1].field_0;
            mapped->field_EA6 = D_actor_421600_8013EF48[1].field_4;
            mapped->field_EA8 = D_actor_421600_8013EF48[1].field_6;
            break;
    }

    Gp_StateF0.field_6      = 8;
    D_actor_421600_80151268 = 8;
    actor->state++;
}

/// Picks one of twelve hit positions out of `D_actor_421600_801510B8` by
/// damage magnitude `arg1`, then spawns effect `Gp_GetIdParam1(arg2)` on the
/// model part that entry names. Same body as the shared
/// `Actor00100_Fn03340` / `func_actor_401300_80134BA4` pair, but it reads this
/// overlay's own table, so it stays a per-overlay copy.
void func_actor_421600_801350BC(Task* arg0, s16 arg1, s32 arg2)
{
    SVECTOR*         sc;
    s32              mag;
    Actor421600Work* work;

    sc   = (SVECTOR*)SCRATCH_PUSH_BYTES(8);
    mag  = (arg1 >= 0) ? arg1 : -arg1;
    work = arg0->work;
    if (mag < 0x200) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        switch ((s32)(Gp_LcgState >> 16) & 3) {
            case 0:
                *sc = D_actor_421600_801510B8[0];
                break;
            case 1:
                *sc = D_actor_421600_801510B8[1];
                break;
            case 2:
                *sc = D_actor_421600_801510B8[2];
                break;
            case 3:
                *sc = D_actor_421600_801510B8[3];
                break;
            default:
                *sc = D_actor_421600_801510B8[4];
                break;
        }
    } else if (mag >= 0x601) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        switch ((s32)(Gp_LcgState >> 16) & 2) {
            case 0:
                *sc = D_actor_421600_801510B8[5];
                break;
            case 1:
                *sc = D_actor_421600_801510B8[6];
                break;
            default:
                *sc = D_actor_421600_801510B8[7];
                break;
        }
    } else if (arg1 > 0) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if ((Gp_LcgState >> 16) & 1) {
            *sc = D_actor_421600_801510B8[8];
        } else {
            *sc = D_actor_421600_801510B8[9];
        }
    } else {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if ((Gp_LcgState >> 16) & 1) {
            *sc = D_actor_421600_801510B8[10];
        } else {
            *sc = D_actor_421600_801510B8[11];
        }
    }
    work->field_890.coord      = &((TmdObject*)arg0->extra)->coords[sc->pad];
    work->field_890.spawnArgLo = 0x100;
    work->field_890.spawnArgHi = 2;
    work->field_898            = *sc;
    func_800FDB18(Gp_GetIdParam1(arg2) & 0xFFFF, &((TmdObject*)arg0->extra)->coords[sc->pad], &work->field_898, &work->field_890);
    SCRATCH_POP_BYTES(8);
}

static __inline__ s32 Actor421600_FindDamageHit(GpRec18* records,
                                                SVECTOR* pos)
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

void func_actor_421600_801354D8(Task* arg0)
{
    s32                       callAngle;
    s32                       debugMode;
    PlayerStatus*             config = &Player_Status;
    s16                       effect;
    s16                       delta;
    s16                       z;
    s32                       state5;
    s16                       damageState;
    s16                       deathState;
    s16                       hurtState;
    s16                       poisonState;
    s16                       state0;
    s16                       state1;
    s16                       state2;
    s16                       state3;
    s16                       state4;
    s16                       wrapped;
    s16                       hitState;
    s16                       nextDeathState;
    GsCOORDINATE2*            objectCoord;
    s32                       tickDamage;
    s32                       dxSquared;
    s32                       dySquared;
    s32                       yaw;
    s32                       deathSound;
    s32                       hurtSound;
    s32                       hitSound;
    s32                       doubleDamage;
    s32                       dx;
    s32                       dy;
    s32                       dz;
    s32                       distance;
    SVECTOR*                  hitPos;
    s32                       soundBase;
    s32                       deathPan;
    s32                       hurtPan;
    s32                       hitPan;
    u16                       totalDamage;
    u32                       kind;
    Actor421600Work*          work;
    GpEnemy*                  enemy;
    Actor421600DamageScratch* scratch;
    void*                     head;
    enemy = arg0->spawnArg2;
    work  = arg0->work;
    if (enemy->hp > 0) {
        head              = SCRATCH_HEAD(void);
        scratch           = (SCRATCH_HEAD(Actor421600DamageScratch) =
                       (Actor421600DamageScratch*)head - 1);
        scratch->field_20 = Actor421600_FindDamageHit(
            &work->field_90C, (SVECTOR*)&scratch->field_18);
        if (scratch->field_20 == 0) {
            hitPos            = (SVECTOR*)&scratch->field_18;
            scratch->field_20 = Actor421600_FindDamageHit(&work->field_A4C, hitPos);
        }
        if (scratch->field_20 != 0) {
            scratch->field_2E = -1;
            work->field_E64   = Gp_GetIdParam2(scratch->field_20);
            kind              = Gp_GetIdParam0(scratch->field_20) & 0xFFFF;
            switch (kind) {
                case 0:
                case 6:
                case 7:
                case 8:
                case 9:
                    state0 = work->field_0;
                    if (state0 == 24 || state0 == 38 || state0 == 39 || state0 == 1) {
                        if (work->field_0 == 0x20) {
                            work->field_0 = 3;
                        } else {
                            work->field_0 = 0x1C;
                        }
                    }
                    if (work->field_0 == 0x21) {
                        work->field_0 = 0x22;
                    }
                    if (work->field_82A == 0) {
                        work->field_E66 = 0U;
                    }
                    state1 = work->field_0;
                    if ((state1 != 0x22) && (state1 != 0x14) && (state1 != 0x11) &&
                        (state1 != 0x15) && (state1 != 0x16) && (state1 != 4) &&
                        (state1 != 0xB) && (state1 != 0x24) && (state1 != 7)) {
                        work->field_82A = 1;
                        work->field_838 = 9;
                        work->field_836 = 2;
                    }
                    break;
                case 4:
                case 5:
                    state2 = work->field_0;
                    if (state2 == 4 || state2 == 11 || state2 == 20 || state2 == 17) {
                        work->field_0 = 11;
                        work->field_2 = -1;
                    } else if (state2 != 21 && state2 != 7) {
                        work->field_0 = 20;
                    }
                    break;
                case 2:
                    state3 = work->field_0;
                    if (state3 == 33 || state3 == 4 || state3 == 11 || state3 == 17) {
                        work->field_0 = 11;
                        work->field_2 = -1;
                    } else if (state3 != 21 && state3 != 7) {
                        work->field_0 = 20;
                    }
                    Gp_SetObjFlag2(enemy, scratch->field_20, 0);
                    break;
                case 3:
                    state4 = work->field_0;
                    if ((state4 == 0x18) || (state4 == 0x26) || (state4 == 1) ||
                        (state4 == 0x20)) {
                        work->field_0 = 0x1C;
                    }
                    if (work->field_0 == 0x21) {
                        work->field_0 = 0x22;
                    }
                    Gp_SetObjFlag4(enemy, scratch->field_20, 0);
                    break;
                case 1:
                    state5 = work->field_0;
                    if (state5 != 7) {
                        if (state5 == 4 || state5 == 11 || state5 == 20 || state5 == 17 ||
                            (state5 == 36 && (s16)work->field_6 < 10)) {
                            hitState      = 11;
                            work->field_0 = hitState;
                        } else if (state5 != 21 && state5 != 0 && state5 != 22 &&
                                   state5 != 7) {
                            hitState      = 20;
                            work->field_0 = hitState;
                        }
                    }
                    break;
            }
            dx                                     = config->coordMtx->t[0] - ((TmdObject*)arg0->extra)->coords->coord.t[0];
            dxSquared                              = dx * dx;
            scratch->field_0                       = dx;
            dy                                     = config->coordMtx->t[1] - ((TmdObject*)arg0->extra)->coords->coord.t[1];
            dySquared                              = dy * dy;
            scratch->field_4                       = dy;
            dz                                     = config->coordMtx->t[2] - ((TmdObject*)arg0->extra)->coords->coord.t[2];
            scratch->field_8                       = dz;
            distance                               = SquareRoot0(dxSquared + dySquared + (dz * dz));
            scratch->field_28                      = distance;
            scratch->field_24                      = Gp_ComputeDamage(scratch->field_20, distance, 0, 0);
            ((TmdObject*)arg0->extra)->coords->flg = 0;
            Gp_UpdateCoord(((TmdObject*)arg0->extra)->coords);
            scratch->field_10 = (u16)((TmdObject*)arg0->extra)->coords->workm.t[0];
            scratch->field_12 = (u16)((TmdObject*)arg0->extra)->coords->workm.t[1];
            scratch->field_14 = (u16)((TmdObject*)arg0->extra)->coords->workm.t[2];
            scratch->field_10 =
                (u16)(scratch->field_18 - ((TmdObject*)arg0->extra)->coords->workm.t[0]);
            scratch->field_12 =
                (u16)(scratch->field_1A - ((TmdObject*)arg0->extra)->coords->workm.t[1]);
            z                 = scratch->field_1C - ((TmdObject*)arg0->extra)->coords->workm.t[2];
            scratch->field_14 = (u16)z;
            yaw               = ratan2((s16)scratch->field_10, z);
            objectCoord       = ((TmdObject*)arg0->extra)->coords;
            delta =
                yaw - ratan2(-objectCoord->workm.m[2][0], objectCoord->workm.m[2][2]);
            wrapped           = delta;
            scratch->field_2C = delta;
            if (delta < 0) {
                while (1) {
                    if (wrapped >= -0x800)
                        break;
                    wrapped += 0x1000;
                }
            } else {
                while (1) {
                    if (wrapped <= 0x800)
                        break;
                    wrapped -= 0x1000;
                }
            }
            callAngle         = wrapped;
            scratch->field_2C = callAngle;
            func_actor_421600_801350BC(arg0, callAngle, scratch->field_20);
            work->field_844 = 0;
            work->field_840 = 0;
            if (Gp_RollEnemyChance(enemy, scratch->field_20, 0) != 0) {
                scratch->field_2E = 0;
                scratch->field_24 = (s32)(scratch->field_24 * 4);
            }
            damageState = work->field_0;
            if ((damageState == 4) || (damageState == 0xB) || (damageState == 0x11) ||
                (damageState == 0x24)) {
                doubleDamage      = scratch->field_24 * 2;
                scratch->field_24 = doubleDamage;
                if (doubleDamage != 0) {
                    scratch->field_2E = 3;
                }
            }
            func_800E2C78(enemy, scratch->field_20, scratch->field_24, 0);
            effect = scratch->field_2E;
            if (effect != -1) {
                Gp_SpawnEff(0x6009C, ((TmdObject*)arg0->extra)->coords + 2, effect, 0);
            }
            scratch->field_24 = (s32)(scratch->field_24 * 2);
            enemy->hp         = (s16)((u16)enemy->hp - (u16)scratch->field_24);
            func_800DA6E8(&enemy->node, scratch->field_24, 0);
            totalDamage     = work->field_E66 + (u16)scratch->field_24;
            work->field_E66 = totalDamage;
            if (enemy->hp <= 0) {
                D_actor_421600_80151268 -= 1;
                if ((Gp_GetIdParam0(scratch->field_20) & 0xFFFF) == 4) {
                    nextDeathState = 8;
                    goto setDeathState;
                }
                deathState = work->field_0;
                if (deathState == 33 || deathState == 17 || deathState == 11 ||
                    deathState == 4) {
                    work->field_0 = 11;
                    work->field_2 = -1;
                } else if (deathState == 7) {
                    work->field_0 = 21;
                    deathSound    = (((u16)enemy->placeKey >> 12) << 8) | 0x40010008;
                    deathPan      = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
                    SndEvt_EnqueueType6(deathSound, deathPan,
                                        (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
                } else {
                    hurtSound = (((u16)enemy->placeKey >> 12) << 8) | 0x40010008;
                    hurtPan   = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
                    SndEvt_EnqueueType6(hurtSound, hurtPan,
                                        (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
                    nextDeathState = 20;
                setDeathState:
                    work->field_0 = nextDeathState;
                }
                work->field_8E8 = 9;
                work->field_8E9 = 1;
                work->field_8EA = 3;
                Gp_DispatchMsg(gameGetPtrSlot(4), 0x7DA, (s32)&work->field_8E8, 0x7DB);
            } else {
                if ((s16)totalDamage >= 0x47) {
                    hurtState = work->field_0;
                    if ((hurtState != 0x21) && (hurtState != 0x14) &&
                        (hurtState != 0x11) && (hurtState != 7) &&
                        (work->field_E90.bytes[2] != 1)) {
                        soundBase     = 0x40010008;
                        work->field_0 = 0x14;
                    } else {
                        goto normalHitSound;
                    }
                } else {
                normalHitSound:
                    soundBase = 0x40010007;
                }
                hitSound = (((u16)enemy->placeKey >> 0xC) << 8) | soundBase;
                hitPan   = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
                SndEvt_EnqueueType6(hitSound, hitPan,
                                    (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
            }
            debugMode = Mc_SaveData.field_5C1;
            if (debugMode == 1) {
                enemy->hp       = 0x64;
                work->field_838 = 9;
                work->field_82A = (s16)debugMode;
                work->field_836 = 2;
            }
        }
        if (enemy->reactionFlags & 0xC) {
            scratch->field_24 = Gp_TickObjFlag4(enemy);
            if (Gp_ObjFlag4Expired(enemy) != 0) {
                enemy->reactionFlags = (u8)(enemy->reactionFlags & 0xF3);
            }
            enemy->hp  = (s16)((u16)enemy->hp - (u16)scratch->field_24);
            tickDamage = scratch->field_24;
            if (tickDamage != 0) {
                func_800DA6E8(&enemy->node, tickDamage, 0);
                if (enemy->hp <= 0) {
                    D_actor_421600_80151268 -= 1;
                    poisonState              = work->field_0;
                    if ((poisonState != 4) && (poisonState != 0xB) &&
                        (poisonState != 0x11)) {
                        work->field_0 = 0xC;
                    } else {
                        work->field_0 = 0x15;
                    }
                } else {
                    if (work->field_0 == 0x1C) {
                        work->field_0 = 0x26;
                    }
                    work->field_82A = 1;
                    work->field_838 = 0x12;
                    work->field_836 = 2;
                }
            }
        }
        SCRATCH_POP_BYTES(0x30);
    }
}

void func_actor_421600_80135F6C(Task* arg0)
{
    SVECTOR          offset;
    Actor421600Work* work;
    TmdObject*       obj;
    s32              nextState;
    s32              state;
    u16              flags;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                             = arg0->extra;
        ((GpEnemy*)arg0->spawnArg2)->node.state.b.flags = 0;
        obj->flags                                      = 0;
        Tmd_AllocBuffers(obj);
        flags = work->field_B6C.flags;
        TOUCH_REG(flags);
        work->field_832       = 0x10;
        work->field_B6C.flags = flags | 0x4000;
        state                 = work->field_82E;
        do {
            nextState = 0xD;
        } while (0);
        if (state == nextState) {
            nextState = 1;
        } else {
            nextState = 2;
        }
        work->field_828 = nextState;
        SOFT_BARRIER();
        work->field_83E = 0;
        work->field_840 = 0;
        func_actor_421600_80134604(arg0);
        return;
    }
    func_actor_421600_80134604(arg0);
    if ((work->field_68 & 2) && (work->field_82E == 0xD)) {
        work->field_82E = 1;
        work->field_828 = 1;
    }
    if (work->field_68 & 0x100) {
        if (work->field_82E == 0xF) {
            work->field_828 = 2;
            work->field_82E = 0x10;
        }
        func_actor_421600_80134604(arg0);
    }
    if (work->field_82E == 0xE) {
        if ((u32)((work->field_5A & 0x3FF) - 8) < 2U) {
            offset.vz = 0;
            offset.vx = 0;
            offset.vy = 0x2BC;
            if (Gp_State1C->roomEffectMode == 2) {
                Gp_SpawnEff(0x60054, ((TmdObject*)arg0->extra)->coords + 7, 0x80002300, &offset);
            }
        }
        if ((work->field_5A & 0x3FF) == 8) {
            offset.vz = 0;
            offset.vx = 0;
            offset.vy = 0x2BC;
            if (Gp_State1C->roomEffectMode == 2) {
                Gp_SpawnEff(0x60054, ((TmdObject*)arg0->extra)->coords + 7, 0x80003400, &offset);
            }
        }
    }
}

static __inline__ s16 Actor421600_Zone(GsCOORDINATE2* coord)
{
    s32 x, z, ix, iz;
    x = coord->coord.t[0];
    z = coord->coord.t[2];
    if (x >= 0xD49)
        ix = 3;
    else if (x > 0)
        ix = 2;
    else
        ix = x >= -0xC7F;
    iz = 0;
    if (z < 0xBB9) {
        iz = 1;
        if (z <= 0) {
            iz = 3;
            if (z >= -0xBB7)
                iz = 2;
        }
    }
    return D_actor_421600_801511C0[ix | (iz * 4)];
}

void func_actor_421600_80136138(Task* arg0)
{
    Actor421600Work*  work;
    ActorTurnScratch *head, *blk;
    GpEnemy*          ctx;
    TmdObject*        obj;
    GsCOORDINATE2 *   coord2, *coord3, *coord4;
    s16               playerZone, zone;
    s16               nextZone;
    s16               angle;
    s32               wrapped;
    s32               state;

    work = arg0->work;
    ctx  = arg0->spawnArg2;
    if (work->field_4 != 0) {
        obj                     = arg0->extra;
        ctx->node.state.b.flags = 0;
        obj->flags              = 0;
        Tmd_AllocBuffers(obj);
        work->field_832        = 0x10;
        work->field_82E        = 0;
        work->field_828        = 1;
        work->field_B6C.flags |= 0x4000;
        func_actor_421600_80134604(arg0);
        return;
    }
    playerZone = Actor421600_Zone(((TmdObject*)Gp_ActorSlots[0]->extra)->coords);
    zone       = Actor421600_Zone(((TmdObject*)arg0->extra)->coords);
    func_actor_421600_8013285C(((TmdObject*)arg0->extra)->coords, &work->field_B8C, 0xC);
    if (playerZone != zone) {
        switch ((s16)(playerZone - 1)) {
            case 0:
            case 1:
                if (zone >= 1 && zone <= 3) {
                    work->field_0 = 0x26;
                    return;
                }
                break;
            case 2:
                if (zone >= 1 && zone <= 6) {
                    work->field_0 = 0x26;
                    return;
                }
                break;
            case 3:
            case 4:
                if (zone >= 3 && zone <= 6) {
                    work->field_0 = 0x26;
                    return;
                }
                break;
            case 5:
                if (zone >= 3 && zone <= 9) {
                    work->field_0 = 0x26;
                    return;
                }
                break;
            case 6:
            case 7:
                if (zone >= 6 && zone <= 9) {
                    state = 0x26;
                    SOFT_TOUCH_REG(state);
                    work->field_0 = state;
                    return;
                }
                break;
            case 8:
                if (zone >= 6 && zone <= 11) {
                    state = 0x26;
                    SOFT_TOUCH_REG(state);
                    work->field_0 = state;
                    return;
                }
                break;
            case 9:
            case 10:
                if (zone >= 9 && zone <= 11) {
                    state = 0x26;
                    SOFT_TOUCH_REG(state);
                    work->field_0 = state;
                    return;
                }
                break;
            case 11:
                if (zone >= 0xB) {
                    work->field_0 = 0x26;
                    return;
                }
                if (zone >= 0xC) {
                    work->field_0 = 0x26;
                    return;
                }
                break;
        }
    } else {
        work->field_0 = 0x26;
        return;
    }
    func_actor_421600_80134604(arg0);
    head = SCRATCH_HEAD(ActorTurnScratch);
    SCRATCH_PUSH(ActorTurnScratch);
    blk = head - 1;
    if (zone > playerZone)
        nextZone = zone - 1;
    else
        nextZone = zone + 1;
    head[-1].delta.vx = D_actor_421600_80151158[nextZone].vx;
    blk->delta.vy     = D_actor_421600_80151158[nextZone].vy;
    blk->delta.vz     = D_actor_421600_80151158[nextZone].vz;
    blk->delta.vx     = blk->delta.vx - (u16)((TmdObject*)arg0->extra)->coords->coord.t[0];
    blk->delta.vy     = 0;
    blk->delta.vz     = blk->delta.vz - (u16)((TmdObject*)arg0->extra)->coords->coord.t[2];
    coord2            = ((TmdObject*)arg0->extra)->coords;
    angle             = ratan2(blk->delta.vx, blk->delta.vz) - ratan2(-coord2->coord.m[2][0], coord2->coord.m[2][2]);
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
    wrapped         = angle;
    blk->angle      = wrapped;
    work->field_840 = wrapped;
    if (blk->angle >= 0x21)
        blk->angle = 0x20;
    if (blk->angle < -0x20)
        blk->angle = -0x20;
    work->field_83E = blk->angle;
    coord3          = ((TmdObject*)arg0->extra)->coords;
    blk->angle      = blk->angle + ratan2(-coord3->coord.m[2][0], coord3->coord.m[2][2]);
    Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->coords->coord, blk->angle, 1);
    if (work->field_82A == 0) {
        coord4 = ((TmdObject*)arg0->extra)->coords;
        actorMoveForward(coord4, 0x14);
    }
    func_actor_421600_80132310(((TmdObject*)arg0->extra)->coords, &work->field_90C, 0xC, &blk->delta);
    func_actor_421600_80133334(((TmdObject*)arg0->extra)->coords);
    SCRATCH_POP(ActorTurnScratch);
    ((TmdObject*)arg0->extra)->coords->flg = 0;
}

/// Rebuild `coord`'s Y rotation from its current yaw (`ratan2` of
/// `-m[2][0], m[2][2]`), scaled by `y` on Y and left at 1.0 on X and Z, through
/// a 0x34-byte block borrowed from the scratchpad. Marks the coordinate dirty.
static __inline__ void Actor421600_ShrinkCoord(GsCOORDINATE2* coord, s16 y)
{
    void*                 head;
    ActorScaleRotScratch* blk;
    s16                   ang;
    u16                   m22;

    head               = SCRATCH_HEAD(void);
    blk                = (ActorScaleRotScratch*)((u8*)head - 0x34);
    SCRATCH_HEAD(void) = blk;

    ang        = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    blk->angle = ang;
    Gfx_RotMatrixY(&blk->m, ang, 1);
    blk->scale.vx = 0x1000;
    blk->scale.vy = y;
    blk->scale.vz = 0x1000;
    ScaleMatrix(&blk->m, &blk->scale);

    coord->coord.m[0][0] =
        *(u16*)&((ActorScaleRotScratch*)((u8*)head - 0x34))->m.m[0][0];
    coord->coord.m[0][1] = *(u16*)&blk->m.m[0][1];
    coord->coord.m[0][2] = *(u16*)&blk->m.m[0][2];
    coord->coord.m[1][0] = *(u16*)&blk->m.m[1][0];
    coord->coord.m[1][1] = *(u16*)&blk->m.m[1][1];
    coord->coord.m[1][2] = *(u16*)&blk->m.m[1][2];
    coord->coord.m[2][0] = *(u16*)&blk->m.m[2][0];
    coord->coord.m[2][1] = *(u16*)&blk->m.m[2][1];
    m22                  = *(u16*)&blk->m.m[2][2];
    SCRATCH_POP_BYTES(0x34);
    coord->flg           = 0;
    coord->coord.m[2][2] = m22;
}

/// Shrink tick: on the live-actor edge it drops the model's dirty flag, clears
/// the 0x4000 bit on the 0xB6C node, marks the enemy's list node and resets
/// `field_6` / `field_8A0`. Then it counts frames in `field_6` and, from frame
/// 0xB on, scales the model's coordinate Y by `0x1000 - (frame - 0xA) * 0x6B`
/// until that factor runs out at 0, through `Actor421600_ShrinkCoord`. The
/// frame counter also drives the light state: 1 sets modes 0 and 1, 20 (and
/// the fall-through from 1) sets mode 2, 38 sets `field_C` 0x80 and the
/// `field_0` state 0x16. Counting stops at 0x401.
void func_actor_421600_801366F4(Task* arg0)
{
    Actor421600Work* work;
    GpEnemy*         ctx;
    TmdObject*       obj;
    s32              t;
    u16              tick;

    work = arg0->work;
    obj  = arg0->extra;
    ctx  = arg0->spawnArg2;
    if (work->field_4 != 0) {
        obj->flags              = 0;
        work->field_B6C.flags  &= 0xBFFF;
        ctx->node.state.b.flags = 1;
        work->field_6           = 0;
        work->field_8A0         = 0;
    }
    if ((s16)work->field_6 < 0x401) {
        tick          = work->field_6 + 1;
        work->field_6 = tick;
        switch ((s16)tick) {
            case 1:
                Gp_SetLightMode(ctx, 0);
                Gp_SetLightMode(ctx, 1);
                /* fallthrough */
            case 20:
                ((TmdObject*)arg0->extra)->flags = 2;
                Gp_SetLightMode(ctx, 2);
                break;
            case 22:
                break;
            case 38:
                ((TmdObject*)arg0->extra)->flags = 0x80;
                work->field_0                    = 0x16;
                break;
        }
        if ((s16)work->field_6 >= 0xB) {
            t = ((s16)work->field_6 - 10) * 0x6B;
            if (t < 0x1000) {
                Actor421600_ShrinkCoord(((TmdObject*)arg0->extra)->coords, 0x1000 - t);
            } else {
                Actor421600_ShrinkCoord(((TmdObject*)arg0->extra)->coords, 0);
            }
        }
    }
}

void func_actor_421600_801369A0(Task* arg0)
{
    Actor421600Work* work;
    GpEnemy*         ctx;
    GpEnemy*         found;
    s32              hi;
    s32              id;
    s32              stageAreaId;

    work = arg0->work;
    ctx  = arg0->spawnArg2;
    if (work->field_4 != 0) {
        if (work->field_E9C == 1) {
            work->field_2 = -1;
            work->field_6 = 0;
            return;
        }
        work->field_6 = 0;
        do {
        } while (0);
        if (Gp_StateF0.field_6 >= 2U) {
            Gp_ReleaseStateF0Add(arg0, 1);
        }
        if (D_actor_421600_80151268 <= 0) {
            Gp_DispatchMsg(gameGetPtrSlot(7), 0x13F4, 0, 0);
            work->field_8A0 = 1;
            work->field_0   = 0;
            return;
        }
    }
    if ((s16)work->field_6 < 0x80) {
        work->field_6 = work->field_6 + 1;
    }
    if (work->field_EAC > 0) {
        work->field_EAC = work->field_EAC - 1;
    }
    if (work->field_E90.bytes[2] != 2) {
        work->field_0 = 0;
        return;
    }
    found = NULL;
    switch (ctx->placeKey >> 0xC) {
        case 0:
            hi    = gGameSession->at4.loc.stage << 8;
            id    = gGameSession->at4.loc.area | 0x1000;
            found = (GpEnemy*)Gp_FindWorkById(id | hi);
            break;
        case 1:
            stageAreaId = (gGameSession->at4.loc.stage << 8) | gGameSession->at4.loc.area;
            found       = (GpEnemy*)Gp_FindWorkById(stageAreaId);
            break;
    }
    if (found != NULL) {
        if (found->hp > 0) {
            if (D_actor_421600_80151268 == 1) {
                work->field_0 = 0;
            }
        }
        if ((D_actor_421600_80151268 >= 2) || ((found->hp <= 0) && (D_actor_421600_80151268 == 1))) {
            switch (ctx->placeKey >> 0xC) {
                case 0:
                    ((TmdObject*)arg0->extra)->coords->coord.t[0] = -0xD40;
                    ((TmdObject*)arg0->extra)->coords->coord.t[2] = 0x104F;
                    ((TmdObject*)arg0->extra)->coords->flg        = 0;
                    Gp_UpdateCoord(((TmdObject*)arg0->extra)->coords);
                    Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->coords->coord, -0x76C, 1);
                    Gp_SetLightMode(ctx, 0);
                    ctx->reactionFlags = 0;
                    ctx->hp            = D_actor_421600_8013EF3C;
                    work->field_0      = 6;
                    break;
                case 1:
                    ((TmdObject*)arg0->extra)->coords->coord.t[0] = 0x138C;
                    ((TmdObject*)arg0->extra)->coords->coord.t[2] = 0x4B2;
                    ((TmdObject*)arg0->extra)->coords->flg        = 0;
                    Gp_UpdateCoord(((TmdObject*)arg0->extra)->coords);
                    Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->coords->coord, 0x7BC, 1);
                    Gp_SetLightMode(ctx, 0);
                    ctx->reactionFlags = 0;
                    ctx->hp            = D_actor_421600_8013EF3C;
                    work->field_0      = 6;
                    break;
            }
        }
    }
}

/// Scans the 12 0x18-byte records at 0xCE4 for one whose `field_4` carries the
/// 0x100000 kind, stopping at the first empty record. The 5-record twin of
/// this body is `Actor00100_HasRecord10`, which reads the same halves of
/// `field_0` instead; `func_actor_421600_80138D24` picks its aim scale with it.
static __inline__ s16 Actor421600_HasRecord10(Task* arg0)
{
    Actor421600Work* work  = arg0->work;
    s16              found = 0;
    s16              i;

    for (i = 0; i < 0xC; i++) {
        if (!work->field_CE4[i].key) {
            break;
        }
        if ((work->field_CE4[i].key & 0xFFFF0000) == 0x100000) {
            found = 1;
        }
    }
    return found;
}

void func_actor_421600_80136C88(Task* arg0)
{
    Actor421600Work*  work;
    GpEnemy*          ctx;
    Actor421600Work*  move;
    ActorTurnScratch* head;
    ActorTurnScratch* scratch;
    TmdObject*        obj;
    GsCOORDINATE2*    coord;
    GsCOORDINATE2*    playerCoord;
    GsCOORDINATE2*    turnCoord;
    GpRec18*          records;
    u16               angle;
    s16               delta;
    s32               value;
    s32               magnitude;
    s16               yaw;

    work = arg0->work;
    if (work->field_4 != 0) {
        ctx                     = arg0->spawnArg2;
        obj                     = arg0->extra;
        ctx->node.state.b.flags = 0;
        obj->flags              = 0;
        Tmd_AllocBuffers(obj);
        work->field_8EC.radius = 0x19C;
        work->field_828        = 1;
        work->field_832        = 0x10;
        work->field_82A        = 0;
        work->field_82E        = 0;
        work->field_83E        = 0;
        work->field_B6C.flags |= 0x4000;
        func_actor_421600_80134604(arg0);
        func_actor_421600_80134604(arg0);
        work->field_6           = 0;
        work->field_CCC.end1.vz = 0x26C;
        return;
    }
    head              = SCRATCH_HEAD(ActorTurnScratch);
    scratch           = (SCRATCH_HEAD(ActorTurnScratch) = head - 1);
    move              = (Actor421600Work*)work;
    head[-1].delta.vx = move->field_C[move->field_14].x - ((TmdObject*)arg0->extra)->coords->coord.t[0];
    scratch->delta.vy = 0;
    scratch->delta.vz = move->field_C[move->field_14].z - ((TmdObject*)arg0->extra)->coords->coord.t[2];
    if (!actorOutsideRadius(&scratch->delta, 0xA0) || (s16)work->field_6 >= 0x15) {
        if (move->field_14 == 0)
            move->field_14 = 1;
        else
            move->field_14 = 0;
        work->field_6 = 0;
    }
    func_actor_421600_80134604(arg0);
    coord           = ((TmdObject*)arg0->extra)->coords;
    angle           = ratan2(scratch->delta.vx, scratch->delta.vz);
    delta           = angle - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    value           = actorNormalizeYaw(delta);
    scratch->angle  = value;
    work->field_840 = value;
    if (scratch->angle >= 0x11)
        scratch->angle = 0x10;
    if (scratch->angle < -0x10)
        scratch->angle = -0x10;
    work->field_83E = scratch->angle;
    turnCoord       = ((TmdObject*)arg0->extra)->coords;
    yaw             = (u16)scratch->angle + ratan2(-turnCoord->coord.m[2][0], turnCoord->coord.m[2][2]);
    scratch->angle  = yaw;
    Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->coords->coord, yaw, 1);
    records = &work->field_90C;
    if ((s16)work->field_82A == 0) {
        if (Actor421600_HasRecord10(arg0)) {
            actorMoveForward(((TmdObject*)arg0->extra)->coords, 20);
        } else {
            actorMoveForward(((TmdObject*)arg0->extra)->coords, 20);
        }
        records = &work->field_90C;
    }
    func_actor_421600_80132310(((TmdObject*)arg0->extra)->coords, records, 0xC, &scratch->delta);
    if (func_actor_421600_8013285C(((TmdObject*)arg0->extra)->coords, &work->field_B8C, 0xC) == 1) {
        magnitude = abs((s16)work->field_840);
        if (magnitude < 0x80)
            work->field_6 = (u16)work->field_6 + 1;
    }
    ((TmdObject*)arg0->extra)->coords->flg = 0;
    playerCoord                            = ((TmdObject*)arg0->extra)->coords;
    scratch->delta.vx                      = Player_Status.coordMtx->t[0] - playerCoord->coord.t[0];
    scratch->delta.vy                      = Player_Status.coordMtx->t[1] - playerCoord->coord.t[1];
    scratch->delta.vz                      = Player_Status.coordMtx->t[2] - playerCoord->coord.t[2];
    if (!actorOutsideRadius(&scratch->delta, 2000)) {
        work->field_0 = 0x1C;
    } else if (!actorOutsideRadius(&scratch->delta, 4000)) {
        coord          = ((TmdObject*)arg0->extra)->coords;
        angle          = ratan2(scratch->delta.vx, scratch->delta.vz);
        delta          = angle - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
        value          = actorNormalizeYaw(delta);
        scratch->angle = value;
        value          = abs(value);
        if (value < 0x300) {
            work->field_0 = 0x1C;
        }
    }
    SCRATCH_POP(ActorTurnScratch);
}

void func_actor_421600_801373D4(Task* arg0)
{
    PlayerStatus* config = &Player_Status;
    SVECTOR       initialDelta;

    SVECTOR             effect;
    s16                 temp_a1_3;
    s16                 temp_a1_4;
    s16                 temp_a1_5;
    s16                 temp_s0_12;
    s16                 temp_s0_15;
    s16                 temp_s0_18;
    s16                 temp_s0_21;
    s16                 temp_s0_6;
    s16                 temp_s0_9;
    s16                 temp_v0_4;
    s32                 temp_v1_2;
    s32                 var_v0_15;
    s32                 var_v0_17;
    s16                 var_v0_24;
    s32                 var_v0_25;
    s32                 var_v0_26;
    s16                 var_v1_2;
    s16                 var_v1_4;
    s16                 var_v1_5;
    s16                 var_v1_6;
    s16                 var_v1_7;
    s16                 var_v1_8;
    GsCOORDINATE2*      temp_a1_2;
    GsCOORDINATE2*      temp_a2_2;
    GsCOORDINATE2*      temp_a2_3;
    GsCOORDINATE2*      temp_s0_13;
    GsCOORDINATE2*      temp_s0_16;
    GsCOORDINATE2*      temp_s0_19;
    GsCOORDINATE2*      temp_s0_4;
    GsCOORDINATE2*      temp_s0_7;
    GsCOORDINATE2*      temp_v0_5;
    GsCOORDINATE2*      temp_v0_7;
    void**              scratchHead;
    s32                 temp_v0;
    s32                 spawnEffect;
    s32                 var_a1_4;
    s32                 effectFlags;
    s32                 effectJoint;
    s32                 var_v0_13;
    s32                 var_v0_14;
    s32                 var_v0_21;
    s32                 var_v0_22;
    s32                 var_v0_5;
    s32                 var_v0_6;
    s32                 var_v0_8;
    s32                 var_v0_9;
    s32                 pan;
    s32                 temp_s0_11;
    s32                 temp_s0_14;
    s32                 temp_s0_17;
    s32                 temp_s0_20;
    s32                 temp_s0_5;
    s32                 temp_s0_8;
    u16                 temp_v0_6;
    u16                 temp_v1;
    u16                 var_a0;
    u16                 var_v1_3;
    u32                 temp_lo;
    GsCOORDINATE2*      temp_s0_10;
    Task*               player;
    TmdObject*          obj;
    GpEnemy*            ctx;
    Actor421600Work*    work;
    GameActor*          playerWork;
    ActorFacingScratch* scratch;

    work       = arg0->work;
    player     = gameGetPtrSlot(3);
    playerWork = (GameActor*)player->work;
    ctx        = arg0->spawnArg2;
    if (work->field_4 != 0) {
        obj              = arg0->extra;
        initialDelta.pad = actorPositionYaw(arg0, &initialDelta, config);
        temp_v0          = initialDelta.pad;
        if (temp_v0 > 0x300) {
            work->field_0 = 9;
        } else if (temp_v0 < -0x300) {
            work->field_0 = 10;
        }
        ctx->node.state.b.flags = 0;
        obj->flags              = 0;
        Tmd_AllocBuffers(obj);
        work->field_8EC.radius = 0x19C;
        work->field_82E        = 2;
        work->field_828        = 1;
        work->field_82A        = 0;
        work->field_83E        = 0;
        work->field_B6C.flags  = (u16)(work->field_B6C.flags | 0x4000);
        work->field_832        = (u16)work->field_834;
        func_actor_421600_80134604(arg0);
        work->field_CCC.end1.vz = 0x320;
        work->field_6           = 0U;
        work->field_8           = 0U;
        work->field_E9E         = 0;
        work->field_840         = 0;
        work->field_8E8         = 9;
        work->field_8E9         = 1;
        work->field_8EA         = 1;
        Gp_DispatchMsg(gameGetPtrSlot(4), 0x7DA, &work->field_8E8, 0x7DB);
        return;
    }
    scratch = SCRATCH_PUSH(ActorFacingScratch);
    if (work->field_82E == 3) {
        work->field_6 = (u16)(work->field_6 + 1);
    }
    if ((func_actor_421600_8013285C(((TmdObject*)arg0->extra)->coords, &work->field_B8C, 0xC) != 0) && ((s16)work->field_6 >= 0xB)) {
        temp_lo                  = D_actor_421600_80151260.vx * D_actor_421600_80151260.vx;
        scratch->distanceSquared = temp_lo;
        scratch->distanceSquared = (u32)(temp_lo + (D_actor_421600_80151260.vz * D_actor_421600_80151260.vz));
        temp_s0_4                = ((TmdObject*)arg0->extra)->coords;
        temp_s0_5                = ratan2(D_actor_421600_80151260.vx, D_actor_421600_80151260.vz);
        temp_s0_6                = temp_s0_5 - ratan2(-temp_s0_4->coord.m[2][0], temp_s0_4->coord.m[2][2]);

        var_v1_2 = actorNormalizeYaw(temp_s0_6);
        var_v0_5 = var_v1_2 << 0x10;

        var_v0_6 = var_v0_5 >> 0x10;
        if (var_v0_6 < 0) {
            var_v0_6 = -var_v0_6;
        }
        if ((var_v0_6 >= 0x601) && ((u32)scratch->distanceSquared >= 0xE11U)) {
            work->field_0 = 0x23;
        }
    }
    if ((func_actor_421600_801335BC(((TmdObject*)arg0->extra)->coords, &work->field_90C, 0xC, (SVECTOR*)scratch) << 0x10) != 0) {
        if ((work->field_82E == 3) && (playerWork->field_954 != 2)) {
            work->field_8E4     = 0x80;
            temp_a1_2           = ((TmdObject*)arg0->extra)->coords;
            scratch->vx         = (s16)(Player_Status.coordMtx->t[0] - temp_a1_2->coord.t[0]);
            scratch->vy         = (s16)(Player_Status.coordMtx->t[1] - temp_a1_2->coord.t[1]);
            temp_v0_4           = Player_Status.coordMtx->t[2] - temp_a1_2->coord.t[2];
            scratch->vz         = temp_v0_4;
            scratch->contactYaw = ratan2(scratch->vx, temp_v0_4);
            temp_v0_5           = ((TmdObject*)arg0->extra)->coords;
            temp_v1             = scratch->contactYaw - ratan2(-temp_v0_5->coord.m[2][0], temp_v0_5->coord.m[2][2]);
            var_a0              = temp_v1;
            scratch->contactYaw = temp_v1;

            var_a0   = actorNormalizeYaw(temp_v1);
            var_v0_8 = var_a0 << 0x10;

            var_v0_9            = var_v0_8 >> 0x10;
            scratch->contactYaw = (u16)var_v0_9;
            var_v0_9            = abs(var_v0_9);
            if (var_v0_9 < 0x180) {
                if (Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F8, &work->field_8D0, 0) == 0) {
                    Gfx_MatrixCol2(&((TmdObject*)arg0->extra)->coords->coord, (SVECTOR*)scratch);
                    temp_v0_6           = ratan2(scratch->vx, scratch->vz) + 0x800;
                    var_v1_3            = temp_v0_6;
                    scratch->contactYaw = temp_v0_6;

                    var_v1_3 = actorNormalizeYaw(temp_v0_6);

                    scratch->contactYaw = var_v1_3;
                    temp_s0_7           = ((TmdObject*)arg0->extra)->coords;
                    temp_s0_8           = ratan2(scratch->vx, scratch->vz);
                    temp_s0_9           = temp_s0_8 - ratan2(-temp_s0_7->coord.m[2][0], temp_s0_7->coord.m[2][2]);

                    var_v1_4 = actorNormalizeYaw(temp_s0_9);

                    scratch->turnYaw = var_v1_4;
                    scratch->vy      = 0;
                    scratch->vx      = (s16) - (s16)(u16)scratch->vx;
                    scratch->vz      = (s16) - (s16)(u16)scratch->vz;
                    temp_s0_10       = ((TmdObject*)gameGetPtrSlot(3)->extra)->coords;
                    temp_s0_11       = ratan2(scratch->vx, scratch->vz);
                    temp_s0_12       = temp_s0_11 - ratan2(-temp_s0_10->coord.m[2][0], temp_s0_10->coord.m[2][2]);

                    var_v1_5  = actorNormalizeYaw(temp_s0_12);
                    var_v0_13 = var_v1_5 << 0x10;

                    var_v0_14          = var_v0_13 >> 0x10;
                    scratch->playerYaw = (s16)var_v0_14;
                    var_v0_14          = abs(var_v0_14);
                    if (var_v0_14 < 0x400) {
                        work->field_E7C = (s32)&D_actor_421600_80151090;
                    } else {
                        work->field_E7C     = (s32)&D_actor_421600_801510A4;
                        scratch->contactYaw = (u16)(scratch->contactYaw + 0x800);
                    }
                    work->field_8C8.vx = 0;
                    work->field_8C8.vy = (u16)scratch->contactYaw;
                    work->field_8C8.vz = 0;
                    work->field_8B8.vx = (s32)((TmdObject*)player->extra)->coords->coord.t[0];
                    work->field_8B8.vy = (s32)((TmdObject*)player->extra)->coords->coord.t[1];
                    work->field_8B8.vz = (s32)((TmdObject*)player->extra)->coords->coord.t[2];
                    Gp_DispatchMsg(player, 0x3E9, &work->field_8B8, 0);
                    if (work->field_E9E < 0x3E8) {
                        if (ctx->hp > 0) {
                            var_v0_15 = scratch->playerYaw;
                            if (var_v0_15 < 0) {
                                var_v0_15 = -var_v0_15;
                            }
                            if (var_v0_15 < 0x400) {
                                scratch->messageResult = actorPlayerContactMessage(ctx, 2);
                            } else {
                                scratch->messageResult = actorPlayerContactMessage(ctx, 3);
                            }
                        }
                        if (scratch->messageResult != 1) {
                            work->field_E80          = 3;
                            work->field_E84          = 0;
                            work->field_E88          = 0;
                            work->field_8A4          = 0;
                            work->field_8A8          = 0;
                            work->field_8AC          = 0;
                            work->field_8B4          = 7;
                            work->field_8B6          = 1;
                            work->field_E9C          = 1;
                            work->field_E90.bytes[3] = 0;
                            Gp_DispatchMsg(player, 0x3FF, &work->field_E7C, 0);
                        }
                        work->field_0 = 0x25;
                        Gp_SpawnPadLerp(3, 0xFF, 8);
                    } else {
                        if (ctx->hp > 0) {
                            var_v0_17 = scratch->playerYaw;
                            if (var_v0_17 < 0) {
                                var_v0_17 = -var_v0_17;
                            }
                            if (var_v0_17 < 0x400) {
                                scratch->messageResult = actorPlayerContactMessage(ctx, 0);
                            } else {
                                scratch->messageResult = actorPlayerContactMessage(ctx, 1);
                            }
                        }
                        if (scratch->messageResult == 1) {
                            ((GameActor*)player->work)->field_956 = 0xA;
                        }
                        work->field_E80          = 1;
                        work->field_E84          = 0;
                        work->field_E88          = 0;
                        work->field_8A4          = 0;
                        work->field_8A8          = 0;
                        work->field_8AC          = 0;
                        work->field_8B4          = 7;
                        work->field_8B6          = 1;
                        work->field_E9C          = 1;
                        work->field_E90.bytes[3] = 0;
                        Gp_DispatchMsg(player, 0x3FF, &work->field_E7C, 0);
                        work->field_0 = 0x1E;
                        Gp_SpawnPadLerp(8, 0xFF, 8);
                    }
                }
            }
            temp_a2_2   = ((TmdObject*)arg0->extra)->coords;
            scratch->vx = (s16)(Player_Status.coordMtx->t[0] - temp_a2_2->coord.t[0]);
            scratch->vy = (s16)(Player_Status.coordMtx->t[1] - temp_a2_2->coord.t[1]);
            temp_a1_3   = Player_Status.coordMtx->t[2] - temp_a2_2->coord.t[2];
            scratch->vz = temp_a1_3;
            temp_s0_13  = ((TmdObject*)arg0->extra)->coords;
            temp_s0_14  = ratan2(scratch->vx, temp_a1_3);
            temp_s0_15  = temp_s0_14 - ratan2(-temp_s0_13->coord.m[2][0], temp_s0_13->coord.m[2][2]);

            var_v1_6 = actorNormalizeYaw(temp_s0_15);

            scratch->targetYaw = var_v1_6;
        } else {
            goto updatePlayerYaw;
        }
    } else {
    updatePlayerYaw:
        temp_a2_3   = ((TmdObject*)arg0->extra)->coords;
        scratch->vx = (s16)(Player_Status.coordMtx->t[0] - temp_a2_3->coord.t[0]);
        scratch->vy = (s16)(Player_Status.coordMtx->t[1] - temp_a2_3->coord.t[1]);
        temp_a1_4   = Player_Status.coordMtx->t[2] - temp_a2_3->coord.t[2];
        scratch->vz = temp_a1_4;
        temp_s0_16  = ((TmdObject*)arg0->extra)->coords;
        temp_s0_17  = ratan2(scratch->vx, temp_a1_4);
        temp_s0_18  = temp_s0_17 - ratan2(-temp_s0_16->coord.m[2][0], temp_s0_16->coord.m[2][2]);

        var_v1_7  = actorNormalizeYaw(temp_s0_18);
        var_v0_21 = var_v1_7 << 0x10;

        var_v0_22          = var_v0_21 >> 0x10;
        scratch->targetYaw = (s16)var_v0_22;
        var_v0_22          = abs(var_v0_22);
        if ((var_v0_22 >= 0x601) && (work->field_82E == 3)) {
            work->field_0 = 0x1D;
        }
    }
    ((TmdObject*)arg0->extra)->coords->flg = 0;
    temp_s0_19                             = ((TmdObject*)arg0->extra)->coords;
    temp_s0_20                             = ratan2(work->field_E70, work->field_E74);
    temp_s0_21                             = temp_s0_20 - ratan2(-temp_s0_19->coord.m[2][0], temp_s0_19->coord.m[2][2]);

    var_v1_8 = actorNormalizeYaw(temp_s0_21);

    scratch->turnYaw = var_v1_8;
    func_actor_421600_80134604(arg0);
    var_a1_4 = 0;
    if (work->field_82E == 2) {
        if (scratch->turnYaw >= 0x41) {
            scratch->turnYaw = 0x40;
        }
        if (scratch->turnYaw < -0x40) {
            scratch->turnYaw = -0x40;
        }
        temp_v0_7        = ((TmdObject*)arg0->extra)->coords;
        temp_a1_5        = (u16)scratch->turnYaw + ratan2(-temp_v0_7->coord.m[2][0], temp_v0_7->coord.m[2][2]);
        scratch->turnYaw = temp_a1_5;
        Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->coords->coord, temp_a1_5, 1);
        ((TmdObject*)arg0->extra)->coords->flg = 0;
    } else {
        var_a1_4 = Actor421600_HasRecord10(arg0);
        if (var_a1_4 != 0) {
            actorMoveForward(((TmdObject*)arg0->extra)->coords, 0x55);
            var_v0_24 = (u16)work->field_E9E + 0x55;
        } else {
            actorMoveForward(((TmdObject*)arg0->extra)->coords, 0xC8);
            var_v0_24 = (u16)work->field_E9E + 0xC8;
        }
        work->field_E9E = var_v0_24;
    }
    if (work->field_82E == 2) {
        work->field_8 = (u16)(work->field_8 + 1);
    }
    if ((s16)work->field_8 > (s16)work->field_EA2) {
        temp_v1_2 = work->field_82E;
        if (temp_v1_2 == 2) {
            var_v0_25 = scratch->targetYaw;
            if (var_v0_25 < 0) {
                var_v0_25 = -var_v0_25;
            }
            if ((var_v0_25 < 0x80) || (work->field_68 & 0x100)) {
                work->field_82E = 3;
                work->field_828 = temp_v1_2;
                pan             = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
                SndEvt_EnqueueType6(0x40010006, pan, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
            }
        }
    }
    if ((s16)work->field_8 >= 0xF) {
        var_v0_26 = scratch->targetYaw;
        if (var_v0_26 < 0) {
            var_v0_26 = -var_v0_26;
        }
        if (var_v0_26 >= 0x81) {
            goto updatePlayerDelta;
        }
    } else {
    updatePlayerDelta:
        if (work->field_82E == 2) {
            work->field_E70 = (s16)(config->coordMtx->t[0] - ((TmdObject*)arg0->extra)->coords->coord.t[0]);
            work->field_E72 = (s16)(config->coordMtx->t[1] - ((TmdObject*)arg0->extra)->coords->coord.t[1]);
            work->field_E74 = (s16)(config->coordMtx->t[2] - ((TmdObject*)arg0->extra)->coords->coord.t[2]);
        }
    }
    if (work->field_82E == 3) {
        switch (work->field_5A & 0x3FF) {
            case 5:
                spawnEffect = 1;
                effectJoint = 7;
                effectFlags = 17152;
                effect.vz   = 0;
                effect.vx   = 0;
                effect.vy   = 700;
                break;
            case 8:
                spawnEffect = 1;
                effectJoint = 9;
                effectFlags = 13568;
                effect.vz   = 0;
                effect.vx   = 0;
                effect.vy   = 700;
                break;
            case 10:
                spawnEffect = 1;
                effectJoint = 14;
                effectFlags = 23040;
                effect.vz   = 0;
                effect.vx   = 0;
                effect.vy   = 600;
                break;
            case 13:
                spawnEffect = 1;
                effectJoint = 17;
                effectFlags = 18432;
                effect.vz   = 0;
                effect.vx   = 0;
                effect.vy   = 600;
                break;
            default:
                spawnEffect = 0;
                effectJoint = 0;
                effectFlags = 1;
                break;
        }
        if (Gp_State1C->roomEffectMode == 2) {
            scratchHead = SCRATCH_HEAD_ADDR;
            if (spawnEffect == 1) {
                Gp_SpawnEff(0x60054, &((TmdObject*)arg0->extra)->coords[effectJoint], effectFlags | 0x80000000, &effect);
                goto releaseScratch;
            }
        } else {
            goto releaseScratch;
        }
    } else {
    releaseScratch:
        scratchHead = SCRATCH_HEAD_ADDR;
    }
    SCRATCH_POP_BYTES_AT(scratchHead, 0x18);
}

/// Spawn/aim tick: on the live-actor edge it resets the display objects, runs
/// the arena vector through the gte rotation, stores the world X (`field_8A4`)
/// and Z (`field_8AC`) it produces, plays the spawn sound, then bumps the
/// `field_6` frame counter. On frame 0xF of the 7 pose it enqueues the second
/// sound and, outside a live-game state, spawns the effect on the player's
/// coordinate. The `field_E90` id picks `field_0` on the 0x100 flag, 5 for
/// 0x11402 and 0x1F otherwise -- the same pair `func_actor_421600_8013E9D8`
/// writes.
void func_actor_421600_8013848C(Task* arg0)
{
    Actor421600Work* work;
    GpEnemy*         ctx;
    SVECTOR*         head;
    SVECTOR*         vec;
    SVECTOR*         gteVec;
    TmdObject*       obj;
    Task*            player;
    s32              x;
    s32              z;
    s32              sound;
    s32              pan;
    s32              eventPan;
    s32              state;
    u16              tick;

    work                  = arg0->work;
    player                = gameGetPtrSlot(3);
    head                  = SCRATCH_HEAD(SVECTOR);
    vec                   = head - 2;
    SCRATCH_HEAD(SVECTOR) = vec;
    ctx                   = arg0->spawnArg2;
    gteVec                = vec;
    if (work->field_4 != 0) {
        TOUCH_REG(gteVec);
        obj                     = arg0->extra;
        ctx->node.state.b.flags = 0;
        obj->flags              = 0;
        Tmd_AllocBuffers(obj);
        work->field_8EC.radius = 0x19C;
        work->field_82E        = 5;
        work->field_828        = 1;
        work->field_82A        = 0;
        work->field_83E        = 0;
        work->field_B6C.flags |= 0x4000;
        work->field_832        = work->field_834;
        func_actor_421600_80134604(arg0);
        Gfx_MatrixCol2(&((TmdObject*)arg0->extra)->coords->coord, vec);
        work->field_6 = 0;
        VectorNormalSS(vec, vec);
        if (work->field_E9E >= 0xFA1) {
            work->field_E9E = 0xFA0;
        }
        gte_lddp(0x85);
        gte_ldsv(gteVec);
        gte_gpf12();
        gte_stsv(gteVec);
        x               = head[-2].vx;
        work->field_8A8 = 0;
        work->field_8A4 = x;
        z               = vec->vz;
        work->field_8B4 = 7;
        work->field_8B6 = 1;
        work->field_8AC = z;
        pan             = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(7, (s32)pan, (s32)(s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
    }
    tick          = work->field_6 + 1;
    work->field_6 = tick;
    if (((s16)tick == 0xF) && (work->field_8B4 == 7)) {
        sound    = (((u16)ctx->placeKey >> 0xC) << 8) | 0x4001000A;
        eventPan = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(sound, (s32)eventPan, (s32)(s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
        if (Gp_State1C->roomEffectMode == 2) {
            Gp_SpawnEff(0x60054, ((TmdObject*)player->extra)->coords + 1, 0x80003A00, NULL);
        }
    }
    func_actor_421600_80134604(arg0);
    if (work->field_68 & 0x100) {
        state = work->field_E90.word & 0xFFFFFF;
        if (state == 0x11402) {
            state = 5;
        } else {
            state = 0x1F;
        }
        work->field_0 = state;
    }
    SCRATCH_HEAD(SVECTOR) += 2;
}

void func_actor_421600_80138750(Task* arg0)
{
    Actor421600Work* work;
    GpEnemy*         ctx;
    TmdObject*       obj;
    SVECTOR*         head;
    SVECTOR*         vec;
    s32              x, z;
    s16              yaw;
    s32              outside;
    s32              state;

    head = SCRATCH_HEAD(SVECTOR);
    vec  = (SCRATCH_HEAD(SVECTOR) = head - 2);
    work = arg0->work;
    ctx  = arg0->spawnArg2;
    if (work->field_4 != 0) {
        obj                     = arg0->extra;
        ctx->node.state.b.flags = 0;
        obj->flags              = 0;
        Tmd_AllocBuffers(obj);
        work->field_8EC.radius = 0x19C;
        work->field_828        = 1;
        work->field_82A        = 0;
        work->field_82E        = 5;
        work->field_83E        = 0;
        work->field_6          = 0;
        work->field_B6C.flags |= 0x4000;
        work->field_832        = work->field_834;
        actorConfigPositionDelta(&Player_Status, ((TmdObject*)arg0->extra)->coords, vec);
        VectorNormalSS(vec, vec);
        gte_lddp(0x20);
        gte_ldsv(vec);
        gte_gpf12();
        gte_stsv(vec);
        x                       = head[-2].vx;
        work->field_8A8         = 0;
        work->field_8A4         = x;
        z                       = vec->vz;
        work->field_8B4         = 7;
        work->field_8B6         = 1;
        work->field_CCC.end1.vz = 0x320;
        work->field_8AC         = z;
        if ((work->field_E90.word & 0xFFFFFF) == 0x11402) {
            work->field_0 = 5;
        }
    }
    work->field_6 += 1;
    func_actor_421600_80134604(arg0);
    state = (s16)work->field_82E;
    switch (state) {
        case 5:
            if (work->field_68 & 0x100) {
                actorConfigPositionDelta(&Player_Status, ((TmdObject*)arg0->extra)->coords, vec);
                outside = actorOutsideRadius(vec, 2000);
                if (outside) {
                    work->field_0 = 0x26;
                } else {
                    work->field_0 = 0x1F;
                }
            }
            break;
        case 3:
            yaw       = actorPositionYaw(arg0, vec, &Player_Status);
            vec[1].vz = yaw;
            if (Actor421600_HasRecord10(arg0)) {
                actorMoveForward(((TmdObject*)arg0->extra)->coords, 85);
            } else {
                actorMoveForward(((TmdObject*)arg0->extra)->coords, 200);
            }
            if (func_actor_421600_8013285C(((TmdObject*)arg0->extra)->coords, &work->field_B8C, 0xC)) {
                work->field_0 = 0x23;
            }
            if ((s16)work->field_6 >= 0x15) {
                work->field_828 = 1;
                work->field_82A = 0;
                work->field_82E = 5;
                work->field_832 = work->field_834;
            }
            break;
    }
    SCRATCH_HEAD(SVECTOR) += 2;
}

/// Aim tick: on the live-actor edge it re-arms the model the way
/// `func_actor_421600_8013848C` does -- buffers reallocated, clip 0x10,
/// `field_82E` 6, the 0xB6C node's 0x4000 flag up -- with `field_8` and the
/// 0xCD8 offset it owns reseeded, then, while `field_6` is inside 9..0x18 and
/// `field_8` below 5, walks the 0xB8C table and counts a retry for every hit.
/// The 0xCE4 records decide which way the model is aimed: one carrying the
/// 0x100000 kind turns it by `-0x55`, none by `-0xC8`, through
/// `actorMoveForward`. Outside that frame window, and in both aim arms,
/// the 0xB8C walk is what runs.
void func_actor_421600_80138D24(Task* arg0)
{
    Actor421600Work* work;
    GpEnemy*         ctx;
    TmdObject*       obj;
    s16              found;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                     = arg0->extra;
        ctx                     = arg0->spawnArg2;
        ctx->node.state.b.flags = 0;
        obj->flags              = 0;
        Tmd_AllocBuffers(obj);
        work->field_8EC.radius = 0x19C;
        work->field_828        = 1;
        work->field_832        = 0x10;
        work->field_82A        = 0;
        work->field_82E        = 6;
        work->field_B6C.flags |= 0x4000;
        func_actor_421600_80134604(arg0);
        work->field_6           = 0;
        work->field_8           = 0;
        work->field_CCC.end1.vz = -0x320;
    }
    work->field_6++;
    func_actor_421600_80134604(arg0);
    if (work->field_68 & 0x100) {
        work->field_0 = 2;
    }
    if (((u32)(work->field_6 - 9) < 0x10) && ((s16)work->field_8 < 5)) {
        if (func_actor_421600_8013285C(((TmdObject*)arg0->extra)->coords, &work->field_B8C, 0xC) != 0) {
            work->field_8++;
        }
        found = Actor421600_HasRecord10(arg0);
        if (found != 0) {
            actorMoveForward(((TmdObject*)arg0->extra)->coords, -0x55);
        } else {
            actorMoveForward(((TmdObject*)arg0->extra)->coords, -0xC8);
        }
    } else {
        func_actor_421600_8013285C(((TmdObject*)arg0->extra)->coords, &work->field_B8C, 0xC);
    }
    ((TmdObject*)arg0->extra)->coords->flg = 0;
}

/// Re-arms the model the way `func_actor_421600_8013848C` does -- buffers
/// reallocated, clip 0x10, `field_82E` 2, the 0xB6C node's 0x4000 flag up --
/// then walks the 0xB8C `GpRec18` table through `func_actor_421600_8013285C`.
/// Takes two `SVECTOR`s off `G_SCRATCH_HEAD` and fills the XZ offset of the
/// model coordinate from `Player_Status.coordMtx` (the player's coordinate matrix),
/// forms the yaw difference against the model's own facing (row 2 of its
/// matrix), wraps it into `[-0x800, 0x800]` into `field_840` and re-aims the
/// coordinate with `Gfx_RotMatrixY`. Ends by writing the view index into
/// `field_0` on the two view transitions.
///
/// The coordinate is read twice into two locals: `coord` only feeds the offset
/// and dies before the first `ratan2`, while `coord2` is live across it, so GCC
/// 2.8.1 keeps them in a caller-saved and a callee-saved register respectively.
/// One local assigned twice is one pseudo with one live range and costs a sixth
/// saved register.
void func_actor_421600_8013903C(Task* arg0)
{
    Actor421600Work* work;
    GpEnemy*         ctx;
    TmdObject*       obj;
    SVECTOR*         head;
    SVECTOR*         vec;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   coord2;
    s16              angle;
    s32              view;

    head                   = SCRATCH_HEAD(SVECTOR);
    SCRATCH_HEAD(SVECTOR) -= 2;
    vec                    = head - 2;
    work                   = arg0->work;
    ctx                    = arg0->spawnArg2;
    if (work->field_4 != 0) {
        obj                     = arg0->extra;
        ctx->node.state.b.flags = 0;
        obj->flags              = 0;
        Tmd_AllocBuffers(obj);
        work->field_8EC.radius = 0x19C;
        work->field_828        = 1;
        work->field_832        = 0x10;
        work->field_82A        = 0;
        work->field_82E        = 2;
        work->field_83E        = 0;
        work->field_B6C.flags |= 0x4000;
        func_actor_421600_80134604(arg0);
        work->field_6 = 0;
    }
    func_actor_421600_8013285C(((TmdObject*)arg0->extra)->coords, &work->field_B8C, 0xC);
    ((TmdObject*)arg0->extra)->coords->flg = 0;
    coord                                  = ((TmdObject*)arg0->extra)->coords;
    head[-2].vx                            = (u16)Player_Status.coordMtx->t[0] - (u16)coord->coord.t[0];
    vec->vy                                = (u16)Player_Status.coordMtx->t[1] - (u16)coord->coord.t[1];
    vec->vz                                = (u16)Player_Status.coordMtx->t[2] - (u16)coord->coord.t[2];
    coord2                                 = ((TmdObject*)arg0->extra)->coords;
    angle                                  = ratan2(head[-2].vx, vec->vz) - ratan2(-coord2->coord.m[2][0], coord2->coord.m[2][2]);
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
    work->field_840 = angle;
    Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->coords->coord, (s16)ratan2(vec->vx, vec->vz), 1);
    ((TmdObject*)arg0->extra)->coords->flg = 0;
    func_actor_421600_80134604(arg0);
    if (((u16)ctx->placeKey >> 0xC) == 0) {
        view = Gp_GetViewIndex() & 0xFF;
        if (view == 3) {
            work->field_0 = view;
        }
    }
    if ((((u16)ctx->placeKey >> 0xC) == 1) && ((Gp_GetViewIndex() & 0xFF) == 8)) {
        work->field_0 = 3;
    }
    SCRATCH_HEAD(SVECTOR) += 2;
}

/// Re-arms the model buffers and the 0x828 motion block the way
/// `func_actor_421600_8013848C` does, with clip 0x10 and pose 7, then walks the
/// two `GpRec18` movement tables 0x90C and 0xA4C through
/// `func_actor_421600_80132310`. `field_0` becomes 0x22 when either walk
/// reports a hit, and again when the squared XZ offset from `Player_Status.coordMtx` is
/// under the squared 0x5DC radius, so the actor only takes the state while the
/// player is close. Ends by clearing the model's `flg`.
void func_actor_421600_801392A8(Task* arg0)
{
    Actor421600Work*     work;
    GpEnemy*             ctx;
    TmdObject*           obj;
    GsCOORDINATE2*       coord;
    MATRIX*              target;
    void*                head;
    OverlayRangeScratch* blk;
    SVECTOR              vec;
    SVECTOR*             dir;
    u32                  spad_a;
    u32                  spad_b;
    s32                  outside;

    work = arg0->work;
    if (work->field_4 != 0) {
        ctx                     = arg0->spawnArg2;
        obj                     = arg0->extra;
        ctx->node.state.b.flags = 0;
        obj->flags              = 0;
        Tmd_AllocBuffers(obj);
        work->field_8EC.radius = 0x19C;
        work->field_828        = 2;
        work->field_832        = 0x10;
        work->field_82A        = 0;
        work->field_82E        = 7;
        work->field_B6C.flags |= 0x4000;
        func_actor_421600_80134604(arg0);
    }
    func_actor_421600_80134604(arg0);
    if (((func_actor_421600_80132310(((TmdObject*)arg0->extra)->coords, &work->field_90C, 0xC, &vec) << 0x10) != 0) || ((func_actor_421600_80132310(((TmdObject*)arg0->extra)->coords, &work->field_A4C, 0xC, &vec) << 0x10) != 0)) {
        work->field_0 = 0x22;
    }
    target                         = Player_Status.coordMtx;
    coord                          = ((TmdObject*)arg0->extra)->coords;
    vec.vx                         = (u16)target->t[0] - (u16)coord->coord.t[0];
    dir                            = &vec;
    dir->vy                        = (u16)target->t[1] - (u16)coord->coord.t[1];
    dir->vz                        = (u16)target->t[2] - (u16)coord->coord.t[2];
    head                           = SCRATCH_HEAD(void);
    blk                            = (OverlayRangeScratch*)((u8*)head - 0xC);
    spad_a                         = (u32)PSX_SCRATCH;
    *(void**)((u8*)spad_a + 0x3FC) = blk;
    blk->dx                        = vec.vx;
    blk->dz                        = dir->vz;
    blk->r                         = 0x5DC;
    blk->dx                        = blk->dx * blk->dx;
    blk->dz                        = blk->dz * blk->dz;
    blk->r                         = blk->r * blk->r;
    spad_b                         = (u32)PSX_SCRATCH + 0x3F8;
    *(void**)((u8*)spad_b + 0x4)   = head;
    outside                        = blk->dx + blk->dz >= blk->r;
    if (outside == 0) {
        work->field_0 = 0x22;
    }
    ((TmdObject*)arg0->extra)->coords->flg = 0;
}

/// Death / respawn tick: re-arms the model buffers and the 0x828 motion block,
/// fires the 0x40010009 spawn sound and the 0x40010007 tick sound (draining
/// `field_40` by 0xF and flooring it at 1), then walks the two `GpRec18`
/// movement tables. While the id word's third byte reads 2 the actor is held
/// in the arena by clamping X -- and Z only when X was already inside -- and
/// otherwise `func_actor_421600_80133334` drags it back. Picks the state
/// `field_0` out of `field_40` / `field_4C`.
void func_actor_421600_8013947C(Task* arg0)
{
    Actor421600Work* work;
    GpEnemy*         ctx;
    GsCOORDINATE2*   coord;
    TmdObject*       obj;
    s32              sound;
    s32              pan;
    s32              eventSound;
    s32              eventPan;
    s32              x;
    s32              z;

    work = arg0->work;
    ctx  = arg0->spawnArg2;
    if (work->field_4 != 0) {
        obj                     = arg0->extra;
        ctx->node.state.b.flags = 0;
        obj->flags              = 0;
        Tmd_AllocBuffers(obj);
        work->field_8EC.radius = 0x19C;
        work->field_832        = 0x10;
        work->field_82E        = 0xA;
        work->field_828        = 1;
        work->field_82A        = 0;
        work->field_B6C.flags |= 0x4000;
        work->field_8EC.flags |= 0x4000;
        func_actor_421600_80134604(arg0);
        sound = (((u16)ctx->placeKey >> 0xC) << 8) | 0x40010009;
        pan   = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(sound, pan, (s32)(s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
        ctx->hp -= 0xF;
        func_800DA6E8(&ctx->node, 0xF, 0);
        if (ctx->hp <= 0) {
            ctx->hp = 1;
        }
        eventSound = (((u16)ctx->placeKey >> 0xC) << 8) | 0x40010007;
        eventPan   = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(eventSound, eventPan,
                            (s32)(s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
    }
    func_actor_421600_8013285C(((TmdObject*)arg0->extra)->coords, &work->field_B8C, 0xC);
    func_actor_421600_8013285C(((TmdObject*)arg0->extra)->coords, &work->field_90C, 0xC);
    if (work->field_E90.bytes[2] == 2) {
        coord = ((TmdObject*)arg0->extra)->coords;
        x     = coord->coord.t[0];
        if (x > 0) {
            if (x >= 0xBEB) {
                coord->coord.t[0] = 0xB54;
            } else {
                goto block_10;
            }
        } else if (x < -0xB22) {
            coord->coord.t[0] = -0xA8C;
        } else {
        block_10:
            z = coord->coord.t[2];
            if (z > 0) {
                if (z >= 0xB23) {
                    coord->coord.t[2] = 0xA8C;
                }
            } else if (z < -0xB22) {
                coord->coord.t[2] = -0xA8C;
            }
        }
    } else {
        func_actor_421600_80133334(((TmdObject*)arg0->extra)->coords);
    }
    ((TmdObject*)arg0->extra)->coords->flg = 0;
    func_actor_421600_80134604(arg0);
    if (work->field_68 & 0x100) {
        if (ctx->hp > 0) {
            if (ctx->reactionFlags & 2) {
                work->field_0 = 4;
            } else {
                work->field_0 = 0x11;
            }
        } else {
            work->field_0 = 0x15;
        }
    }
}

void func_actor_421600_80139718(Task* arg0)
{
    s32               radius = 0x5DC;
    Actor421600Work*  work;
    GpRec18*          record;
    GsCOORDINATE2*    coord;
    GsCOORDINATE2*    coord2;
    GsCOORDINATE2*    coord3;
    GsCOORDINATE2*    facing3;
    GsCOORDINATE2*    facing4;
    GsCOORDINATE2*    facing5;
    GsCOORDINATE2*    facing;
    GsCOORDINATE2*    facing2;
    GsCOORDINATE2*    turnCoord;
    MATRIX*           matrix;
    ActorMoveScratch* scratch;
    SVECTOR*          target;
    SVECTOR*          target2;
    ActorMoveScratch* head;
    SVECTOR*          direction;
    ActorMoveScratch* head2;
    TmdObject*        obj;
    s16               targetDelta;
    s16               delta;
    s16               yaw;
    s16               delta3;
    s16               delta4;
    s16               delta5;
    s32               playerX;
    s16               delta1;
    s16               delta2;
    s16               targetYaw;
    s16               z;
    s32               magnitude;
    s32               targetMagnitude;
    s16               adjustedDelta;
    s32               originalMagnitude;
    s16               wrapped;
    s16               wrapped2;
    s16               wrapped3;
    s16               wrapped4;
    s16               wrapped5;
    s16               wrappedYaw;
    s32               angle3;
    s32               angle4;
    s32               angle5;
    s32               angle;
    s32               angle2;
    s32               finalYaw;
    s32               turnDelta;
    s32               finalDelta;
    s32               yawDifference;
    u16               unsignedDelta;
    work = arg0->work;
    if (work->field_4 != 0) {
        head                                            = SCRATCH_HEAD(ActorMoveScratch);
        obj                                             = arg0->extra;
        scratch                                         = (SCRATCH_HEAD(ActorMoveScratch) = head - 1);
        ((GpEnemy*)arg0->spawnArg2)->node.state.b.flags = 0;
        obj->flags                                      = 0;
        Tmd_AllocBuffers(obj);
        work->field_8EC.radius = 0x19C;
        work->field_828        = 1;
        work->field_832        = 0x10;
        work->field_82A        = 0;
        work->field_82E        = 0;
        work->field_B6C.flags |= 0x4000;

        func_actor_421600_80134604(arg0);
        func_actor_421600_80134604(arg0);
        work->field_6   = 0;
        work->field_8   = 0;
        coord           = ((TmdObject*)arg0->extra)->coords;
        head[-1].vec.vx = (s16)(Player_Status.coordMtx->t[0] - coord->coord.t[0]);
        scratch->vec.vy = Player_Status.coordMtx->t[1] - coord->coord.t[1];
        z               = Player_Status.coordMtx->t[2] - coord->coord.t[2];
        scratch->vec.vz = z;
        facing          = ((TmdObject*)arg0->extra)->coords;
        angle           = ratan2((s32)head[-1].vec.vx, (s32)z);
        delta1          = angle - ratan2((s32)-facing->coord.m[2][0], (s32)facing->coord.m[2][2]);
        wrapped         = delta1;
        if (delta1 < 0) {
        wrapNegative:
            if (wrapped < -0x800) {
                wrapped += 0x1000;
                goto wrapNegative;
            }
        } else {
        wrapPositive:
            if (wrapped >= 0x801) {
                wrapped -= 0x1000;
                goto wrapPositive;
            }
        }
        work->field_840 = wrapped;
        matrix          = &scratch->matrix;
        Gfx_RotMatrixY(matrix, (s16)ratan2((s32)scratch->vec.vx, (s32)scratch->vec.vz) + 0x3E8, 1);
        Gfx_MatrixCol2(matrix, &scratch->vec);
        VectorNormalSS(&scratch->vec, &scratch->vec);
        gte_lddp(1000);
        gte_ldsv(&scratch->vec);
        gte_gpf12();
        gte_stsv(&scratch->vec);
        work->field_14     = 0;
        work->field_C[0].x = (s16)((u16)scratch->vec.vx + ((TmdObject*)arg0->extra)->coords->coord.t[0]);
        SCRATCH_POP(ActorMoveScratch);
        work->field_C[0].z      = (s16)((u16)scratch->vec.vz + ((TmdObject*)arg0->extra)->coords->coord.t[2]);
        work->field_CCC.end1.vz = 0x26C;
        return;
    }
    work->field_8      += 1;
    head2               = SCRATCH_HEAD(ActorMoveScratch);
    scratch             = (SCRATCH_HEAD(ActorMoveScratch) = head2 - 1);
    head2[-1].vec.vx    = (s16)(work->field_C[work->field_14].x - ((TmdObject*)arg0->extra)->coords->coord.t[0]);
    scratch->vec.vy     = 0;
    scratch->vec.vz     = work->field_C[work->field_14].z - ((TmdObject*)arg0->extra)->coords->coord.t[2];
    coord2              = ((TmdObject*)arg0->extra)->coords;
    head2[-1].target.vx = (s16)(Player_Status.coordMtx->t[0] - coord2->coord.t[0]);
    target              = &head2[-1].target;
    target->vy          = Player_Status.coordMtx->t[1] - coord2->coord.t[1];
    target->vz          = Player_Status.coordMtx->t[2] - coord2->coord.t[2];
    if (!actorOutsideRadius(&scratch->vec, 0xA0) || (s16)work->field_6 >= 0x15) {
        facing2  = ((TmdObject*)arg0->extra)->coords;
        angle2   = ratan2((s32)head2[-1].target.vx, (s32)target->vz);
        delta2   = angle2 - ratan2((s32)-facing2->coord.m[2][0], (s32)facing2->coord.m[2][2]);
        wrapped2 = delta2;
        if (delta2 < 0) {
        wrapNegative2:
            if (wrapped2 < -0x800) {
                wrapped2 += 0x1000;
                goto wrapNegative2;
            }
        } else {
        wrapPositive2:
            if (wrapped2 >= 0x801) {
                wrapped2 -= 0x1000;
                goto wrapPositive2;
            }
        }
        work->field_840 = wrapped2;
        if (work->field_14 == 0) {
            Gfx_RotMatrixY(&scratch->matrix, (s16)ratan2((s32)scratch->target.vx, (s32)scratch->target.vz) - 0x2EE, 1);
            work->field_14 = 1;
        } else {
            Gfx_RotMatrixY(&scratch->matrix, (s16)ratan2((s32)scratch->target.vx, (s32)scratch->target.vz) + 0x2EE, 1);
            work->field_14 = 0;
        }
        direction = &scratch->target;
        Gfx_MatrixCol2(&scratch->matrix, direction);
        VectorNormalSS(direction, direction);
        gte_lddp(2000);
        gte_ldsv(direction);
        gte_gpf12();
        gte_stsv(direction);
        work->field_C[work->field_14].x = (s16)((u16)scratch->target.vx + ((TmdObject*)arg0->extra)->coords->coord.t[0]);
        work->field_C[work->field_14].z = (s16)((u16)scratch->target.vz + ((TmdObject*)arg0->extra)->coords->coord.t[2]);
        work->field_6                   = 0;
    }
    func_actor_421600_80134604(arg0);
    facing3  = ((TmdObject*)arg0->extra)->coords;
    angle3   = ratan2((s32)scratch->target.vx, (s32)scratch->target.vz);
    delta3   = angle3 - ratan2((s32)-facing3->coord.m[2][0], (s32)facing3->coord.m[2][2]);
    wrapped3 = delta3;
    if (delta3 < 0) {
    wrapNegative3:
        if (wrapped3 < -0x800) {
            wrapped3 += 0x1000;
            goto wrapNegative3;
        }
    } else {
    wrapPositive3:
        if (wrapped3 >= 0x801) {
            wrapped3 -= 0x1000;
            goto wrapPositive3;
        }
    }
    work->field_840 = wrapped3;
    facing4         = ((TmdObject*)arg0->extra)->coords;
    angle4          = ratan2((s32)scratch->vec.vx, (s32)scratch->vec.vz);
    delta4          = angle4 - ratan2((s32)-facing4->coord.m[2][0], (s32)facing4->coord.m[2][2]);
    wrapped4        = delta4;
    if (delta4 < 0) {
    wrapNegative4:
        if (wrapped4 < -0x800) {
            wrapped4 += 0x1000;
            goto wrapNegative4;
        }
    } else {
    wrapPositive4:
        if (wrapped4 >= 0x801) {
            wrapped4 -= 0x1000;
            goto wrapPositive4;
        }
    }
    turnDelta         = wrapped4;
    scratch->original = (scratch->delta = (s16)turnDelta);
    delta             = scratch->delta;
    unsignedDelta     = (u16)scratch->delta;
    magnitude         = abs(scratch->delta);
    if (magnitude >= 0x601) {
        targetDelta     = (s16)work->field_840;
        targetMagnitude = abs(targetDelta);
        if ((targetMagnitude >= 0x101) && ((targetDelta * delta) < 0)) {
            adjustedDelta = unsignedDelta - 0x1000;
            if (delta < 0) {
                adjustedDelta = unsignedDelta + 0x1000;
            }
            scratch->delta = adjustedDelta;
        }
    }
    if (scratch->delta >= 0x21) {
        scratch->delta = 0x20;
    }
    if (scratch->delta < -0x20) {
        scratch->delta = -0x20;
    }
    work->field_83E = scratch->delta * 0x10;
    turnCoord       = ((TmdObject*)arg0->extra)->coords;
    yaw             = (u16)scratch->delta + ratan2((s32)-turnCoord->coord.m[2][0], (s32)turnCoord->coord.m[2][2]);
    scratch->delta  = yaw;
    Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->coords->coord, (s32)yaw, 1);
    record = &work->field_90C;
    if ((s16)work->field_82A == 0) {
        if (Actor421600_HasRecord10(arg0)) {
            actorMoveForward(((TmdObject*)arg0->extra)->coords, 20);
        } else {
            actorMoveForward(((TmdObject*)arg0->extra)->coords, 20);
        }
        record = &work->field_90C;
    }
    func_actor_421600_80132310(((TmdObject*)arg0->extra)->coords, record, 12, &scratch->vec);
    if (func_actor_421600_8013285C(((TmdObject*)arg0->extra)->coords, &work->field_B8C, 12) == 1) {
        originalMagnitude = abs(scratch->original);
        if (originalMagnitude < 0x20) {
            work->field_6 += 1;
        }
    }
    ((TmdObject*)arg0->extra)->coords->flg = 0;
    coord3                                 = ((TmdObject*)arg0->extra)->coords;
    scratch->target.vx                     = (s16)(Player_Status.coordMtx->t[0] - coord3->coord.t[0]);
    target2                                = &scratch->target;
    target2->vy                            = Player_Status.coordMtx->t[1] - coord3->coord.t[1];
    target2->vz                            = Player_Status.coordMtx->t[2] - coord3->coord.t[2];
    if ((s16)work->field_8 > (s16)work->field_EA6) {
        if ((s16)work->field_EAA <= 0) {

            if (actorOutsideRadius(&scratch->target, radius)) {
                if (!actorOutsideRadius(&scratch->target, 0x1F40) && (s16)work->field_8 >= 0x1C3) {
                    facing5  = ((TmdObject*)arg0->extra)->coords;
                    angle5   = ratan2((s32)scratch->vec.vx, (s32)scratch->vec.vz);
                    delta5   = angle5 - ratan2((s32)-facing5->coord.m[2][0], (s32)facing5->coord.m[2][2]);
                    wrapped5 = delta5;
                    if (delta5 < 0) {
                    wrapNegative5:
                        if (wrapped5 < -0x800) {
                            wrapped5 += 0x1000;
                            goto wrapNegative5;
                        }
                    } else {
                    wrapPositive5:
                        if (wrapped5 >= 0x801) {
                            wrapped5 -= 0x1000;
                            goto wrapPositive5;
                        }
                    }
                    finalDelta     = wrapped5;
                    scratch->delta = (s16)finalDelta;
                    finalDelta     = abs(finalDelta);
                    if (finalDelta < 0x300) {
                        goto changeState;
                    }
                }
            } else {
            changeState:
                work->field_0 = 0x1C;
            }
            playerX            = -((TmdObject*)(gameGetPtrSlot(3))->extra)->coords->coord.m[2][0];
            scratch->playerYaw = ratan2((s32)playerX, (s32)((TmdObject*)(gameGetPtrSlot(3))->extra)->coords->coord.m[2][2]);
            targetYaw          = ratan2((s32)scratch->target.vx, (s32)scratch->target.vz) + 0x800;
            wrappedYaw         = targetYaw;
            scratch->yaw       = targetYaw;
            if (targetYaw < 0) {
            wrapYawNegative:
                if (wrappedYaw < -0x800) {
                    wrappedYaw += 0x1000;
                    goto wrapYawNegative;
                }
            } else {
            wrapYawPositive:
                if (wrappedYaw >= 0x801) {
                    wrappedYaw -= 0x1000;
                    goto wrapYawPositive;
                }
            }
            finalYaw      = wrappedYaw;
            scratch->yaw  = (s16)finalYaw;
            yawDifference = finalYaw - scratch->playerYaw;
            if (yawDifference < 0) {
                yawDifference = -yawDifference;
            }
            if (yawDifference >= 0x601) {
                work->field_0 = 0x1C;
            }
        } else {
            work->field_EAA -= 1;
        }
    }
    func_actor_421600_80133334(((TmdObject*)arg0->extra)->coords);
    SCRATCH_POP(ActorMoveScratch);
    ((TmdObject*)arg0->extra)->coords->flg = 0;
}

void func_actor_421600_8013A404(Task* arg0)
{
    Actor421600Work* temp_s0;
    GsCOORDINATE2*   temp_v0_2;
    s32              temp_a0;
    s32              temp_a1;
    s32              var_a0;
    s32              var_v1;
    u32              temp_v0;
    u8               temp_v1;

    temp_s0 = arg0->work;
    if (temp_s0->field_4 != 0) {
        temp_v0          = (Gp_LcgState * 5) + 0x71357911;
        Gp_LcgState      = temp_v0;
        temp_s0->field_6 = temp_s0->field_EA4 + ((temp_v0 >> 0x10) & 0xF);
    }
    temp_s0->field_6 -= 1;
    func_actor_421600_80134604(arg0);
    if ((s16)temp_s0->field_6 < 0) {
        temp_v1 = temp_s0->field_E90.bytes[2];
        if ((temp_v1 == 1) || (temp_v1 == 3)) {
            temp_s0->field_0 = 5;
        } else if (temp_v1 == 2) {
            temp_v0_2 = ((TmdObject*)arg0->extra)->coords;
            temp_a0   = temp_v0_2->coord.t[0];
            temp_a1   = temp_v0_2->coord.t[2];
            if (temp_a0 >= 0xD49) {
                var_a0 = 3;
            } else if (temp_a0 > 0) {
                var_a0 = 2;
            } else {
                var_a0 = temp_a0 >= -0xC7F;
            }
            var_v1 = 0;
            if (temp_a1 < 0xBB9) {
                var_v1 = 1;
                if (temp_a1 <= 0) {
                    var_v1 = 3;
                    if (temp_a1 >= -0xBB7) {
                        var_v1 = 2;
                    }
                }
            }
            if (D_actor_421600_801511C0[var_a0 | (var_v1 * 4)] >= 0xB) {
                temp_s0->field_0 = 0x24;
            } else {
                temp_s0->field_0 = 6;
            }
        } else {
            temp_s0->field_0 = 0x24;
        }
    }
}

void func_actor_421600_8013A554(Task* arg0)
{
    SVECTOR                   effect;
    s16                       aimZ;
    s16                       fallbackZ;
    s32                       fallbackAngle;
    s16                       fallbackDelta;
    s32                       moveAngle;
    s16                       moveDelta;
    s32                       playerX;
    s32                       facingAngle;
    s16                       facingDelta;
    s32                       aimAngle;
    s16                       aimDelta;
    s16                       targetZ;
    s16                       yaw;
    s16                       nextState;
    GsCOORDINATE2*            targetCoord;
    GsCOORDINATE2*            aimCoord;
    GsCOORDINATE2*            fallbackCoord;
    GsCOORDINATE2*            fallbackFacing;
    GsCOORDINATE2*            moveCoord;
    GsCOORDINATE2*            facingCoord;
    GsCOORDINATE2*            aimFacing;
    GsCOORDINATE2*            stepCoord;
    GsCOORDINATE2*            coord;
    s32                       sound;
    s32                       spawnEffect;
    s32                       effectFlags;
    s32                       part;
    s32                       fallbackYaw;
    s32                       distance;
    s32                       closeDistance;
    s32                       farDistance;
    s32                       pan;
    TmdObject*                obj;
    GpEnemy*                  ctx;
    Task*                     player;
    Actor421600Work*          work;
    GpEnemy*                  enemy;
    Actor421600AttackScratch* head;
    Actor421600AttackScratch* scratch;

    work   = arg0->work;
    enemy  = arg0->spawnArg2;
    player = gameGetPtrSlot(3);
    if (work->field_4 != 0) {
        obj                     = arg0->extra;
        ctx                     = arg0->spawnArg2;
        ctx->node.state.b.flags = 0;
        Gp_ArmStateF0(1);
        obj->flags = 0;
        Tmd_AllocBuffers(obj);
        work->field_8EC.radius = 0x19C;
        work->field_828        = 1;
        work->field_832        = 0x10;
        work->field_82A        = 0;
        work->field_82E        = 3;
        work->field_83E        = 0;
        work->field_B6C.flags  = (u16)(work->field_B6C.flags | 0x4000);
        func_actor_421600_80134604(arg0);
        work->field_6   = 0;
        work->field_8   = 0;
        work->field_E9E = 0;
        sound           = (((u16)ctx->placeKey >> 0xC) << 8) | 0x40010006;
        pan             = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(sound, pan, (s32)(s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
        return;
    }
    head          = SCRATCH_HEAD(Actor421600AttackScratch);
    scratch       = (SCRATCH_HEAD(Actor421600AttackScratch) = head - 1);
    coord         = ((TmdObject*)arg0->extra)->coords;
    scratch->zone = Actor421600_Zone(coord);
    if ((func_actor_421600_8013285C(((TmdObject*)arg0->extra)->coords, &work->field_B8C, 0xC) != 0) && ((s16)work->field_6 >= 0xB)) {
        work->field_0 = 5;
    }
    if ((func_actor_421600_80132310(((TmdObject*)arg0->extra)->coords, &work->field_90C, 0xC, &scratch->vec) << 0x10) != 0 && work->field_82E == 3) {
        work->field_8E4 = 8;
        if (Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F8, (s32)&work->field_8D0, 0) == 0) {
            playerX            = -((TmdObject*)gameGetPtrSlot(3)->extra)->coords->coord.m[2][0];
            scratch->playerYaw = ratan2(playerX, ((TmdObject*)gameGetPtrSlot(3)->extra)->coords->coord.m[2][2]);
            targetCoord        = ((TmdObject*)arg0->extra)->coords;
            scratch->vec.vx    = (s16)(Player_Status.coordMtx->t[0] - targetCoord->coord.t[0]);
            scratch->vec.vy    = (s16)(Player_Status.coordMtx->t[1] - targetCoord->coord.t[1]);
            targetZ            = Player_Status.coordMtx->t[2] - targetCoord->coord.t[2];
            scratch->vec.vz    = targetZ;
            yaw                = ratan2(scratch->vec.vx, targetZ) + 0x800;
            scratch->yaw       = yaw;
            scratch->yaw       = actorNormalizeYaw(yaw);
            facingCoord        = ((TmdObject*)arg0->extra)->coords;
            facingAngle        = ratan2(scratch->vec.vx, scratch->vec.vz);
            facingDelta        = facingAngle - ratan2(-facingCoord->coord.m[2][0], facingCoord->coord.m[2][2]);
            scratch->delta     = actorNormalizeYaw(facingDelta);
            distance           = scratch->yaw - scratch->playerYaw;
            distance           = abs(distance);
            if (distance < 0x400) {
                work->field_E7C = (s32)&D_actor_421600_80151090;
            } else {
                work->field_E7C = (s32)&D_actor_421600_801510A4;
                scratch->yaw    = (s16)((u16)scratch->yaw + 0x800);
            }
            work->field_8C8.vx = 0;
            work->field_8C8.vy = (u16)scratch->yaw;
            work->field_8C8.vz = 0;
            work->field_8B8.vx = (s32)((TmdObject*)player->extra)->coords->coord.t[0];
            work->field_8B8.vy = (s32)((TmdObject*)player->extra)->coords->coord.t[1];
            work->field_8B8.vz = (s32)((TmdObject*)player->extra)->coords->coord.t[2];
            Gp_DispatchMsg(player, 0x3E9, (s32)&work->field_8B8, 0);
            if (work->field_E9E < 0x3E8) {
                if (enemy->hp > 0) {
                    closeDistance = scratch->yaw - scratch->playerYaw;
                    closeDistance = abs(closeDistance);
                    if (closeDistance < 0x400) {
                        scratch->reply = Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F9, Gp_PackObjPair(enemy, 2), 0);
                    } else {
                        scratch->reply = Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F9, Gp_PackObjPair(enemy, 3), 0);
                    }
                }
                if (scratch->reply != 1) {
                    work->field_E80          = 3;
                    work->field_E84          = 0;
                    work->field_E88          = 0;
                    work->field_8A4          = 0;
                    work->field_8A8          = 0;
                    work->field_8AC          = 0;
                    work->field_8B4          = 7;
                    work->field_8B6          = 1;
                    work->field_E9C          = 1;
                    work->field_E90.bytes[3] = 0;
                    Gp_DispatchMsg(player, 0x3FF, (s32)&work->field_E7C, 0);
                }
                nextState = 0x25;
            } else {
                if (enemy->hp > 0) {
                    farDistance = scratch->yaw - scratch->playerYaw;
                    farDistance = abs(farDistance);
                    if (farDistance < 0x400) {
                        scratch->reply = Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F9, Gp_PackObjPair(enemy, 0), 0);
                    } else {
                        scratch->reply = Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F9, Gp_PackObjPair(enemy, 1), 0);
                    }
                }
                if (scratch->reply == 1) {
                    ((GameActor*)player->work)->field_956 = 0xA;
                }
                work->field_E80          = 1;
                work->field_E84          = 0;
                work->field_E88          = 0;
                work->field_8A4          = 0;
                work->field_8A8          = 0;
                work->field_8AC          = 0;
                work->field_8B4          = 7;
                work->field_8B6          = 1;
                work->field_E9C          = 1;
                work->field_E90.bytes[3] = 0;
                Gp_DispatchMsg(player, 0x3FF, (s32)&work->field_E7C, 0);
                nextState = 0x1E;
            }
            work->field_0 = nextState;
        }
        aimCoord        = ((TmdObject*)arg0->extra)->coords;
        scratch->vec.vx = (s16)(Player_Status.coordMtx->t[0] - aimCoord->coord.t[0]);
        scratch->vec.vy = (s16)(Player_Status.coordMtx->t[1] - aimCoord->coord.t[1]);
        aimZ            = Player_Status.coordMtx->t[2] - aimCoord->coord.t[2];
        scratch->vec.vz = aimZ;
        aimFacing       = ((TmdObject*)arg0->extra)->coords;
        aimAngle        = ratan2(scratch->vec.vx, aimZ);
        aimDelta        = aimAngle - ratan2(-aimFacing->coord.m[2][0], aimFacing->coord.m[2][2]);
        scratch->aim    = actorNormalizeYaw(aimDelta);
    } else {
        fallbackCoord   = ((TmdObject*)arg0->extra)->coords;
        scratch->vec.vx = (s16)(Player_Status.coordMtx->t[0] - fallbackCoord->coord.t[0]);
        scratch->vec.vy = (s16)(Player_Status.coordMtx->t[1] - fallbackCoord->coord.t[1]);
        fallbackZ       = Player_Status.coordMtx->t[2] - fallbackCoord->coord.t[2];
        scratch->vec.vz = fallbackZ;
        fallbackFacing  = ((TmdObject*)arg0->extra)->coords;
        fallbackAngle   = ratan2(scratch->vec.vx, fallbackZ);
        fallbackDelta   = fallbackAngle - ratan2(-fallbackFacing->coord.m[2][0], fallbackFacing->coord.m[2][2]);
        fallbackYaw     = actorNormalizeYaw(fallbackDelta);
        scratch->aim    = (s16)fallbackYaw;
        fallbackYaw     = abs(fallbackYaw);
        if (fallbackYaw >= 0x601) {
            work->field_0 = 0x1D;
        }
    }
    ((TmdObject*)arg0->extra)->coords->flg = 0;
    moveCoord                              = ((TmdObject*)arg0->extra)->coords;
    moveAngle                              = ratan2(work->field_E70, work->field_E74);
    moveDelta                              = moveAngle - ratan2(-moveCoord->coord.m[2][0], moveCoord->coord.m[2][2]);
    scratch->delta                         = actorNormalizeYaw(moveDelta);
    func_actor_421600_80134604(arg0);
    stepCoord = ((TmdObject*)arg0->extra)->coords;
    actorMoveForward(stepCoord, 200);
    work->field_E9E = (s16)((u16)work->field_E9E + 0xC8);
    if (work->field_82E == 3) {
        switch (work->field_5A & 0x3FF) {
            case 5:
                spawnEffect = 1;
                part        = 7;
                effectFlags = 0x4300;
                effect.vz   = 0;
                effect.vx   = 0;
                effect.vy   = 700;
                break;
            case 8:
                spawnEffect = 1;
                part        = 9;
                effectFlags = 0x3500;
                effect.vz   = 0;
                effect.vx   = 0;
                effect.vy   = 700;
                break;
            case 10:
                spawnEffect = 1;
                part        = 14;
                effectFlags = 0x5A00;
                effect.vz   = 0;
                effect.vx   = 0;
                effect.vy   = 600;
                break;
            case 13:
                spawnEffect = 1;
                part        = 17;
                effectFlags = 0x4800;
                effect.vz   = 0;
                effect.vx   = 0;
                effect.vy   = 600;
                break;
            default:
                effectFlags = 0;
                spawnEffect = 0;
                part        = 0;
                break;
        }
        if ((Gp_State1C->roomEffectMode == 2) && (spawnEffect == 1)) {
            Gp_SpawnEff(0x60054, ((TmdObject*)arg0->extra)->coords + part, effectFlags | 0x80000000, &effect);
        }
    }
    SCRATCH_POP(Actor421600AttackScratch);
}

void func_actor_421600_8013B00C(Task* arg0)
{
    Actor421600Work*  work;
    ActorTurnScratch* head;
    ActorTurnScratch* blk;
    GpEnemy*          ctx;
    TmdObject*        obj;
    GsCOORDINATE2*    coord;
    GsCOORDINATE2*    coord2;
    GsCOORDINATE2*    coord3;
    GsCOORDINATE2*    coord4;
    s32               zone;
    s32               x_entry;
    s32               z_entry;
    s32               var_a0_entry;
    s32               var_v1_entry;
    Task*             task;
    GsCOORDINATE2*    playerCoord;
    s32               x;
    s32               z;
    s16               angle;
    s32               wrapped;
    s32               var_a0;
    s32               var_v1;

    work = arg0->work;
    task = gameGetPtrSlot(3);
    if (work->field_4 != 0) {
        obj                     = arg0->extra;
        ctx                     = arg0->spawnArg2;
        ctx->node.state.b.flags = 0;
        obj->flags              = 0;
        Tmd_AllocBuffers(obj);
        work->field_832        = 0x10;
        work->field_82E        = 3;
        work->field_828        = 1;
        work->field_B6C.flags |= 0x4000;
        func_actor_421600_80134604(arg0);
        playerCoord = ((TmdObject*)task->extra)->coords;
        x_entry     = playerCoord->coord.t[0];
        z_entry     = playerCoord->coord.t[2];
        if (x_entry >= 0xD49) {
            var_a0_entry = 3;
        } else if (x_entry > 0) {
            var_a0_entry = 2;
        } else {
            var_a0_entry = x_entry >= -0xC7F;
        }
        var_v1_entry = 0;
        if (z_entry < 0xBB9) {
            var_v1_entry = 1;
            if (z_entry <= 0) {
                var_v1_entry = 3;
                if (z_entry >= -0xBB7) {
                    var_v1_entry = 2;
                }
            }
        }
        if (D_actor_421600_801511C0[var_a0_entry | (var_v1_entry * 4)] >= 7) {
            work->field_E78 = 1;
            return;
        }
        work->field_E78 = 0xB;
        return;
    }
    coord = ((TmdObject*)arg0->extra)->coords;
    x     = coord->coord.t[0];
    z     = coord->coord.t[2];
    if (x >= 0xD49) {
        var_a0 = 3;
    } else if (x > 0) {
        var_a0 = 2;
    } else {
        var_a0 = x >= -0xC7F;
    }
    var_v1 = 0;
    if (z < 0xBB9) {
        var_v1 = 1;
        if (z <= 0) {
            var_v1 = 3;
            if (z >= -0xBB7) {
                var_v1 = 2;
            }
        }
    }
    zone = D_actor_421600_801511C0[var_a0 | (var_v1 * 4)];
    if ((s16)zone == work->field_E78) {
        work->field_0 = 0;
        return;
    }
    head = SCRATCH_HEAD(ActorTurnScratch);
    SCRATCH_PUSH(ActorTurnScratch);
    blk = head - 1;
    if ((s16)zone > work->field_E78) {
        head[-1].delta.vx = D_actor_421600_80151158[zone - 1].vx;
        blk->delta.vy     = D_actor_421600_80151158[zone - 1].vy;
        blk->delta.vz     = D_actor_421600_80151158[zone - 1].vz;
    } else {
        head[-1].delta.vx = D_actor_421600_80151158[zone + 1].vx;
        blk->delta.vy     = D_actor_421600_80151158[zone + 1].vy;
        blk->delta.vz     = D_actor_421600_80151158[zone + 1].vz;
    }
    blk->delta.vx = blk->delta.vx - (u16)((TmdObject*)arg0->extra)->coords->coord.t[0];
    blk->delta.vy = 0;
    blk->delta.vz = blk->delta.vz - (u16)((TmdObject*)arg0->extra)->coords->coord.t[2];
    func_actor_421600_80134604(arg0);
    coord2 = ((TmdObject*)arg0->extra)->coords;
    angle  = ratan2(blk->delta.vx, blk->delta.vz) - ratan2(-coord2->coord.m[2][0], coord2->coord.m[2][2]);
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
    wrapped         = angle;
    blk->angle      = wrapped;
    work->field_840 = wrapped;
    if (blk->angle >= 0x81) {
        blk->angle = 0x80;
    }
    if (blk->angle < -0x80) {
        blk->angle = -0x80;
    }
    work->field_83E = blk->angle;
    coord3          = ((TmdObject*)arg0->extra)->coords;
    blk->angle      = blk->angle + ratan2(-coord3->coord.m[2][0], coord3->coord.m[2][2]);
    Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->coords->coord, blk->angle, 1);
    if (work->field_82A == 0) {
        coord4 = ((TmdObject*)arg0->extra)->coords;
        actorMoveForward(coord4, 0xC8);
    }
    func_actor_421600_80132310(((TmdObject*)arg0->extra)->coords, &work->field_90C, 0xC, &blk->delta);
    SCRATCH_POP(ActorTurnScratch);
    ((TmdObject*)arg0->extra)->coords->flg = 0;
}

/// Zone-aim tick: the live-actor edge re-arms the model the way
/// `func_actor_421600_80138D24` does -- buffers reallocated, clip 0x10, pose 3,
/// motion 1, the 0xB6C node's 0x4000 flag up.
///
/// Otherwise the X and Z of the actor's coordinate are bucketed into the 4x4
/// zone table `D_actor_421600_801511C0` exactly as `func_actor_421600_8013A404`
/// does, and zone 5 abandons the tick into state 7. Any other zone picks the
/// neighbouring entry of the 8-byte pose table `D_actor_421600_80151158` --
/// `zone - 1` above the table's midpoint `mode`, `zone + 1` at or below it --
/// and copies all three halfwords into a 0xC block taken off `G_SCRATCH_HEAD`,
/// which becomes the XZ direction from the actor to that pose.
///
/// `mode` and the `(s8)` casts on `zone` are load-bearing, and so is the
/// `blk->vec.vy = 0` between the two coordinate subtractions. A plain `5`
/// literal lets expand fold `zone > 5` into `zone < 6`, which drops the two
/// register copies and the `slt` the ROM has; keeping the limit in a
/// declaration-initialised `s8` leaves it a register operand so the fold never
/// runs. The midpoint store then lands in the load-delay slot the subtractions
/// leave open.
void func_actor_421600_8013B4C4(Task* arg0)
{
    Actor421600Work*  work;
    ActorTurnScratch* head;
    ActorTurnScratch* blk;
    GpEnemy*          ctx;
    TmdObject*        obj;
    GsCOORDINATE2*    coord;
    GsCOORDINATE2*    coord2;
    GsCOORDINATE2*    coord3;
    GsCOORDINATE2*    coord4;
    s32               zone;
    s8                mode = 5;
    s32               v;
    s32               x;
    s32               z;
    s16               angle;
    s32               wrapped;
    s32               var_a0;
    s32               var_v1;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                     = arg0->extra;
        ctx                     = arg0->spawnArg2;
        ctx->node.state.b.flags = 1;
        obj->flags              = 0;
        Tmd_AllocBuffers(obj);
        work->field_832        = 0x10;
        work->field_82E        = 3;
        work->field_828        = 1;
        work->field_B6C.flags |= 0x4000;
        func_actor_421600_80134604(arg0);
        return;
    }
    coord = ((TmdObject*)arg0->extra)->coords;
    x     = coord->coord.t[0];
    z     = coord->coord.t[2];
    if (x >= 0xD49) {
        var_a0 = 3;
    } else if (x > 0) {
        var_a0 = 2;
    } else {
        var_a0 = x >= -0xC7F;
    }
    var_v1 = 0;
    if (z < 0xBB9) {
        var_v1 = 1;
        if (z <= 0) {
            var_v1 = 3;
            if (z >= -0xBB7) {
                var_v1 = 2;
            }
        }
    }
    zone = D_actor_421600_801511C0[var_a0 | (var_v1 * 4)];
    if ((s8)zone == mode) {
        work->field_0 = 7;
        return;
    }
    head = SCRATCH_HEAD(ActorTurnScratch);
    SCRATCH_PUSH(ActorTurnScratch);
    blk = head - 1;
    if ((s8)zone > mode) {
        head[-1].delta.vx = D_actor_421600_80151158[zone - 1].vx;
        blk->delta.vy     = D_actor_421600_80151158[zone - 1].vy;
        blk->delta.vz     = D_actor_421600_80151158[zone - 1].vz;
    } else {
        head[-1].delta.vx = D_actor_421600_80151158[zone + 1].vx;
        blk->delta.vy     = D_actor_421600_80151158[zone + 1].vy;
        blk->delta.vz     = D_actor_421600_80151158[zone + 1].vz;
    }
    blk->delta.vx = blk->delta.vx - (u16)((TmdObject*)arg0->extra)->coords->coord.t[0];
    blk->delta.vy = 0;
    blk->delta.vz = blk->delta.vz - (u16)((TmdObject*)arg0->extra)->coords->coord.t[2];
    func_actor_421600_80134604(arg0);
    coord2 = ((TmdObject*)arg0->extra)->coords;
    angle  = ratan2(blk->delta.vx, blk->delta.vz) - ratan2(-coord2->coord.m[2][0], coord2->coord.m[2][2]);
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
    wrapped         = angle;
    blk->angle      = wrapped;
    work->field_840 = wrapped;
    if (blk->angle >= 0x81) {
        blk->angle = 0x80;
    }
    if (blk->angle < -0x80) {
        blk->angle = -0x80;
    }
    work->field_83E = blk->angle;
    coord3          = ((TmdObject*)arg0->extra)->coords;
    blk->angle      = blk->angle + ratan2(-coord3->coord.m[2][0], coord3->coord.m[2][2]);
    Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->coords->coord, blk->angle, 1);
    if (work->field_82A == 0) {
        coord4 = ((TmdObject*)arg0->extra)->coords;
        actorMoveForward(coord4, 0xC8);
    }
    func_actor_421600_80132310(((TmdObject*)arg0->extra)->coords, &work->field_90C, 0xC, &blk->delta);
    SCRATCH_POP(ActorTurnScratch);
    ((TmdObject*)arg0->extra)->coords->flg = 0;
}

void func_actor_421600_8013B8E0(Task* arg0)
{
    Actor421600Work* temp_s1;
    TmdObject*       temp_a0;

    temp_s1 = arg0->work;
    if (temp_s1->field_4 != 0) {
        temp_a0                                         = arg0->extra;
        ((GpEnemy*)arg0->spawnArg2)->node.state.b.flags = 0;
        temp_a0->flags                                  = 0;
        Tmd_AllocBuffers(temp_a0);
        temp_s1->field_832                            = 0x10;
        temp_s1->field_82E                            = 0x11;
        temp_s1->field_828                            = 2;
        temp_s1->field_B6C.flags                     |= 0x4000;
        ((TmdObject*)arg0->extra)->coords->coord.t[0] = 0;
        ((TmdObject*)arg0->extra)->coords->coord.t[1] = 0;
        ((TmdObject*)arg0->extra)->coords->coord.t[2] = 0;
        ((TmdObject*)arg0->extra)->coords->flg        = 0;
        Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->coords->coord, 0, 1);
        func_actor_421600_80134604(arg0);
    }
    func_actor_421600_80134604(arg0);
    if (temp_s1->field_68 & 0x100) {
        ((TmdObject*)arg0->extra)->coords->coord.t[0] = -0x334;
        ((TmdObject*)arg0->extra)->coords->coord.t[1] = 0;
        ((TmdObject*)arg0->extra)->coords->coord.t[2] = -0x4C4;
        ((TmdObject*)arg0->extra)->coords->flg        = 0;
        Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->coords->coord, 0x400, 1);
        temp_s1->field_828 = 2;
        temp_s1->field_82E = 0;
        func_actor_421600_80134604(arg0);
        func_actor_421600_80134604(arg0);
        temp_s1->field_0 = 0x27;
    }
}

static __inline__ s32 Actor421600_RouteZone(s32 x, s32 z)
{
    s32 ix = x > 0;
    s32 iz = z < 1;
    return D_actor_421600_801511D0[ix + (iz * 2)];
}

void func_actor_421600_8013BA70(Task* arg0)
{
    s32                      radius = 0x5DC;
    Actor421600Work*         work;
    GpEnemy*                 enemy;
    GsCOORDINATE2*           zoneCoord;
    GsCOORDINATE2*           clampCoord;
    s32                      x, zClamp;
    GpRec18*                 record;
    GsCOORDINATE2*           coord;
    GsCOORDINATE2*           coord2;
    GsCOORDINATE2*           coord3;
    GsCOORDINATE2*           facing3;
    GsCOORDINATE2*           facing4;
    GsCOORDINATE2*           facing5;
    GsCOORDINATE2*           facing;
    GsCOORDINATE2*           facing2;
    GsCOORDINATE2*           turnCoord;
    Actor421600RouteScratch* scratch;
    SVECTOR*                 target;
    SVECTOR*                 target2;
    Actor421600RouteScratch* head;
    Actor421600RouteScratch* head2;
    TmdObject*               obj;
    s16                      targetDelta;
    s16                      delta;
    s16                      yaw;
    s16                      delta3;
    s16                      delta4;
    s16                      delta5;
    s32                      playerX;
    s16                      delta1;
    s16                      delta2;
    s16                      targetYaw;
    s16                      z;
    s32                      magnitude;
    s32                      targetMagnitude;
    s16                      adjustedDelta;
    s32                      originalMagnitude;
    s16                      wrapped;
    s16                      wrapped2;
    s16                      wrapped3;
    s16                      wrapped4;
    s16                      wrapped5;
    s16                      wrappedYaw;
    s32                      angle3;
    s32                      angle4;
    s32                      angle5;
    s32                      angle;
    s32                      angle2;
    s32                      finalYaw;
    s32                      turnDelta;
    s32                      finalDelta;
    s32                      yawDifference;
    u16                      unsignedDelta;
    work  = arg0->work;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        head                      = SCRATCH_HEAD(Actor421600RouteScratch);
        obj                       = arg0->extra;
        scratch                   = (SCRATCH_HEAD(Actor421600RouteScratch) = head - 1);
        enemy->node.state.b.flags = 0;
        obj->flags                = 0;
        Tmd_AllocBuffers(obj);
        work->field_8EC.radius = 0x19C;
        work->field_828        = 1;
        work->field_832        = 0x10;
        work->field_82A        = 0;
        work->field_82E        = 0;
        work->field_B6C.flags |= 0x4000;

        func_actor_421600_80134604(arg0);
        func_actor_421600_80134604(arg0);
        work->field_6   = 0;
        work->field_8   = 0;
        zoneCoord       = ((TmdObject*)Gp_ActorSlots[0]->extra)->coords;
        scratch->zone   = Actor421600_RouteZone(zoneCoord->coord.t[0], zoneCoord->coord.t[2]);
        coord           = ((TmdObject*)arg0->extra)->coords;
        head[-1].vec.vx = (s16)(Player_Status.coordMtx->t[0] - coord->coord.t[0]);
        scratch->vec.vy = Player_Status.coordMtx->t[1] - coord->coord.t[1];
        z               = Player_Status.coordMtx->t[2] - coord->coord.t[2];
        scratch->vec.vz = z;
        facing          = ((TmdObject*)arg0->extra)->coords;
        angle           = ratan2((s32)head[-1].vec.vx, (s32)z);
        delta1          = angle - ratan2((s32)-facing->coord.m[2][0], (s32)facing->coord.m[2][2]);
        wrapped         = delta1;
        if (delta1 < 0) {
        wrapNegative:
            if (wrapped < -0x800) {
                wrapped += 0x1000;
                goto wrapNegative;
            }
        } else {
        wrapPositive:
            if (wrapped >= 0x801) {
                wrapped -= 0x1000;
                goto wrapPositive;
            }
        }
        work->field_840 = wrapped;
        work->field_14  = 0;
        if ((enemy->placeKey >> 12) == 0) {
            work->field_C[0].x = D_actor_421600_801511D4[scratch->zone][0];
            work->field_C[0].z = D_actor_421600_801511D4[scratch->zone][1];
            work->field_C[1].x = D_actor_421600_801511D4[scratch->zone][2];
            work->field_C[1].z = D_actor_421600_801511D4[scratch->zone][3];
        } else {
            work->field_C[0].x = D_actor_421600_801511D4[scratch->zone][4];
            work->field_C[0].z = D_actor_421600_801511D4[scratch->zone][5];
            work->field_C[1].x = D_actor_421600_801511D4[scratch->zone][6];
            work->field_C[1].z = D_actor_421600_801511D4[scratch->zone][7];
        }
        SCRATCH_POP(Actor421600RouteScratch);
        work->field_CCC.end1.vz = 0x26C;
        return;
    }
    work->field_8      += 1;
    head2               = SCRATCH_HEAD(Actor421600RouteScratch);
    scratch             = (SCRATCH_HEAD(Actor421600RouteScratch) = head2 - 1);
    head2[-1].vec.vx    = (s16)(work->field_C[work->field_14].x - ((TmdObject*)arg0->extra)->coords->coord.t[0]);
    scratch->vec.vy     = 0;
    scratch->vec.vz     = work->field_C[work->field_14].z - ((TmdObject*)arg0->extra)->coords->coord.t[2];
    coord2              = ((TmdObject*)arg0->extra)->coords;
    head2[-1].target.vx = (s16)(Player_Status.coordMtx->t[0] - coord2->coord.t[0]);
    target              = &head2[-1].target;
    target->vy          = Player_Status.coordMtx->t[1] - coord2->coord.t[1];
    target->vz          = Player_Status.coordMtx->t[2] - coord2->coord.t[2];
    if (!actorOutsideRadius(&scratch->vec, 0xA0) || (s16)work->field_6 >= 0x15) {
        facing2  = ((TmdObject*)arg0->extra)->coords;
        angle2   = ratan2((s32)head2[-1].target.vx, (s32)target->vz);
        delta2   = angle2 - ratan2((s32)-facing2->coord.m[2][0], (s32)facing2->coord.m[2][2]);
        wrapped2 = delta2;
        if (delta2 < 0) {
        wrapNegative2:
            if (wrapped2 < -0x800) {
                wrapped2 += 0x1000;
                goto wrapNegative2;
            }
        } else {
        wrapPositive2:
            if (wrapped2 >= 0x801) {
                wrapped2 -= 0x1000;
                goto wrapPositive2;
            }
        }
        work->field_840 = wrapped2;
        if (work->field_14 == 0) {
            Gfx_RotMatrixY(&scratch->matrix, (s16)ratan2((s32)scratch->target.vx, (s32)scratch->target.vz) - 0x2EE, 1);
            work->field_14 = 1;
        } else {
            Gfx_RotMatrixY(&scratch->matrix, (s16)ratan2((s32)scratch->target.vx, (s32)scratch->target.vz) + 0x2EE, 1);
            work->field_14 = 0;
        }
        zoneCoord     = ((TmdObject*)Gp_ActorSlots[0]->extra)->coords;
        scratch->zone = Actor421600_RouteZone(zoneCoord->coord.t[0], zoneCoord->coord.t[2]);
        if ((enemy->placeKey >> 12) == 0) {
            work->field_C[0].x = D_actor_421600_801511D4[scratch->zone][0];
            work->field_C[0].z = D_actor_421600_801511D4[scratch->zone][1];
            work->field_C[1].x = D_actor_421600_801511D4[scratch->zone][2];
            work->field_C[1].z = D_actor_421600_801511D4[scratch->zone][3];
        } else {
            work->field_C[0].x = D_actor_421600_801511D4[scratch->zone][4];
            work->field_C[0].z = D_actor_421600_801511D4[scratch->zone][5];
            work->field_C[1].x = D_actor_421600_801511D4[scratch->zone][6];
            work->field_C[1].z = D_actor_421600_801511D4[scratch->zone][7];
        }
        work->field_6 = 0;
    }
    func_actor_421600_80134604(arg0);
    facing3  = ((TmdObject*)arg0->extra)->coords;
    angle3   = ratan2((s32)scratch->target.vx, (s32)scratch->target.vz);
    delta3   = angle3 - ratan2((s32)-facing3->coord.m[2][0], (s32)facing3->coord.m[2][2]);
    wrapped3 = delta3;
    if (delta3 < 0) {
    wrapNegative3:
        if (wrapped3 < -0x800) {
            wrapped3 += 0x1000;
            goto wrapNegative3;
        }
    } else {
    wrapPositive3:
        if (wrapped3 >= 0x801) {
            wrapped3 -= 0x1000;
            goto wrapPositive3;
        }
    }
    work->field_840 = wrapped3;
    facing4         = ((TmdObject*)arg0->extra)->coords;
    angle4          = ratan2((s32)scratch->vec.vx, (s32)scratch->vec.vz);
    delta4          = angle4 - ratan2((s32)-facing4->coord.m[2][0], (s32)facing4->coord.m[2][2]);
    wrapped4        = delta4;
    if (delta4 < 0) {
    wrapNegative4:
        if (wrapped4 < -0x800) {
            wrapped4 += 0x1000;
            goto wrapNegative4;
        }
    } else {
    wrapPositive4:
        if (wrapped4 >= 0x801) {
            wrapped4 -= 0x1000;
            goto wrapPositive4;
        }
    }
    turnDelta         = wrapped4;
    scratch->original = (scratch->delta = (s16)turnDelta);
    delta             = scratch->delta;
    unsignedDelta     = (u16)scratch->delta;
    magnitude         = abs(scratch->delta);
    if (magnitude >= 0x601) {
        targetDelta     = (s16)work->field_840;
        targetMagnitude = abs(targetDelta);
        if ((targetMagnitude >= 0x101) && ((targetDelta * delta) < 0)) {
            adjustedDelta = unsignedDelta - 0x1000;
            if (delta < 0) {
                adjustedDelta = unsignedDelta + 0x1000;
            }
            scratch->delta = adjustedDelta;
        }
    }
    if (scratch->delta >= 0x21) {
        scratch->delta = 0x20;
    }
    if (scratch->delta < -0x20) {
        scratch->delta = -0x20;
    }
    work->field_83E = scratch->delta * 0x10;
    turnCoord       = ((TmdObject*)arg0->extra)->coords;
    yaw             = (u16)scratch->delta + ratan2((s32)-turnCoord->coord.m[2][0], (s32)turnCoord->coord.m[2][2]);
    scratch->delta  = yaw;
    Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->coords->coord, (s32)yaw, 1);
    record = &work->field_90C;
    if ((s16)work->field_82A == 0) {
        if (Actor421600_HasRecord10(arg0)) {
            actorMoveForward(((TmdObject*)arg0->extra)->coords, 20);
        } else {
            actorMoveForward(((TmdObject*)arg0->extra)->coords, 20);
        }
        record = &work->field_90C;
    }
    func_actor_421600_80132310(((TmdObject*)arg0->extra)->coords, record, 12, &scratch->vec);
    if (func_actor_421600_8013285C(((TmdObject*)arg0->extra)->coords, &work->field_B8C, 12) == 1) {
        originalMagnitude = abs(scratch->original);
        if (originalMagnitude < 0x20) {
            work->field_6 += 1;
        }
    }
    ((TmdObject*)arg0->extra)->coords->flg = 0;
    coord3                                 = ((TmdObject*)arg0->extra)->coords;
    scratch->target.vx                     = (s16)(Player_Status.coordMtx->t[0] - coord3->coord.t[0]);
    target2                                = &scratch->target;
    target2->vy                            = Player_Status.coordMtx->t[1] - coord3->coord.t[1];
    target2->vz                            = Player_Status.coordMtx->t[2] - coord3->coord.t[2];
    if ((s16)work->field_8 > (s16)work->field_EA2) {
        if ((s16)work->field_EAA <= 0) {

            if (actorOutsideRadius(&scratch->target, radius)) {
                if (!actorOutsideRadius(&scratch->target, 0x1F40) && (s16)work->field_8 >= 0x1C3) {
                    facing5  = ((TmdObject*)arg0->extra)->coords;
                    angle5   = ratan2((s32)scratch->vec.vx, (s32)scratch->vec.vz);
                    delta5   = angle5 - ratan2((s32)-facing5->coord.m[2][0], (s32)facing5->coord.m[2][2]);
                    wrapped5 = delta5;
                    if (delta5 < 0) {
                    wrapNegative5:
                        if (wrapped5 < -0x800) {
                            wrapped5 += 0x1000;
                            goto wrapNegative5;
                        }
                    } else {
                    wrapPositive5:
                        if (wrapped5 >= 0x801) {
                            wrapped5 -= 0x1000;
                            goto wrapPositive5;
                        }
                    }
                    finalDelta     = wrapped5;
                    scratch->delta = (s16)finalDelta;
                    finalDelta     = abs(finalDelta);
                    if (finalDelta < 0x300) {
                        goto changeState;
                    }
                }
            } else {
            changeState:
                work->field_0 = 0x1C;
            }
            playerX            = -((TmdObject*)(gameGetPtrSlot(3))->extra)->coords->coord.m[2][0];
            scratch->playerYaw = ratan2((s32)playerX, (s32)((TmdObject*)(gameGetPtrSlot(3))->extra)->coords->coord.m[2][2]);
            targetYaw          = ratan2((s32)scratch->target.vx, (s32)scratch->target.vz) + 0x800;
            wrappedYaw         = targetYaw;
            scratch->yaw       = targetYaw;
            if (targetYaw < 0) {
            wrapYawNegative:
                if (wrappedYaw < -0x800) {
                    wrappedYaw += 0x1000;
                    goto wrapYawNegative;
                }
            } else {
            wrapYawPositive:
                if (wrappedYaw >= 0x801) {
                    wrappedYaw -= 0x1000;
                    goto wrapYawPositive;
                }
            }
            finalYaw      = wrappedYaw;
            scratch->yaw  = (s16)finalYaw;
            yawDifference = finalYaw - scratch->playerYaw;
            if (yawDifference < 0) {
                yawDifference = -yawDifference;
            }
            if (yawDifference >= 0x601 || Gp_NodeSlotMask(&enemy->node) == 0) {
                work->field_0 = 0x1C;
            }
        } else {
            work->field_EAA -= 1;
        }
    }
    clampCoord = ((TmdObject*)arg0->extra)->coords;
    x          = clampCoord->coord.t[0];
    if (x > 0) {
        if (x >= 0xBEB) {
            clampCoord->coord.t[0] = 0xB54;
        } else {
            goto block_10;
        }
    } else if (x < -0xB22) {
        clampCoord->coord.t[0] = -0xA8C;
    } else {
    block_10:
        zClamp = clampCoord->coord.t[2];
        if (zClamp > 0) {
            if (zClamp >= 0xB23) {
                clampCoord->coord.t[2] = 0xA8C;
            }
        } else if (zClamp < -0xB22) {
            clampCoord->coord.t[2] = -0xA8C;
        }
    }
    SCRATCH_POP(Actor421600RouteScratch);
    ((TmdObject*)arg0->extra)->coords->flg = 0;
}

/// Death tick: the live-actor edge arms the model (dirty 0x80, clip 0x19C, the
/// 0xB6C node's 0x4000 flag down, the enemy's list node marked, the 0x83E /
/// 0x840 / 0x844 triple and `field_6` cleared) and spawns the 0x60030 effect on
/// the second coordinate. Frames 2, 3, 5, 7 and 8 then free the model buffers
/// and spawn one effect each -- 0xA0005 on coordinate 9, 12, 1 and 3 -- whose
/// model is tinted from the enemy's area record (`field_24` / `field_25`) and
/// re-streamed. Frame 0xA writes the 0x16 state. The counter stops at 0x400.
void func_actor_421600_8013C8E0(Task* arg0)
{
    Actor421600Work* work;
    GpEnemy*         ctx;
    TmdObject*       obj;
    SVECTOR          vec;
    GpAreaKey        key;
    GpAreaKey*       keyPtr;
    GpAreaKey*       sessionKey1;
    GpAreaKey*       sessionKey2;
    GpAreaKey*       sessionKey3;
    GpAreaKey*       sessionKey4;
    u32              raw1;
    u32              raw2;
    u32              raw3;
    u32              raw4;
    u32              index1;
    u32              index2;
    u32              index3;
    u32              index4;
    GpEffWork*       effect1;
    GpEffWork*       effect2;
    GpEffWork*       effect3;
    GpEffWork*       effect4;
    TmdObject*       model1;
    TmdObject*       model2;
    TmdObject*       model3;
    TmdObject*       model4;
    GpAreaRec*       rec1;
    GpAreaRec*       rec2;
    GpAreaRec*       rec3;
    GpAreaRec*       rec4;
    GpAreaPlace*     entry1;
    GpAreaPlace*     entry2;
    GpAreaPlace*     entry3;
    GpAreaPlace*     entry4;
    u8               areaByte0;
    u16              tick;

    work = arg0->work;
    ctx  = arg0->spawnArg2;
    obj  = arg0->extra;
    if (work->field_4 != 0) {
        obj->flags              = 0x80;
        work->field_8EC.radius  = 0x19C;
        work->field_B6C.flags   = (u16)(work->field_B6C.flags & 0xBFFF);
        ctx->node.state.b.flags = 1;
        work->field_844         = 0;
        work->field_840         = 0;
        work->field_83E         = 0;
        work->field_6           = 0;
        vec.vx                  = 0x64;
        vec.vz                  = 0;
        vec.vy                  = 0;
        Gp_SpawnEff(0x60030, ((TmdObject*)arg0->extra)->coords + 1, 0x10300, &vec);
    }
    if ((s16)work->field_6 == 2) {
        obj->flags |= 4;
        Tmd_FreeBuffers(obj);
    }
    if ((s16)work->field_6 == 3) {
        D_80114B78[0] = &D_actor_421600_80143EF4;
        vec.vz        = 0x64;
        vec.vy        = 0;
        vec.vx        = 0;
        effect1       = Gp_SpawnEff(0xA0005, ((TmdObject*)arg0->extra)->coords + 9, 0x200, &vec);
        if (effect1 != NULL) {
            sessionKey1 = (GpAreaKey*)&gGameSession->at4.loc;
            raw1        = ctx->placeKey;
            model1      = (TmdObject*)effect1->task->extra;
            key.stage   = sessionKey1->stage;
            key.area    = sessionKey1->area;
            key.room    = sessionKey1->room;
            areaByte0   = gGameSession->at4.loc.view;
            index1      = raw1 >> 12;
            SOFT_BARRIER();
            keyPtr = &key;
            TOUCH_REG(keyPtr);
            key.view = areaByte0;
            Gp_SyncAreaKeyIndex(keyPtr);
            rec1          = Gp_GetNestedAreaRec(&key);
            entry1        = (GpAreaPlace*)((index1 * 0x10) + (s32)rec1->field_0);
            model1->tpage = entry1->tpage;
            model1->clut  = entry1->clut;
            if (model1->buffer != NULL) {
                tmdProcessStream(model1);
                tmdProcessStream(model1);
            }
        }
    }
    if ((s16)work->field_6 == 5) {
        D_80114B78[0] = &D_actor_421600_801443C8;
        vec.vy        = 0;
        vec.vx        = 0;
        effect2       = Gp_SpawnEff(0xA0005, ((TmdObject*)arg0->extra)->coords + 12, 0x200, &vec);
        if (effect2 != NULL) {
            sessionKey2 = (GpAreaKey*)&gGameSession->at4.loc;
            raw2        = ctx->placeKey;
            model2      = (TmdObject*)effect2->task->extra;
            key.stage   = sessionKey2->stage;
            key.area    = sessionKey2->area;
            key.room    = sessionKey2->room;
            areaByte0   = gGameSession->at4.loc.view;
            index2      = raw2 >> 12;
            SOFT_BARRIER();
            keyPtr = &key;
            TOUCH_REG(keyPtr);
            key.view = areaByte0;
            Gp_SyncAreaKeyIndex(keyPtr);
            rec2          = Gp_GetNestedAreaRec(&key);
            entry2        = (GpAreaPlace*)((index2 * 0x10) + (s32)rec2->field_0);
            model2->tpage = entry2->tpage;
            model2->clut  = entry2->clut;
            if (model2->buffer != NULL) {
                tmdProcessStream(model2);
                tmdProcessStream(model2);
            }
        }
    }
    if ((s16)work->field_6 == 7) {
        D_80114B78[0] = &D_actor_421600_80145604;
        effect3       = Gp_SpawnEff(0xA0005, ((TmdObject*)arg0->extra)->coords + 1, 0x200, NULL);
        if (effect3 != NULL) {
            sessionKey3 = (GpAreaKey*)&gGameSession->at4.loc;
            raw3        = ctx->placeKey;
            model3      = (TmdObject*)effect3->task->extra;
            key.stage   = sessionKey3->stage;
            key.area    = sessionKey3->area;
            key.room    = sessionKey3->room;
            areaByte0   = gGameSession->at4.loc.view;
            index3      = raw3 >> 12;
            SOFT_BARRIER();
            keyPtr = &key;
            TOUCH_REG(keyPtr);
            key.view = areaByte0;
            Gp_SyncAreaKeyIndex(keyPtr);
            rec3          = Gp_GetNestedAreaRec(&key);
            entry3        = (GpAreaPlace*)((index3 * 0x10) + (s32)rec3->field_0);
            model3->tpage = entry3->tpage;
            model3->clut  = entry3->clut;
            if (model3->buffer != NULL) {
                tmdProcessStream(model3);
                tmdProcessStream(model3);
            }
        }
    }
    if ((s16)work->field_6 == 8) {
        D_80114B78[0] = &D_actor_421600_80145124;
        effect4       = Gp_SpawnEff(0xA0005, ((TmdObject*)arg0->extra)->coords + 3, 0x200, NULL);
        if (effect4 != NULL) {
            sessionKey4 = (GpAreaKey*)&gGameSession->at4.loc;
            raw4        = ctx->placeKey;
            model4      = (TmdObject*)effect4->task->extra;
            key.stage   = sessionKey4->stage;
            key.area    = sessionKey4->area;
            key.room    = sessionKey4->room;
            areaByte0   = gGameSession->at4.loc.view;
            index4      = raw4 >> 12;
            SOFT_BARRIER();
            keyPtr = &key;
            TOUCH_REG(keyPtr);
            key.view = areaByte0;
            Gp_SyncAreaKeyIndex(keyPtr);
            rec4          = Gp_GetNestedAreaRec(&key);
            entry4        = (GpAreaPlace*)((index4 * 0x10) + (s32)rec4->field_0);
            model4->tpage = entry4->tpage;
            model4->clut  = entry4->clut;
            if (model4->buffer != NULL) {
                tmdProcessStream(model4);
                tmdProcessStream(model4);
            }
        }
    }
    if ((s16)work->field_6 == 0xA) {
        work->field_0 = 0x16;
    }
    if ((s16)work->field_6 < 0x400) {
        tick          = work->field_6 + 1;
        work->field_6 = tick;
    }
}

void func_actor_421600_8013CD3C(Task* arg0)
{
    TmdObject*            obj;
    GpEnemy*              ctx;
    Actor421600Work*      work;
    GsCOORDINATE2*        coord;
    GsCOORDINATE2*        coord2;
    GsCOORDINATE2*        targetCoord;
    GsCOORDINATE2*        facing;
    GsCOORDINATE2*        facing2;
    ActorTurnStepScratch* head;
    ActorTurnStepScratch* scratch;
    s16                   yaw;
    s16                   delta;
    s16                   z;
    s16                   steps;
    s16                   wrapped;
    s32                   angle;
    s32                   firstDelta;

    head    = SCRATCH_HEAD(ActorTurnStepScratch);
    scratch = (SCRATCH_HEAD(ActorTurnStepScratch) = head - 1);
    work    = arg0->work;
    ctx     = arg0->spawnArg2;
    if (work->field_4 != 0) {
        obj                     = arg0->extra;
        obj->flags              = 0;
        work->field_8EC.radius  = 0x19C;
        work->field_B6C.flags  |= 0x4000;
        ctx->node.state.b.flags = 0;
        work->field_828         = 1;
        work->field_82E         = 0x14;
        work->field_832         = 0x10;
        work->field_6           = 0;
    }
    work->field_6 += 1;
    func_actor_421600_80134604(arg0);
    targetCoord     = ((TmdObject*)arg0->extra)->coords;
    head[-1].vec.vx = (s16)(Player_Status.coordMtx->t[0] - targetCoord->coord.t[0]);
    scratch->vec.vy = Player_Status.coordMtx->t[1] - targetCoord->coord.t[1];
    z               = Player_Status.coordMtx->t[2] - targetCoord->coord.t[2];
    scratch->vec.vz = z;
    facing          = ((TmdObject*)arg0->extra)->coords;
    angle           = ratan2((s32)head[-1].vec.vx, (s32)z);
    delta           = angle - ratan2((s32)-facing->coord.m[2][0], (s32)facing->coord.m[2][2]);
    wrapped         = delta;
    if (delta < 0) {
    wrapNegative:
        if (wrapped < -0x800) {
            wrapped += 0x1000;
            goto wrapNegative;
        }
    } else {
    wrapPositive:
        if (wrapped >= 0x801) {
            wrapped -= 0x1000;
            goto wrapPositive;
        }
    }
    firstDelta      = wrapped;
    scratch->delta  = (s16)firstDelta;
    work->field_840 = (u16)firstDelta;
    if (scratch->delta < 0) {
        if (abs(scratch->delta) >= 0x401) {
            work->field_840 = firstDelta + 0x800;
            scratch->delta += 0x800;
        }
    }
    if (abs(scratch->delta) < 0x80) {
        work->field_0 = 0x1C;
    }
    steps          = 0x1E - work->field_6;
    scratch->steps = steps;
    if (steps == 0) {
        scratch->steps = 1;
    }
    facing2      = ((TmdObject*)arg0->extra)->coords;
    yaw          = ((s16)scratch->delta / (s16)scratch->steps) + ratan2((s32)-facing2->coord.m[2][0], (s32)facing2->coord.m[2][2]);
    scratch->yaw = yaw;
    Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->coords->coord, (s32)yaw, 1);
    Gfx_MatrixCol2(&((TmdObject*)arg0->extra)->coords->coord, &scratch->vec);
    VectorNormalSS(&scratch->vec, &scratch->vec);
    gte_lddp(-0x1A);
    gte_ldsv(&scratch->vec);
    gte_gpf12();
    gte_stsv(&scratch->vec);
    coord               = ((TmdObject*)arg0->extra)->coords;
    coord->coord.t[0]  += scratch->vec.vx;
    coord2              = ((TmdObject*)arg0->extra)->coords;
    coord2->coord.t[2] += scratch->vec.vz;
    actorMoveForward(((TmdObject*)arg0->extra)->coords, -8);
    func_actor_421600_8013285C(((TmdObject*)arg0->extra)->coords, &work->field_B8C, 0xC);
    ((TmdObject*)arg0->extra)->coords->flg = 0;
    if (abs(scratch->delta) < 0x20) {
        work->field_0 = 0x1C;
    }
    if (work->field_68 & 0x100) {
        work->field_0 = 0x1C;
    }
    SCRATCH_POP(ActorTurnStepScratch);
}

void func_actor_421600_8013D1DC(Task* arg0)
{
    TmdObject*            obj;
    GpEnemy*              ctx;
    Actor421600Work*      work;
    GsCOORDINATE2*        coord;
    GsCOORDINATE2*        coord2;
    GsCOORDINATE2*        targetCoord;
    GsCOORDINATE2*        facing;
    GsCOORDINATE2*        facing2;
    ActorTurnStepScratch* head;
    ActorTurnStepScratch* scratch;
    s16                   yaw;
    s16                   delta;
    s16                   z;
    s16                   steps;
    s16                   wrapped;
    s32                   angle;
    s32                   firstDelta;

    head    = SCRATCH_HEAD(ActorTurnStepScratch);
    scratch = (SCRATCH_HEAD(ActorTurnStepScratch) = head - 1);
    work    = arg0->work;
    ctx     = arg0->spawnArg2;
    if (work->field_4 != 0) {
        obj                     = arg0->extra;
        obj->flags              = 0;
        work->field_8EC.radius  = 0x19C;
        work->field_B6C.flags  |= 0x4000;
        ctx->node.state.b.flags = 0;
        work->field_828         = 1;
        work->field_82E         = 0x15;
        work->field_832         = 0x10;
        work->field_6           = 0;
    }
    work->field_6 += 1;
    func_actor_421600_80134604(arg0);
    targetCoord     = ((TmdObject*)arg0->extra)->coords;
    head[-1].vec.vx = (s16)(Player_Status.coordMtx->t[0] - targetCoord->coord.t[0]);
    scratch->vec.vy = Player_Status.coordMtx->t[1] - targetCoord->coord.t[1];
    z               = Player_Status.coordMtx->t[2] - targetCoord->coord.t[2];
    scratch->vec.vz = z;
    facing          = ((TmdObject*)arg0->extra)->coords;
    angle           = ratan2((s32)head[-1].vec.vx, (s32)z);
    delta           = angle - ratan2((s32)-facing->coord.m[2][0], (s32)facing->coord.m[2][2]);
    wrapped         = delta;
    if (delta < 0) {
    wrapNegative:
        if (wrapped < -0x800) {
            wrapped += 0x1000;
            goto wrapNegative;
        }
    } else {
    wrapPositive:
        if (wrapped >= 0x801) {
            wrapped -= 0x1000;
            goto wrapPositive;
        }
    }
    firstDelta      = wrapped;
    scratch->delta  = (s16)firstDelta;
    work->field_840 = (u16)firstDelta;
    if (scratch->delta > 0) {
        if (abs(scratch->delta) >= 0x401) {
            work->field_840 = firstDelta - 0x800;
            scratch->delta -= 0x800;
        }
    }
    steps          = 0x1E - work->field_6;
    scratch->steps = steps;
    if (steps == 0) {
        scratch->steps = 1;
    }
    facing2      = ((TmdObject*)arg0->extra)->coords;
    yaw          = ((s16)scratch->delta / (s16)scratch->steps) + ratan2((s32)-facing2->coord.m[2][0], (s32)facing2->coord.m[2][2]);
    scratch->yaw = yaw;
    Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->coords->coord, (s32)yaw, 1);
    Gfx_MatrixCol2(&((TmdObject*)arg0->extra)->coords->coord, &scratch->vec);
    VectorNormalSS(&scratch->vec, &scratch->vec);
    gte_lddp(0x1A);
    gte_ldsv(&scratch->vec);
    gte_gpf12();
    gte_stsv(&scratch->vec);
    coord               = ((TmdObject*)arg0->extra)->coords;
    coord->coord.t[0]  += scratch->vec.vx;
    coord2              = ((TmdObject*)arg0->extra)->coords;
    coord2->coord.t[2] += scratch->vec.vz;
    actorMoveForward(((TmdObject*)arg0->extra)->coords, -8);
    func_actor_421600_8013285C(((TmdObject*)arg0->extra)->coords, &work->field_B8C, 0xC);
    ((TmdObject*)arg0->extra)->coords->flg = 0;
    if (abs(scratch->delta) < 0x20) {
        work->field_0 = 0x1C;
    }
    if (work->field_68 & 0x100) {
        work->field_0 = 0x1C;
    }
    SCRATCH_POP(ActorTurnStepScratch);
}

const Actor421600StateTable D_actor_421600_80131EFC = { { func_actor_421600_8013E858,
                                                          func_actor_421600_80135F6C,
                                                          func_actor_421600_80136138,
                                                          func_actor_421600_8013A554,
                                                          func_actor_421600_8013E8AC,
                                                          func_actor_421600_8013B00C,
                                                          func_actor_421600_8013B4C4,
                                                          func_actor_421600_8013B8E0,
                                                          func_actor_421600_8013C8E0,
                                                          func_actor_421600_8013CD3C,
                                                          func_actor_421600_8013D1DC,
                                                          func_actor_421600_8013ED24,
                                                          func_actor_421600_8013EE0C,
                                                          NULL,
                                                          NULL,
                                                          NULL,
                                                          NULL,
                                                          func_actor_421600_8013A404,
                                                          NULL,
                                                          NULL,
                                                          func_actor_421600_8013EC28,
                                                          func_actor_421600_801366F4,
                                                          func_actor_421600_801369A0,
                                                          NULL,
                                                          func_actor_421600_80136C88,
                                                          NULL,
                                                          NULL,
                                                          NULL,
                                                          func_actor_421600_801373D4,
                                                          func_actor_421600_8013E9D8,
                                                          func_actor_421600_8013848C,
                                                          func_actor_421600_80138D24,
                                                          func_actor_421600_8013903C,
                                                          func_actor_421600_801392A8,
                                                          func_actor_421600_8013EAAC,
                                                          func_actor_421600_8013947C,
                                                          func_actor_421600_8013EB7C,
                                                          func_actor_421600_80138750,
                                                          func_actor_421600_80139718,
                                                          func_actor_421600_8013BA70 } };
void                        func_actor_421600_8013D658(GpEnemy* enemy, Task* actor)
{
    PlayerStatus*         config;
    VECTOR                pos;
    Actor421600StateTable states;

    s16                       view;
    s16                       height;
    s16                       state;
    s16                       activeState;
    s16                       finalState;
    GsCOORDINATE2*            playerCoord;
    GsCOORDINATE2*            actorCoord;
    GsCOORDINATE2*            actorCoord2;
    GsCOORDINATE2*            playerCoord2;
    s32                       action;
    s32                       x;
    s32                       z;
    s32                       nextAction;
    s32                       result;
    u8                        kind;
    s32                       contactKind;
    void**                    scratchHead;
    Actor421600AnimCommand*   nextCommand;
    Actor421600Work*          actorWork;
    Actor421600Work*          actorWork2;
    Actor421600Work*          work;
    Task*                     player;
    Actor421600AnimCommand*   command;
    Task*                     slot;
    Task*                     slot2;
    Actor421600UpdateScratch* scratch;
    GsCOORDINATE2*            clampCoord;
    void*                     message;
    void*                     nextMessage;

    work                                    = actor->work;
    player                                  = gameGetPtrSlot(3);
    config                                  = &Player_Status;
    view                                    = Gp_GetViewIndex() & 0xFF;
    states                                  = D_actor_421600_80131EFC;
    ((TmdObject*)actor->extra)->coords->flg = 0;
    Gp_UpdateCoord(((TmdObject*)actor->extra)->coords);
    pos.vx = ((TmdObject*)actor->extra)->coords->workm.t[0];
    pos.vy = ((TmdObject*)actor->extra)->coords->workm.t[1];
    pos.vz = ((TmdObject*)actor->extra)->coords->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);
    switch (Gp_StateF0.field_4) {
        case 0:
            if (work->field_0 != 0x15 && work->field_0 != 0 && work->field_0 != 0x16 && work->field_0 != 7 && work->field_0 != 8) {
                height = ((TmdObject*)actor->extra)->coords->coord.t[1];
                func_actor_421600_80132EC0(actor, 1, 3, 0x12C, (s32)height, 0xFF);
                func_actor_421600_80132EC0(actor, 3, 4, 0xC8, (s32)height, 0xFF);
                func_actor_421600_80132EC0(actor, 1, 0xB, 0xFA, (s32)height, 0xFF);
                if ((Gp_GetViewIndex() & 0xFF) == 0x13) {
                    ((TmdObject*)actor->extra)->flags = 0x80;
                } else {
                    ((TmdObject*)actor->extra)->flags = 0;
                }
            }
            break;
        case 1:
            if (work->field_0 != 0x15 && work->field_0 != 0 && work->field_0 != 0x16 && work->field_0 != 7 && work->field_0 != 8) {
                height = ((TmdObject*)actor->extra)->coords->coord.t[1];
                func_actor_421600_80132EC0(actor, 1, 3, 0x12C, (s32)height, 0xFF);
                func_actor_421600_80132EC0(actor, 3, 4, 0xC8, (s32)height, 0xFF);
                func_actor_421600_80132EC0(actor, 1, 0xB, 0xFA, (s32)height, 0xFF);
                if ((Gp_GetViewIndex() & 0xFF) == 0x13) {
                    ((TmdObject*)actor->extra)->flags = 0x80;
                } else {
                    ((TmdObject*)actor->extra)->flags = 0;
                }
            }
            Gp_ClearRec18Occupied(&work->field_B8C);
            Gp_ClearRec18Occupied(&work->field_90C);
            Gp_ClearRec18Occupied(&work->field_A4C);
            Gp_ClearRec18Occupied(work->field_CE4);
            return;
        case 2:
            ((TmdObject*)actor->extra)->flags = 0x80;
            Gp_ClearRec18Occupied(&work->field_B8C);
            Gp_ClearRec18Occupied(&work->field_90C);
            Gp_ClearRec18Occupied(&work->field_A4C);
            Gp_ClearRec18Occupied(work->field_CE4);
            return;
    }
    scratchHead = SCRATCH_HEAD_ADDR;
    scratch     = Actor421600_AllocUpdateScratch((Actor421600UpdateScratch**)scratchHead);
    if (work->field_E64 > 0) {
        work->field_E64 = (s16)((u16)work->field_E64 - 1);
    } else if (config->hp > 0) {
        func_actor_421600_801354D8(actor);
    }
    kind = work->field_E90.bytes[2];
    if ((kind == 2) && ((state = work->field_0, (state == 0x26)) || (state == kind))) {
        work->field_0 = 0x27;
    }
    if (work->field_2 != work->field_0) {
        work->field_4 = 1;
    } else {
        work->field_4 = 0;
    }
    work->field_2 = (s16)(u16)work->field_0;
    scratch->zone = Actor421600_Zone(((TmdObject*)actor->extra)->coords);
    if (work->field_E9C == 1) {
        activeState = work->field_0;
        if ((activeState != 0x15) && (activeState != 0) && (activeState != 8)) {
            actorWork = actor->work;
            slot      = gameGetPtrSlot(3);
            if ((slot != NULL) && (actorWork->field_8B4 == 7)) {
                playerCoord = ((TmdObject*)slot->extra)->coords;
                actorCoord  = ((TmdObject*)actor->extra)->coords;
                if (abs(playerCoord->coord.t[1] - actorCoord->coord.t[1]) >= 0x12D) {
                    playerCoord->coord.t[1]                = actorCoord->coord.t[1];
                    ((TmdObject*)slot->extra)->coords->flg = 0;
                }
            }
        }
        actorWork2 = actor->work;
        slot2      = gameGetPtrSlot(3);
        if ((slot2 != NULL) && (actorWork2->field_8B4 == 7)) {
            playerCoord2 = ((TmdObject*)slot2->extra)->coords;
            actorCoord2  = ((TmdObject*)actor->extra)->coords;
            if (abs(playerCoord2->coord.t[1] - actorCoord2->coord.t[1]) >= 0x12D) {
                playerCoord2->coord.t[1]                = actorCoord2->coord.t[1];
                ((TmdObject*)slot2->extra)->coords->flg = 0;
            }
        }
        contactKind              = work->field_E90.bytes[2];
        work->field_E90.bytes[3] = (u8)(work->field_E90.bytes[3] + 1);
        if (contactKind == 1) {
            if (D_801876AA == (D_801876A8 + 1)) {
                Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F1, NULL, 0);
                work->field_E9C = 0;
            }
            if ((work->field_E90.bytes[2] == contactKind) && ((s32)D_801876AA < (D_801876A8 + 3))) {
                work->field_8A4 = 0;
                work->field_8A8 = 0;
                work->field_8AC = 0;
            }
        }
        action = work->field_E80;
        switch (action) {
            case 4:
                break;
            case 1:
                if (Gp_DispatchMsg(player, 0x3FE, &work->field_8A4, 0) == 1) {
                    work->field_8A4 = 0;
                    work->field_8A8 = 0;
                    work->field_8AC = 0;
                }
                if (((u8)work->field_E90.bytes[3] >= 0xFU) && (work->field_8B4 == 7)) {
                    work->field_8A4 = (s32)((s32)work->field_8A4 >> 1);
                    work->field_8A8 = (s32)((s32)work->field_8A8 >> 1);
                    work->field_8AC = (s32)((s32)work->field_8AC >> 1);
                }
                break;
            case 2:
                command = (Actor421600AnimCommand*)work->field_E7C;
                if (command == &D_actor_421600_801510A4) {
                    if ((config->hp > 0) && ((u8)work->field_E90.bytes[3] >= 0x17U)) {
                        message           = &work->field_E7C;
                        command->field_10 = (s32)(Gp_PlayerAnimBlkTbl[Gp_WeaponIdBase[D_8007218A - 1] + D_80073BA9])->field_1C;
                        work->field_E80   = 4;
                        work->field_E84   = 1;
                        work->field_E88   = 3;
                        Gp_DispatchMsg(player, 0x3FF, message, 0);
                        work->field_E90.bytes[3] = 0U;
                    }
                } else if ((config->hp > 0) && ((u8)work->field_E90.bytes[3] >= 0x22U)) {
                    message                       = &work->field_E7C;
                    D_actor_421600_801510A0.value = (Gp_PlayerAnimBlkTbl[Gp_WeaponIdBase[D_8007218A - 1] + D_80073BA9])->field_1C;

                    work->field_E80 = 4;
                    work->field_E84 = 1;
                    work->field_E88 = 3;
                    Gp_DispatchMsg(player, 0x3FF, message, 0);
                    work->field_E90.bytes[3] = 0U;
                }
                break;
            case 3:
                if (((u8)work->field_E90.bytes[3] < 6U) && (config->hp > 0)) {
                    result = Gp_DispatchMsg(player, 0x3FE, &work->field_8A4, 0);
                    if (result == 1) {
                        work->field_8A4 = 0;
                        work->field_8A8 = 0;
                        work->field_8AC = 0;
                        work->field_8B6 = result;
                    }
                }
                break;
            case 5:
                if ((config->hp > 0) && ((u8)work->field_E90.bytes[3] >= 7U)) {
                    Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F1, (void*)2, 0);
                    work->field_E9C = 0;
                }
                break;
        }
        if (Gp_DispatchMsg(gameGetPtrSlot(3), 0x3ED, NULL, 0) == 0) {
            nextAction = work->field_E80;
            switch (nextAction) {
                case 1:
                    if (((((*(u32*)&gGameSession->at4.loc) & 0xFFFF0000) != 0x04010000) || (work->field_8B4 != 0x38)) && (config->hp > 0)) {
                        nextMessage     = &work->field_E7C;
                        work->field_E84 = 0;
                        work->field_E88 = 0;
                        work->field_E80 = 2;

                        Gp_DispatchMsg(player, 0x3FF, nextMessage, 0);
                        work->field_E90.bytes[3] = 0U;
                    }
                    break;
                case 3:
                    if (config->hp > 0) {
                        work->field_E84 = 1;
                        work->field_E88 = 6;
                        work->field_E80 = 5;
                        nextCommand     = (Actor421600AnimCommand*)work->field_E7C;
                        if (nextCommand == &D_actor_421600_801510A4) {
                            nextCommand->field_14 = (s32)(Gp_PlayerAnimBlkTbl[Gp_WeaponIdBase[D_8007218A - 1] + D_80073BA9])->field_24;
                        } else {
                            Actor421600FallbackEnd.value = (Gp_PlayerAnimBlkTbl[Gp_WeaponIdBase[D_8007218A - 1] + D_80073BA9])->field_24;
                        }
                        nextMessage = &work->field_E7C;
                        Gp_DispatchMsg(player, 0x3FF, nextMessage, 0);
                        work->field_E90.bytes[3] = 0U;
                    }
                    break;
                case 4:
                case 7:
                    if (config->hp > 0) {
                        Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F1, (void*)2, 0);
                        work->field_E9C = 0;
                    }
                    break;
            }
        }
    }
    states.fn[work->field_0](actor);
    if (work->field_0 == 1 && work->field_E90.bytes[2] == 0) {
        if ((Gp_GetViewIndex() & 0xFF) == 5)
            work->field_0 = 2;
    }
    if (Actor421600_HasPlayerContact(&work->field_90C)) {
        switch (view) {
            case 9:
            case 0x12:
                clampCoord = ((TmdObject*)player->extra)->coords;
                x          = clampCoord->coord.t[0];
                if (x > 0) {
                    if (x >= 0xBEB) {
                        clampCoord->coord.t[0] = 0xB54;
                    } else {
                        goto clampZ;
                    }
                } else if (x < -0xB22) {
                    clampCoord->coord.t[0] = -0xA8C;
                } else {
                clampZ:
                    z = clampCoord->coord.t[2];
                    if (z > 0) {
                        if (z >= 0xB23) {
                            clampCoord->coord.t[2] = 0xA8C;
                        }
                    } else if (z < -0xB22) {
                        clampCoord->coord.t[2] = -0xA8C;
                    }
                }

                break;
            case 0xE:
                break;
            default:
                func_actor_421600_80133334(((TmdObject*)player->extra)->coords);
                break;
        }
    }
    finalState = work->field_0;
    if ((finalState != 0x15) && (finalState != 0) && (finalState != 5) && (finalState != 0x16) && (finalState != 8)) {
        work->field_8EC.flags |= 0x8000;
        work->field_A2C.flags |= 0x8000;
    } else {
        work->field_8EC.flags &= 0x7FFF;
        work->field_A2C.flags &= 0x7FFF;
    }
    Gp_ClearRec18Occupied(&work->field_B8C);
    Gp_ClearRec18Occupied(&work->field_90C);
    Gp_ClearRec18Occupied(&work->field_A4C);
    Gp_ClearRec18Occupied(work->field_CE4);
    scratch->pos.vx = 0U;
    scratch->pos.vy = 0;
    scratch->pos.vz = 0;
    actorTransformToView(((TmdObject*)actor->extra)->coords + 2, &scratch->pos);
    enemy->bodyPos.vx = (s32)(s16)scratch->pos.vx;
    enemy->bodyPos.vy = (s32)scratch->pos.vy;
    enemy->bodyPos.vz = (s32)scratch->pos.vz;
    enemy->coord      = &gGfxViewCoord;
    SCRATCH_POP_BYTES(0x1C);
    func_actor_421600_80133444(((TmdObject*)actor->extra)->coords);
}

void func_actor_421600_8013E424(void)
{
}

/// The enemy task's state handlers - spawn, per-frame tick and teardown - run by
/// `func_actor_421600_8013EEC8`.
const GpEnemyTaskFuncTable3 D_actor_421600_80131FB0 = {
    {
        func_actor_421600_80134AD4,
        func_actor_421600_8013D658,
        Gp_DestroyEnemy,
    },
};

/// Handler for message 0x7D5: set the model's display mode. Modes 0 and 1
/// reset the model flags (0 with 0x80 set) and reallocate its buffers; 2 and 3
/// set flag 4, 3 clearing the others first. The work block's state is reset to
/// 0, or to 0x18 for mode 1.
s32 func_actor_421600_8013E42C(Task* task, s32 arg1, s32 mode)
{
    TmdObject*       obj;
    Actor421600Work* work;

    obj  = (TmdObject*)task->extra;
    work = (Actor421600Work*)task->work;
    switch (mode) {
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

/// Handler for message 0x7D6: returns 1 while the enemy still has hit points
/// or its model is shown (flag 0x80 clear), 0 once it is dead and hidden.
s32 func_actor_421600_8013E4EC(Task* task)
{
    if (((GpEnemy*)task->spawnArg2)->hp > 0) {
        goto return_one;
    }

    if ((((TmdObject*)task->extra)->flags & 0x80) != 0) {
        return 0;
    }

return_one:
    return 1;
}

/// Handler for message 0x7D4: write `placement` onto the actor's root
/// coordinate (translation, then yaw / pitch / roll), then read the resulting
/// yaw back out of the matrix's third row and cache it in the work block.
///
/// The `TmdObject` is re-read from `Task::extra` for every access because the
/// stores and the `Gfx_RotMatrix*` calls in between may alias it.
s32 func_actor_421600_8013E52C(Task* task, s32 arg1, GpXformArg* placement)
{
    Actor421600Work* work;

    work                                          = (Actor421600Work*)task->work;
    ((TmdObject*)task->extra)->coords->coord.t[0] = placement->pos.vx;
    ((TmdObject*)task->extra)->coords->coord.t[1] = placement->pos.vy;
    ((TmdObject*)task->extra)->coords->coord.t[2] = placement->pos.vz;
    Gfx_RotMatrixY(&((TmdObject*)task->extra)->coords->coord, placement->rot.vy, 1);
    Gfx_RotMatrixX(&((TmdObject*)task->extra)->coords->coord, placement->rot.vx, 0);
    Gfx_RotMatrixZ(&((TmdObject*)task->extra)->coords->coord, placement->rot.vz, 0);
    ((TmdObject*)task->extra)->coords->flg = 0;
    work->field_16                         = ratan2(-((TmdObject*)task->extra)->coords->coord.m[2][0],
                                                    ((TmdObject*)task->extra)->coords->coord.m[2][2]);
    return 1;
}

/// Handler for message 0x7D3: latch the requested animation id into
/// `field_82E` and restart the state machine at state 1.
s32 func_actor_421600_8013E62C(Task* task, s32 arg1, GpAnimArg* msg, s32 arg3)
{
    Actor421600Work* work = (Actor421600Work*)task->work;

    work->field_82E = msg->field_4;
    work->field_0   = 1;
    work->field_2   = -1;
    return 0;
}

s32 func_actor_421600_8013E654(Task* task)
{
    Actor421600Work* work = (Actor421600Work*)task->work;

    work->field_EAC = 0x1E;
    return 1;
}

/// `Task::exitCallback` teardown: kill the two helper tasks, unlink the three
/// display nodes, clear the enemy's `recs`, then `Gp_DestroyEnemy`.
void func_actor_421600_8013E668(Task* task)
{
    Actor421600Work* work;
    GpEnemy*         enemy;

    work  = (Actor421600Work*)task->work;
    enemy = (GpEnemy*)task->spawnArg2;
    if (work != NULL) {
        if (work->field_E94 != NULL) {
            taskKill(work->field_E94);
        }
        if (work->field_E98 != NULL) {
            taskKill(work->field_E98);
        }
        Gp_UnlinkObj(&work->field_8EC);
        Gp_UnlinkObj(&work->field_A2C);
        Gp_UnlinkObj(&work->field_B6C);
        enemy->recs = 0;
    }
    Gp_DestroyEnemy(enemy, task);
}

/// Spawn effect 0x60054 on model part `part` when the room's effect mode is 2.
/// Only parts 0, 1, 7, 9, 14 and 17 emit; each carries its own vertical offset
/// in the effect's position argument. `flags` goes to `Gp_SpawnEff` with the
/// top bit set.
void func_actor_421600_8013E700(Task* arg0, s16 part, s16 flags)
{
    SVECTOR sp10;
    s32     spawn;

    switch (part) {
        case 0:
        case 1:
            spawn   = 1;
            sp10.vz = 0;
            sp10.vx = 0;
            sp10.vy = 0;
            break;
        case 9:
            spawn   = 1;
            sp10.vz = 0;
            sp10.vx = 0;
            sp10.vy = 0x2BC;
            break;
        case 7:
            spawn   = 1;
            sp10.vz = 0;
            sp10.vx = 0;
            sp10.vy = 0x2BC;
            break;
        case 14:
        case 17:
            spawn   = 1;
            sp10.vz = 0;
            sp10.vx = 0;
            sp10.vy = 0x258;
            break;
        default:
            spawn = 0;
            break;
    }

    if (Gp_State1C->roomEffectMode == 2 && spawn == 1) {
        Gp_SpawnEff(0x60054, &((TmdObject*)arg0->extra)->coords[part], flags | 0x80000000, &sp10);
    }
}

/// Copy the `vx`/`vy`/`vz` of entry `arg1` of the pose table into `arg0`.
void func_actor_421600_8013E7F8(SVECTOR* arg0, s32 arg1)
{
    arg0->vx = D_actor_421600_80151158[(s16)arg1].vx;
    arg0->vy = D_actor_421600_80151158[(s16)arg1].vy;
    arg0->vz = D_actor_421600_80151158[(s16)arg1].vz;
}

s8 func_actor_421600_8013E830(s32 arg0, s32 arg1)
{
    s8* p;
    s32 a;
    s32 b;

    p = D_actor_421600_801511D0;
    a = arg0 > 0;
    b = arg1 < 1;
    return p[a + (b << 1)];
}

void func_actor_421600_8013E858(Task* arg0)
{
    TmdObject*       obj;
    Actor421600Work* work;
    GpEnemy*         enemy;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                       = arg0->extra;
        enemy                     = arg0->spawnArg2;
        enemy->node.state.b.flags = 1;
        obj->flags               |= 0x80;
        work->field_B6C.flags    &= 0xBFFF;
        enemy->hp                 = 0;
    }
}

void func_actor_421600_8013E8AC(Task* arg0)
{
    Actor421600Work* work;
    GpEnemy*         enemy;
    TmdObject*       obj;
    s32              value;
    u32              magnitude;

    work  = arg0->work;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        obj                       = arg0->extra;
        enemy->node.state.b.flags = 0;
        obj->flags                = 0;
        Tmd_AllocBuffers(obj);
        work->field_82E        = 0x18;
        work->field_828        = 2;
        work->field_832        = 0x10;
        work->field_B6C.flags |= 0x4000;
        do {
            func_actor_421600_80134604(arg0);
        } while ((work->field_5A & 0x3FF) != 0xC);
        work->field_832 = 0x20;
        return;
    }
    ((TmdObject*)arg0->extra)->coords->flg = 0;
    value                                  = (s16)work->field_832 / 2;
    work->field_832                        = (u16)value;
    magnitude                              = 0x10U;
    if (value == 1) {
        work->field_832 = -magnitude;
    }
    if ((s16)work->field_832 == -1) {
        work->field_832 = 0x10;
    }
    func_actor_421600_80134604(arg0);
    if (Gp_TickObjFlag2(enemy) == 1) {
        enemy->reactionFlags &= 0xFD;
        work->field_0         = 0x24;
    }
}

/// State handler: on the live-actor edge (`field_4` set) show the model, start
/// animation 4 and step it once; afterwards step the animation and, once its
/// flag 0x100 is up, go to state 5 when `field_E90` masks to 0x11402, else 2.
void func_actor_421600_8013E9D8(Task* arg0)
{
    TmdObject*       obj;
    Actor421600Work* work;
    s32              state;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                             = arg0->extra;
        ((GpEnemy*)arg0->spawnArg2)->node.state.b.flags = 0;
        obj->flags                                      = 0;
        Tmd_AllocBuffers(obj);
        work->field_8EC.radius = 0x19C;
        work->field_828        = 1;
        work->field_832        = 0x10;
        work->field_82A        = 0;
        work->field_82E        = 4;
        work->field_B6C.flags |= 0x4000;
        func_actor_421600_80134604(arg0);
        return;
    }
    func_actor_421600_80134604(arg0);
    if (work->field_68 & 0x100) {
        state = work->field_E90.word & 0xFFFFFF;
        if (state == 0x11402) {
            state = 5;
        } else {
            state = 2;
        }
        work->field_0 = state;
    }
}

void func_actor_421600_8013EAAC(Task* arg0)
{
    TmdObject*       obj;
    Actor421600Work* work;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                             = arg0->extra;
        ((GpEnemy*)arg0->spawnArg2)->node.state.b.flags = 0;
        obj->flags                                      = 0;
        Tmd_AllocBuffers(obj);
        work->field_8EC.radius = 0x19C;
        work->field_828        = 1;
        work->field_832        = 0x10;
        work->field_82A        = 0;
        work->field_82E        = 8;
        work->field_B6C.flags |= 0x4000;
        func_actor_421600_80134604(arg0);
    }
    func_actor_421600_8013285C(((TmdObject*)arg0->extra)->coords, &work->field_B8C, 0xC);
    ((TmdObject*)arg0->extra)->coords->flg = 0;
    func_actor_421600_80134604(arg0);
    if (work->field_68 & 0x100) {
        work->field_0 = 0x1C;
    }
}

void func_actor_421600_8013EB7C(Task* arg0)
{
    TmdObject*       obj;
    Actor421600Work* work;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                             = arg0->extra;
        ((GpEnemy*)arg0->spawnArg2)->node.state.b.flags = 0;
        obj->flags                                      = 0;
        Tmd_AllocBuffers(obj);
        work->field_8EC.radius = 0x19C;
        work->field_828        = 1;
        work->field_832        = 0x10;
        work->field_82A        = 0;
        work->field_82E        = 0xC;
        work->field_B6C.flags |= 0x4000;
        func_actor_421600_80134604(arg0);
    }
    func_actor_421600_80134604(arg0);
    if (work->field_68 & 0x100) {
        work->field_0 = 2;
    }
}

void func_actor_421600_8013EC28(Task* arg0)
{
    Actor421600Work* work;
    GpEnemy*         enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        ((TmdObject*)arg0->extra)->flags = 0;
        work->field_8EC.radius           = 0x19C;
        work->field_B6C.flags           |= 0x4000;
        enemy->node.state.b.flags        = 0;
        work->field_828                  = 1;
        work->field_82E                  = 0xA;
        work->field_832                  = 0x10;
        work->field_844                  = 0;
        work->field_840                  = 0;
        work->field_83E                  = 0;
        if (enemy->hp <= 0) {
            Gp_SetStateF0Byte3(1);
        }
    }
    func_actor_421600_80134604(arg0);
    if (work->field_68 & 0x100) {
        if (work->field_82E == 0xA) {
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

void func_actor_421600_8013ED24(Task* arg0)
{
    Actor421600Work* work;
    GpEnemy*         enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        ((TmdObject*)arg0->extra)->flags = 0;
        work->field_8EC.radius           = 0x19C;
        work->field_B6C.flags           |= 0x4000;
        enemy->node.state.b.flags        = 0;
        work->field_828                  = 1;
        work->field_82E                  = 0x13;
        work->field_832                  = 0x10;
        work->field_840                  = 0;
        work->field_83E                  = 0;
        if (enemy->hp <= 0) {
            Gp_SetStateF0Byte3(1);
        }
    }
    func_actor_421600_80134604(arg0);
    if (work->field_68 & 0x100) {
        if (enemy->hp > 0) {
            if (enemy->reactionFlags & 2) {
                work->field_0 = 4;
            } else {
                work->field_0 = 0x24;
            }
        } else {
            work->field_0 = 0x15;
        }
    }
}

void func_actor_421600_8013EE0C(Task* arg0)
{
    TmdObject*       obj;
    Actor421600Work* work;
    GpEnemy*         enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        obj                       = arg0->extra;
        obj->flags                = 0;
        work->field_8EC.radius    = 0x19C;
        work->field_B6C.flags    |= 0x4000;
        enemy->node.state.b.flags = 1;
        work->field_828           = 1;
        work->field_82E           = 0x16;
        work->field_832           = 0x10;
        work->field_840           = 0;
        work->field_83E           = 0;
        if (enemy->hp <= 0) {
            Gp_SetStateF0Byte3(1);
        }
    }
    func_actor_421600_80134604(arg0);
    if (work->field_68 & 0x100) {
        work->field_0 = 0x15;
    }
}

/// The enemy task's per-frame entry: runs the handler for the task's current
/// state - spawn, tick or teardown - from a stack copy of the state table.
void func_actor_421600_8013EEC8(Task* task)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_421600_80131FB0;
    sp.funcs[task->state](task->spawnArg2, task);
}
