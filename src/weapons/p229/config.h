#ifndef WEAPONS_P229_CONFIG_H
#define WEAPONS_P229_CONFIG_H

// Configuration of the P229.

#include "gameplay/world_collision_types.h"
#include "main/sound_ids.h"
#include "weapons/sounds.h"
#include "weapons/weapon_index.h"

// ---------------------------------------------------------------------------
// Sounds

/// A sound of the P229's bank, by slot (`WEAPON_SOUND_SLOT_*`, or one below).
#define P229_SOUND(slot) SOUND_WEAPON(WEAPON_INDEX_P229, (slot))

enum {
    P229_SOUND_SECONDARY_SHOT = 5, // A round of the second load
};

// ---------------------------------------------------------------------------
// Attack

/// Attack tuning and ids; times in dispatch ticks.
enum {
    P229_READY_BLEND_FRAMES        = 1,
    P229_MOVING_READY_BLEND_FRAMES = 5,
    P229_SECONDARY_SPREAD_RADIUS   = 0xC00,
    P229_PRIMARY_CANCEL_FRAMES     = 0xA,
    P229_SECONDARY_CANCEL_FRAMES   = 6,
    P229_SECONDARY_COOLDOWN_FRAMES = 0x12,
    P229_PRIMARY_COOLDOWN_FRAMES   = 0xC,
    P229_PRIMARY_KEY_BASE          = WORLD_COLLISION_CONTACT_ATTACK | (WEAPON_INDEX_P229 << 8),
    P229_SECONDARY_KEY             = WORLD_COLLISION_CONTACT_ATTACK | (WEAPON_INDEX_P229 << 8) | 0x16,
};

#endif // WEAPONS_P229_CONFIG_H
