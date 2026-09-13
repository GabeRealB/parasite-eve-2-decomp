#ifndef ACTORS_SHARED_8014AF2C_H
#define ACTORS_SHARED_8014AF2C_H

#include "common.h"

#include "main/task.h"

/// Rebinds the work's animation id to its three helper slots; the work block
/// is the `Actor207200Work` layout, which the three enemy slots carrying this
/// body share (`actor_104600`, `actor_204600`, `actor_207200`).
void ActorsShared8014af2c(Task* arg0);

#endif
