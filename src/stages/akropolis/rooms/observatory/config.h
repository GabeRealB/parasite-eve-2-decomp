#ifndef STAGES_AKROPOLIS_ROOMS_OBSERVATORY_CONFIG_H
#define STAGES_AKROPOLIS_ROOMS_OBSERVATORY_CONFIG_H

// Configuration of the Akropolis observatory.

#include "../../room_ids.h"

// ---------------------------------------------------------------------------
// Doors and rides

/// The warps the rides arrive through, and the stairs' arrivals used when a ride does not play.
enum {
    AKROPOLIS_OBSERVATORY_PROMENADE_RIDE_ARRIVAL_WARP   = 3,
    AKROPOLIS_OBSERVATORY_FORKED_ROAD_RIDE_ARRIVAL_WARP = 4,
    AKROPOLIS_OBSERVATORY_FORKED_ROAD_STAIRS_WARP       = 5, // In the forked road
    AKROPOLIS_OBSERVATORY_PROMENADE_STAIRS_WARP         = 1, // In the promenade
};

/// Entries of the arrival task table.
enum {
    AKROPOLIS_OBSERVATORY_FORKED_ROAD_RIDE_TASK_ENTRY = 0,
    AKROPOLIS_OBSERVATORY_PROMENADE_RIDE_TASK_ENTRY   = 1,
};

/// The room action that starts the bat ambush.
enum { AKROPOLIS_OBSERVATORY_ACTION_BAT_AMBUSH = 1 };

#endif
