#ifndef ACTORS_SHARED_801366FC_H
#define ACTORS_SHARED_801366FC_H

#include "common.h"

#include "main/task.h"

/// `Task::exitCallback` shared by two dozen enemy actors: hands the task's
/// `GpEnemy` (parked in `Task::spawnArg2` by the spawn descriptor) back to
/// `Gp_DestroyEnemy`.
void ActorsShared801366fc(Task* task);

#endif
