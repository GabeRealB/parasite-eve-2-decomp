#ifndef WEAPONS_M4A1_HAMMER_CONFIG_H
#define WEAPONS_M4A1_HAMMER_CONFIG_H

// Configuration of the M4A1 Hammer.

#include "gameplay/world_collision_types.h"
#include "main/sound_ids.h"
#include "weapons/sounds.h"
#include "weapons/weapon_index.h"

// ---------------------------------------------------------------------------
// Sounds

/// A sound of the M4A1 Hammer's bank, by slot (`WEAPON_SOUND_SLOT_*`, or one below).
#define M4A1_HAMMER_SOUND(slot) SOUND_WEAPON(WEAPON_INDEX_M4A1_HAMMER, (slot))

/// The M4A1 Hammer's own slots.
enum {
    M4A1_HAMMER_SOUND_DISCHARGE = 5,
};

// ---------------------------------------------------------------------------
// Attack

/// Attack tuning and ids; times in dispatch ticks.
enum {
    M4A1_HAMMER_READY_BLEND_FRAMES           = 1,
    M4A1_HAMMER_MOVING_READY_BLEND_FRAMES    = 8,
    M4A1_HAMMER_BURST_ROUNDS                 = 3,
    M4A1_HAMMER_BURST_CANCEL_FRAMES          = 9,
    M4A1_HAMMER_SECONDARY_CANCEL_FRAMES      = 0x1C,
    M4A1_HAMMER_BURST_DELAY_FRAMES           = 3,
    M4A1_HAMMER_RECOVERY_COOLDOWN_FRAMES     = 0xC,
    M4A1_HAMMER_RIFLE_RADIUS                 = 0x100,
    M4A1_HAMMER_DISCHARGE_RADIUS             = 0x400,
    M4A1_HAMMER_SECONDARY_ACTION_FRAMES      = 0x14,
    M4A1_HAMMER_RIFLE_KEY_BASE               = WORLD_COLLISION_CONTACT_ATTACK | (WEAPON_INDEX_M4A1_HAMMER << 8),
    M4A1_HAMMER_DISCHARGE_KEY                = WORLD_COLLISION_CONTACT_ATTACK | (WEAPON_INDEX_M4A1_HAMMER << 8) | 0x1C,
    M4A1_HAMMER_DISCHARGE_REACH              = 2560,
    M4A1_HAMMER_BURST_IMPACT_TICKS_LEFT      = 2,
    M4A1_HAMMER_SECONDARY_TURN_RATE_INDEX    = 2,
    M4A1_HAMMER_DISCHARGE_WINDUP_FRAMES      = 3,
    M4A1_HAMMER_DISCHARGE_CONTACT_TICKS_LEFT = 1,
};

// ---------------------------------------------------------------------------
// Glow

/// The attachment's charge glow (`m4a1HammerGlowTask`): times in dispatch ticks, light radii in game units.
enum {
    M4A1_HAMMER_LIGHT_SLOT           = 1,
    M4A1_HAMMER_LIGHT_REFRESH_FRAMES = 4,
    M4A1_HAMMER_CHARGED_TICKS        = 5,
    M4A1_HAMMER_SPIN_TICK_MASK       = 15,
    M4A1_HAMMER_IDLE_LIGHT_INNER     = 0x80,
    M4A1_HAMMER_IDLE_LIGHT_OUTER     = 0x400,
    M4A1_HAMMER_CHARGED_LIGHT_INNER  = 0x400,
    M4A1_HAMMER_CHARGED_LIGHT_OUTER  = 0x4000,
};

// ---------------------------------------------------------------------------
// Impact flash

/// The discharge's impact flash and beam (`m4a1HammerImpactFlashTask`): times in dispatch ticks, sizes in game units.
enum {
    /// Signed perspective-sizing numerator for the attachment and contact beams.
    M4A1_HAMMER_BEAM_WIDTH_SCALE    = 0x280,
    M4A1_HAMMER_IMPACT_SPRITE_SIZE  = 0x400, // Perspective-sizing numerator
    M4A1_HAMMER_IMPACT_BEAM_END_AGE = 8,
    M4A1_HAMMER_IMPACT_LIFETIME     = 25,
};

#endif // WEAPONS_M4A1_HAMMER_CONFIG_H
