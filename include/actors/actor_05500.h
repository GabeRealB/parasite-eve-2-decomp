#ifndef INCLUDE_ACTORS_ACTOR_05500_H
#define INCLUDE_ACTORS_ACTOR_05500_H

#include "main/task_types.h"

/// Caterpillar body and puff spawn descriptors: indices 0 and 1, no terminator.
///
/// Both run at priority `TASK_PRIORITY_ACTOR`; the body allocates an eight-part TMD with
/// automatic model buffers, and the puff has one coordinate. Spawn through
/// the enemy API with a live owning `Enemy` in `Task::spawnArg2.pointer`.
/// The body retains the table for child spawns. Keep the package callbacks
/// and borrowed geometry loaded while either task lives.
extern TaskDesc gActor05500MaggotCaterpillarTasks[2];

/// Slot-1 export of `gActor05500MaggotCaterpillarTasks` from actor_105500.
extern TaskDesc gActor105500MaggotCaterpillarTasks[2];

/// Slot-2 export of `gActor05500MaggotCaterpillarTasks` from actor_205500.
extern TaskDesc gActor205500MaggotCaterpillarTasks[2];

#endif // INCLUDE_ACTORS_ACTOR_05500_H
