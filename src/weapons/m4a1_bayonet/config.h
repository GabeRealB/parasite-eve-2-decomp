#ifndef WEAPONS_M4A1_BAYONET_CONFIG_H
#define WEAPONS_M4A1_BAYONET_CONFIG_H

// Configuration of the M4A1 Bayonet.

#include "main/sound_ids.h"
#include "weapons/sounds.h"
#include "weapons/weapon_index.h"

// ---------------------------------------------------------------------------
// Sounds

/// A sound of the M4A1 Bayonet's bank, by slot (`WEAPON_SOUND_SLOT_*`, or one below).
#define M4A1_BAYONET_SOUND(slot) SOUND_WEAPON(WEAPON_INDEX_M4A1_BAYONET, (slot))

/// The M4A1 Bayonet's own slots.
enum {
    M4A1_BAYONET_SOUND_CONTACT = 5,
    M4A1_BAYONET_SOUND_THRUST  = 6,
};

// ---------------------------------------------------------------------------
// Attack

/// Attack tuning and ids; times in dispatch ticks.
enum {
    M4A1_BAYONET_READY_BLEND_FRAMES        = 1,
    M4A1_BAYONET_MOVING_READY_BLEND_FRAMES = 8,
    M4A1_BAYONET_BURST_ROUNDS              = 3,
    M4A1_BAYONET_SHOT_DELAY_FRAMES         = 3,
    M4A1_BAYONET_IMPACT_DELAY_FRAMES       = 2,
    M4A1_BAYONET_BURST_CANCEL_FRAMES       = 9,
    M4A1_BAYONET_THRUST_CANCEL_FRAMES      = 0xA,
    M4A1_BAYONET_THRUST_WINDUP_FRAMES      = 0x12,
    M4A1_BAYONET_THRUST_CONTACT_FRAMES     = 0xA,
    M4A1_BAYONET_THRUST_COOLDOWN_FRAMES    = 0x1C,
    M4A1_BAYONET_RECOVERY_COOLDOWN_FRAMES  = 0xC,
    M4A1_BAYONET_THRUST_REACH              = 0x340,
    M4A1_BAYONET_ATTACK_KEY_BASE           = 0x21A00,
    M4A1_BAYONET_THRUST_KEY                = 0x21A1D,
};

// ---------------------------------------------------------------------------
// Trail

/// The thrust's ribbon trail (`m4a1BayonetTrailTask`); times in dispatch ticks.
enum {
    M4A1_BAYONET_TRAIL_LIFETIME_TICKS = 13,
};

#endif // WEAPONS_M4A1_BAYONET_CONFIG_H
