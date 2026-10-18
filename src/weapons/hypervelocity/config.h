#ifndef WEAPONS_HYPERVELOCITY_CONFIG_H
#define WEAPONS_HYPERVELOCITY_CONFIG_H

// Configuration of the Hypervelocity.

#include "gameplay/world_collision_types.h"

#include "weapons/hypervelocity.h"

// ---------------------------------------------------------------------------
// Sounds

/// The Hypervelocity's slots the attack state plays alone (`HYPERVELOCITY_SOUND`; the charge's are public).
enum {
    HYPERVELOCITY_SOUND_SECOND_STAGE = 2,
    HYPERVELOCITY_SOUND_DISCHARGE    = 6,
    HYPERVELOCITY_SOUND_SHOT         = 7,
};

// ---------------------------------------------------------------------------
// Attack

/// Attack tuning and ids; times in dispatch ticks.
enum {
    HYPERVELOCITY_RECOIL_FRAMES             = 0x15,
    HYPERVELOCITY_CHARGE_FRAMES             = 90,
    HYPERVELOCITY_SECOND_CHARGE_STAGE_FRAME = 60,
    HYPERVELOCITY_RECOIL_PUSH_WINDOW        = 19,
    HYPERVELOCITY_RECOIL_FIRST_PUSH_FRAME   = 18,
    HYPERVELOCITY_RECOIL_DIVISOR            = 378,
    HYPERVELOCITY_RECOIL_FIRST_PUSH_DIVISOR = 244,
};

// ---------------------------------------------------------------------------
// Charge effect

/// The charge glow, discharge and cooling smoke (`hypervelocityChargeEffectTask`): times in dispatch ticks, brightness 0..255, radii in game units.
enum {
    HYPERVELOCITY_CHARGE_EFFECT_TIMEOUT_FRAMES        = 65,
    HYPERVELOCITY_CHARGE_EFFECT_DISCHARGE_FRAMES      = 24,
    HYPERVELOCITY_CHARGE_EFFECT_RETIRE_AGE            = 111,
    HYPERVELOCITY_CHARGE_EFFECT_ARC_START_BRIGHTNESS  = 129,
    HYPERVELOCITY_CHARGE_EFFECT_RADIUS_START          = 64,
    HYPERVELOCITY_CHARGE_EFFECT_RADIUS_MAX            = 512,
    HYPERVELOCITY_CHARGE_EFFECT_RADIUS_STEP           = 8,
    HYPERVELOCITY_CHARGE_EFFECT_FADE_STEP             = 32,
    HYPERVELOCITY_CHARGE_EFFECT_PARTICLE_SIZE         = 384,
    HYPERVELOCITY_CHARGE_EFFECT_LIGHT_FRAMES          = 4,
    HYPERVELOCITY_CHARGE_EFFECT_LIGHT_RANDOM_MASK     = 0x700,
    HYPERVELOCITY_CHARGE_EFFECT_CHARGING_BLUE_MIN     = 1024,
    HYPERVELOCITY_CHARGE_EFFECT_DISCHARGING_BLUE_MIN  = 2048,
    HYPERVELOCITY_CHARGE_EFFECT_LIGHT_INNER           = 256,
    HYPERVELOCITY_CHARGE_EFFECT_LIGHT_OUTER           = 4096,
    HYPERVELOCITY_CHARGE_EFFECT_DISCHARGE_LIGHT_INNER = 1024,
    HYPERVELOCITY_CHARGE_EFFECT_DISCHARGE_LIGHT_OUTER = 16384,
    HYPERVELOCITY_CHARGE_EFFECT_DUST_ARGUMENT         = 0x2300,
    HYPERVELOCITY_CHARGE_EFFECT_DRIFT_AGE_MASK        = 15,
    HYPERVELOCITY_CHARGE_EFFECT_CHARGE_DRIFT_SHIFT    = 5,
    HYPERVELOCITY_CHARGE_EFFECT_DISCHARGE_DRIFT_SHIFT = 6,
    HYPERVELOCITY_CHARGE_EFFECT_ARC_RADIUS            = 96,
    HYPERVELOCITY_CHARGE_EFFECT_ARC_COUNTDOWN_SCALE   = 128,
};

// ---------------------------------------------------------------------------
// Round

/// The fired round (`hypervelocityRoundTask`) and the table rows its attack is read from: times in dispatch ticks, radii in game units.
enum {
    HYPERVELOCITY_ROUND_ATTACK_ROW   = 26,
    HYPERVELOCITY_ROUND_DISTANCE_ROW = 22,
    /// Category-2 weapon-attack key carried by the Hypervelocity round's sphere.
    ///
    /// Int constant expression: low seven bits select weapon attack row 26
    /// (2000 base damage, stagger, spark burst, 10-frame hit cooldown); bits 8..13
    /// select distance/critical row 22, the Hypervelocity weapon index. Attachment,
    /// companion and alternate-critical bits are clear. Independent of live ammunition.
    HYPERVELOCITY_ROUND_COLLISION_KEY       = WORLD_COLLISION_CONTACT_ATTACK | (HYPERVELOCITY_ROUND_DISTANCE_ROW << 8) | HYPERVELOCITY_ROUND_ATTACK_ROW,
    HYPERVELOCITY_ROUND_SPEED               = 0x400,
    HYPERVELOCITY_ROUND_BRIGHTNESS          = 0xC0,
    HYPERVELOCITY_ROUND_VISUAL_RADIUS       = 0x500,
    HYPERVELOCITY_ROUND_COLLISION_RADIUS    = 0x800,
    HYPERVELOCITY_ROUND_LIFETIME_TICKS      = 0x15,
    HYPERVELOCITY_ROUND_COLLAPSE_STEP       = 0x40,
    HYPERVELOCITY_ROUND_COLLAPSE_MIN_RADIUS = 0x80,
    HYPERVELOCITY_ROUND_SPARK_ARGUMENT      = 0x400,
    HYPERVELOCITY_ROUND_LIGHT_SLOT          = 0,
    HYPERVELOCITY_ROUND_LIGHT_TTL           = 4,
    HYPERVELOCITY_ROUND_LIGHT_INNER_BASE    = 0x200,
    HYPERVELOCITY_ROUND_LIGHT_INDEX_SHIFT   = 9,
    HYPERVELOCITY_ROUND_LIGHT_OUTER_SCALE   = 16,
    HYPERVELOCITY_ROUND_LIGHT_RANDOM_MASK   = 0x700,
    HYPERVELOCITY_ROUND_LIGHT_BLUE_BASE     = 0x800,
    HYPERVELOCITY_ROUND_JITTER_MASK         = 0xFF,
};

// ---------------------------------------------------------------------------
// Discharge cone

/// The discharge cones (`hypervelocityDischargeConeTask`): brightness 0..255, extents in game units.
enum {
    HYPERVELOCITY_DISCHARGE_CONE_COLLAR_HEIGHT        = 0x700,
    HYPERVELOCITY_DISCHARGE_CONE_INITIAL_MOUTH_HEIGHT = 0x600,
    HYPERVELOCITY_DISCHARGE_CONE_INITIAL_FLARE        = 0x200,
    HYPERVELOCITY_DISCHARGE_CONE_AGE_GEOMETRY_SHIFT   = 7,
    HYPERVELOCITY_DISCHARGE_CONE_INITIAL_BRIGHTNESS   = 0x80,
    HYPERVELOCITY_DISCHARGE_CONE_INITIAL_HALF_EXTENT  = 0x200,
    HYPERVELOCITY_DISCHARGE_CONE_HALF_EXTENT_STEP     = 0x60,
    HYPERVELOCITY_DISCHARGE_CONE_FADE_STEP            = 8,
    HYPERVELOCITY_DISCHARGE_CONE_MIN_BRIGHTNESS       = 6,
};

// ---------------------------------------------------------------------------
// Shock ring

/// The launch's shock ring (`hypervelocityShockRingTask`): brightness 0..255, radii in game units.
enum {
    HYPERVELOCITY_SHOCK_RING_INITIAL_BRIGHTNESS = 0xF0,
    HYPERVELOCITY_SHOCK_RING_INITIAL_RADIUS     = 0x100,
    HYPERVELOCITY_SHOCK_RING_RADIUS_STEP        = 0x40,
    HYPERVELOCITY_SHOCK_RING_FADE_STEP          = 0x10,
};

// ---------------------------------------------------------------------------
// Model

/// The weapon model and its moving parts (`hypervelocityWeaponModelTask`): the task bank and descriptors that spawn them, and their charge motion.
enum {
    HYPERVELOCITY_MODEL_SLIDE_MAX_TICKS      = 60,
    HYPERVELOCITY_MODEL_SLIDE_UNITS_PER_TICK = 4,
    HYPERVELOCITY_MODEL_HINGE_STEP           = 0x110,
    HYPERVELOCITY_MODEL_HINGE_LIMIT          = -0x400,
    HYPERVELOCITY_MODEL_TASK_BANK            = 7,
    HYPERVELOCITY_MODEL_SLIDE_DESCRIPTOR     = 0x70,
    HYPERVELOCITY_MODEL_HINGE_DESCRIPTOR     = 0x74,
};

#endif // WEAPONS_HYPERVELOCITY_CONFIG_H
