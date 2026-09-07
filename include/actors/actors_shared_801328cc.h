#ifndef ACTORS_SHARED_801328CC_H
#define ACTORS_SHARED_801328CC_H

#include "common.h"

#include "main/task.h"

/// The four state handlers this dispatcher runs. Every carrier holds its own
/// table at its own address, named there by the family's symbol maps, so the
/// shared object owns no data.
extern TaskFuncTable4 ActorsShared801328ccTable;

/// Per-frame entry point of a four-state task: runs the handler its state
/// selects. The table is a local, so GCC copies it from `.rodata` onto the
/// stack every frame. Five actor overlays share this body. `actor_107600` and
/// `actor_342400` carry it twice, so they keep their own copies.
void ActorsShared801328cc(Task* task);

#endif
