#ifndef WEAPONS_PA3_CONFIG_H
#define WEAPONS_PA3_CONFIG_H

// Configuration of the PA3 and SP12.

#include "main/sound_ids.h"
#include "weapons/sounds.h"
#include "weapons/weapon_index.h"

// ---------------------------------------------------------------------------
// Sounds

/// A sound of the bank of the package being built (`WEAPON_INDEX`), by slot (`WEAPON_SOUND_SLOT_*`, `SHOTGUN_SOUND_SLOT_*`, or one below).
#define PA3_SOUND(slot) SOUND_WEAPON(WEAPON_INDEX, (slot))

enum {
    /// The pump: the shotguns' banks keep it in the reload-start slot, which their reload plays as its last cue.
    PA3_SOUND_PUMP = WEAPON_SOUND_SLOT_RELOAD_START,
};

// ---------------------------------------------------------------------------
// Attack

/// Attack tuning and ids; times in dispatch ticks.
enum {
    PA3_READY_BLEND_FRAMES        = 1,
    PA3_MOVING_READY_BLEND_FRAMES = 8,
    PA3_PUMP_DELAY_FRAMES         = 0x1F,
};

#endif // WEAPONS_PA3_CONFIG_H
