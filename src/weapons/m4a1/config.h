#ifndef WEAPONS_M4A1_CONFIG_H
#define WEAPONS_M4A1_CONFIG_H

// Configuration of the M4A1 Rifle and its upgrades.

#include "main/sound_ids.h"
#include "weapons/sounds.h"

// ---------------------------------------------------------------------------
// Sounds

/// A sound of the bank of the M4A1 Rifle or its upgrade being built, by slot (`WEAPON_SOUND_SLOT_*`).
#define M4A1_SOUND(slot) SOUND_WEAPON(WEAPON_INDEX, (slot))

// ---------------------------------------------------------------------------
// Attack

/// Attack tuning and ids; times in dispatch ticks.
enum {
    M4A1_READY_BLEND_FRAMES        = 1,
    M4A1_MOVING_READY_BLEND_FRAMES = 6,
    M4A1_BURST_ROUNDS              = 3,
    M4A1_SHOT_DELAY_FRAMES         = 3,
    M4A1_IMPACT_DELAY_FRAMES       = 2,
    M4A1_CANCEL_FRAMES             = 9,
    M4A1_RECOVERY_COOLDOWN_FRAMES  = 0xC,
};

#endif // WEAPONS_M4A1_CONFIG_H
