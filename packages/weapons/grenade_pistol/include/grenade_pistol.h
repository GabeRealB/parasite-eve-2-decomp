/* The grenade launchers, one package and its variant: the Grenade Pistol and the MM1. */

#ifndef WEAPONS_GRENADE_PISTOL_H
#define WEAPONS_GRENADE_PISTOL_H

#include <gameplay/animation.h>

#include <engine/task_types.h>
#include <engine/tmd_types.h>

/// The player's clips while the Grenade Pistol is equipped, by slot (`PLAYER_ANIMATION_*`): gameplay's bank for
/// weapon 11.
extern AnimationSet* gGrenadePistolPlayerAnimationBank[ANIMATION_BANK_SET_CAPACITY];

/// The player's clips while the MM1 is equipped, by slot (`PLAYER_ANIMATION_*`): gameplay's bank for weapon 12.
extern AnimationSet* gMm1PlayerAnimationBank[ANIMATION_BANK_SET_CAPACITY];

/// The Grenade Pistol, held in the player's weapon hand.
extern TmdSource gGrenadePistolWeaponModel;

/// The MM1, held in the player's weapon hand.
extern TmdSource gMm1WeaponModel;

/// The grenade the Grenade Pistol fires.
extern TmdSource gGrenadePistolShellModel;

/// The grenade the MM1 fires.
extern TmdSource gMm1ShellModel;

/// The Grenade Pistol's attack: one grenade of the loaded round, then a recovery that runs its course.
void grenadePistolAttackState(Task* playerTask);

/// `grenadePistolAttackState` as gameplay names it in its variant's image, the MM1's.
void mm1AttackState(Task* playerTask);

/// A grenade the Grenade Pistol fires (`playerActorSpawnGrenadeProjectile`): it arcs trailing smoke and explodes on an
/// enemy, a solid surface or at the end of its flight. `spawnArg1` packs its round, the weapon, the launcher's row
/// (0 Grenade Pistol, 1 MM1) and bit 20 for the companion's shots.
void grenadePistolShellTask(Task* task);

/// `grenadePistolShellTask` as gameplay names it in the MM1's image.
void mm1ShellTask(Task* task);

#endif // WEAPONS_GRENADE_PISTOL_H
