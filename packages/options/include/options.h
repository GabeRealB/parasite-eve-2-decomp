#ifndef OPTIONS_OPTIONS_H
#define OPTIONS_OPTIONS_H

#include <engine/task_types.h>

/// What an options task's first spawn argument asks for: the whole list, or only its vibration row.
enum {
    OPTIONS_MENU_FULL           = 0,
    OPTIONS_MENU_VIBRATION_ONLY = 1
};

/// The options panel's task, on the `UiObject` in spawnArg2. It draws the list and takes the pad: Cancel closes the
/// panel (its result confirm, resultValue 1) and Menu cancels the flow that opened it. A key configuration child it opened
/// closes on its confirm, giving control back, and its cancel cancels this panel's flow too. One panel of each layout at a
/// time: each keeps its list's state.
void optionsUpdateMenuTask(Task* owningTask);

#endif // OPTIONS_OPTIONS_H
