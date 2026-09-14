#ifndef ACTORS_SHARED_80162850_H
#define ACTORS_SHARED_80162850_H

#include "common.h"

#include "main/task.h"

/// `Task::exitCallback` the overlay's controller task is handed once the actor
/// is up: it only hands the task to `Task_Kill`.
void ActorsShared80162850(Task* task);

#endif
