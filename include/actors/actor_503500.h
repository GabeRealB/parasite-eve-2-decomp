#ifndef INCLUDE_ACTORS_ACTOR_503500_H
#define INCLUDE_ACTORS_ACTOR_503500_H

#include "main/task_types.h"
#include "main/tmd_types.h"

/// Descriptor indices for the R48 drift emitters and fade from black.
enum {
    ACTOR_503500_SCENE_TASK_DRIFT_EMITTER   = 0,
    ACTOR_503500_SCENE_TASK_FADE_FROM_BLACK = 1,
    ACTOR_503500_SCENE_TASK_COUNT           = 2,
};

/// Extent of the aligned enemy descriptor, parameter and live-slot tables.
enum { ACTOR_503500_SLOT_COUNT = 17 };

/// Scene-effect spawn recipes borrowed synchronously by the actor scripts and R48.
///
/// Drift emitter 0 owns a coordinate body; spawn argument 1 selects room position
/// 0..3 and argument 2 is unused. Fade 1 has no body; argument 1 is its signed
/// hold countdown in actor-running ticks and argument 2 is unused. Both run at
/// priority 192. Keep the actor overlay and R48 effect resources loaded.
extern TaskDesc gActor503500SceneEffectTaskDescs[ACTOR_503500_SCENE_TASK_COUNT];

/// Enemy spawn recipes for the boss at slot 0 and attached parts at slots 1..16.
///
/// The index and spawn argument 1 must select the same slot; enemy spawning
/// supplies the live Enemy in argument 2. Priorities are 96. Slots 4..9 and 12
/// own coordinate bodies; all others own models. Chain and arm recipes skip
/// automatic primitive-buffer allocation. Descriptors are borrowed only during
/// spawn; callbacks and model data require the actor overlay to remain loaded.
/// R48 resource kind 35 exports this table, aligned with the package's parameters.
extern TaskDesc gActor503500EnemyTaskDescs[ACTOR_503500_SLOT_COUNT];

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
