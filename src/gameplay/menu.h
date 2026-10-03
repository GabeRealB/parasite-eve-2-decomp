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

/// Caption sprite of one element column in the menu's Parasite Energy summary panel.
///
/// The panel lays the twelve Parasite Energy levels out as four columns of
/// three, one column per element in the order the levels are stored: fire,
/// wind, water, earth. An entry locates that element's caption in the menu
/// texture sheet and places it over its column; the panel derives the
/// caption's size and palette from the column number.
typedef struct {
    u8 u;       // Left edge of the caption in the menu texture sheet, in texels
    u8 v;       // Top edge of the caption in the menu texture sheet, in texels
    u8 xOffset; // Pixels the caption is drawn to the right of its column's left edge
} MenuParasiteEnergyCaption;
STATIC_ASSERT_SIZEOF(MenuParasiteEnergyCaption, 3);

#endif // GAMEPLAY_PRIVATE_MENU_H
