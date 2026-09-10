#ifndef ACTORS_SHARED_801327B4_H
#define ACTORS_SHARED_801327B4_H

#include "common.h"

#include "main/task.h"

/// `Task::exitCallback` shared by the enemy actors whose work block is a plain
/// allocation rather than a display object: it only hands the task to
/// `Gp_EnemyTaskExit`.
void ActorsShared801327b4(Task* task);

#endif
