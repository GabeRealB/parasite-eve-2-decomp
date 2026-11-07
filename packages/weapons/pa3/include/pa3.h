/* The pump shotguns, one package and its variant: the PA3 and the SP12. */

#ifndef WEAPONS_PA3_H
#define WEAPONS_PA3_H

#include <gameplay/animation.h>

#include <engine/task_types.h>
#include <engine/tmd_types.h>

/// The player's clips while the PA3 is equipped, by slot (`PLAYER_ANIMATION_*`): gameplay's bank for weapon 13.
extern AnimationSet* gPa3PlayerAnimationBank[ANIMATION_BANK_SET_CAPACITY];

/// The player's clips while the SP12 is equipped, by slot (`PLAYER_ANIMATION_*`): gameplay's bank for weapon 14.
extern AnimationSet* gSp12PlayerAnimationBank[ANIMATION_BANK_SET_CAPACITY];

/// The PA3, held in the player's weapon hand.
extern TmdSource gPa3WeaponModel;

/// The PA3's attack: one shot per press, pumped 31 frames later. Buckshot makes no impact sound and Firefly sounds at
/// its first contact; a held action input ends recovery after 32 frames (the SP12's 36).
void pa3AttackState(Task* playerTask);

/// `pa3AttackState` as gameplay names it in its variant's image, the SP12's.
void sp12AttackState(Task* playerTask);

#endif // WEAPONS_PA3_H
