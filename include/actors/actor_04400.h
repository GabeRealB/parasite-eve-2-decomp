#ifndef INCLUDE_ACTORS_ACTOR_04400_H
#define INCLUDE_ACTORS_ACTOR_04400_H

#include "main/task_types.h"

/// Spawn indices of the Mad Chaser package carried in actor slots 1 and 3.
enum {
    ACTOR_04400_MAD_CHASER_TASK_ORDINARY           = 0,
    ACTOR_04400_MAD_CHASER_TASK_SCRIPTED_NO_BUFFER = 1,
    ACTOR_04400_MAD_CHASER_TASK_KILL               = 2,
    ACTOR_04400_MAD_CHASER_TASK_SCRIPTED           = 3,
    ACTOR_04400_MAD_CHASER_TASK_COUNT              = 4
};

/// Four Mad Chaser spawn recipes, indexed by `ACTOR_04400_MAD_CHASER_TASK_*`.
///
/// Slot 0 starts ordinary lurking or dangling. Slots 1 and 3 start the scripted
/// emergence task; slot 1 defers automatic model-buffer allocation, while slot 3
/// allocates it normally. Slot 2 creates a coordinate body whose callback kills
/// the task. All recipes use execution priority 96. Enemy recipes require a live
/// `Enemy` in the second spawn argument; the callbacks and borrowed body geometry
/// must remain loaded until teardown. The descriptor table has no terminator.
extern TaskDesc gActor04400MadChaserTaskDescs[ACTOR_04400_MAD_CHASER_TASK_COUNT];

#endif /* INCLUDE_ACTORS_ACTOR_04400_H */
