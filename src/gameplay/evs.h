#ifndef GAMEPLAY_PRIVATE_EVS_H
#define GAMEPLAY_PRIVATE_EVS_H

#include "common.h"

/// Three overlay ids passed through `func_800E7498` (table cmd 0xFA6)
/// to `CdCmd_StartOverlay`. Also the current evs triple held in
/// `D_801156F4` (`Gp_CapExit` formats `"evs%d_%d_%d.txt"` from it).
typedef struct _GpOverlayIds {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
    /* 0x4 */ u16 field_4;
} GpOverlayIds;
STATIC_ASSERT_SIZEOF(GpOverlayIds, 6);

#endif // GAMEPLAY_PRIVATE_EVS_H
