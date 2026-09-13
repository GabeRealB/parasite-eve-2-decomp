#ifndef ACTORS_SHARED_8014B128_H
#define ACTORS_SHARED_8014B128_H

#include "common.h"

#include "main/task.h"

/// Folds the work's stored transform into the model's first coordinate node,
/// scaled along Y by the work's decaying angle field; the work block is the
/// `Actor207200Work` layout, which the three enemy slots carrying this body
/// share (`actor_104600`, `actor_204600`, `actor_207200`).
void ActorsShared8014b128(Task* arg0);

#endif
