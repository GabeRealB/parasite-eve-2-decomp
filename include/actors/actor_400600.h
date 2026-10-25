#ifndef INCLUDE_ACTORS_ACTOR_400600_H
#define INCLUDE_ACTORS_ACTOR_400600_H

#include "main/task_types.h"

/// Spawn recipe for the Zebra Stalker body task, running at priority 96.
///
/// Requires this actor overlay's model, animation records and code to remain
/// loaded until the body and its arm tasks are released. The callback takes
/// ownership of the Enemy in spawnArg2.pointer. The high nibble of spawnArg1's
/// low byte selects 0 ordinary, 1 water, 2 group drop, 3 Junk Yard, 4 floor drop
/// or 5 delayed group entrance; the low nibble is ignored.
extern TaskDesc gActor400600ZebraStalkerTaskDesc;

#endif // INCLUDE_ACTORS_ACTOR_400600_H
