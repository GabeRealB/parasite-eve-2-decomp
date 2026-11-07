/* The P08 family, one package and its variants: the P08, its snail-magazine variant and the Mongoose. */

#ifndef WEAPONS_P08_H
#define WEAPONS_P08_H

#include <gameplay/animation.h>

#include <engine/task_types.h>
#include <engine/tmd_types.h>

/// The player's clips while a P08 is equipped, by slot (`PLAYER_ANIMATION_*`): gameplay's bank for weapons 1 (the
/// snail magazine's) and 4, and for no weapon, which loads the snail-magazine package.
extern AnimationSet* gP08PlayerAnimationBank[ANIMATION_BANK_SET_CAPACITY];

/// The player's clips while the Mongoose is equipped, by slot (`PLAYER_ANIMATION_*`): gameplay's bank for weapon 9.
extern AnimationSet* gMongoosePlayerAnimationBank[ANIMATION_BANK_SET_CAPACITY];

/// The P08, held in the player's weapon hand, with either magazine.
extern TmdSource gP08WeaponModel;

/// The P08's attack: one round per press; a held action input ends recovery after 11 frames, and a cooldown of 12
/// frames follows (the Mongoose's 15). Its variants build it with their own rounds, report and flash.
void p08AttackState(Task* playerTask);

/// `p08AttackState` as gameplay names it in its variants' images: the snail magazine's and the Mongoose's.
void p08SnailAttackState(Task* playerTask);
void mongooseAttackState(Task* playerTask);

#endif // WEAPONS_P08_H
