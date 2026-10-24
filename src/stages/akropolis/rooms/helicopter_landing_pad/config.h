#ifndef STAGES_AKROPOLIS_ROOMS_HELICOPTER_LANDING_PAD_CONFIG_H
#define STAGES_AKROPOLIS_ROOMS_HELICOPTER_LANDING_PAD_CONFIG_H

// Configuration of the Akropolis helicopter landing pad.

#include "main/areas.h"
#include "main/sound_ids.h"
#include "main/sound_types.h"
#include "../../room_ids.h"

// ---------------------------------------------------------------------------
// Sounds

/// A sound of the landing pad's area bank.
#define AKROPOLIS_HELICOPTER_LANDING_PAD_SOUND(cue) SOUND_AREA(GAME_STAGE_AKROPOLIS, AKROPOLIS_ROOM_ID_HELICOPTER_LANDING_PAD, (cue))

/// Entries of the landing pad's area bank.
enum {
    AKROPOLIS_HELICOPTER_LANDING_PAD_SOUND_LIGHT_SPARK            = 1,
    AKROPOLIS_HELICOPTER_LANDING_PAD_SOUND_LIFT_MOVE              = 3,
    AKROPOLIS_HELICOPTER_LANDING_PAD_SOUND_AMBIENCE               = 5,
    AKROPOLIS_HELICOPTER_LANDING_PAD_SOUND_ENCOUNTER_INTRO_IMPACT = 6,
};

#endif
