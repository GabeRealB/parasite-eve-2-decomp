#ifndef WEAPONS_HYPERVELOCITY_H
#define WEAPONS_HYPERVELOCITY_H

#include <gameplay/animation.h>

#include <engine/sound_ids.h>
#include <engine/task_types.h>
#include <engine/tmd_types.h>

#include <weapons/weapon/weapon_index.h>

/// A sound of the Hypervelocity's bank (weapons/hypervelocity/sounds), by slot (`WEAPON_SOUND_SLOT_*`, or one below).
#define HYPERVELOCITY_SOUND(slot) SOUND_WEAPON(WEAPON_INDEX_HYPERVELOCITY, (slot))

/// The charge's sounds, which gameplay also stops when the attack is interrupted.
enum {
    HYPERVELOCITY_SOUND_CHARGE_START  = 3, // Started with the charge, stopped at the second stage or on cancel
    HYPERVELOCITY_SOUND_CHARGE_CANCEL = 4, // The charge released early (power-down)
    HYPERVELOCITY_SOUND_CHARGE_LOOP   = 5, // Runs for the whole charge, stopped when the shot fires or is canceled
};

/// The player's clips while the Hypervelocity is equipped, by slot (`PLAYER_ANIMATION_*`): gameplay's bank for weapon
/// 22.
extern AnimationSet* gHypervelocityPlayerAnimationBank[ANIMATION_BANK_SET_CAPACITY];

/// The Hypervelocity's body, held in the player's weapon hand; the slide and the hinge hang from it.
extern TmdSource gHypervelocityRootModel;

/// The Hypervelocity's slide, drawn back as it charges.
extern TmdSource gHypervelocitySlideModel;

/// The Hypervelocity's hinged part, which opens at the charge's second stage.
extern TmdSource gHypervelocityHingeModel;

/// A cone of the Hypervelocity round's wake, left behind it as it flies, widening and fading.
void hypervelocityDischargeConeTask(Task* task);

/// The Hypervelocity's round: a bright, lit bolt with a jet trail flying straight through enemies, shedding sparks and
/// wake cones, until its time is up; a wall stops it, and it shrinks away there.
void hypervelocityRoundTask(Task* task);

/// The blue-white shock ring at a Hypervelocity round's launch, widening and fading.
void hypervelocityShockRingTask(Task* task);

/// The Hypervelocity's charge glow, discharge and cooling smoke, the player's persistent weapon effect
/// (`GameActor::weaponEffectTask`). `spawnArg1` is its request: 1 starts the charge, a negative value cancels, and 2 or
/// more starts a discharge that long; the round flies when the discharge ends.
void hypervelocityChargeEffectTask(Task* task);

/// The Hypervelocity held in the player's hand, the low nibble of `spawnArg1` choosing the part: 0 the body
/// (`TASK_MODEL_AYA_WEAPONS`), which spawns the others and carries the charge's motions, 1 the slide and 2 the hinge.
void hypervelocityWeaponModelTask(Task* task);

/// The Hypervelocity's attack: holding R1 or R2 charges it, opening the weapon partway through, and a full charge fires
/// one round and kicks the player back; letting go first cancels it.
void hypervelocityAttackState(Task* playerTask);

#endif // WEAPONS_HYPERVELOCITY_H
