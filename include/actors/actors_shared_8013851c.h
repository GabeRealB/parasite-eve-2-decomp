#ifndef ACTORS_SHARED_8013851C_H
#define ACTORS_SHARED_8013851C_H

#include "common.h"

#include "main/task.h"

/// Copies the texture page and CLUT row from `src`'s model onto `dst`'s, then
/// re-runs `dst`'s stream so the geometry it already built picks the new pair
/// up. Callers pass a freshly spawned effect's task as `dst` and the actor that
/// spawned it as `src`, so the effect draws with the actor's palette. The
/// stream is processed twice because `Tmd_ProcessStream` advances one of the
/// two half-buffers per call, and both have to carry the new values.
void ActorsShared8013851c(Task* dst, Task* src);

#endif
