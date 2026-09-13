#ifndef ACTORS_SHARED_80134700_H
#define ACTORS_SHARED_80134700_H

#include "common.h"

#include "main/task.h"

/// Draws the actor's ground shadow quad under the model root. The world
/// position is the translation of the root part's `workm`, staged in a
/// scratchpad VECTOR3 rather than on the stack.
void ActorsShared80134700(Task* task);

#endif
