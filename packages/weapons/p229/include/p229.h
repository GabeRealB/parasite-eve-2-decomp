#ifndef WEAPONS_P229_H
#define WEAPONS_P229_H

#include <gameplay/animation.h>

#include <engine/task_types.h>

/// The player's clips while the P229 is equipped, by slot (`PLAYER_ANIMATION_*`): gameplay's bank for weapon 5.
extern AnimationSet* gP229PlayerAnimationBank[ANIMATION_BANK_SET_CAPACITY];

/// The flash of the P229's secondary load (`EFFECT_P229_MUZZLE_FLASH`): a textured core, an additive screen tint, four
/// streaks and a white point light at the muzzle, halving in size and brightness each update, gone after seven.
void p229MuzzleFlashTask(Task* task);

/// The P229's attack: one round on the primary button, or on the secondary a wide shot of the second load with a
/// flash of its own; a held action input ends recovery early, and a 12-frame (secondary 18-frame) cooldown follows.
void p229AttackState(Task* playerTask);

#endif // WEAPONS_P229_H
