#ifndef WEAPONS_WEAPON_INDEX_H
#define WEAPONS_WEAPON_INDEX_H

/// One-based weapon indices: the weapon's item id minus 0x7F (`WEAPON_ITEM_ID` converts back). The index numbers the
/// weapon's package (file 10300 + index), its sound bank, its contact keys' bank (`index << 8`) and its animation bank.
/// Preprocessor constants, so a variant can test `#if PE2_OPTION(WEAPON_INDEX) == ...`. Unused indices are named by
/// item id; `P08_SNAIL` is the P08 with the snail magazine, `_P1`/`_P2` the M4A1's and MP5A5's (+1)/(+2) upgrades.
#define WEAPON_INDEX_P08_SNAIL      1
#define WEAPON_INDEX_M93R           2
#define WEAPON_INDEX_M950           3
#define WEAPON_INDEX_P08            4
#define WEAPON_INDEX_P229           5
#define WEAPON_INDEX_UNUSED_85      6
#define WEAPON_INDEX_UNUSED_86      7
#define WEAPON_INDEX_UNUSED_87      8
#define WEAPON_INDEX_MONGOOSE       9
#define WEAPON_INDEX_UNUSED_89      10
#define WEAPON_INDEX_GRENADE_PISTOL 11
#define WEAPON_INDEX_MM1            12
#define WEAPON_INDEX_PA3            13
#define WEAPON_INDEX_SP12           14
#define WEAPON_INDEX_AS12           15
#define WEAPON_INDEX_M4A1           16
#define WEAPON_INDEX_M249           17
#define WEAPON_INDEX_UNUSED_91      18
#define WEAPON_INDEX_TONFA_BATON    19
#define WEAPON_INDEX_M4A1_P1        20
#define WEAPON_INDEX_M4A1_P2        21
#define WEAPON_INDEX_HYPERVELOCITY  22
#define WEAPON_INDEX_GUNBLADE       23
#define WEAPON_INDEX_UNUSED_97      24
#define WEAPON_INDEX_M4A1_HAMMER    25
#define WEAPON_INDEX_M4A1_BAYONET   26
#define WEAPON_INDEX_M4A1_GRENADE   27
#define WEAPON_INDEX_M4A1_PYKE      28
#define WEAPON_INDEX_M4A1_JAVELIN   29
#define WEAPON_INDEX_MP5A5          30
#define WEAPON_INDEX_MP5A5_P1       31
#define WEAPON_INDEX_MP5A5_P2       32

#endif // WEAPONS_WEAPON_INDEX_H
