#ifndef ACTORS_SHARED_8014AE08_H
#define ACTORS_SHARED_8014AE08_H

#include "common.h"

#include "main/task.h"

/// Clears the pending bits of the enemy work's flag byte at `GpEnemy::field_4C`
/// and restarts the work's state machine when the "state changed" bit is set;
/// the work block is the `Actor207200Work` layout, which the three enemy slots
/// carrying this body share (`actor_104600`, `actor_204600`, `actor_207200`).
void ActorsShared8014ae08(Task* arg0);

#endif
