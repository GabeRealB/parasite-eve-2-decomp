#ifndef GAMEPLAY_ATTACHMENT_STATE_H
#define GAMEPLAY_ATTACHMENT_STATE_H

#include "common.h"

#include "gameplay/weapon_data.h"

/// 16-byte records selected by `Gp_GetIdParam2` when the id's 0x8000 bit is
/// set. Indexed by `id & 0x7F`.
/// Both row and byte-offset access address this complete table.
typedef union GpIdParamTable {
    GpRec16 rows[55];
    u8      bytes[55 * sizeof(GpRec16)];
} GpIdParamTable;
STATIC_ASSERT_SIZEOF(GpIdParamTable, 0x370);

/// Global at `Gp_StateC08`. `field_0` is a u16 loaded by many helpers.
/// `field_2` is a signed byte (`lb` as splat `D_80114C0A`); `Gp_SetAttachState`
/// writes the low byte of `Gp_GetAttachParam(3)`, replacing it with 1 when
/// that value is <= 0. `field_3` is a signed state byte (`lb`);
/// `func_80109290` compares it to -2 and `func_80109374` requires 0.
/// `field_5` is a signed category index (`lb` as splat `D_80114C0D`);
/// `Gp_GetAttachParam` uses it to pick a `Gp_IdParamHi` row when it is `< 0xC`.
/// `field_B` is the same kind of signed index (`lb`); `Gp_ApplyAttachStats`
/// uses `field_5` when its first arg is 1 and `field_B` otherwise.
/// `field_6` is a flags byte (bit 0 gates `func_800A7DB8` writing
/// `field_E`; bit 1 is cleared by `Gp_ResetHudFx` and forces
/// `func_800A7E5C` to 0 when that function's arg is 0). `field_9` is
/// cleared by `Gp_SetAttachState`. `field_A` is a signed byte (`lb`, splat
/// `Gp_StateC08.field_A`); `func_800A7DE0` sets `field_3 = 2` when it is >= 2,
/// then clears it. `func_80109FC4` loads it unsigned (`lbu`) and skips
/// the `field_25` bit `0x80` timer when the value is 2 or 3.
/// `Gp_ResetHudFx` also zeros `field_A`, `field_C`..`field_F`,
/// `field_10`/`field_12`/`field_14`, and `field_16`/`field_17`. Those
/// two bytes are also the item 4 / item 8 gates in `Gp_ItemIsUnusable`
/// (`lb`). `Gp_UpdateAttachCombo` packs a nibble plus `field_0 % 10` into
/// `field_C` / `field_D` / `field_F` and stores a table duration in
/// `field_10` / `field_12` / `field_14`. `field_7` and `field_8` are signed
/// bytes (`lb`): `Gp_UseItemTask` treats `field_7` as a positive-only sound id
/// (`blez` clears it) and steps `field_8` 1 -> 2 -> 0 as the attach sound is
/// queued and the category is committed. `field_E` is a signed pending
/// category (`lb`), copied into `field_5` / `field_B` once the pad is idle,
/// and `field_10` / `field_12` / `field_14` are `s16` countdowns that clear
/// `field_C` / `field_D` / `field_F` when they reach 0.
typedef struct _GpStateC08 {
    /* 0x00 */ u16  field_0;
    /* 0x02 */ s8   field_2;
    /* 0x03 */ s8   field_3;
    /* 0x04 */ byte pad_4;
    /* 0x05 */ s8   field_5;
    /* 0x06 */ u8   field_6;
    /* 0x07 */ s8   field_7;
    /* 0x08 */ s8   field_8;
    /* 0x09 */ s8   field_9;
    /* 0x0A */ s8   field_A;
    /* 0x0B */ s8   field_B;
    /* 0x0C */ s8   field_C;
    /* 0x0D */ s8   field_D;
    /* 0x0E */ s8   field_E;
    /* 0x0F */ u8   field_F;
    /* 0x10 */ s16  field_10;
    /* 0x12 */ s16  field_12;
    /* 0x14 */ s16  field_14;
    /* 0x16 */ s8   field_16;
    /* 0x17 */ s8   field_17;
} GpStateC08;
STATIC_ASSERT_SIZEOF(GpStateC08, 0x18);

/// 8-byte dispatch record selected by `Gp_ApplyAttachStats` as
/// `Gp_AttachParams[idx * 3 + ret].dispatch`. `field_0` is the switch key
/// (0..4). `field_2` / `field_4` are scaled by 100 into the follow-up
/// calls. `field_6` is passed as `lh` and also read as `lbu` + 2 into
/// `GpIdMapC.field_16`.
typedef struct _GpRec8 {
    /* 0x0 */ s16 field_0;
    /* 0x2 */ s16 field_2;
    /* 0x4 */ s16 field_4;
    /* 0x6 */ s16 field_6;
} GpRec8;
STATIC_ASSERT_SIZEOF(GpRec8, 8);

/// 8-byte item-effect row used by `Gp_UpdateAttachCombo`. Indexed by
/// `Gp_StateC08.field_0 % 10`. `field_6` is loaded `lhu` into
/// `GpStateC08.field_10` / `field_12` / `field_14`.
typedef struct _GpItemRec8 {
    /* 0x0 */ u16 pad_0[3];
    /* 0x6 */ u16 field_6;
} GpItemRec8;
STATIC_ASSERT_SIZEOF(GpItemRec8, 8);

/// Row zero holds four damage percentages; the next 54 rows hold
/// three upgrade levels for each of the eighteen attachment abilities.
/// The dispatch and combo paths read signed parameters and an unsigned count.
typedef union {
    u16        percentages[4];
    GpRec8     dispatch;
    GpItemRec8 combo;
} GpAttachParam;
STATIC_ASSERT_SIZEOF(GpAttachParam, 8);

#endif // GAMEPLAY_ATTACHMENT_STATE_H
