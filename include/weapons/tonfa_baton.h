#ifndef WEAPONS_TONFA_BATON_H
#define WEAPONS_TONFA_BATON_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgs.h>

#include "main/gfx.h"

/// The tonfa's coordinate node, reached through `TmdObject.field_8`. It is
/// the game's `GsCOORDINATE2` variant: `field_44` is the reparent flag and
/// `sub` (0x4C) the parent link, the same convention as `GpCoordExt` in
/// gameplay. This overlay additionally keeps the baton's swing angle in the
/// otherwise unused 16-bit slot at 0x48 and feeds it to `Gfx_RotMatrixZ`.
typedef struct TonfaCoord {
    /* 0x00 */ s32    flg;
    /* 0x04 */ MATRIX coord;
    /* 0x24 */ MATRIX workm;
    /* 0x44 */ s16    field_44;
    /* 0x46 */ s16    field_46;
    /* 0x48 */ s16    angle;
    /* 0x4A */ byte   pad_4A[2];
    /* 0x4C */ s32*   sub;
} TonfaCoord;
STATIC_ASSERT_SIZEOF(TonfaCoord, 0x50);

/// 0x18-byte scratchpad block `func_tonfa_baton_8011DBFC` reserves for one
/// frame of the swing. `dir` receives the third column of the weapon's
/// coordinate matrix from `Gfx_MatrixCol2`; each axis is then scaled to
/// 1/84th and multiplied by the swing flag to give the per-frame translation
/// added to the coordinate.
typedef struct TonfaSwing {
    /* 0x00 */ s32     vx;
    /* 0x04 */ s32     vy;
    /* 0x08 */ s32     vz;
    /* 0x0C */ byte    pad_C[4];
    /* 0x10 */ SVECTOR dir;
} TonfaSwing;
STATIC_ASSERT_SIZEOF(TonfaSwing, 0x18);

/// Translation of the baton trail's two coordinate frames inside the weapon
/// frame: `[0]` is the near end and `[1]` the far end. State 0 of
/// `func_tonfa_baton_8011D1EC` seeds the task's own coord from `[0]` and the
/// stack coord from `[1]`.
///
/// `D_tonfa_baton_8011E0F8` is that same far-end vector under its own name.
/// State 0 reaches it as `D_tonfa_baton_8011E0F0[1]`, so the address is derived
/// from the array base already in a register; state 1 loads the symbol on its
/// own. Both spellings are needed to match.
extern SVECTOR D_tonfa_baton_8011E0F0[2];
extern SVECTOR D_tonfa_baton_8011E0F8;

/// The eight-segment swing trails, one array per end of the baton. Every entry
/// is parented to `Gfx_ViewCoord`.
extern GsCOORDINATE2 D_tonfa_baton_8012BBEC[8];
extern GsCOORDINATE2 D_tonfa_baton_8012BE6C[8];

/// Primitive/blend selector for the trail, seeded by state 0 from
/// `Task::spawnArg1` and passed to `func_tonfa_baton_8011D6B0` every frame.
extern s16 D_tonfa_baton_8012C0EC;

/// Draws one segment of the trail: `slot` is the index into the trail arrays,
/// `flags` the primitive/blend selector.
void func_tonfa_baton_8011D6B0(s32 slot, s32 flags);

#endif
