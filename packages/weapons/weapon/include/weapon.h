#ifndef WEAPONS_WEAPON_H
#define WEAPONS_WEAPON_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include <core/pe2.h>

#include <gameplay/actor.h>
#include <gameplay/geometry.h>
#include <gameplay/inventory.h>

#include <engine/mc_types.h>
#include <engine/session_types.h>
#include <engine/task_types.h>
#include <engine/tmd_types.h>

#include "weapon_index.h"

/// Work of a fired grenade, at `Task::work`: the launchers' shell (Grenade Pistol, MM1, Kyle) and the M4A1's grenade.
///
/// In flight the shell steps by `dir` divided by the integer half of `flightTimer`, so it slows as that 16.16 word
/// climbs; detonation reuses the word as the blast's frames left.
typedef struct {
    WorldCollisionBody    sphereBody;         // Contact sphere (category 2); detonation widens it into the blast
    WorldCollisionBody    capsuleBody;        // Capsule probing room geometry along the flight
    WorldCollisionContact sphereContacts[1];  // `sphereBody`'s contacts: category 3 detonates, a grid hit is a surface
    WorldCollisionContact capsuleContacts[1]; // `capsuleBody`'s contacts, resolved before the sphere's
    WorldCollisionCapsule capsule;            // `capsuleBody`'s segment; its far end follows `flightTimer`
    Fixed16               flightTimer;        // 16.16 flight clock and step divisor; in the blast, frames left
    s32                   smokeInterval;      // Flight frames per smoke puff (1..4)
    s32                   flightFrame;        // Flight frames elapsed; every seventh widens `smokeInterval`
    SVECTOR               dir;                // Direction of motion, 4096 per unit; gravity adds 0x10 to vy each frame
    byte                  field_9C[4];        // Never accessed
} WeaponGrenadeWork;
STATIC_ASSERT_SIZEOF(WeaponGrenadeWork, 0xA0);

/// What a grenade's flight divisor (`WeaponGrenadeWork::flightTimer`, 16.16) grows by each flight frame: 3/32, so each
/// step is shorter than the last.
#define LAUNCHER_GRENADE_FLIGHT_DIVISOR_STEP 0x1800

/// The one-based ammunition index of a consumable item id: items 0xA0..0xBF are 1..32. It keys a weapon's attacks and
/// selects its projectile and sound variants. An empty load (0) or no secondary load (0xFF) gives no valid index.
#define WEAPON_AMMUNITION_INDEX(itemId) ((itemId) - (INVENTORY_CONSUMABLE_ITEM_FIRST - 1))

/// The 40mm grenade rounds, as `WEAPON_AMMUNITION_INDEX` of their items. The round selects the explosion and the
/// blast's radius and duration; per-round tables and sound variants are indexed from `GRENADE_ROUND_FIRST`.
enum {
    GRENADE_ROUND_FIRST         = 0xA,
    GRENADE_ROUND_FRAGMENTATION = 0xA, // Grenade (item 0xA9)
    GRENADE_ROUND_AIRBURST      = 0xB, // Airburst (item 0xAA)
    GRENADE_ROUND_RIOT          = 0xC  // Riot (item 0xAB)
};

/// The 12-gauge shells of the shotguns (PA3, SP12, AS12) and the Gunblade, as `WEAPON_AMMUNITION_INDEX` of their items.
/// The shell selects the sound bank and the shot's spread, flash and impact; tables and sound variants per shell are
/// indexed by the shell minus `SHOTGUN_ROUND_FIRST`.
enum {
    SHOTGUN_ROUND_FIRST       = 0xD,
    SHOTGUN_ROUND_BUCKSHOT    = 0xD, // Buckshot (item 0xAC)
    SHOTGUN_ROUND_FIREFLY     = 0xE, // Firefly (item 0xAD)
    SHOTGUN_ROUND_RIFLED_SLUG = 0xF  // R. Slug (item 0xAE)
};

/// The item id of a one-based weapon index: weapons 1..32 are items 0x80..0x9F, the key of the weapon's saved load
/// (`equipmentGetWeaponLoad`).
#define WEAPON_ITEM_ID(weaponIndex) ((weaponIndex) + (EQUIPMENT_WEAPON_ITEM_FIRST - 1))

/// The held model of the unused weapon 6, item 0x85 (asset weapons/unused_85/model).
extern TmdSource gUnused85HeldWeaponModel;

/// The Mongoose's held model (asset weapons/mongoose/model).
extern TmdSource gMongooseModel;

/// The SP12's held model (asset weapons/sp12/model).
extern TmdSource gSp12Model;

/// The held tonfa baton, shaft and side grip (asset weapons/tonfa_baton/model).
extern TmdSource gTonfaBatonWeaponModel;

/// The P229's held model (asset weapons/p229/model).
extern TmdSource gP229WeaponModel;

/// The tonfa baton's shaft without the side grip, in model-task slot 111; whether the game draws it is unknown
/// (asset weapons/tonfa_baton/shaft_model).
extern TmdSource gTonfaBatonShaftModel;

/// The held M4A1 with its bayonet (asset weapons/m4a1_bayonet/model).
extern TmdSource gM4a1BayonetWeaponModel;

/// The MP5A5's held model, which its two upgrades share (asset weapons/mp5a5/model).
extern TmdSource gMp5a5WeaponModel;

/// The Gunblade's held model (asset weapons/gunblade/model).
extern TmdSource gGunbladeModel;

#endif // WEAPONS_WEAPON_H
