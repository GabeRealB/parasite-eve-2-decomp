#ifndef ROOMS_SHARED_80183304_H
#define ROOMS_SHARED_80183304_H

#include "common.h"

#include "main/task.h"

/// The four message pointers this block picks its two lines from, one table
/// per carrying room.
extern u8* RoomsShared80183304Lines[4];

/// Opens a two-line text block: allocates the `RoomTextBlock`, fills its two
/// line nodes from the room's message table (the second pair when `spawnArg1`
/// is 1) and hands the list to `Ui_SpawnTextBlock`. Two rooms carry this body.
void RoomsShared80183304(Task* task);

#endif // ROOMS_SHARED_80183304_H
