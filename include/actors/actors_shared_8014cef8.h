#ifndef ACTORS_SHARED_8014CEF8_H
#define ACTORS_SHARED_8014CEF8_H

#include "common.h"

#include "main/task.h"

/// Per-frame handler of a decorative sub-model task: a task whose `Task::extra`
/// is the sub-model's own `TmdObject` and whose parent is the actor that spawned
/// it. This is `ActorsShared801328bc` with two differences: the object's
/// `field_C` is cleared on the first frame, and the sub-model's root coordinate
/// is parented to part 4 of the parent's model rather than part 7.
///
/// On the first frame (state 0) it aims the sub-model's light and colour
/// matrices at the two `MATRIX`es the parent keeps at the head of the work block
/// hung off `Task::work`, parents the sub-model's root coordinate to that part
/// of the parent's own model, then advances to state 1. From then on it only
/// clears the coordinate's `flg`, so the coordinate is recomputed from its new
/// parent every frame.
///
/// Shared by `actor_160700` and `actor_215100`.
void ActorsShared8014cef8(Task* task);

#endif
