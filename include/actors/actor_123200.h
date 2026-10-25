#ifndef INCLUDE_ACTORS_ACTOR_123200_H
#define INCLUDE_ACTORS_ACTOR_123200_H

#include "main/task_types.h"

/// Spawn recipe for the staged Bone Suckler used by the motel-room event.
///
/// Index 0 creates a six-part TMD body at priority `TASK_PRIORITY_ACTOR` with automatic buffers.
/// Spawn through the enemy API with a live owned `Enemy` in spawn argument 2.
/// Keep actor_123200's callbacks and borrowed geometry loaded until teardown.
extern TaskDesc gActor123200BoneSucklerTaskDesc;

#endif // INCLUDE_ACTORS_ACTOR_123200_H
