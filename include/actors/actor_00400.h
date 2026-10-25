#ifndef INCLUDE_ACTORS_ACTOR_00400_H
#define INCLUDE_ACTORS_ACTOR_00400_H

#include "main/task_types.h"

/// Indices of the Bog Diver's body and its independently owned child tasks.
enum {
    ACTOR_00400_BODY_TASK_INDEX       = 0,
    ACTOR_00400_SHOT_TASK_INDEX       = 1,
    ACTOR_00400_STAIN_DESCRIPTOR      = 2,
    ACTOR_00400_TASK_DESCRIPTOR_COUNT = 3
};

/// Bog Diver spawn recipes: TMD body, coordinate-body shot and ground stain.
///
/// All three have priority `TASK_PRIORITY_ACTOR`; the body has automatic model buffers. It receives
/// a live Enemy in the second spawn payload; the first payload selects its
/// placement behavior. Body initialization owns its work and may spawn the
/// wounded-ground stain. The body spawns and initializes each shot's work;
/// shots and stains run and release their storage as separate tasks.
/// The three-entry table has no terminator. Keep the selected actor package's
/// callbacks, model and animation storage loaded until its tasks are released.
extern TaskDesc gActor00400Tasks[ACTOR_00400_TASK_DESCRIPTOR_COUNT];

/// Slot-1 export of `gActor00400Tasks`; requires actor_100400 to be loaded.
extern TaskDesc gActor100400Tasks[ACTOR_00400_TASK_DESCRIPTOR_COUNT];

/// Slot-2 export of `gActor00400Tasks`; requires actor_200400 to be loaded.
extern TaskDesc gActor200400Tasks[ACTOR_00400_TASK_DESCRIPTOR_COUNT];

#endif // INCLUDE_ACTORS_ACTOR_00400_H
