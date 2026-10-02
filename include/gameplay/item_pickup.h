#ifndef GAMEPLAY_ITEM_PICKUP_H
#define GAMEPLAY_ITEM_PICKUP_H

#include <psyq/sys/types.h>

#include "common.h"

/// 12-byte location-keyed grant record walked by `Gp_GrantLocationItems`.
/// `field_0` is `(stage << 24) | (area << 16) | (sub << 8)` from
/// `GameLocationKey.stage` / `field_2` / `field_5`, or `-1` to end
/// the list. `items[0..3]` are item ids granted with `Gp_GiveItem`
/// when `func_800B7420` is 0; a 0 slot is skipped. `items[3]` also
/// requires `func_800B9D80(0x80000)`.
typedef struct _GpGiveRec {
    /* 0x0 */ s32 field_0;
    /* 0x4 */ u16 items[4];
} GpGiveRec;
STATIC_ASSERT_SIZEOF(GpGiveRec, 0xC);

#endif // GAMEPLAY_ITEM_PICKUP_H
