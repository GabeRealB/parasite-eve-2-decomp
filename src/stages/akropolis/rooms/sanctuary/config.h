#ifndef STAGES_AKROPOLIS_ROOMS_SANCTUARY_CONFIG_H
#define STAGES_AKROPOLIS_ROOMS_SANCTUARY_CONFIG_H

// Configuration of the Akropolis sanctuary.

#include "main/areas.h"
#include "main/sound_ids.h"
#include "main/sound_types.h"
#include "../../room_ids.h"

// ---------------------------------------------------------------------------
// Sounds

/// A sound of the sanctuary's area bank.
#define AKROPOLIS_SANCTUARY_SOUND(cue) SOUND_AREA(GAME_STAGE_AKROPOLIS, AKROPOLIS_ROOM_ID_SANCTUARY, (cue))

/// Entries of the sanctuary's area bank.
enum {
    AKROPOLIS_SANCTUARY_SOUND_PROMENADE_DOOR_OPEN    = 1,
    AKROPOLIS_SANCTUARY_SOUND_PROMENADE_DOOR_CLOSE   = 2,
    AKROPOLIS_SANCTUARY_SOUND_ROOF_GARDEN_DOOR_OPEN  = 3,
    AKROPOLIS_SANCTUARY_SOUND_ROOF_GARDEN_DOOR_CLOSE = 4,
    AKROPOLIS_SANCTUARY_SOUND_ITEM_LID_OPEN          = 5,
    AKROPOLIS_SANCTUARY_SOUND_ITEM_LID_CLOSE         = 6,
    AKROPOLIS_SANCTUARY_SOUND_RADIO_CHATTER          = 7, // Through the bomb scene
    AKROPOLIS_SANCTUARY_SOUND_RADIO_CLICK            = 8, // Its end
};

// ---------------------------------------------------------------------------
// Scenes

/// The warp the arrival scene comes in through, and its scene event.
enum {
    AKROPOLIS_SANCTUARY_SCENE_ARRIVAL_WARP  = 3,
    AKROPOLIS_SANCTUARY_ARRIVAL_SCENE_EVENT = 6,
};

/// The room action that starts the bomb scene, and the room command of Rupert in the doorway.
enum {
    AKROPOLIS_SANCTUARY_ROOM_ACTION_BOMB_SCENE = 1,
    AKROPOLIS_SANCTUARY_ROOM_COMMAND_RUPERT    = 0,
};

/// Placed actors: Rupert.
enum { AKROPOLIS_SANCTUARY_RUPERT_ACTOR = 1 };

#endif
