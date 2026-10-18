#ifndef WEAPONS_M93R_CONFIG_H
#define WEAPONS_M93R_CONFIG_H

// Configuration of the M93R.

#include "main/sound_ids.h"
#include "weapons/sounds.h"
#include "weapons/weapon_index.h"

// ---------------------------------------------------------------------------
// Sounds

/// A sound of the M93R's bank (weapons/m93r/sounds), by slot (`WEAPON_SOUND_SLOT_*`).
#define M93R_SOUND(slot) SOUND_WEAPON(WEAPON_INDEX_M93R, (slot))

// ---------------------------------------------------------------------------
// Attack

/// Burst and recovery timing, in dispatch ticks.
enum {
    M93R_BURST_ROUNDS              = 3,   // Rounds of a primary-button burst; the secondary button fires one
    M93R_BURST_DELAY_FRAMES        = 1,   // Ticks between the rounds of a burst
    M93R_CANCEL_FRAMES             = 0xA, // Recovery ticks before a held action input can cancel it
    M93R_HELD_COOLDOWN_FRAMES      = 0xA, // Cooldown after a recovery cancelled by the held input
    M93R_READY_BLEND_FRAMES        = 1,   // Blend into the ready pose standing still
    M93R_MOVING_READY_BLEND_FRAMES = 6,   // Blend into the ready pose while moving or turning
};

#endif // WEAPONS_M93R_CONFIG_H
