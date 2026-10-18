#ifndef WEAPONS_M249_CONFIG_H
#define WEAPONS_M249_CONFIG_H

// Configuration of the M249.

#include "main/sound_ids.h"
#include "weapons/sounds.h"
#include "weapons/weapon_index.h"

// ---------------------------------------------------------------------------
// Sounds

/// A sound of the M249's bank, by slot (`WEAPON_SOUND_SLOT_*`).
#define M249_SOUND(slot) SOUND_WEAPON(WEAPON_INDEX_M249, (slot))

// ---------------------------------------------------------------------------
// Attack

/// Attack tuning and ids; times in dispatch ticks.
enum {
    M249_READY_BLEND_FRAMES        = 1,
    M249_MOVING_READY_BLEND_FRAMES = 8,
    M249_SHOT_DELAY_FRAMES         = 2,
};

#endif // WEAPONS_M249_CONFIG_H
