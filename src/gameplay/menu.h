#ifndef GAMEPLAY_PRIVATE_MENU_H
#define GAMEPLAY_PRIVATE_MENU_H

#include "common.h"

#include "main/task_types.h"

struct UiObject;

/// Callback for UiObject + Task state handlers (e.g. entries in `Gp_ItemMenuStates`).
typedef void (*UiObjectTaskFunc)(struct UiObject* arg0, Task* arg1);

/// Fixed-size table of `UiObjectTaskFunc` callbacks. Copied onto the stack by
/// `Gp_ItemMenuTask` so the call uses a local jump table.
typedef struct {
    UiObjectTaskFunc funcs[3];
} UiObjectTaskFuncTable3;

/// Per-column icon descriptor for the P.Energy attach panel (`Gp_PeGridPanelTask`).
/// One 3-byte entry per weapon/armour column: `u`/`v` are the SPRT texture
/// coordinates of the column caption and `xOffset` shifts the caption right of
/// the column origin. The table `D_8010E844` holds the four columns.
typedef struct _GpEnergyIcon {
    /* 0x0 */ u8 u;
    /* 0x1 */ u8 v;
    /* 0x2 */ u8 xOffset;
} GpEnergyIcon;
STATIC_ASSERT_SIZEOF(GpEnergyIcon, 3);

#endif // GAMEPLAY_PRIVATE_MENU_H
