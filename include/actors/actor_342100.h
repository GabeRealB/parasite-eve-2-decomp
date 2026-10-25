#ifndef INCLUDE_ACTORS_ACTOR_342100_H
#define INCLUDE_ACTORS_ACTOR_342100_H

#include "main/task_types.h"

/// Dumping-hole spawn recipes: 0 encounter/blaze controller, 1 kill placeholder, 2 fade, 3 body fire, 4 fire emitter.
///
/// All run at priority `TASK_PRIORITY_CONTROL`; only entry 4 has a coordinate body, the others are
/// bodyless. Indices 0..4 have no terminator. Fade argument 2 borrows the live
/// controller; emitter argument 1 carries its start delay. Keep actor_342100 and
/// the dumping-hole resources loaded through the encounter and burn sequence.
extern TaskDesc gActor342100EncounterBlazeTaskDescriptors[5];

#endif // INCLUDE_ACTORS_ACTOR_342100_H
