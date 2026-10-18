#ifndef WEAPONS_P08_CONFIG_H
#define WEAPONS_P08_CONFIG_H

// Configuration of the P08, its snail-magazine variant and the Mongoose.

#include "main/sound_ids.h"
#include "weapons/sounds.h"
#include "weapons/weapon_index.h"

// ---------------------------------------------------------------------------
// Sounds

/// A sound of the bank of the package being built (`WEAPON_INDEX`), by slot (`WEAPON_SOUND_SLOT_*`).
#define P08_SOUND(slot) SOUND_WEAPON(WEAPON_INDEX, (slot))

// ---------------------------------------------------------------------------
// Attack

/// Attack tuning and ids; times in dispatch ticks.
enum {
    P08_READY_BLEND_FRAMES        = 1,
    P08_MOVING_READY_BLEND_FRAMES = 5,
    P08_CANCEL_FRAMES             = 0xB,
    P08_FLASH_PROFILE_HANDGUN     = 1,
    P08_FLASH_PROFILE_MONGOOSE    = 9,
};

#endif // WEAPONS_P08_CONFIG_H
