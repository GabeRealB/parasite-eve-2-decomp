#ifndef WEAPONS_GUNBLADE_CONFIG_H
#define WEAPONS_GUNBLADE_CONFIG_H

// Configuration of the Gunblade.

#include "main/sound_ids.h"
#include "weapons/sounds.h"
#include "weapons/weapon_index.h"

// ---------------------------------------------------------------------------
// Sounds

/// A sound of the Gunblade's bank, by slot (`WEAPON_SOUND_SLOT_*`, or one below).
#define GUNBLADE_SOUND(slot) SOUND_WEAPON(WEAPON_INDEX_GUNBLADE, (slot))

/// The Gunblade's own slots.
enum {
    GUNBLADE_SOUND_FIREFLY_IMPACT = 4,
    GUNBLADE_SOUND_SHOT           = 5,
    GUNBLADE_SOUND_SLASH          = 6,
    GUNBLADE_SOUND_CONTACT        = 7,
    GUNBLADE_SOUND_CHARGED_SLASH  = 8, // The slash fired with a round (the slash's contact and the shot)
};

// ---------------------------------------------------------------------------
// Attack

/// Attack tuning and ids; times in dispatch ticks.
enum {
    GUNBLADE_READY_BLEND_FRAMES            = 1,
    GUNBLADE_MOVING_READY_BLEND_FRAMES     = 6,
    GUNBLADE_SLASH_WINDUP_FRAMES           = 0x12,
    GUNBLADE_SPIN_COUNTER_TICKS            = 0x39,
    GUNBLADE_CHARGE_WINDOW_FRAMES          = 8,
    GUNBLADE_SPIN_YAW_STEP                 = 0xC,
    GUNBLADE_SLASH_KEY                     = 0x2171B,
    GUNBLADE_ATTACK_KEY_BASE               = 0x21700,
    GUNBLADE_SLASH_RADIUS                  = 0x180,
    GUNBLADE_SHOT_REACH                    = 0x2200,
    GUNBLADE_SHOT_RADIUS                   = 0x100,
    GUNBLADE_BUCKSHOT_RADIUS               = 0x900,
    GUNBLADE_CHARGED_SLUG_KEY_LOW          = 0x20,
    GUNBLADE_CHARGED_AMMUNITION_KEY_OFFSET = 0xB,
    /// Divisor that turns the model's forward axis into the distance the spinning
    /// slash covers per frame: 4096 / 136, about 30 coordinate units.
    GUNBLADE_SPIN_ADVANCE_DIVISOR = 136,
};

// ---------------------------------------------------------------------------
// Trail

/// The slash's ribbon trail (`gunbladeTrailTask`); times in dispatch ticks.
enum {
    GUNBLADE_TRAIL_CHARGE_START_TICKS = 9,
    GUNBLADE_TRAIL_LIFETIME_TICKS     = 13,
};

// ---------------------------------------------------------------------------
// Charge flash

/// The charged slash's flash (`gunbladeChargeFlashTask`): brightness 0..255, radii in game units, angles in 4096 per turn.
enum {
    GUNBLADE_FLASH_INITIAL_BRIGHTNESS        = 0xE0,
    GUNBLADE_FLASH_INITIAL_HALF_RADIUS       = 0x80,
    GUNBLADE_FLASH_DISC_RADIUS_STEP          = 0x10,
    GUNBLADE_FLASH_BAND_RADIUS_STEP          = 0x40,
    GUNBLADE_FLASH_BAND_BRIGHTNESS_STEP      = 0x10,
    GUNBLADE_FLASH_DISC_BRIGHTNESS_STEP      = 0x20,
    GUNBLADE_FLASH_BAND_BRIGHTNESS_MIN       = 0x11,
    GUNBLADE_FLASH_DISC_BRIGHTNESS_MIN       = 0x20,
    GUNBLADE_FLASH_BAND_WIDTH                = 0x60,
    GUNBLADE_FLASH_IMPACT_RADIUS             = 0x600,
    GUNBLADE_FLASH_BAND_BUCKSHOT_ARGUMENT    = 0x10000,
    GUNBLADE_FLASH_BAND_FIREFLY_ARGUMENT     = 0x20000,
    GUNBLADE_FLASH_BAND_SLUG_ARGUMENT        = 0x30000,
    GUNBLADE_FLASH_BAND_SECOND_ANGLE         = 0x2AA,
    GUNBLADE_FLASH_BAND_THIRD_ANGLE          = 0x555,
    GUNBLADE_FLASH_SPARK_COUNT               = 8,
    GUNBLADE_FLASH_BOUNCING_SPARK_COUNT      = 4,
    GUNBLADE_FLASH_BOUNCING_SPARK_SPEED_MASK = 0x3F,
    GUNBLADE_FLASH_BOUNCING_SPARK_COLOR      = 0x100,
    GUNBLADE_FLASH_SLUG_SPARK_ARGUMENT       = 1,
};

#endif // WEAPONS_GUNBLADE_CONFIG_H
