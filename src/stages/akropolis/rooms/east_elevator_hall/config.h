#ifndef STAGES_AKROPOLIS_ROOMS_EAST_ELEVATOR_HALL_CONFIG_H
#define STAGES_AKROPOLIS_ROOMS_EAST_ELEVATOR_HALL_CONFIG_H

// Configuration of the Akropolis east elevator hall.

#include "main/areas.h"
#include "main/sound_ids.h"
#include "main/sound_types.h"
#include "../../room_ids.h"

// ---------------------------------------------------------------------------
// Sounds

/// A sound of the hall's area bank.
#define AKROPOLIS_EAST_ELEVATOR_HALL_SOUND(cue) SOUND_AREA(GAME_STAGE_AKROPOLIS, AKROPOLIS_ROOM_ID_EAST_ELEVATOR_HALL, (cue))

/// Entries of the hall's area bank.
enum {
    AKROPOLIS_EAST_ELEVATOR_HALL_SOUND_ELEVATOR_DEPARTURE = 1, // Leaving through the elevator door
    AKROPOLIS_EAST_ELEVATOR_HALL_SOUND_ELEVATOR_ARRIVAL   = 2, // Arriving through the elevator door
    AKROPOLIS_EAST_ELEVATOR_HALL_SOUND_OPENING            = 3, // The opening scene
};

/// Footsteps on the hall's floor.
enum {
    AKROPOLIS_EAST_ELEVATOR_HALL_FOOTSTEP_WALK = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x11,
    AKROPOLIS_EAST_ELEVATOR_HALL_FOOTSTEP_RUN  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x13,
};

// ---------------------------------------------------------------------------
// Doors

/// Trigger parameter of the elevator door: its departure warp and the square's arrival warp.
enum { AKROPOLIS_EAST_ELEVATOR_HALL_TRIGGER_SQUARE_WARPS = (1 << 4) | 2 };

// ---------------------------------------------------------------------------
// The opening

/// The room action that starts the opening: the wounded SWAT member's "Freeze!", his plea to save the civilian in
/// the cafeteria, and his "Watch your back!" (CAP 1).
enum { AKROPOLIS_EAST_ELEVATOR_HALL_OPENING_ACTION = 0 };

/// Room commands and CAP sequences.
enum {
    AKROPOLIS_EAST_ELEVATOR_HALL_ROOM_COMMAND_URGE_HURRY = 2,  // Talking to him again: "Go! Hurry..." (CAP 2)
    AKROPOLIS_EAST_ELEVATOR_HALL_SKIP_CAP_SEQUENCE       = 16, // The opening's last step, when it is skipped
};

/// The wounded SWAT member: placed actor 0.
enum { AKROPOLIS_EAST_ELEVATOR_HALL_SWAT_MEMBER_ACTOR = 0 };

// ---------------------------------------------------------------------------
// Tasks and messages

/// Player reflections: their task and its two modes.
enum {
    AKROPOLIS_EAST_ELEVATOR_HALL_REFLECTION_TASK_BANK = 1,
    AKROPOLIS_EAST_ELEVATOR_HALL_REFLECTION_TASK_SLOT = 37,
    AKROPOLIS_EAST_ELEVATOR_HALL_REFLECTION_FLOOR     = 0,
    AKROPOLIS_EAST_ELEVATOR_HALL_REFLECTION_PLANE     = 1,
};

// ---------------------------------------------------------------------------
// Tuning

/// The view the six red beacons show in, the wall they sit on (the first's position is a marker), and their pulse
/// rates and sizes.
enum {
    AKROPOLIS_EAST_ELEVATOR_HALL_BEACON_VIEW       = 2,
    AKROPOLIS_EAST_ELEVATOR_HALL_BEACON_WALL_X     = 5632,
    AKROPOLIS_EAST_ELEVATOR_HALL_BEACON_SMALL_RATE = 3,
    AKROPOLIS_EAST_ELEVATOR_HALL_BEACON_SMALL_SIZE = 12,
    AKROPOLIS_EAST_ELEVATOR_HALL_BEACON_LARGE_RATE = 4,
    AKROPOLIS_EAST_ELEVATOR_HALL_BEACON_LARGE_SIZE = 18,
};

/// Gray level of the point tile.
enum { AKROPOLIS_EAST_ELEVATOR_HALL_POINT_TILE_GRAY = 0x80 };

/// Where the debug display prints the player's label.
enum {
    AKROPOLIS_EAST_ELEVATOR_HALL_PLAYER_DEBUG_X = -140,
    AKROPOLIS_EAST_ELEVATOR_HALL_PLAYER_DEBUG_Y = -50,
};

#endif
