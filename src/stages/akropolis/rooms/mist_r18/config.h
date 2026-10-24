#ifndef STAGES_AKROPOLIS_ROOMS_MIST_R18_CONFIG_H
#define STAGES_AKROPOLIS_ROOMS_MIST_R18_CONFIG_H

// Configuration of the MIST briefing room (r18).

#include "main/areas.h"
#include "main/sound_ids.h"
#include "main/sound_types.h"
#include "../../room_ids.h"

// ---------------------------------------------------------------------------
// Sounds

/// A sound of the briefing room's area bank.
#define MIST_R18_SOUND(cue) SOUND_AREA(GAME_STAGE_AKROPOLIS, AKROPOLIS_ROOM_ID_MIST_R18, (cue))

/// Entries of the briefing room's area bank, played by the briefing's script.
enum {
    MIST_R18_SOUND_BRIEFING_BALDWIN_GESTURE   = 3,
    MIST_R18_SOUND_BRIEFING_HELD_MODEL_REVEAL = 4,
};

// ---------------------------------------------------------------------------
// Departure

/// The parking lot's arrival after the briefing: its warp and room state.
enum {
    MIST_R18_PARKING_ENTRY_WARP = 3,
    MIST_R18_PARKING_ENTRY_ROOM = 3,
};

#endif
