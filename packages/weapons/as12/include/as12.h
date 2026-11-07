#ifndef WEAPONS_AS12_H
#define WEAPONS_AS12_H

#include <gameplay/animation.h>

#include <engine/task_types.h>
#include <engine/tmd_types.h>

/// The player's clips while the AS12 is equipped, by slot (`PLAYER_ANIMATION_*`): gameplay's bank for weapon 15.
extern AnimationSet* gAs12PlayerAnimationBank[ANIMATION_BANK_SET_CAPACITY];

/// The AS12, held in the player's weapon hand.
extern TmdSource gAs12Model;

/// The AS12's attack: a shot every 33 frames while the attack button is held and shells remain. Buckshot makes no
/// impact sound and Firefly sounds at its first contact; a held action input ends recovery after 22 frames.
void as12AttackState(Task* playerTask);

#endif // WEAPONS_AS12_H
