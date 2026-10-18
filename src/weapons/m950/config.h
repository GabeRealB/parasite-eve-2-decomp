#ifndef WEAPONS_M950_CONFIG_H
#define WEAPONS_M950_CONFIG_H

// Configuration of the M950.

#include "main/sound_ids.h"
#include "weapons/sounds.h"
#include "weapons/weapon_index.h"

// ---------------------------------------------------------------------------
// Sounds

/// A sound of the M950's bank, by slot (`WEAPON_SOUND_SLOT_*`).
#define M950_SOUND(slot) SOUND_WEAPON(WEAPON_INDEX_M950, (slot))

// ---------------------------------------------------------------------------
// Attack

/// Attack tuning and ids; times in dispatch ticks.
enum {
    M950_READY_BLEND_FRAMES        = 1,
    M950_MOVING_READY_BLEND_FRAMES = 5,
    M950_SHOT_DELAY_FRAMES         = 4,
    M950_CANCEL_FRAMES             = 9,
};

#endif // WEAPONS_M950_CONFIG_H
