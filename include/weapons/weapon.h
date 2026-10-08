#ifndef INCLUDE_WEAPONS_WEAPON_H
#define INCLUDE_WEAPONS_WEAPON_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "gameplay/actor.h"
#include "gameplay/geometry.h"

#include "main/session_types.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

/// Work block of a fired grenade, allocated zeroed by the projectile's spawn
/// state and kept at `Task::work`.
///
/// The grenade-shell task (Grenade Pistol, MM1 and Kyle) and the M4A1 grenade
/// launcher share the block. Spawn links both collision bodies; the exit
/// callback unlinks them. During flight the shell steps by `dir` divided by
/// the integer half of `flightTimer`, so it slows as that 16.16 word climbs.
/// Detonation replaces the word with the blast's remaining frames.
typedef struct {
    WorldCollisionBody    sphereBody;         // Pair-and-grid sphere (contact category 2); detonation widens it into the blast
    WorldCollisionBody    capsuleBody;        // Grid-only capsule body; a zero key omits it from pair contacts
    WorldCollisionContact sphereContacts[1];  // Sphere table: category 3 detonates, a grid contact names a surface
    WorldCollisionContact capsuleContacts[1]; // Capsule table; its grid contacts are resolved before the sphere's
    WorldCollisionCapsule capsule;            // Segment borrowed by `capsuleBody`; callers set the far end from `flightTimer`
    Fixed16               flightTimer;        // 16.16 flight clock and step divisor; blast reuses the word as frames left
    s32                   smokeInterval;      // Flight frames per smoke puff (1..4)
    s32                   flightFrame;        // Flight frames elapsed; every seventh widens `smokeInterval` up to 4
    SVECTOR               dir;                // Motion direction, 4096 per unit: pitched muzzle forward, then +0x10 on vy each frame
    byte                  field_9C[4];        // No recovered access. Keeps the block at 0xA0; role unproven
} WeaponGrenadeWork;
STATIC_ASSERT_SIZEOF(WeaponGrenadeWork, 0xA0);

/// Added to `flightTimer` each flight frame, in 16.16 units.
///
/// The integer half of the resulting word divides `dir` to produce that
/// frame's step, so the shell slows as the word climbs. The shared
/// grenade-shell flight and the M4A1 grenade both add this; the word that
/// ends the flight differs between them.
#define GRENADE_SHELL_FLIGHT_STEP 0x1800

/// The index a weapon load's consumable is known by once it is fired: its item
/// id minus 0x9F, so the first consumable (item 0xA0) is 1.
///
/// `itemId` is evaluated once. An empty load, whose item id is 0, gives a
/// negative value unless the result is narrowed to a byte.
#define WEAPON_AMMUNITION_INDEX(itemId) ((itemId) - 0x9F)

/// The 40mm grenade rounds, as `WEAPON_AMMUNITION_INDEX` of their items.
///
/// A grenade's detonation is keyed on the round it was loaded with: the round
/// is the explosion effect's spawn argument and selects the blast's radius and
/// how many frames it stays live. Tables and sound variants with one entry per
/// round are indexed by the round minus `GRENADE_ROUND_FIRST`.
enum {
    GRENADE_ROUND_FIRST         = 0xA,
    GRENADE_ROUND_FRAGMENTATION = 0xA, // Grenade (item 0xA9)
    GRENADE_ROUND_AIRBURST      = 0xB, // Airburst (item 0xAA)
    GRENADE_ROUND_RIOT          = 0xC  // Riot (item 0xAB)
};

/// The item a weapon's rounds are taken from, given the weapon's index. Weapon
/// items follow item 0x7F in weapon order, so every weapon consumes item
/// `index + 0x7F`.
#define WEAPON_ITEM(index) ((index) + 0x7F)

/// Models those descriptors attach.
extern TmdSource D_unused_85_8011D53C;

extern TmdSource D_p08_8011D924;

extern TmdSource D_mongoose_8011D934;

extern TmdSource D_pa3_8011DA04;

extern TmdSource D_m93r_8011DA74;

extern TmdSource D_m950_8011DA9C;

extern TmdSource D_sp12_8011DB44;

extern TmdSource D_as12_8011DCF0;

extern TmdSource D_m249_8011DE74;

extern TmdSource D_m4a1_8011DEC4;

extern TmdSource D_grenade_pistol_8011E28C;

extern TmdSource D_tonfa_baton_8011E460;

extern TmdSource D_mm1_8011E494;

extern TmdSource D_p229_8011E5E4;

extern TmdSource D_tonfa_baton_8011E5EC;

extern TmdSource D_m4a1_bayonet_8011E9FC;

extern TmdSource D_m4a1_grenade_8011EA2C;

extern TmdSource D_mp5a5_8011EAFC;

extern TmdSource D_gunblade_8011EEB0;

extern TmdSource D_m4a1_pyke_8011F56C;

extern TmdSource D_m4a1_hammer_8011F778;

extern TmdSource D_hypervelocity_801202F8;

extern TmdSource D_hypervelocity_801205E4;

extern TmdSource D_m4a1_javelin_8012071C;

extern TmdSource D_hypervelocity_80120860;

/// Model arguments of descriptors that point into the zeroed tail of their
/// package, where the disc image holds no model record.
extern TmdSource D_grenade_pistol_8012B5A4;

extern TmdSource D_mm1_8012D444;

extern TmdSource D_m4a1_grenade_8012E1FC;

#endif // INCLUDE_WEAPONS_WEAPON_H
