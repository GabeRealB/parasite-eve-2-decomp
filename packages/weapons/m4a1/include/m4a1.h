/* The M4A1 Rifle and its two upgrades, one package and its variants. */

#ifndef WEAPONS_M4A1_H
#define WEAPONS_M4A1_H

#include <gameplay/animation.h>

#include <engine/task_types.h>
#include <engine/tmd_types.h>

/// The player's clips while the M4A1 or an upgrade is equipped, by slot (`PLAYER_ANIMATION_*`): gameplay's bank for
/// weapons 16, 20 and 21.
extern AnimationSet* gM4a1PlayerAnimationBank[ANIMATION_BANK_SET_CAPACITY];

/// The M4A1 Rifle, held in the player's weapon hand; its upgrades hold it too.
extern TmdSource gM4a1RifleModel;

/// The M4A1's attack: a three-round burst on the primary button, one round on the secondary, stopping when the
/// magazine runs out; a held action input ends recovery after nine frames, and a 12-frame cooldown follows.
void m4a1AttackState(Task* playerTask);

/// `m4a1AttackState` as gameplay names it in the upgrades' images (the variants `m4a1_p1` and `m4a1_p2`).
void m4a1P1AttackState(Task* playerTask);
void m4a1P2AttackState(Task* playerTask);

#endif // WEAPONS_M4A1_H
