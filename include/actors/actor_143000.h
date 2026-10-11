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

#endif // ACTORS_ACTOR_143000_H
