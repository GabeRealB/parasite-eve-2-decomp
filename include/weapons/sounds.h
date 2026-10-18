#ifndef INCLUDE_WEAPONS_SOUNDS_H
#define INCLUDE_WEAPONS_SOUNDS_H

#include "main/sound_ids.h"

#include "weapons/weapon_index.h"

/// A weapon sound id's ammunition variant: the bank of the loaded round, bits 24..27 of the id (bits 8..11 of the
/// bank). `variant` is the round's index from its family's first (`SHOTGUN_ROUND_FIRST`, `GRENADE_ROUND_FIRST`).
#define WEAPON_SOUND_VARIANT(variant) ((variant) << 24)

/// Slots of a weapon's sound bank (the entry byte of `SOUND_WEAPON`) that the player code plays for whichever
/// weapon is equipped (`SOUND_WEAPON(0, slot)` with the weapon ORed into the bank bits), and the shot that most
/// weapons' attack states play.
///
/// A weapon's bank is its weapon index (`weapons/weapon_index.h`); a weapon with several ammunitions has a bank per
/// ammunition, selected by the variant in bits 24..27 of the id. The banks are the assets weapons/<weapon>/sounds,
/// or <ammunition>_sounds. A bank fills a slot with whatever its weapon makes there: the M4A1 family's reload start
/// is the click, the shotguns' the pump. Slots past these differ by weapon and are named by the weapon.
enum {
    WEAPON_SOUND_SLOT_CLICK        = 1, // Empty chamber: the attack button with no rounds loaded
    WEAPON_SOUND_SLOT_RELOAD_START = 2, // The reload's opening cue; a shotgun's reload ends on it
    WEAPON_SOUND_SLOT_RELOAD_LOAD  = 3, // The reload's load cue
    WEAPON_SOUND_SLOT_SHOT         = 4, // A round fired (not the shotguns, the Hypervelocity or the Tonfa Baton)
};

/// Slots of the shotguns' banks (PA3, SP12, AS12), one bank per ammunition: buckshot, Firefly, rifled slug.
enum {
    SHOTGUN_SOUND_SLOT_FIREFLY_IMPACT = 4, // The Firefly round's impact (Firefly bank only)
    SHOTGUN_SOUND_SLOT_SHOT           = 5,
};

#endif // INCLUDE_WEAPONS_SOUNDS_H
