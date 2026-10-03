#ifndef GAMEPLAY_PRIVATE_MENU_H
#define GAMEPLAY_PRIVATE_MENU_H

#include "common.h"

#include "main/task_types.h"
#include "main/ui_types.h"

/// Menu handler applied to one task-owned UI node. Takes the node and the task
/// that owns it, and returns nothing.
///
/// The pair is always one object seen from both ends: `object` is the node in
/// the task's `spawnArg2.pointer`, and `task` is that node's `owner`. Callers
/// hold one of the two and derive the other, so a handler may use whichever it
/// needs and ignore the other.
///
/// Three kinds of caller use it. A node's content task selects a state handler
/// by `Task::state` and passes itself. A parent walks its child tasks and
/// applies one handler to each child's node. A table keyed by item id supplies
/// the handler that carries out an item's use, and a NULL entry there means
/// the item has none. Both pointers are live on entry. A handler may tear the
/// node down, so a caller walking children reads the next sibling first.
typedef void (*UiObjectTaskFunc)(UiObject* object, Task* task);

/// Three `UiObjectTaskFunc` handlers stored as a value for whole-table copies.
///
/// A node's content task copies the table and calls the slot its
/// `Task::state` selects, passing the node from its `spawnArg2.pointer` and
/// itself. Each table defines its slots' roles. Dispatch requires an index in
/// 0..2 and a non-NULL entry: there is no terminator or bounds check in the
/// table, so the handlers themselves keep the state inside that range.
/// Copying it copies callback pointers, not node or task storage.
typedef struct {
    UiObjectTaskFunc funcs[3]; // Handlers in task-state order; slot meanings belong to each table
} UiObjectTaskFuncTable3;
STATIC_ASSERT_SIZEOF(UiObjectTaskFuncTable3, 0xC);

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
