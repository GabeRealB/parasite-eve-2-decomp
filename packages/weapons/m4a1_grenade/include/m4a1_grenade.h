#ifndef WEAPONS_M4A1_GRENADE_H
#define WEAPONS_M4A1_GRENADE_H

#include <gameplay/animation.h>

#include <engine/sound_ids.h>
#include <engine/task_types.h>
#include <engine/tmd_types.h>

/// The launcher's reload cues, played in this order by gameplay's reload state; the attack state's reload after a
/// launch plays the first and then slot 9.
enum {
    M4A1_GRENADE_SOUND_GRENADE_RELOAD_FIRST  = 8,
    M4A1_GRENADE_SOUND_GRENADE_RELOAD_SECOND = 5,
    M4A1_GRENADE_SOUND_GRENADE_RELOAD_LOADED = 9,
};

/// The player's clips while the M4A1 Grenade is equipped, by slot (`PLAYER_ANIMATION_*`): gameplay's bank for weapon
/// 27.
extern AnimationSet* gM4a1GrenadePlayerAnimationBank[ANIMATION_BANK_SET_CAPACITY];

/// The M4A1 with its grenade launcher attachment, held in the player's weapon hand.
extern TmdSource gM4a1GrenadeWeaponModel;

/// The 40mm grenade it fires.
extern TmdSource gM4a1GrenadeProjectileModel;

/// The M4A1 Grenade's attack: a three-round burst on the primary button, stopping when the magazine runs out, and one
/// grenade of the loaded round on the secondary; a held action input ends its recovery early.
void m4a1GrenadeAttackState(Task* playerTask);

/// A grenade the M4A1 Grenade fires (`playerActorSpawnGrenadeProjectile`): it arcs trailing smoke and explodes on an
/// enemy, a solid surface or at the end of its flight, with the blast of the round loaded when it explodes.
void m4a1GrenadeShellTask(Task* task);

#endif // WEAPONS_M4A1_GRENADE_H
