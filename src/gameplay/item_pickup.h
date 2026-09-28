#ifndef GAMEPLAY_PRIVATE_ITEM_PICKUP_H
#define GAMEPLAY_PRIVATE_ITEM_PICKUP_H

#include "common.h"

/// 10-byte table entry at `Gp_IdParamLo`. Selected when the id's 0x8000 bit
/// is clear. `Gp_GetIdParam0` / `Gp_GetIdParam1` / `Gp_GetIdParam2` return
/// `field_4` / `field_6` / `field_8` for index `id & 0x7F`.
typedef struct _GpRec10 {
    u16 params[5];
} GpRec10;
STATIC_ASSERT_SIZEOF(GpRec10, 0xA);

#endif // GAMEPLAY_PRIVATE_ITEM_PICKUP_H
