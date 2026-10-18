#ifndef INCLUDE_WEAPONS_WEAPON_H
#define INCLUDE_WEAPONS_WEAPON_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "gameplay/actor.h"
#include "gameplay/geometry.h"
#include "gameplay/inventory.h"

#include "main/mc_types.h"
#include "main/session_types.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "weapons/weapon_index.h"

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

/// Increase of the grenade's flight divisor per flight tick, in signed 16.16 units.
///
/// 0x1800 represents 3/32 of a whole divisor unit. Translation divides each
/// direction component by the positive integer half of
/// `WeaponGrenadeWork::flightTimer` before adding this increment; increasing
/// the divisor slows subsequent steps. Shared launcher shells and M4A1 grenades
/// use the same increment with different flight limits. Applies only in flight;
/// detonation reuses the timer word as a whole-frame blast countdown.
#define LAUNCHER_GRENADE_FLIGHT_DIVISOR_STEP 0x1800

/// Converts a loaded consumable item id to its one-based weapon ammunition index.
///
/// Item ids 0xA0..0xBF map to indices 1..32, used in weapon attack keys and
/// projectile/sound variants. This is independent of inventory row position
/// and load quantity. With a byte item id, an empty load (0) produces -159 and
/// an unavailable secondary slot (0xFF) produces 96; callers handle sentinels.
/// `itemId` is evaluated once with normal integer promotions, without range
/// checks or narrowing. An integer constant argument gives an integer constant
/// expression, suitable for ammunition enumerators.
#define WEAPON_AMMUNITION_INDEX(itemId) ((itemId) - (INVENTORY_CONSUMABLE_ITEM_FIRST - 1))

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

/// The 12-gauge shells of the shotguns (PA3, SP12, AS12) and the Gunblade, as `WEAPON_AMMUNITION_INDEX` of their items.
///
/// The loaded shell selects the weapon's sound bank and its shot's spread, flash and impact. Tables and sound variants
/// with one entry per shell are indexed by the shell minus `SHOTGUN_ROUND_FIRST`.
enum {
    SHOTGUN_ROUND_FIRST       = 0xD,
    SHOTGUN_ROUND_BUCKSHOT    = 0xD, // Buckshot (item 0xAC)
    SHOTGUN_ROUND_FIREFLY     = 0xE, // Firefly (item 0xAD)
    SHOTGUN_ROUND_RIFLED_SLUG = 0xF  // R. Slug (item 0xAE)
};

/// Converts a one-based weapon index to the weapon's inventory item id.
///
/// Indices 1..32 map to ids 0x80..0x9F, selecting the saved weapon-load record
/// passed to `equipmentGetWeaponLoad` or `equipmentConsumeWeaponLoad`.
/// Index 0 produces 0x7F, which is outside the weapon-load domain.
/// `weaponIndex` is evaluated once with normal integer promotions, without
/// range checks or narrowing. An integer constant argument gives an integer
/// constant expression.
#define WEAPON_ITEM_ID(weaponIndex) ((weaponIndex) + (EQUIPMENT_WEAPON_ITEM_FIRST - 1))

/// Held-weapon model for unused weapon index 6 (item 0x85) (asset weapons/unused_85/model).
///
/// The resident model-task descriptor borrows this single-part source while
/// `unused_85` is loaded. Keep the package loaded until its model tasks are
/// released; creation resolves the writable command stream in place.
extern TmdSource gUnused85HeldWeaponModel;

/// Single-part Mongoose model attached to the player's weapon hand (asset weapons/mongoose/model).
///
/// The source and its geometry stay live while the Mongoose package is loaded.
/// Model creation resolves draw handlers in its writable stream; release all
/// attached instances before replacing the package.
extern TmdSource gMongooseModel;

/// Rigid held SP12 shotgun model, attached through the resident weapon-task bank (asset weapons/sp12/model).
///
/// The SP12 overlay must remain loaded while a model instance borrows this
/// source; creation resolves its writable command stream and copies the
/// initial root transform.
extern TmdSource gSp12Model;

/// Single-part held tonfa baton, including its shaft and side grip (asset weapons/tonfa_baton/model).
///
/// Instances borrow the loaded package's 24 vertices, 20 normals and writable
/// command stream. Each buffer half holds 16 gouraud textured quads. Release
/// all instances before unloading the package; creation resolves stream handlers
/// in place and copies the initial root transform.
extern TmdSource gTonfaBatonWeaponModel;

/// Rigid P229 pistol model attached to the player's weapon hand (asset weapons/p229/model).
///
/// Model instances borrow the source, geometry and writable command stream
/// while the P229 package is loaded; release them before replacing the
/// package. Creation resolves draw handlers in place and copies the initial
/// root transform.
extern TmdSource gP229WeaponModel;

/// Single-part tonfa baton shaft without the side grip, in model-task slot 111 (asset weapons/tonfa_baton/shaft_model).
///
/// Eight vertices and eight normals define six gouraud textured quads. Instances
/// borrow the loaded package's geometry and writable stream through teardown;
/// creation resolves stream handlers in place. Runtime use of this resident
/// descriptor is unproven.
extern TmdSource gTonfaBatonShaftModel;

/// Single-part held M4A1 rifle and bayonet model attached through the resident weapon-task bank (asset weapons/m4a1_bayonet/model).
///
/// Instances borrow the package's 63 vertices, 63 lighting normals and writable
/// command stream. Model creation resolves draw handlers and copies the identity root
/// pose. Keep the M4A1 Bayonet package loaded until all instances are released; the
/// player's animation bank is a separate resource.
extern TmdSource gM4a1BayonetWeaponModel;

/// Single-part equipped MP5A5 model used by the base weapon and both upgrades (asset weapons/mp5a5/model).
///
/// Instances borrow the loaded weapon package's geometry and command stream;
/// keep the package loaded until those instances are released. The stream's
/// draw-handler slots are resolved in place on first use.
extern TmdSource gMp5a5WeaponModel;

/// Held Gunblade geometry and initial one-part pose (asset weapons/gunblade/model).
///
/// Model objects borrow this record and its arrays while the Gunblade overlay
/// remains loaded. Creation resolves the writable command stream in place and
/// copies its initial skeleton.
extern TmdSource gGunbladeModel;

#endif // INCLUDE_WEAPONS_WEAPON_H
