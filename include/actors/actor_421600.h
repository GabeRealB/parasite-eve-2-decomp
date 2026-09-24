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

#endif
