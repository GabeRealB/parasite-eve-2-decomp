#ifndef STAGES_AKROPOLIS_ROOMS_WEST_ELEVATOR_HALL_CONFIG_H
#define STAGES_AKROPOLIS_ROOMS_WEST_ELEVATOR_HALL_CONFIG_H

// Configuration of the Akropolis west elevator hall.

#include "main/areas.h"
#include "main/sound_ids.h"
#include "main/sound_types.h"
#include "../../room_ids.h"

// ---------------------------------------------------------------------------
// Sounds

/// A sound of the west elevator hall's area bank.
#define AKROPOLIS_WEST_ELEVATOR_HALL_SOUND(cue) SOUND_AREA(GAME_STAGE_AKROPOLIS, AKROPOLIS_ROOM_ID_WEST_ELEVATOR_HALL, (cue))

/// Entries of the west elevator hall's area bank.
enum {
    AKROPOLIS_WEST_ELEVATOR_HALL_SOUND_DOOR_OPEN           = 1,
    AKROPOLIS_WEST_ELEVATOR_HALL_SOUND_DOOR_CLOSE          = 2,
    AKROPOLIS_WEST_ELEVATOR_HALL_SOUND_ELEVATOR_ARRIVAL    = 3,
    AKROPOLIS_WEST_ELEVATOR_HALL_SOUND_ELEVATOR_DOOR_CLOSE = 4,
};

// ---------------------------------------------------------------------------
// Arrival and departure

/// The warp the arrival scene comes in through, and the square's arrival the first departure leads to.
enum {
    AKROPOLIS_WEST_ELEVATOR_HALL_ARRIVAL_WARP        = 1,
    AKROPOLIS_WEST_ELEVATOR_HALL_SQUARE_ARRIVAL_WARP = 7,
};

/// Scene events: the first departure for the square.
enum { AKROPOLIS_WEST_ELEVATOR_HALL_DEPARTURE_SCENE_EVENT = 1 };

/// The objective set by the arrival scene.
enum { AKROPOLIS_WEST_ELEVATOR_HALL_ARRIVAL_OBJECTIVE = 1 };

#endif
