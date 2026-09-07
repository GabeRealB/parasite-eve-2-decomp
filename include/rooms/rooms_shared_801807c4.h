#ifndef ROOMS_SHARED_801807C4_H
#define ROOMS_SHARED_801807C4_H

#include "main/task.h"

/// Moves the effect coordinate toward the coordinate in Task::spawnArg1.
/// State 0 transforms their world displacement into the effect's parent frame
/// and scales it by 0xCC / 0x1000. State 1 applies that step and calls
/// Room_Draw14 every other tick, advancing the draw phase. The work block is
/// released at tick 20, or when the room departure state reaches 4.
void RoomsShared801807c4(Task* task);

#endif // ROOMS_SHARED_801807C4_H
