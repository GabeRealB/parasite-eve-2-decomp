#ifndef STAGES_AKROPOLIS_ROOMS_PROMENADE_CONFIG_H
#define STAGES_AKROPOLIS_ROOMS_PROMENADE_CONFIG_H

// Configuration of the Akropolis promenade.

#include "main/areas.h"
#include "main/sound_ids.h"
#include "main/sound_types.h"
#include "../../room_ids.h"

// ---------------------------------------------------------------------------
// Sounds

/// A sound of the promenade's area bank.
#define AKROPOLIS_PROMENADE_SOUND(cue) SOUND_AREA(GAME_STAGE_AKROPOLIS, AKROPOLIS_ROOM_ID_PROMENADE, (cue))

/// Entries of the promenade's area bank.
enum {
    AKROPOLIS_PROMENADE_SOUND_SANCTUARY_DOOR_OPEN   = 5,
    AKROPOLIS_PROMENADE_SOUND_SANCTUARY_DOOR_CLOSE  = 6,
    AKROPOLIS_PROMENADE_SOUND_FAR_DOOR_OPEN         = 7,
    AKROPOLIS_PROMENADE_SOUND_FAR_DOOR_CLOSE        = 8,
    AKROPOLIS_PROMENADE_SOUND_INSTRUMENT_CASE_OPEN  = 9,
    AKROPOLIS_PROMENADE_SOUND_INSTRUMENT_CASE_CLOSE = 10,
    AKROPOLIS_PROMENADE_SOUND_POST_BATTLE_SCENE_1   = 12,
    AKROPOLIS_PROMENADE_SOUND_BRIDGE_DOOR_OPEN      = 13,
    AKROPOLIS_PROMENADE_SOUND_BRIDGE_DOOR_CLOSE     = 14,
    AKROPOLIS_PROMENADE_SOUND_POST_BATTLE_SCENE_2   = 15,
};

/// Room sound cues: 10, and 103, which CAP command 3 sends when it completes.
enum {
    AKROPOLIS_PROMENADE_SOUND_CUE_INSTRUMENT_CASE_OPEN  = 10,
    AKROPOLIS_PROMENADE_SOUND_CUE_INSTRUMENT_CASE_CLOSE = 103,
};

// ---------------------------------------------------------------------------
// Doors

/// Warps: the observatory's stairs arrival, used when the ride does not play, and the sanctuary's scene arrival.
enum {
    AKROPOLIS_PROMENADE_OBSERVATORY_STAIRS_WARP = 1,
    AKROPOLIS_PROMENADE_SANCTUARY_SCENE_WARP    = 3,
    AKROPOLIS_PROMENADE_RIDE_ARRIVAL_WARP       = 4,
};

/// The bridge door's caption while it is refused, and the scene event of the walk into the sanctuary.
enum {
    AKROPOLIS_PROMENADE_BRIDGE_REFUSED_CAP_COMMAND     = 2,
    AKROPOLIS_PROMENADE_SCENE_EVENT_SANCTUARY_APPROACH = 4,
};

#endif
