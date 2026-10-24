#ifndef STAGES_AKROPOLIS_ROOMS_MIST_SHOOTING_GALLERY_CONFIG_H
#define STAGES_AKROPOLIS_ROOMS_MIST_SHOOTING_GALLERY_CONFIG_H

// Configuration of MIST's shooting gallery.

#include "main/areas.h"
#include "main/sound_ids.h"
#include "main/sound_types.h"
#include "../../room_ids.h"

// ---------------------------------------------------------------------------
// Sounds

/// A sound of the gallery's area bank.
#define MIST_SHOOTING_GALLERY_SOUND(cue) SOUND_AREA(GAME_STAGE_AKROPOLIS, AKROPOLIS_ROOM_ID_MIST_SHOOTING_GALLERY, (cue))

/// Entries of the gallery's area bank: the doors (warp 1, warps 2 and 4, warps 5 and 6) and the courses.
enum {
    MIST_SHOOTING_GALLERY_SOUND_DOOR_1_OPEN            = 1,
    MIST_SHOOTING_GALLERY_SOUND_DOOR_1_CLOSE           = 2,
    MIST_SHOOTING_GALLERY_SOUND_DOOR_2_OPEN            = 3,
    MIST_SHOOTING_GALLERY_SOUND_DOOR_2_CLOSE           = 4,
    MIST_SHOOTING_GALLERY_SOUND_DOOR_3_OPEN            = 5,
    MIST_SHOOTING_GALLERY_SOUND_DOOR_3_CLOSE           = 6,
    MIST_SHOOTING_GALLERY_SOUND_TARGET_APPEAR          = 7,
    MIST_SHOOTING_GALLERY_SOUND_TARGET_HIT             = 8,
    MIST_SHOOTING_GALLERY_SOUND_TARGET_DEATH           = 9,
    MIST_SHOOTING_GALLERY_SOUND_STORY_PIERCE_CUE       = 11,
    MIST_SHOOTING_GALLERY_SOUND_GALLERY_DEMO           = 13,
    MIST_SHOOTING_GALLERY_SOUND_TARGET_ATTACK          = 14,
    MIST_SHOOTING_GALLERY_SOUND_ROUND_START            = 15,
    MIST_SHOOTING_GALLERY_SOUND_TARGET_KILL_KIND_9     = 16,
    MIST_SHOOTING_GALLERY_SOUND_TARGET_KILL            = 17,
    MIST_SHOOTING_GALLERY_SOUND_TARGET_KILL_KIND_10_UP = 18,
    MIST_SHOOTING_GALLERY_SOUND_TARGET_CHARGE          = 19,
};

#endif
