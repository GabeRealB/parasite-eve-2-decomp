#ifndef STAGES_AKROPOLIS_ROOMS_BRIDGE_CONFIG_H
#define STAGES_AKROPOLIS_ROOMS_BRIDGE_CONFIG_H

// Configuration of the Akropolis bridge.

#include "main/areas.h"
#include "main/sound_ids.h"
#include "main/sound_types.h"
#include "../../room_ids.h"

// ---------------------------------------------------------------------------
// Sounds

/// A sound of the bridge's area bank.
#define AKROPOLIS_BRIDGE_SOUND(cue) SOUND_AREA(GAME_STAGE_AKROPOLIS, AKROPOLIS_ROOM_ID_BRIDGE, (cue))

/// Entries of the bridge's area bank.
enum {
    AKROPOLIS_BRIDGE_SOUND_KEYPAD_BEEP            = 3,
    AKROPOLIS_BRIDGE_SOUND_CODE_REJECTED          = 4,
    AKROPOLIS_BRIDGE_SOUND_FIRE_ESCAPE_DOOR_OPEN  = 5,
    AKROPOLIS_BRIDGE_SOUND_FIRE_ESCAPE_DOOR_CLOSE = 6,
    AKROPOLIS_BRIDGE_SOUND_PROMENADE_DOOR_OPEN    = 7,
    AKROPOLIS_BRIDGE_SOUND_PROMENADE_DOOR_CLOSE   = 8,
    AKROPOLIS_BRIDGE_SOUND_CODE_ACCEPTED          = 9,
};

// ---------------------------------------------------------------------------
// Doors

/// The sanctuary's placement variant once Rupert has left.
enum { AKROPOLIS_BRIDGE_SANCTUARY_RUPERT_LEFT_VARIANT = 3 };

#endif
