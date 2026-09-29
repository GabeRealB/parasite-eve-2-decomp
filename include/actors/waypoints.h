#ifndef INCLUDE_ACTORS_WAYPOINTS_H
#define INCLUDE_ACTORS_WAYPOINTS_H

#include "common.h"

// Exported word storage whose low halfword supplies an actor's base height.
typedef union ActorWaypointHeight {
    s32 storage;
    s16 height;
} ActorWaypointHeight;
STATIC_ASSERT_SIZEOF(ActorWaypointHeight, 4);

#endif // INCLUDE_ACTORS_WAYPOINTS_H
