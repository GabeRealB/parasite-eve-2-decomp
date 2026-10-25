#ifndef STAGES_AKROPOLIS_ROOMS_FOUNTAIN_CONFIG_H
#define STAGES_AKROPOLIS_ROOMS_FOUNTAIN_CONFIG_H

// Configuration of the Akropolis fountain.

#include "main/areas.h"
#include "main/sound_ids.h"
#include "main/sound_types.h"
#include "../../room_ids.h"

// ---------------------------------------------------------------------------
// Sounds

/// A sound of the fountain's area bank.
#define AKROPOLIS_FOUNTAIN_SOUND(cue) SOUND_AREA(GAME_STAGE_AKROPOLIS, AKROPOLIS_ROOM_ID_FOUNTAIN, (cue))

/// Entries of the fountain's area bank; entry 1, the water loop, is SOUND_AKROPOLIS_FOUNTAIN_WATER_LOOP.
enum {
    AKROPOLIS_FOUNTAIN_SOUND_AMMO_CASE_OPEN     = 3, // Also room sound cue 3
    AKROPOLIS_FOUNTAIN_SOUND_DOOR_UNBAR         = 4, // Also room sound cue 4
    AKROPOLIS_FOUNTAIN_SOUND_DOOR_OPEN          = 5, // Leaving through the patio or forked-road door
    AKROPOLIS_FOUNTAIN_SOUND_DOOR_CLOSE         = 6, // Arriving through them
    AKROPOLIS_FOUNTAIN_SOUND_HALLWAY_DOOR_OPEN  = 7,
    AKROPOLIS_FOUNTAIN_SOUND_HALLWAY_DOOR_CLOSE = 8,
    AKROPOLIS_FOUNTAIN_SOUND_AMMO_CASE_CLOSE    = 9, // Also room sound cue 9
};

/// Footsteps on surface classes 4 and 5; the scripted climb uses the walking pair.
enum {
    AKROPOLIS_FOUNTAIN_FOOTSTEP_CLASS4_WALK = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x11,
    AKROPOLIS_FOUNTAIN_FOOTSTEP_CLASS4_RUN  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x13,
    AKROPOLIS_FOUNTAIN_FOOTSTEP_CLASS5_WALK = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x39,
    AKROPOLIS_FOUNTAIN_FOOTSTEP_CLASS5_RUN  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x3B,
};

// ---------------------------------------------------------------------------
// Doors

/// The doors' warps; a trigger packs its departure warp in the upper nibble.
enum {
    AKROPOLIS_FOUNTAIN_WARP_PATIO          = 1,
    AKROPOLIS_FOUNTAIN_WARP_FORKED_ROAD    = 2,
    AKROPOLIS_FOUNTAIN_WARP_HALLWAY        = 3,
    AKROPOLIS_FOUNTAIN_WARP_ENDPOINT_SHIFT = 4,
};

// ---------------------------------------------------------------------------
// Commands and tasks

/// Room commands.
enum {
    AKROPOLIS_FOUNTAIN_ROOM_COMMAND_FOUNTAIN    = 3, // Examining the fountain: CAP 3, or CAP 6 once the red key is in
    AKROPOLIS_FOUNTAIN_ROOM_COMMAND_HIDDEN_PATH = 4, // The red-leaved tree's path (CAP 4); enables the climb trigger
};

/// CAP commands.
enum {
    AKROPOLIS_FOUNTAIN_CAP_UNBAR_PATIO_DOOR = 1, // "I can remove the bar from this side."
    AKROPOLIS_FOUNTAIN_CAP_FOUNTAIN         = 3, // "Poseidon's Fountain."
    AKROPOLIS_FOUNTAIN_CAP_HIDDEN_PATH      = 4, // "This is the only tree with red leaves."
    AKROPOLIS_FOUNTAIN_CAP_FOUNTAIN_SWAT    = 6, // "A SWAT member in the fountain..."
};

#endif
