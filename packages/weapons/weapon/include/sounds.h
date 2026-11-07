#ifndef WEAPONS_SOUNDS_H
#define WEAPONS_SOUNDS_H

#include <engine/sound_ids.h>

#include "weapon_index.h"

/// A weapon sound id's ammunition variant, bits 24..27: which of the weapon's banks, one per round, plays it.
/// `variant` is the round's index from its family's first (`SHOTGUN_ROUND_FIRST`, `GRENADE_ROUND_FIRST`).
#define WEAPON_SOUND_VARIANT(variant) ((variant) << 24)

/// Slots of a weapon's sound bank that the player code plays for whichever weapon is equipped, and the shot most
/// weapons' attacks play. A weapon's bank is its weapon index, one per round where it takes several (assets
/// weapons/<weapon>/sounds or <round>_sounds); slots past these are named by each weapon.
enum {
    WEAPON_SOUND_SLOT_CLICK        = 1, // Empty chamber: the attack button with no rounds loaded
    WEAPON_SOUND_SLOT_RELOAD_START = 2, // The reload's opening cue: the M4A1s' click, the shotguns' pump, which ends it
    WEAPON_SOUND_SLOT_RELOAD_LOAD  = 3, // The reload's load cue
    WEAPON_SOUND_SLOT_SHOT         = 4, // A round fired (not the shotguns, the Hypervelocity or the Tonfa Baton)
};

/// Slots of the shotguns' banks (PA3, SP12, AS12), one bank per round: buckshot, Firefly, rifled slug.
enum {
    SHOTGUN_SOUND_SLOT_FIREFLY_IMPACT = 4, // The Firefly round's impact (Firefly bank only)
    SHOTGUN_SOUND_SLOT_SHOT           = 5,
};

#endif // WEAPONS_SOUNDS_H
