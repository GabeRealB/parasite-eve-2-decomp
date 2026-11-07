#ifndef WEAPONS_M950_H
#define WEAPONS_M950_H

#include <gameplay/animation.h>

#include <engine/task_types.h>
#include <engine/tmd_types.h>

/// The player's clips while the M950 is equipped, by slot (`PLAYER_ANIMATION_*`): gameplay's bank for weapon 3.
extern AnimationSet* gM950PlayerAnimationBank[ANIMATION_BANK_SET_CAPACITY];

/// The M950, held in the player's weapon hand.
extern TmdSource gM950WeaponModel;

/// The M950's attack: a round every four frames while the attack button is held and the magazine lasts, the aim
/// following the locked target; a held action input ends recovery after nine frames.
void m950AttackState(Task* playerTask);

#endif // WEAPONS_M950_H
