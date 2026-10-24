#ifndef STAGES_AKROPOLIS_ROOMS_SECURITY_ROOM_CONFIG_H
#define STAGES_AKROPOLIS_ROOMS_SECURITY_ROOM_CONFIG_H

// Configuration of the Akropolis security room.

#include "main/areas.h"
#include "main/sound_ids.h"
#include "main/sound_types.h"
#include "../../room_ids.h"

// ---------------------------------------------------------------------------
// Sounds

/// A sound of the security room's area bank.
#define AKROPOLIS_SECURITY_ROOM_SOUND(cue) SOUND_AREA(GAME_STAGE_AKROPOLIS, AKROPOLIS_ROOM_ID_SECURITY_ROOM, (cue))

/// Entries of the area bank the room's code plays; the others are named in main/sound_ids.h.
enum {
    AKROPOLIS_SECURITY_ROOM_SOUND_UNLOCK_FADE = 2, // The screen fades out after a lock is released
};

#endif
