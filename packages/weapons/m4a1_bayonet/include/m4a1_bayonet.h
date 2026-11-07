#ifndef WEAPONS_M4A1_BAYONET_H
#define WEAPONS_M4A1_BAYONET_H

#include <gameplay/animation.h>

#include <engine/task_types.h>

/// The player's clips while the M4A1 Bayonet is equipped, by slot (`PLAYER_ANIMATION_*`): gameplay's bank for weapon
/// 26.
extern AnimationSet* gM4a1BayonetPlayerAnimationBank[ANIMATION_BANK_SET_CAPACITY];

/// The blue-white ribbon behind the bayonet's thrust, fading as it ages.
void m4a1BayonetTrailTask(Task* task);

/// The M4A1 Bayonet's attack: a three-round burst on the primary button, stopping when the magazine runs out, and a
/// bayonet thrust on the secondary; a held action input ends its recovery early.
void m4a1BayonetAttackState(Task* playerTask);

#endif // WEAPONS_M4A1_BAYONET_H
