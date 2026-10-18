#ifndef INCLUDE_WEAPONS_M4A1_GRENADE_H
#define INCLUDE_WEAPONS_M4A1_GRENADE_H

#include "gameplay/animation.h"

#include "main/sound_ids.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "weapons/weapon_index.h"

/// A sound of the M4A1 Grenade's bank, by slot (`WEAPON_SOUND_SLOT_*`, or one below); one bank per round
/// (`GRENADE_ROUND_*`, the variant in bits 24..27).
#define M4A1_GRENADE_SOUND(slot) SOUND_WEAPON(WEAPON_INDEX_M4A1_GRENADE, (slot))

/// The launcher's reload cues, played in this order by gameplay's reload state; the attack state's reload after a
/// launch plays the first and then slot 9.
enum {
    M4A1_GRENADE_SOUND_GRENADE_RELOAD_FIRST  = 8,
    M4A1_GRENADE_SOUND_GRENADE_RELOAD_SECOND = 5,
    M4A1_GRENADE_SOUND_GRENADE_RELOAD_LOADED = 9,
};

/// Native player-rig animation bank for the M4A1 grenade launcher (asset weapons/m4a1_grenade/bank).
///
/// Player bank index 28 selects this package for weapon 27. Its 47 native
/// entries contain 44 clip pointers to 38 distinct sets; entries 0, 45 and 46
/// are absent. The 32 extension words start at zero and remain writable for
/// animation-bank copy requests, which can also copy non-clip payload words.
/// Follow an extension entry as a clip only when it contains a loaded set
/// pointer. Playback borrows the bank and its clip data; keep this package and
/// any copied clip resources loaded while they are used.
extern AnimationSet* gM4a1GrenadePlayerAnimationBank[ANIMATION_BANK_SET_CAPACITY];

/// Held M4A1 rifle with its grenade-launcher attachment (asset weapons/m4a1_grenade/model).
///
/// Objects borrow the source and its writable command stream; keep this package
/// loaded until they are released.
extern TmdSource gM4a1GrenadeWeaponModel;

/// Fired 40mm grenade model used during flight and the blast countdown.
///
/// Objects borrow the source and its writable command stream for their whole
/// lifetime, so this package must remain loaded until the projectile task is
/// released.
extern TmdSource gM4a1GrenadeProjectileModel;

/// Runs the M4A1 grenade launcher's rifle burst or single grenade attack.
///
/// Requires live player `GameActor` work, native animation slots, the six-entry
/// weapon contact table, equipment slot 1 and the M4A1 Grenade overlay. Primary
/// fire uses up to three rifle rounds, with three countdown ticks between shots;
/// secondary fire uses one grenade and launches its projectile. The secondary
/// load's ammunition index selects sound variants, defaulting to fragmentation
/// when empty. A live load must contain one of the three compatible 40mm rounds.
/// `GameActor::statePhase` must be 0..5. Reserves one temporary `GfxCoord` for room
/// impact sounds and releases it before returning; only its cached translation
/// is supplied by the impact query. Recovery permits cancellation after the
/// attack's delay, then sets a 12-tick cooldown.
void m4a1GrenadeAttackState(Task* playerTask);

/// Dispatches an M4A1 grenade's launch, flight, blast countdown and teardown.
///
/// Bank-7 descriptor 0x60 supplies the projectile model. `Task::state` must be
/// 0..3; the weapon and gameplay overlays must remain loaded. Launch allocates
/// `WeaponGrenadeWork` and links its sphere and capsule; later states require
/// that work until teardown unlinks both bodies and releases the task.
/// The projectile borrows the muzzle coordinate only during initialization,
/// then flies parented to the view coordinate. Detonation reads the player's
/// current secondary load rather than the ammunition packed at launch.
void m4a1GrenadeShellTask(Task* task);

#endif // INCLUDE_WEAPONS_M4A1_GRENADE_H
