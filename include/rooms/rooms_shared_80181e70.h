#ifndef ROOMS_SHARED_80181E70_H
#define ROOMS_SHARED_80181E70_H

#include "common.h"

#include "main/task.h"

/// The state table this dispatcher runs, one per carrying room.
extern TaskFuncTable4 RoomsShared80181e70Table;

/// Per-frame entry point of a four-state task: runs the handler its state
/// selects. The table is a local, so GCC copies it from `.rodata` onto the
/// stack every frame. Seven rooms carry this body.
void RoomsShared80181e70(Task* task);

#endif // ROOMS_SHARED_80181E70_H
