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

/// Held-weapon model for unused weapon index 6 (item 0x85).
///
/// The resident model-task descriptor borrows this single-part source while
/// `unused_85` is loaded. Keep the package loaded until its model tasks are
/// released; creation resolves the writable command stream in place.
extern TmdSource gUnused85HeldWeaponModel;

/// Single-part Mongoose model attached to the player's weapon hand.
///
/// The source and its geometry stay live while the Mongoose package is loaded.
/// Model creation resolves draw handlers in its writable stream; release all
/// attached instances before replacing the package.
extern TmdSource gMongooseModel;

/// Rigid held SP12 shotgun model, attached through the resident weapon-task bank.
///
/// One root part holds 34 vertices and 30 lighting normals. The SP12 overlay
/// must remain loaded while a model instance borrows this source; creation
/// resolves its writable command stream and copies the initial root transform.
extern TmdSource gSp12Model;

/// Single-part held tonfa baton, including its shaft and side grip.
///
/// Instances borrow the loaded package's 24 vertices, 20 normals and writable
/// command stream. Each buffer half holds 16 gouraud textured quads. Release
/// all instances before unloading the package; creation resolves stream handlers
/// in place and copies the initial root transform.
extern TmdSource gTonfaBatonWeaponModel;

/// Rigid P229 pistol model attached to the player's weapon hand.
///
/// One root part holds 30 vertices and 24 lighting normals. Model instances
/// borrow the source, geometry and writable command stream while the P229
/// package is loaded; release them before replacing the package. Creation
/// resolves draw handlers in place and copies the initial root transform.
extern TmdSource gP229WeaponModel;

/// Single-part tonfa baton shaft without the side grip, in model-task slot 111.
///
/// Eight vertices and eight normals define six gouraud textured quads. Instances
/// borrow the loaded package's geometry and writable stream through teardown;
/// creation resolves stream handlers in place. Runtime use of this resident
/// descriptor is unproven.
extern TmdSource gTonfaBatonShaftModel;

/// Single-part held M4A1 rifle and bayonet model attached through the resident weapon-task bank.
///
/// Instances borrow the package's 63 vertices, 63 lighting normals and writable
/// command stream. Model creation resolves draw handlers and copies the identity
/// root pose. Each primitive-buffer half holds 3256 bytes of directly transformed
/// primitives. Keep the M4A1 Bayonet package loaded until all instances are released;
/// the player's nineteen-track animation bank is a separate resource.
extern TmdSource gM4a1BayonetWeaponModel;

/// Single-part equipped MP5A5 model used by the base weapon and both upgrades.
///
/// Instances borrow the loaded weapon package's geometry and command stream;
/// keep the package loaded until those instances are released. The stream's
/// draw-handler slots are resolved in place on first use.
extern TmdSource gMp5a5WeaponModel;

/// Held Gunblade geometry and initial one-part pose.
///
/// Model objects borrow this record and its arrays while the Gunblade overlay
/// remains loaded. Creation resolves the writable command stream in place and
/// copies its initial skeleton. Each primitive-buffer half has 2224 bytes,
/// entirely for directly transformed primitives; player animation uses a
/// separate bank and rig.
extern TmdSource gGunbladeModel;

#endif // INCLUDE_WEAPONS_WEAPON_H
