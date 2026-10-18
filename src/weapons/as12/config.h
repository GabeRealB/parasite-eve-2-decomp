#ifndef WEAPONS_AS12_CONFIG_H
#define WEAPONS_AS12_CONFIG_H

// Configuration of the AS12.

#include "main/sound_ids.h"
#include "weapons/sounds.h"
#include "weapons/weapon_index.h"

// ---------------------------------------------------------------------------
// Sounds

/// A sound of the AS12's bank, by slot (`WEAPON_SOUND_SLOT_*` or `SHOTGUN_SOUND_SLOT_*`).
#define AS12_SOUND(slot) SOUND_WEAPON(WEAPON_INDEX_AS12, (slot))

// ---------------------------------------------------------------------------
// Attack

/// Attack tuning and ids; times in dispatch ticks.
enum {
    AS12_READY_BLEND_FRAMES        = 1,
    AS12_MOVING_READY_BLEND_FRAMES = 5,
    AS12_COOLDOWN_FRAMES           = 0x21,
    AS12_CANCEL_FRAMES             = 0x16,
};

#endif // WEAPONS_AS12_CONFIG_H
