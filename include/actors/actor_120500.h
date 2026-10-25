#ifndef INCLUDE_ACTORS_ACTOR_120500_H
#define INCLUDE_ACTORS_ACTOR_120500_H

#include "main/task_types.h"

/// Motel scene spawn recipes: 0 empty placement, 1 Kyle Madigan's view-parented body.
///
/// Both run at priority `TASK_PRIORITY_CONTROL` with unused spawn arguments and no terminator.
/// Entry 0 has no body and ends on first dispatch. Entry 1 creates the
/// twenty-part TMD with automatic buffers disabled; choreography allocates
/// them when the double appears. Keep actor_120500 and the motel room/CAP
/// resources loaded through the room wrapper's actor stop-request handshake.
extern TaskDesc gActor120500EmptyPlacementTaskDesc[2];

#endif // INCLUDE_ACTORS_ACTOR_120500_H
