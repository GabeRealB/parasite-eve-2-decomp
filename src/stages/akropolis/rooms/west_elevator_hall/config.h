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

// ---------------------------------------------------------------------------
// The elevator

/// Messages the arrival script sends: the room task opens and closes the elevator door, the room effect task lights
/// the elevator bay.
enum {
    AKROPOLIS_WEST_ELEVATOR_HALL_MESSAGE_OPEN_DOOR          = 5100,
    AKROPOLIS_WEST_ELEVATOR_HALL_MESSAGE_CLOSE_DOOR         = 5101,
    AKROPOLIS_WEST_ELEVATOR_HALL_MESSAGE_START_BAY_LIGHTING = 3100,
};

/// The elevator bay's view, the only one that shows its door, its light and its lit palette.
enum { AKROPOLIS_WEST_ELEVATOR_HALL_BAY_VIEW = 5 };

/// The elevator door, in world units: where its two leaves meet when shut, and how fast and how far each slides.
enum {
    AKROPOLIS_WEST_ELEVATOR_HALL_DOOR_CLOSED_X        = -1000,
    AKROPOLIS_WEST_ELEVATOR_HALL_DOOR_CLOSED_Y        = -20,
    AKROPOLIS_WEST_ELEVATOR_HALL_DOOR_CLOSED_Z        = 2420,
    AKROPOLIS_WEST_ELEVATOR_HALL_DOOR_LEAF_SPEED      = 20,
    AKROPOLIS_WEST_ELEVATOR_HALL_DOOR_LEAF_TRAVEL_MAX = 720,
};

/// Video memory row of the bay's 256-color palette, which the bay light's glow also draws with, so the glow brightens
/// as the bay lights up.
enum { AKROPOLIS_WEST_ELEVATOR_HALL_BAY_CLUT_Y = 270 };

// ---------------------------------------------------------------------------
// Effects

/// Player reflections: their task and its two modes.
enum {
    AKROPOLIS_WEST_ELEVATOR_HALL_REFLECTION_TASK_BANK = 1,
    AKROPOLIS_WEST_ELEVATOR_HALL_REFLECTION_TASK_SLOT = 37,
    AKROPOLIS_WEST_ELEVATOR_HALL_REFLECTION_FLOOR     = 0,
    AKROPOLIS_WEST_ELEVATOR_HALL_REFLECTION_PLANE     = 1,
};

/// The view the three red beacons show in, where the two small ones sit (the large one.s position is a marker), and
/// their pulse rates and sizes.
enum {
    AKROPOLIS_WEST_ELEVATOR_HALL_BEACON_VIEW       = 2,
    AKROPOLIS_WEST_ELEVATOR_HALL_BEACON_SMALL_X    = -6144,
    AKROPOLIS_WEST_ELEVATOR_HALL_BEACON_SMALL_Y    = -1264,
    AKROPOLIS_WEST_ELEVATOR_HALL_BEACON_LARGE_RATE = 4,
    AKROPOLIS_WEST_ELEVATOR_HALL_BEACON_LARGE_SIZE = 24,
    AKROPOLIS_WEST_ELEVATOR_HALL_BEACON_SMALL_RATE = 3,
    AKROPOLIS_WEST_ELEVATOR_HALL_BEACON_SMALL_SIZE = 8,
};

#endif
