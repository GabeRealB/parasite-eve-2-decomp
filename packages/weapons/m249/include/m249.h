#ifndef WEAPONS_M249_H
#define WEAPONS_M249_H

#include <gameplay/animation.h>

#include <engine/task_types.h>
#include <engine/tmd_types.h>

/// The player's clips while the M249 is equipped, by slot (`PLAYER_ANIMATION_*`): gameplay's bank for weapon 17.
extern AnimationSet* gM249PlayerAnimationBank[ANIMATION_BANK_SET_CAPACITY];

/// The M249, held in the player's weapon hand.
extern TmdSource gM249WeaponModel;

/// The M249's attack: a round every two frames while the attack button is held and the magazine lasts, the aim
/// following the locked target.
void m249AttackState(Task* playerTask);

#endif // WEAPONS_M249_H
