#ifndef ACTORS_SHARED_8013231C_H
#define ACTORS_SHARED_8013231C_H

#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/message.h"
#include "main/task.h"

/// Places the actor at `args`: drops the opcode's translation straight into the
/// root part's local matrix, stores its Euler angles in the coordinate's own
/// `rot` slot and rebuilds the rotation from them. Clearing `flg` makes
/// `_gpUpdateCoordTree` recompute the world matrix from it.
s32 ActorsShared8013231c(Task* task, s32 arg1, GpPlaceArg* args);

#endif
