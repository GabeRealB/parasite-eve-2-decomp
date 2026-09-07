#ifndef ACTORS_SHARED_80135DF4_H
#define ACTORS_SHARED_80135DF4_H

#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

/// The enemy's three state handlers - spawn/setup, per-frame tick and
/// teardown. Every carrier holds its own table at its own address, named there
/// by the family's symbol maps, so the shared object owns no data.
extern GpEnemyTaskFuncTable3 ActorsShared80135df4Table;

/// Runs the enemy's current state handler, copying the table onto the stack
/// before the call. 31 actor overlays carry this body.
void ActorsShared80135df4(Task* task);

#endif
