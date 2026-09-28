#ifndef GAMEPLAY_WEAPON_DATA_H
#define GAMEPLAY_WEAPON_DATA_H

#include "common.h"

/// 16-byte table entry at `Gp_IdParamHi`. Selected when the id's 0x8000 bit
/// is set. `Gp_GetIdParam0` / `Gp_GetIdParam1` / `Gp_GetIdParam2` return
/// `field[5]` / `field[6]` / `field[7]` for index `id & 0x7F`.
/// `func_800D50D4` indexes `field[arg1]` after remapping a packed id as
/// `((id>>4&3)*3 + (id>>2&3))*3 + (id&3)`.
typedef struct _GpRec16 {
    /* 0x0 */ u16 field[8];
} GpRec16;
STATIC_ASSERT_SIZEOF(GpRec16, 0x10);

#endif // GAMEPLAY_WEAPON_DATA_H
