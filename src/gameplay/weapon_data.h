#ifndef GAMEPLAY_PRIVATE_WEAPON_DATA_H
#define GAMEPLAY_PRIVATE_WEAPON_DATA_H

#include "common.h"

/// Parameters of one player weapon attack: a round of ammunition, or a strike
/// built into a weapon.
///
/// A weapon attack id (category 2 in the high halfword) with bit 0x8000 clear
/// selects row `id & 0x7F`; with the bit set the same columns come from
/// `AttachmentLevelRow` instead. Ammunition item `itemId` uses row
/// `itemId - 0x9F`, the value `PlayerStatus::weaponSlotItem` holds. Row 0 is
/// empty.
typedef struct {
    u16 amount;      // Base damage; the ammunition panel shows it as power
    u16 field_2;     // Zero in every row; no reader found, role unproven
    u16 hitReaction; // Reaction or special attribute (0 none, 1 stagger, 2 buildup, 3 poison, 4 burst, 5 piercing, 6 explosion, 7 incendiary, 9 flash)
    u16 effectId;    // Effect spawned for the hit
    u16 hitCooldown; // Frames the hit imposes. Victims arm a cooldown or a stun from it
} WeaponAttackRow;
STATIC_ASSERT_SIZEOF(WeaponAttackRow, 0xA);

/// Base damage, hit reaction, effect and cooldown for category-2 weapon attacks.
///
/// With bit 15 clear, the low seven key bits select a row in 0..46. Row 0 is
/// empty; ammunition ids 0xA0..0xBF select rows 1..32 by itemId - 0x9F.
/// Built-in weapon strikes also use these rows. Rows 33..46 include enemy
/// blast/projectile attacks that hurt other enemies. The distance/critical
/// scale row in key bits 8..13 is independent of this power/reaction index.
/// The catalog is borrowed for the gameplay image's lifetime.
extern WeaponAttackRow gWeaponAttackRows[47];

#endif // GAMEPLAY_PRIVATE_WEAPON_DATA_H
