#ifndef WEAPONS_MP5A5_CONFIG_H
#define WEAPONS_MP5A5_CONFIG_H

// Configuration of the MP5A5 and its upgrades.

#include "gameplay/world_collision_types.h"
#include "main/sound_ids.h"
#include "weapons/sounds.h"

// ---------------------------------------------------------------------------
// Sounds

/// A sound of the bank of the package being built (`WEAPON_INDEX`), by slot (`WEAPON_SOUND_SLOT_*`, or one below).
#define MP5A5_SOUND(slot) SOUND_WEAPON(WEAPON_INDEX, (slot))

enum {
    MP5A5_SOUND_SECONDARY_SHOT = 5, // A round of the second load
};

// ---------------------------------------------------------------------------
// Attack

/// Attack tuning and ids; times in dispatch ticks.
enum {
    MP5A5_READY_BLEND_FRAMES        = 1,
    MP5A5_MOVING_READY_BLEND_FRAMES = 5,
    MP5A5_SECONDARY_SPREAD_RADIUS   = 0xC00,
    MP5A5_SECONDARY_COOLDOWN_FRAMES = 0x12,
    MP5A5_SECONDARY_ACTIVE_FRAMES   = 0x12,
    MP5A5_PRIMARY_SHOT_DELAY_FRAMES = 3,
    MP5A5_SECONDARY_KEY_BASE        = WORLD_COLLISION_CONTACT_ATTACK | 0x16,
};

#endif // WEAPONS_MP5A5_CONFIG_H
