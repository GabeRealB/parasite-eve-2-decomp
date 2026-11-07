#ifndef WEAPONS_TONFA_BATON_H
#define WEAPONS_TONFA_BATON_H

#include <gameplay/animation.h>

#include <engine/task_types.h>

/// The player's clips while the tonfa baton is equipped, by slot (`PLAYER_ANIMATION_*`): gameplay's bank for weapon 19.
extern AnimationSet* gTonfaBatonPlayerAnimationBank[ANIMATION_BANK_SET_CAPACITY];

/// The ribbon behind a tonfa baton swing, fading as it ages: blue for the first strike, cyan for the second
/// (`spawnArg1` nonzero).
void tonfaBatonSwingTrailTask(Task* task);

/// The baton held in the player's hand, swung by the low nibble of `spawnArg1`: 0 returns it to rest, 1 strikes,
/// others hold its angle; it returns to rest when the player leaves the attack.
void tonfaBatonModelTask(Task* task);

/// The tonfa baton's attack: a strike that carries the player forward, and a second strike when R1 or R2 is pressed
/// again during the first.
void tonfaBatonAttackState(Task* playerTask);

#endif // WEAPONS_TONFA_BATON_H
