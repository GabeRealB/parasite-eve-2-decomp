#ifndef ACTORS_SHARED_80138C6C_H
#define ACTORS_SHARED_80138C6C_H

#include "actors/actors_shared_80138efc.h"

/// Arms motion 1 with a random 2-, 60-, 120-, or 180-frame countdown, then
/// selects state 1..4 when it expires and clears the run-once latch.
/// Shared by actor_101100, actor_104900, actor_201100, actor_204900, and actor_301100.
void ActorsShared80138c6c(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work);

#endif // ACTORS_SHARED_80138C6C_H
