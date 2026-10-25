#ifndef STAGES_AKROPOLIS_ROOMS_ROOF_GARDEN_CONFIG_H
#define STAGES_AKROPOLIS_ROOMS_ROOF_GARDEN_CONFIG_H

// Configuration of the Akropolis roof garden.

#include "main/areas.h"
#include "main/sound_ids.h"
#include "main/sound_types.h"
#include "../../room_ids.h"

// ---------------------------------------------------------------------------
// Sounds

/// A sound of the roof garden's area bank.
#define AKROPOLIS_ROOF_GARDEN_SOUND(cue) SOUND_AREA(GAME_STAGE_AKROPOLIS, AKROPOLIS_ROOM_ID_ROOF_GARDEN, (cue))

/// Entries of the roof garden's area bank.
enum {
    AKROPOLIS_ROOF_GARDEN_SOUND_DOOR_OPEN          = 1,
    AKROPOLIS_ROOF_GARDEN_SOUND_DOOR_CLOSE         = 2,
    AKROPOLIS_ROOF_GARDEN_SOUND_DOOR_LOCKED        = 3, // Also room sound cue 3
    AKROPOLIS_ROOF_GARDEN_SOUND_AMBIENCE           = 5,
    AKROPOLIS_ROOF_GARDEN_SOUND_ARRIVAL_MOVEMENT_1 = 6, // The arrival scene's opening, in order
    AKROPOLIS_ROOF_GARDEN_SOUND_ARRIVAL_MOVEMENT_2 = 7,
    AKROPOLIS_ROOF_GARDEN_SOUND_ARRIVAL_MOVEMENT_3 = 8,
    AKROPOLIS_ROOF_GARDEN_SOUND_ARRIVAL_MOVEMENT_4 = 9,
    AKROPOLIS_ROOF_GARDEN_SOUND_ARRIVAL_MOVEMENT_5 = 10,
    AKROPOLIS_ROOF_GARDEN_SOUND_SWAT_MEMBER_STEP_1 = 13, // The SWAT member on the radio (roof_garden_swat_member)
    AKROPOLIS_ROOF_GARDEN_SOUND_SWAT_MEMBER_STEP_2 = 14,
    AKROPOLIS_ROOF_GARDEN_SOUND_STEP_1             = 15, // The arrival scene's close, and the SWAT member's last clip
    AKROPOLIS_ROOF_GARDEN_SOUND_STEP_2             = 16,
    AKROPOLIS_ROOF_GARDEN_SOUND_RADIO_CLICK        = 17, // The SWAT member's radio
    AKROPOLIS_ROOF_GARDEN_SOUND_CRAWLER_DROP       = 18,
};

// ---------------------------------------------------------------------------
// Scenes

/// The warp the arrival scene comes in through.
enum { AKROPOLIS_ROOF_GARDEN_ARRIVAL_SCENE_WARP = 2 };

/// The sanctuary's placement variant once Rupert has left.
enum { AKROPOLIS_ROOF_GARDEN_SANCTUARY_RUPERT_LEFT_VARIANT = 3 };

/// The pickup's object, and its states.
enum {
    AKROPOLIS_ROOF_GARDEN_PICKUP_OBJECT        = 0x13,
    AKROPOLIS_ROOF_GARDEN_PICKUP_STATE_UNSET   = 0,
    AKROPOLIS_ROOF_GARDEN_PICKUP_STATE_PRESENT = 1,
    AKROPOLIS_ROOF_GARDEN_PICKUP_STATE_HIDDEN  = 2,
};

#endif
