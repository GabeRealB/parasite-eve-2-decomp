#ifndef INCLUDE_ACTORS_ACTOR_503500_H
#define INCLUDE_ACTORS_ACTOR_503500_H

#include "main/task_types.h"
#include "main/tmd_types.h"

extern TmdSource gActor503500Model14DA0;

extern TmdSource gActor503500Model15820;

/// An enemy task and the models its descriptors attach, named by the enemy
/// descriptor tables of the Shelter map UI overlay.
void func_actor_503500_8013270C(Task* task);

/// Clears the borrowed fade-from-black task handle when Shelter R48 initializes.
///
/// Requires this actor overlay to be loaded. Does not stop or release a task;
/// the room calls this before starting its entry script. `unused` is ignored.
void actor503500ClearFadeFromBlackHandle(s32 unused);

#endif // INCLUDE_ACTORS_ACTOR_503500_H
