#ifndef GAMEPLAY_PRIVATE_TELEPHONE_H
#define GAMEPLAY_PRIVATE_TELEPHONE_H

#include "main/task_types.h"
#include "main/ui_types.h"

/// Telephone save/statistics panel recipe for a placed save-point action.
///
/// Uses a titled 128-by-64-pixel panel and dispatches by the saved stage/area.
/// The owning room's telephone code and UI resources must remain loaded for
/// the spawned menu's lifetime. The descriptor itself is read only at spawn.
extern const UiObjectDesc gTelephoneMenuDescriptor;

/// Dispatches the telephone save/statistics menu to the saved area's room overlay.
///
/// Routes by the live save's stage/area, ignoring room/view. The selected room
/// overlay and its telephone UI resources must remain loaded for every update.
/// `task` must be a live menu task in state 0..3 with its writable
/// UiObject in `spawnArg2.pointer`; save/notice/statistics children must remain
/// linked until their answers are consumed. `spawnArg1` is unused by the menu.
/// Room callbacks publish dismissal through the object's result; the enclosing
/// UI flow closes/releases the menu. Unsupported stage/area keys do nothing.
void telephoneDispatchMenuTask(Task* task);

#endif // GAMEPLAY_PRIVATE_TELEPHONE_H
