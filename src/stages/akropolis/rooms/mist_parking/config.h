#ifndef STAGES_AKROPOLIS_ROOMS_MIST_PARKING_CONFIG_H
#define STAGES_AKROPOLIS_ROOMS_MIST_PARKING_CONFIG_H

// Configuration of MIST's parking lot.

#include "main/areas.h"
#include "main/sound_ids.h"
#include "main/sound_types.h"
#include "../../room_ids.h"

// ---------------------------------------------------------------------------
// Sounds

/// A sound of the parking lot's area bank.
#define MIST_PARKING_SOUND(cue) SOUND_AREA(GAME_STAGE_AKROPOLIS, AKROPOLIS_ROOM_ID_MIST_PARKING, (cue))

/// Entries of the parking lot's area bank.
enum {
    MIST_PARKING_SOUND_ROOM_CHANGE_DOOR_OPEN     = 1,
    MIST_PARKING_SOUND_ROOM_CHANGE_DOOR_CLOSE    = 2,
    MIST_PARKING_SOUND_CUTSCENE_START            = 3,
    MIST_PARKING_SOUND_CUTSCENE_END              = 4,
    MIST_PARKING_SOUND_ARRIVAL_MODEL_REVEAL      = 5,
    MIST_PARKING_SOUND_ARRIVAL_PIERCE_WALK_STEPS = 6,
    MIST_PARKING_SOUND_ARRIVAL_PIERCE_GESTURE    = 8,
    MIST_PARKING_SOUND_CUTSCENE_PLAYBACK         = 11,
    MIST_PARKING_SOUND_ARRIVAL_STEPS_2           = 12,
    MIST_PARKING_SOUND_ARRIVAL_STEPS_3           = 13,
    MIST_PARKING_SOUND_ARRIVAL_PIERCE_AIM        = 14,
    MIST_PARKING_SOUND_PIERCE_STEP_1             = 15,
    MIST_PARKING_SOUND_PIERCE_STEP_2             = 16,
    MIST_PARKING_SOUND_ARRIVAL_PLAYER_GESTURE    = 17,
    MIST_PARKING_SOUND_CUTSCENE_AFTER            = 18,
    MIST_PARKING_SOUND_PIERCE_STEP_ALTERNATE     = 19,
    MIST_PARKING_SOUND_DOOR_OPEN                 = 20,
    MIST_PARKING_SOUND_DOOR_CLOSE                = 21,
};

#endif
