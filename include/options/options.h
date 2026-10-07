#ifndef OPTIONS_OPTIONS_H
#define OPTIONS_OPTIONS_H

#include "main/task_types.h"

/// Layout selectors carried in an options task's first spawn argument.
enum {
    OPTIONS_MENU_FULL           = 0,
    OPTIONS_MENU_VIBRATION_ONLY = 1
};

/// Updates an options panel and publishes requests to leave it.
///
/// The options overlay (file 20100) must remain loaded. `owningTask` owns a live
/// `UiObject` in spawnArg2; spawnArg1 equal to `OPTIONS_MENU_VIBRATION_ONLY`
/// selects the single vibration row, and every other value selects all seven
/// settings. Keep that selector unchanged for the panel's lifetime. State zero
/// fits the list and advances to one; the vibration layout also centers the
/// panel with a 192-pixel content width. Only one panel per layout may use its
/// mutable list at a time. The UI lifecycle supplies layout, clipping, textures,
/// primitive storage and ordering-table entries required by `uiUpdateList`.
///
/// Each call clears result, then draws and handles controller port zero. With
/// active input, Cancel publishes confirm with resultValue 1; Menu publishes
/// cancel. Cancel wins if both are pressed. A first child's confirm requests
/// closing of that child's UI tree and restores parent input; its cancel
/// overrides the parent's result. Child tasks must own live UI objects in
/// spawnArg2. Setting callbacks may spawn a key-configuration child. Closing
/// detaches tasks but defers their release; the parent handles exit results.
void optionsUpdateMenuTask(Task* owningTask);

#endif // OPTIONS_OPTIONS_H
