#ifndef WEAPONS_M4A1_GRENADE_CONFIG_H
#define WEAPONS_M4A1_GRENADE_CONFIG_H

// Configuration of the M4A1 Grenade.

#include "weapons/m4a1_grenade.h"

// ---------------------------------------------------------------------------
// Sounds

/// The M4A1 Grenade's slots its own code plays alone (`M4A1_GRENADE_SOUND`; the reload cues are public).
enum {
    M4A1_GRENADE_SOUND_LAUNCH    = 6,
    M4A1_GRENADE_SOUND_EXPLOSION = 7, // The projectile exploding
};

// ---------------------------------------------------------------------------
// Attack

/// Attack tuning and ids; times in dispatch ticks.
enum {
    M4A1_GRENADE_READY_BLEND_FRAMES      = 1,
    M4A1_GRENADE_MOVING_BLEND_FRAMES     = 8,
    M4A1_GRENADE_RIFLE_BLEND_FRAMES      = 2,
    M4A1_GRENADE_LAUNCH_BLEND_FRAMES     = 3,
    M4A1_GRENADE_RIFLE_BURST_ROUNDS      = 3,
    M4A1_GRENADE_RIFLE_SHOT_TICKS        = 3,
    M4A1_GRENADE_RIFLE_IMPACT_TICKS      = 2,
    M4A1_GRENADE_RIFLE_CANCEL_TICKS      = 9,
    M4A1_GRENADE_LAUNCH_CANCEL_TICKS     = 34,
    M4A1_GRENADE_LAUNCH_COOLDOWN_TICKS   = 40,
    M4A1_GRENADE_RECOVERY_COOLDOWN_TICKS = 12,
};

// ---------------------------------------------------------------------------
// Projectile

/// The launched grenade: launch point and pitch in the weapon's frame, radii in game units, times in dispatch ticks.
enum {
    M4A1_GRENADE_LAUNCH_Y                  = 0x220,
    M4A1_GRENADE_LAUNCH_Z                  = 0x28,
    M4A1_GRENADE_LAUNCH_PITCH              = -0x400,
    M4A1_GRENADE_INITIAL_FLIGHT_TIMER      = 10 << 16,
    M4A1_GRENADE_PROJECTILE_RADIUS         = 0x94,
    M4A1_GRENADE_BLAST_FRAMES              = 8,
    M4A1_GRENADE_AIRBURST_BLAST_FRAMES     = 1,
    M4A1_GRENADE_FLIGHT_TIMER_LIMIT        = 0xFFFFF,
    M4A1_GRENADE_GRAVITY_STEP              = 0x10,
    M4A1_GRENADE_SMOKE_INTERVAL_MAX        = 4,
    M4A1_GRENADE_SMOKE_THINNING_TICKS      = 7,
    M4A1_GRENADE_CAPSULE_TIMER_SHIFT       = 10, // The flight timer's divisor: the capsule's reach and each tick's step
    M4A1_GRENADE_WATER_TOWER_SURFACE_CLASS = 1,
};

#endif // WEAPONS_M4A1_GRENADE_CONFIG_H
