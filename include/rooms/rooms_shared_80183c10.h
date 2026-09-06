#ifndef ROOMS_SHARED_80183C10_H
#define ROOMS_SHARED_80183C10_H

#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

/// The room enemy's three state handlers. Both carrying rooms hold their own
/// table at their own address, named there by the family's symbol maps, so the
/// shared object owns no data.
extern GpEnemyTaskFuncTable3 RoomsShared80183c10Table;

/// Runs the room enemy's current state handler - spawn/setup, per-frame tick
/// or teardown - copying the table onto the stack before the call. Two rooms
/// carry this body.
void RoomsShared80183c10(Task* task);

#endif // ROOMS_SHARED_80183C10_H
