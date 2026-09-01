#ifndef WEAPONS_TONFA_BATON_H
#define WEAPONS_TONFA_BATON_H

#include "common.h"

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

#endif
