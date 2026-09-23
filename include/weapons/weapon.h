#ifndef WEAPONS_WEAPON_H
#define WEAPONS_WEAPON_H

/// The item a weapon's rounds are taken from, given the weapon's index. Weapon
/// items follow item 0x7F in weapon order, so every weapon consumes item
/// `index + 0x7F`.
#define WEAPON_ITEM(index) ((index) + 0x7F)

#endif /* WEAPONS_WEAPON_H */
