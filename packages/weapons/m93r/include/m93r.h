#ifndef WEAPONS_M93R_H
#define WEAPONS_M93R_H

#include <gameplay/animation.h>

#include <engine/task_types.h>
#include <engine/tmd_types.h>

/// The player's clips while the M93R is equipped, by slot (`PLAYER_ANIMATION_*`): gameplay's bank for weapon 2.
extern AnimationSet* gM93rPlayerAnimationBank[ANIMATION_BANK_SET_CAPACITY];

/// The M93R, held in the player's weapon hand.
extern TmdSource gM93rWeaponModel;

/// The M93R's attack: a three-round burst on the primary button, one round on the secondary, stopping when the
/// magazine runs out; a held action input ends its recovery early.
void m93rAttackState(Task* playerTask);

#endif // WEAPONS_M93R_H
