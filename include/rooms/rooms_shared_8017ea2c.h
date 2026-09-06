#ifndef ROOMS_SHARED_8017EA2C_H
#define ROOMS_SHARED_8017EA2C_H

#include "common.h"

#include "main/task.h"

/// The task the spawner below starts. Both carrying rooms hold the pointer at
/// their own address, named there by the family's symbol maps, so the shared
/// object owns no data.
extern Task* RoomsShared8017ea2cTask;

/// Starts the room's RoomsShared8017e5b8 task with `spawnArg1` 4 and keeps the
/// handle. Two rooms carry this body.
void RoomsShared8017ea2c(void);

#endif // ROOMS_SHARED_8017EA2C_H
