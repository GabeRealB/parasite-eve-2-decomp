#ifndef GAMEPLAY_3688_H
#define GAMEPLAY_3688_H

#include "common.h"

/// 0xE-byte per-room record in tables pointed to by `D_8010F0F4`.
/// Indexed by `GameSession.field_7 - 1` then `GameSession.field_6`.
/// field_0/field_2 are signed coords, field_4/field_6 unsigned extents,
/// field_8/field_A signed scales (`func_800D02A4`); field_C is the
/// room id stored in `D_80114DF0` (`func_800D1FD4`).
typedef struct _GpMapRec {
    /* 0x00 */ s16 field_0;
    /* 0x02 */ s16 field_2;
    /* 0x04 */ u16 field_4;
    /* 0x06 */ u16 field_6;
    /* 0x08 */ s16 field_8;
    /* 0x0A */ s16 field_A;
    /* 0x0C */ u8  field_C;
    /* 0x0D */ u8  field_D;
} GpMapRec;
STATIC_ASSERT_SIZEOF(GpMapRec, 0xE);

/// Per-stage table of `GpMapRec` arrays. Index is `GameSession.field_7 - 1`.
extern GpMapRec* D_8010F0F4[];

/// Current room id copied from `GpMapRec.field_C` by `func_800D1FD4`.
extern u8 D_80114DF0;

u8 func_800D1FD4(void);

#endif // GAMEPLAY_3688_H
