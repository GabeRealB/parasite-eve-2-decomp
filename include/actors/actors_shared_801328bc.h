#ifndef ACTORS_SHARED_801328BC_H
#define ACTORS_SHARED_801328BC_H

#include "common.h"

#include "main/task.h"

/// Per-frame handler of a decorative sub-model task: a task whose `Task::extra`
/// is the sub-model's own `TmdObject` and whose parent is the actor that spawned
/// it.
///
/// On the first frame (state 0) it aims the sub-model's light and colour
/// matrices at the two `MATRIX`es the parent keeps at the head of the work block
/// hung off `Task::idMap`, parents the sub-model's root coordinate to part 7 of
/// the parent's own model, then advances to state 1. From then on it only clears
/// the coordinate's `flg`, so the coordinate is recomputed from its new parent
/// every frame.
///
/// Shared verbatim by `actor_150400`, `actor_450800` and `actor_535700`.
void ActorsShared801328bc(Task* task);

#endif
