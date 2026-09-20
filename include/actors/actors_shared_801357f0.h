#ifndef ACTORS_SHARED_801357F0_H
#define ACTORS_SHARED_801357F0_H

#include "actors/actors_shared_80138efc.h"

/// Stores the yaw toward actor slot 0 in field_B90 and steps field_B8E by
/// at most 0xC0 toward that yaw, clamped to the range -0x300 through 0x300.
/// Carried by actor_101100, actor_104900, actor_201100, actor_204900 and actor_301100.
void ActorsShared801357f0(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work);

#endif // ACTORS_SHARED_801357F0_H
