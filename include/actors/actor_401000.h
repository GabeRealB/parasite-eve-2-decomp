#ifndef INCLUDE_ACTORS_ACTOR_401000_H
#define INCLUDE_ACTORS_ACTOR_401000_H

#include "main/task_types.h"

/// Odd Stranger spawn recipe exported by actor_401000 to room resources.
///
/// Uses a TMD body at priority `TASK_PRIORITY_ACTOR` and defers primitive-buffer allocation
/// until the chosen opening state needs drawing. Borrows a live `Enemy` as
/// spawn argument 2; the actor overlay and its model data must remain loaded.
extern TaskDesc gActor401000StrangerTaskDesc;

#endif // INCLUDE_ACTORS_ACTOR_401000_H
