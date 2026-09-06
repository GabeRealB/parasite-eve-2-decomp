#ifndef ROOMS_SHARED_8017F128_H
#define ROOMS_SHARED_8017F128_H

#include "common.h"

#include <psyq/libgte.h>

#include "main/task.h"

/// Which part of the mirrored player each reflected slot hangs off, and the
/// scale that flips the reflection across X. Each carrying room holds its own
/// copy.
extern u8           RoomsShared8017f128Parts[];
extern const VECTOR RoomsShared8017f128Scale;

/// Per-frame callback of a held-object reflection. `Task::spawnArg2` is the
/// mirror task and the parent is the held-object task being reflected. On the
/// first frame it clones the parent's TMD source, parents the clone's root
/// coordinate to the mirrored player's corresponding part, points the clone at
/// the mirror's light and color matrices and negates the X translation; every
/// frame it republishes the mirror model's draw flags onto the clone. Six rooms
/// carry this body.
void RoomsShared8017f128(Task* task);

#endif // ROOMS_SHARED_8017F128_H
