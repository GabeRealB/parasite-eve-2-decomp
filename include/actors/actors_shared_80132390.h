#ifndef ACTORS_SHARED_80132390_H
#define ACTORS_SHARED_80132390_H

#include "common.h"

#include "main/task.h"

/// Mirrors flag bits 0x80 and 0x4 of the parent task's `TmdObject` - the task
/// named by `spawnArg2` - onto the calling task's own object, which is the
/// model that task owns. `Tmd_Create` starts `field_C` at 0x80 and sets bit
/// 0x4 when it allocated the draw buffers, so clearing 0x4 hands the object
/// back to `Tmd_AllocBuffers` to rebuild them, while bit 0x80 is the deferred
/// kill `taskKill` sets.
void ActorsShared80132390(Task* task);

#endif
