#ifndef WEAPONS_M4A1_PYKE_CONFIG_H
#define WEAPONS_M4A1_PYKE_CONFIG_H

// Configuration of the M4A1 Pyke.

// ---------------------------------------------------------------------------
// Attack

/// Attack tuning and ids; times in dispatch ticks.
enum {
    M4A1_PYKE_READY_BLEND_FRAMES        = 1,
    M4A1_PYKE_MOVING_READY_BLEND_FRAMES = 8,
    M4A1_PYKE_BURST_ROUNDS              = 3,
    M4A1_PYKE_BURST_CANCEL_FRAMES       = 9,
    M4A1_PYKE_SECONDARY_CANCEL_FRAMES   = 0x1C,
    M4A1_PYKE_BURST_DELAY_FRAMES        = 3,
    M4A1_PYKE_RECOVERY_COOLDOWN_FRAMES  = 0xC,
    M4A1_PYKE_FUEL_INTERVAL_FRAMES      = 0x14,
    M4A1_PYKE_FLAME_COOLDOWN_FRAMES     = 0x28,
    M4A1_PYKE_BURST_IMPACT_TICKS_LEFT   = 2,
    M4A1_PYKE_SECONDARY_TURN_RATE_INDEX = 2,
};

// ---------------------------------------------------------------------------
// Nozzle

/// The nozzle's glow, flame emission and light (`m4a1PykeNozzleTask`): times in dispatch ticks, light radii in game units.
enum {
    M4A1_PYKE_NOZZLE_LIGHT_FRAMES      = 4,
    M4A1_PYKE_NOZZLE_SPEED_STEP        = 64,
    M4A1_PYKE_NOZZLE_SPEED_MAX         = 384,
    M4A1_PYKE_NOZZLE_IDLE_LIGHT_INNER  = 128,
    M4A1_PYKE_NOZZLE_IDLE_LIGHT_OUTER  = 1024,
    M4A1_PYKE_NOZZLE_FIRE_LIGHT_INNER  = 1024,
    M4A1_PYKE_NOZZLE_FIRE_LIGHT_OUTER  = 16384,
    M4A1_PYKE_NOZZLE_LIGHT_RANDOM_MASK = 0x700,
    M4A1_PYKE_NOZZLE_IDLE_RED_MIN      = 0x400,
    M4A1_PYKE_NOZZLE_FIRE_RED_MIN      = 0x800,
};

#endif // WEAPONS_M4A1_PYKE_CONFIG_H
