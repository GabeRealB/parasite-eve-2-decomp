#ifndef WEAPONS_M4A1_JAVELIN_CONFIG_H
#define WEAPONS_M4A1_JAVELIN_CONFIG_H

// Configuration of the M4A1 Javelin.

#include "gameplay/world_collision_types.h"
#include "main/sound_ids.h"
#include "weapons/sounds.h"
#include "weapons/weapon_index.h"

// ---------------------------------------------------------------------------
// Sounds

/// A sound of the M4A1 Javelin's bank, by slot (`WEAPON_SOUND_SLOT_*`, or one below).
#define M4A1_JAVELIN_SOUND(slot) SOUND_WEAPON(WEAPON_INDEX_M4A1_JAVELIN, (slot))

/// The M4A1 Javelin's own slots.
enum {
    M4A1_JAVELIN_SOUND_BEAM = 5,
};

// ---------------------------------------------------------------------------
// Attack

/// Attack tuning and ids; times in dispatch ticks.
enum {
    M4A1_JAVELIN_READY_BLEND_FRAMES        = 1,
    M4A1_JAVELIN_MOVING_READY_BLEND_FRAMES = 8,
    M4A1_JAVELIN_BURST_ROUNDS              = 3,
    M4A1_JAVELIN_BURST_CANCEL_FRAMES       = 9,
    M4A1_JAVELIN_SECONDARY_CANCEL_FRAMES   = 0x1C,
    M4A1_JAVELIN_BURST_DELAY_FRAMES        = 3,
    M4A1_JAVELIN_RECOVERY_COOLDOWN_FRAMES  = 0xC,
    M4A1_JAVELIN_BEAM_DELAY_FRAMES         = 6,
    M4A1_JAVELIN_BEAM_ACTIVE_FRAMES        = 0x1C,
    M4A1_JAVELIN_RIFLE_KEY_BASE            = WORLD_COLLISION_CONTACT_ATTACK | (WEAPON_INDEX_M4A1_JAVELIN << 8),
    M4A1_JAVELIN_BEAM_KEY                  = WORLD_COLLISION_CONTACT_ATTACK | (WEAPON_INDEX_M4A1_JAVELIN << 8) | 0x1F,
    M4A1_JAVELIN_BURST_IMPACT_TICKS_LEFT   = 2,
    M4A1_JAVELIN_SECONDARY_TURN_RATE_INDEX = 2,
};

// ---------------------------------------------------------------------------
// Guide beam

/// The guide beam and muzzle flare (`m4a1JavelinGuideBeamTask`): lengths in game units, times in dispatch ticks.
enum {
    M4A1_JAVELIN_BEAM_LENGTH            = 8000,
    M4A1_JAVELIN_FLARE_INITIAL_RADIUS   = 0x600,
    M4A1_JAVELIN_FLARE_RADIUS_DECREMENT = 0xF0,
    M4A1_JAVELIN_FLARE_FORWARD_STEP     = 0xC0,
    M4A1_JAVELIN_FLARE_BRIGHTNESS_STEP  = 0x20,
    M4A1_JAVELIN_FLARE_MAX_BRIGHTNESS   = 0xC0,
    M4A1_JAVELIN_GUIDE_HOLD_TICKS       = 32,
    M4A1_JAVELIN_LIGHT_LIFETIME_TICKS   = 4,
    M4A1_JAVELIN_BEAM_BASE_RGB444       = 0x36C,
    M4A1_JAVELIN_RADIUS_DEPTH_PRODUCT   = 0x4000, // The beam's screen radius times its depth
    M4A1_JAVELIN_PROBE_RAISE            = 0x100,
};

// ---------------------------------------------------------------------------
// Contact flash

/// The contact flash (`m4a1JavelinContactFlashTask`); size in game units.
enum {
    M4A1_JAVELIN_CONTACT_FLASH_SIZE = 512,
};

#endif // WEAPONS_M4A1_JAVELIN_CONFIG_H
