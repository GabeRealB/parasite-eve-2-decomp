#ifndef STAGES_AKROPOLIS_ROOMS_FIRE_ESCAPE_CONFIG_H
#define STAGES_AKROPOLIS_ROOMS_FIRE_ESCAPE_CONFIG_H

// Configuration of the Akropolis fire escape.

#include "main/areas.h"
#include "main/sound_ids.h"
#include "main/sound_types.h"
#include "../../room_ids.h"

// ---------------------------------------------------------------------------
// Sounds

/// A sound of the fire escape's area bank.
#define AKROPOLIS_FIRE_ESCAPE_SOUND(cue) SOUND_AREA(GAME_STAGE_AKROPOLIS, AKROPOLIS_ROOM_ID_FIRE_ESCAPE, (cue))

/// Entries of the fire escape's area bank; entry 5, the ambience, is SOUND_AKROPOLIS_FIRE_ESCAPE_AMBIENCE.
enum {
    AKROPOLIS_FIRE_ESCAPE_SOUND_SCENE_START       = 1, // The telephone call's scene
    AKROPOLIS_FIRE_ESCAPE_SOUND_BRIDGE_DOOR_OPEN  = 2,
    AKROPOLIS_FIRE_ESCAPE_SOUND_BRIDGE_DOOR_CLOSE = 3,
    AKROPOLIS_FIRE_ESCAPE_SOUND_SCENE_END         = 4,
    AKROPOLIS_FIRE_ESCAPE_SOUND_LIGHT_FLICKER     = 6,
    AKROPOLIS_FIRE_ESCAPE_SOUND_SCENE_PLAYBACK    = 7,
    AKROPOLIS_FIRE_ESCAPE_SOUND_SCENE_AFTER       = 8,
};

// ---------------------------------------------------------------------------
// Commands

/// Room commands.
enum {
    AKROPOLIS_FIRE_ESCAPE_ROOM_COMMAND_ACTOR_DIALOGUE   = 1,
    AKROPOLIS_FIRE_ESCAPE_ROOM_COMMAND_ADVANCE_DIALOGUE = 3,
    AKROPOLIS_FIRE_ESCAPE_ROOM_COMMAND_TELEPHONE        = 4, // CAP 11 the first time, the call's scene after
};

#endif
