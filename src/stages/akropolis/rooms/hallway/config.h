#ifndef STAGES_AKROPOLIS_ROOMS_HALLWAY_CONFIG_H
#define STAGES_AKROPOLIS_ROOMS_HALLWAY_CONFIG_H

// Configuration of the Akropolis hallway.

#include "main/areas.h"
#include "main/sound_ids.h"
#include "main/sound_types.h"
#include "../../room_ids.h"

// ---------------------------------------------------------------------------
// Sounds

/// A sound of the hallway's area bank.
#define AKROPOLIS_HALLWAY_SOUND(cue) SOUND_AREA(GAME_STAGE_AKROPOLIS, AKROPOLIS_ROOM_ID_HALLWAY, (cue))

/// Entries of the hallway's area bank the room's sound cues play.
enum {
    AKROPOLIS_HALLWAY_SOUND_KEY_BOX_OPEN  = 6, // Also room sound cue 6
    AKROPOLIS_HALLWAY_SOUND_KEY_BOX_CLOSE = 7, // Also room sound cue 7
};

// ---------------------------------------------------------------------------
// Doors

/// The cafeteria door held until MIST is contacted, and the CAP command it plays.
enum {
    AKROPOLIS_HALLWAY_CAFETERIA_HELD_WARP    = 3,
    AKROPOLIS_HALLWAY_CAFETERIA_HELD_COMMAND = 1, // "This one's locked, too."
};

#endif
