#ifndef GAMEPLAY_ITEM_PICKUP_H
#define GAMEPLAY_ITEM_PICKUP_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

/// 12-byte location-keyed grant record walked by `Gp_GrantLocationItems`.
/// `field_0` is `(stage << 24) | (area << 16) | (sub << 8)` from
/// `GpAreaKey.stage` / `field_2` / `field_5`, or `-1` to end
/// the list. `items[0..3]` are item ids granted with `Gp_GiveItem`
/// when `func_800B7420` is 0; a 0 slot is skipped. `items[3]` also
/// requires `func_800B9D80(0x80000)`.
typedef struct _GpGiveRec {
    /* 0x0 */ s32 field_0;
    /* 0x4 */ u16 items[4];
} GpGiveRec;
STATIC_ASSERT_SIZEOF(GpGiveRec, 0xC);

/// 16-byte VRAM upload record walked by `Gp_LoadImages`. `field_0 == 0`
/// uploads `rect` / `data` via `LoadImage`; non-zero ends the walk.
/// `Gp_LoadActorImage` fills `rect` from a source RECT plus the TMD tpage at
/// `TmdObject.tpage` (`x = tpage * 64 + (src.x + 1) / 2 + 0x180`,
/// `y = src.y + 0x100`).
typedef struct _GpImgRec {
    /* 0x0 */ u16     field_0;
    /* 0x2 */ u16     pad_2;
    /* 0x4 */ RECT    rect;
    /* 0xC */ u_long* data;
} GpImgRec;
STATIC_ASSERT_SIZEOF(GpImgRec, 0x10);

#endif // GAMEPLAY_ITEM_PICKUP_H
