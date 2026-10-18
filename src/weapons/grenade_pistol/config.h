#ifndef WEAPONS_GRENADE_PISTOL_CONFIG_H
#define WEAPONS_GRENADE_PISTOL_CONFIG_H

// Configuration of the Grenade Pistol and the MM1.

#include "main/sound_ids.h"
#include "weapons/weapon_index.h"

// ---------------------------------------------------------------------------
// Sounds

/// A sound of the bank of the package being built (`WEAPON_INDEX`), by slot (`WEAPON_SOUND_SLOT_*`, or one below).
#define GRENADE_PISTOL_SOUND(slot) SOUND_WEAPON(GRENADE_PISTOL_WEAPON_INDEX, (slot))

enum {
    GRENADE_PISTOL_SOUND_LAUNCH = 4, // The bank's shot slot (the detonation is public)
};

// ---------------------------------------------------------------------------
// Attack

/// Attack tuning and ids; times in dispatch ticks.
enum {
    GRENADE_PISTOL_READY_BLEND_FRAMES  = 1,
    GRENADE_PISTOL_MOVING_BLEND_FRAMES = 8,
    GRENADE_PISTOL_FIRE_BLEND_FRAMES   = 3,
    GRENADE_PISTOL_SHOT_COOLDOWN_TICKS = 40,
};

#endif // WEAPONS_GRENADE_PISTOL_CONFIG_H
