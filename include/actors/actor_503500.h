#ifndef INCLUDE_ACTORS_ACTOR_503500_H
#define INCLUDE_ACTORS_ACTOR_503500_H

#include "main/task_types.h"
#include "main/tmd_types.h"

extern TmdSource gActor503500Model14DA0;

extern TmdSource gActor503500Model15820;

/// Dispatches an intro slab slider through initialization, update and exit.
///
/// Requires a live TMD model, its enemy in `spawnArg2.pointer`, and `state`
/// 0..2. Only running scene actor control dispatches any state. Initialization
/// owns allocated work; commands select either 360-update path or stationary
/// shake. Copies the handler table before dispatch; exit may destroy the task.
void actor503500SliderTask(Task* task);

/// Clears the borrowed fade-from-black task handle when Shelter R48 initializes.
///
/// Requires this actor overlay to be loaded. Does not stop or release a task;
/// the room calls this before starting its entry script. `unused` is ignored.
void actor503500ClearFadeFromBlackHandle(s32 unused);

#endif // INCLUDE_ACTORS_ACTOR_503500_H
