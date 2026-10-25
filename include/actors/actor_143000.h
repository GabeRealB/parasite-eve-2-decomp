#ifndef ACTORS_ACTOR_143000_H
#define ACTORS_ACTOR_143000_H

#include "main/task_types.h"

/// Bodyless laboratory keypad task, including its cursor and login presentation.
///
/// Spawn slot 0 with zero spawn arguments while actor_143000 and the laboratory
/// resources are loaded. The task owns its work and cursor companion, takes
/// menu presentation and player control, and starts the terminal on acceptance.
/// Keep this package loaded through the keypad and subsequent terminal session.
extern TaskDesc gActor143000KeypadTaskDesc;

/// Laboratory terminal task table: slot 0 ends a placement task, slot 1 runs login/menu/quiz control.
///
/// Slot 1 borrows a persistent `ScreenFade` as its second spawn argument and
/// requests its return on initialization; the actor overlay must remain loaded.
extern TaskDesc gActor143000TerminalTasks[2];

#endif // ACTORS_ACTOR_143000_H
