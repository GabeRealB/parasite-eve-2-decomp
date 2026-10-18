#ifndef WEAPONS_TONFA_BATON_CONFIG_H
#define WEAPONS_TONFA_BATON_CONFIG_H

// Configuration of the Tonfa Baton.

#include "main/sound_ids.h"
#include "weapons/weapon_index.h"

// ---------------------------------------------------------------------------
// Sounds

/// A sound of the Tonfa Baton's bank, by slot (`WEAPON_SOUND_SLOT_*`, or one below).
#define TONFA_BATON_SOUND(slot) SOUND_WEAPON(WEAPON_INDEX_TONFA_BATON, (slot))

/// The Tonfa Baton's own slots.
enum {
    TONFA_BATON_SOUND_FIRST_SWING   = 1,
    TONFA_BATON_SOUND_COMBO_SWING   = 2,
    TONFA_BATON_SOUND_FIRST_CONTACT = 3,
    TONFA_BATON_SOUND_COMBO_CONTACT = 4,
};

// ---------------------------------------------------------------------------
// Attack

/// Attack tuning and ids; times in dispatch ticks.
enum {
    TONFA_BATON_FIRST_STRIKE_KEY          = 0x21317,
    TONFA_BATON_COMBO_STRIKE_KEY          = 0x21315,
    TONFA_BATON_WINDUP_FRAMES             = 8,
    TONFA_BATON_COMBO_ADVANCE_FRAMES      = 0xC,
    TONFA_BATON_RECOVERY_ADVANCE_FRAMES   = 0xA,
    TONFA_BATON_COMBO_CONTACT_FRAMES      = 9,
    TONFA_BATON_COMBO_CONTACT_START_TICKS = 3,
    /// Divisor that turns the model's forward axis into the distance a baton
    /// strike carries the actor per frame: 4096 / 84, about 48 coordinate units.
    TONFA_BATON_ATTACK_ADVANCE_DIVISOR = 84,
};

// ---------------------------------------------------------------------------
// Trail

/// The swing's ribbon trail (`tonfaBatonSwingTrailTask`): times in dispatch ticks, tints as RGB444.
enum {
    TONFA_BATON_TRAIL_LIFETIME_TICKS = 31,
    TONFA_BATON_TRAIL_TINT_BLUE      = 0x001, // RGB multipliers (0, 0, 1)
    TONFA_BATON_TRAIL_TINT_CYAN      = 0x013, // RGB multipliers (0, 1, 3)
};

// ---------------------------------------------------------------------------
// Model

/// The baton's swing pose (`tonfaBatonModelTask`): steps in 4096 per turn, offsets in game units.
enum {
    TONFA_BATON_POSE_RETURN_STEP = 0x100,
    TONFA_BATON_POSE_STRIKE_STEP = 0x1C0,
    TONFA_BATON_GRIP_OFFSET_Y    = 0x60,
};

#endif // WEAPONS_TONFA_BATON_CONFIG_H
