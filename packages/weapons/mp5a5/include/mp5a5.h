/* The MP5A5 and its two upgrades, one package and its variants. */

#ifndef WEAPONS_MP5A5_H
#define WEAPONS_MP5A5_H

#include <gameplay/animation.h>

#include <engine/task_types.h>

/// The player's clips while the MP5A5 or an upgrade is equipped, by slot (`PLAYER_ANIMATION_*`): gameplay's bank for
/// weapons 30 to 32.
extern AnimationSet* gMp5a5PlayerAnimationBank[ANIMATION_BANK_SET_CAPACITY];

/// The flash of the MP5A5's secondary load (`EFFECT_MP5A5_ALT_FIRE_MUZZLE_FLASH`): a textured core, an additive screen
/// tint, four streaks and a point light at the muzzle, halving in size and brightness each update, gone after seven.
void mp5a5MuzzleFlashTask(Task* task);

/// The MP5A5's attack: a round every three frames while the primary button is held and the magazine lasts, or on the
/// secondary a wide shot of the second load with a flash of its own and an 18-frame cooldown; the aim follows the
/// locked target.
void mp5a5AttackState(Task* playerTask);

/// `mp5a5AttackState` as gameplay names it in the upgrades' images (the variants `mp5a5_p1` and `mp5a5_p2`).
void mp5a5P1AttackState(Task* playerTask);
void mp5a5P2AttackState(Task* playerTask);

#endif // WEAPONS_MP5A5_H
