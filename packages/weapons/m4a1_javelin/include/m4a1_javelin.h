#ifndef WEAPONS_M4A1_JAVELIN_H
#define WEAPONS_M4A1_JAVELIN_H

#include <gameplay/animation.h>

#include <engine/task_types.h>
#include <engine/tmd_types.h>

/// The player's clips while the M4A1 Javelin is equipped, by slot (`PLAYER_ANIMATION_*`): gameplay's bank for weapon
/// 29.
extern AnimationSet* gM4a1JavelinPlayerAnimationBank[ANIMATION_BANK_SET_CAPACITY];

/// The M4A1 with its Javelin attachment, held in the player's weapon hand.
extern TmdSource gM4a1JavelinWeaponModel;

/// The Javelin's beam: a flare at the muzzle, then a beam to what it hits, or far ahead, with a glow on the ground
/// below; it fades out when its time is up or the attack ends.
void m4a1JavelinGuideBeamTask(Task* task);

/// A short flash where the Javelin's beam hits.
void m4a1JavelinContactFlashTask(Task* task);

/// The M4A1 Javelin's attack: a three-round burst on the primary button, stopping when the magazine runs out, and the
/// attachment's beam on the secondary, flashing where it hits; a held action input ends its recovery early.
void m4a1JavelinAttackState(Task* playerTask);

#endif // WEAPONS_M4A1_JAVELIN_H
