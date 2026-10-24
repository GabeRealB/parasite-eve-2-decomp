#ifndef STAGES_AKROPOLIS_ROOMS_FORKED_ROAD_CONFIG_H
#define STAGES_AKROPOLIS_ROOMS_FORKED_ROAD_CONFIG_H

// Configuration of the Akropolis forked road.

#include "main/areas.h"
#include "main/sound_ids.h"
#include "main/sound_types.h"
#include "../../room_ids.h"

// ---------------------------------------------------------------------------
// Sounds

/// A sound of the forked road's area bank.
#define AKROPOLIS_FORKED_ROAD_SOUND(cue) SOUND_AREA(GAME_STAGE_AKROPOLIS, AKROPOLIS_ROOM_ID_FORKED_ROAD, (cue))

/// Entries of the forked road's area bank.
enum {
    AKROPOLIS_FORKED_ROAD_SOUND_FOUNTAIN_DOOR_OPEN   = 2,
    AKROPOLIS_FORKED_ROAD_SOUND_FOUNTAIN_DOOR_CLOSE  = 3,
    AKROPOLIS_FORKED_ROAD_SOUND_FOUNTAIN_DOOR_LOCKED = 6,
    AKROPOLIS_FORKED_ROAD_SOUND_CRAWLER_LEAP         = 7,
    AKROPOLIS_FORKED_ROAD_SOUND_STRANGER_DORMANT_1   = 8,
    AKROPOLIS_FORKED_ROAD_SOUND_STRANGER_DORMANT_2   = 9,
};

// ---------------------------------------------------------------------------
// Doors and scenes

/// The doors' warps: the observatory's, and the warp the return ride arrives through.
enum {
    AKROPOLIS_FORKED_ROAD_OBSERVATORY_WARP    = 2,
    AKROPOLIS_FORKED_ROAD_RETURN_ARRIVAL_WARP = 2,
};

/// CAP commands and sequences of the doors.
enum {
    AKROPOLIS_FORKED_ROAD_FOUNTAIN_CAP_SEQUENCE        = 1, // The fountain door, held until MIST is contacted
    AKROPOLIS_FORKED_ROAD_FOUNTAIN_FIRST_OPEN_CAP_STEP = 3, // Its step the first time it opens
    AKROPOLIS_FORKED_ROAD_OBSERVATORY_HELD_CAP_COMMAND = 2, // The observatory door, held until the escalator works
};

/// The view the ride to the observatory starts in.
enum { AKROPOLIS_FORKED_ROAD_OUTBOUND_MOVIE_VIEW = 7 };

#endif
