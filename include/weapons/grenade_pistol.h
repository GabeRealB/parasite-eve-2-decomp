#ifndef INCLUDE_WEAPONS_GRENADE_PISTOL_H
#define INCLUDE_WEAPONS_GRENADE_PISTOL_H

#include "gameplay/animation.h"

#include "main/task_types.h"
#include "main/tmd_types.h"

/// Player motion bank used while the Grenade Pistol is equipped (asset weapons/grenade_pistol/bank).
///
/// Gameplay selects bank index 12: character-1 base 1 plus weapon 11. Base
/// slots 1..44 reference 36 distinct clips for movement, reactions, attacks
/// and other player actions; base slots 0, 45 and 46 are NULL. The 32 extension
/// words at indices 47..78 start cleared and remain writable for scripted
/// copies, which may include non-pointer payloads. Play only entries known
/// to contain valid clip pointers. Keep the Grenade Pistol overlay loaded
/// while playback borrows its bank or native clips; copied clips require
/// their source overlays to remain loaded as well.
extern AnimationSet* gGrenadePistolPlayerAnimationBank[ANIMATION_BANK_SET_CAPACITY];

/// Native player motion bank used while the MM1 is equipped (asset weapons/mm1/bank).
///
/// Gameplay selects bank index 13: character-1 base 1 plus weapon 12. Slots
/// 1..44 contain 36 distinct clips for locomotion, aiming, attacks, reactions
/// and scripted actions; native slots 0, 45 and 46 are NULL. The extension at
/// indices 47..78 starts cleared and remains writable for raw-word copies,
/// including non-pointer payloads. Play only entries containing valid clip
/// pointers. Keep the MM1 package loaded while playback borrows the bank or its
/// native clips; copied clips also require their source packages to stay loaded.
extern AnimationSet* gMm1PlayerAnimationBank[ANIMATION_BANK_SET_CAPACITY];

/// Single-part held Grenade Pistol model attached to the player's weapon hand (asset weapons/grenade_pistol/model).
///
/// Geometry and draw commands are shared by its runtime instances; initial
/// transforms are copied into each instance. The Grenade Pistol overlay and
/// its textures must remain loaded until those instances are released. The
/// source and stream stay writable because creation resolves draw handlers in
/// place.
extern TmdSource gGrenadePistolWeaponModel;

/// Single-part held MM1 model attached to the player's weapon hand (asset weapons/mm1/model).
///
/// Instances borrow its geometry and writable command stream; creation copies
/// the read-only rest transform and resolves stream draw handlers in place.
/// Keep the MM1 overlay and its textures loaded until every referring instance
/// is released.
extern TmdSource gMm1WeaponModel;

/// Single-part Grenade Pistol projectile model used during flight and the blast countdown (asset weapons/grenade_pistol/grenade_model).
///
/// Instances borrow the geometry and writable command stream, while creation
/// copies the initial root transform and resolves stream draw handlers in place.
/// Keep the Grenade Pistol overlay and its textures loaded until every referring
/// task is released.
extern TmdSource gGrenadePistolShellModel;

/// Single-part MM1 grenade-projectile model shared by its flying shells (asset weapons/mm1/grenade_model).
///
/// Instances borrow its geometry and writable command stream; creation copies
/// the read-only rest transform and resolves stream draw handlers in place.
/// Keep the MM1 overlay and its textures loaded until every referring
/// projectile is released.
extern TmdSource gMm1ShellModel;

/// Runs the player's Grenade Pistol ready, fire and recovery phases.
///
/// Requires live player `GameActor` work, native animation slots, equipment slot
/// 1 and the equipped weapon overlay. Phase 0 enters normal actor state 4;
/// phase 2 consumes one primary round and spawns a projectile using the current
/// ammunition index. The shot sets a 40-tick cooldown; recovery ends with the
/// selected character's weapon animation. `GameActor::statePhase` must be 0..3.
void grenadePistolAttackState(Task* playerTask);

/// Runs the same attack phases for the MM1's load, muzzle row and projectile.
///
/// Uses the Grenade Pistol attack contract with the MM1 overlay loaded.
void mm1AttackState(Task* playerTask);

void func_mm1_8011DBD0(Task* arg0);

/// Dispatches the Grenade Pistol projectile's spawn, flight, blast and teardown states.
///
/// Requires task state 0..3 and the matching weapon/gameplay overlays loaded.
/// Spawn owns the `WeaponGrenadeWork` allocation and initializes both collision
/// bodies; flight and blast require it live. Teardown unlinks both bodies and
/// releases the task/work. The projectile's packed spawn argument supplies
/// ammunition and launcher-row selectors to the states. The MM1 build exports
/// its own alias of this dispatcher; its model and launch tuning remain separate.
void grenadePistolShellTask(Task* task);

#endif // INCLUDE_WEAPONS_GRENADE_PISTOL_H
