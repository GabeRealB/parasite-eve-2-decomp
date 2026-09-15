#ifndef ACTORS_SHARED_801385D4_H
#define ACTORS_SHARED_801385D4_H

#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

/// The enemy's five state handlers. Every carrier holds its own table at its
/// own address, named there by the family's symbol maps, so the shared object
/// owns no data.
extern GpEnemyTaskFuncTable5 ActorsShared801385d4Table;

/// Runs the enemy's current state handler, copying the table onto the stack
/// before the call. Two actor overlays carry this body.
void ActorsShared801385d4(Task* task);

#endif
