#ifndef INCLUDE_ACTORS_ACTOR_341700_H
#define INCLUDE_ACTORS_ACTOR_341700_H

#include "main/task_types.h"

/// Spawn indices of the dumping-hole Mad Chaser package.
enum {
    ACTOR_341700_MAD_CHASER_TASK_ORDINARY           = 0,
    ACTOR_341700_MAD_CHASER_TASK_SCRIPTED_NO_BUFFER = 1,
    ACTOR_341700_MAD_CHASER_TASK_KILL               = 2,
    ACTOR_341700_MAD_CHASER_TASK_SCRIPTED           = 3,
    ACTOR_341700_MAD_CHASER_TASK_COUNT              = 4
};

/// Four Mad Chaser spawn recipes, indexed by `ACTOR_341700_MAD_CHASER_TASK_*`.
///
/// Slot 0 starts ordinary lurking or dangling. Slots 1 and 3 wait for scripted
/// emergence; slot 1 skips automatic model buffers, while slot 3 enables them.
/// Slot 2 creates a coordinate body and kills the task on its first dispatch.
/// All entries use priority `TASK_PRIORITY_ACTOR` and the table has no terminator. Enemy entries
/// require a live `Enemy` in the second spawn argument. Keep this package's
/// callbacks and borrowed model geometry loaded until the tasks are destroyed.
extern TaskDesc gActor341700MadChaserTaskDescs[ACTOR_341700_MAD_CHASER_TASK_COUNT];

/// Spawn recipe for the dumping-hole event's eleven-part rising prop.
///
/// Index 0 creates a TMD body at priority `TASK_PRIORITY_ACTOR`, skipping automatic model buffers.
/// Requires a live `Enemy` in the second spawn argument; it remains untargetable
/// with zero HP. Placement and room commands control its transform, visibility
/// and shaking rise. Keep the package's callback and model geometry loaded
/// until teardown; work and any explicitly allocated buffer belong to the task.
extern TaskDesc gActor341700PropTaskDesc;

#endif /* INCLUDE_ACTORS_ACTOR_341700_H */
