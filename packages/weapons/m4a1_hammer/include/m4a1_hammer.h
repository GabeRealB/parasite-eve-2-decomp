#ifndef WEAPONS_M4A1_HAMMER_H
#define WEAPONS_M4A1_HAMMER_H

#include <gameplay/animation.h>

#include <engine/task_types.h>
#include <engine/tmd_types.h>

/// The player's clips while the M4A1 Hammer is equipped, by slot (`PLAYER_ANIMATION_*`): gameplay's bank for weapon 25.
extern AnimationSet* gM4a1HammerPlayerAnimationBank[ANIMATION_BANK_SET_CAPACITY];

/// The M4A1 with its Hammer attachment, held in the player's weapon hand.
extern TmdSource gM4a1HammerModel;

/// Requests for the attachment glow, stored in `Task::spawnArg1.value`.
enum {
    M4A1_HAMMER_GLOW_OFF     = 0,
    M4A1_HAMMER_GLOW_IDLE    = 1,
    M4A1_HAMMER_GLOW_CHARGED = 2,
    M4A1_HAMMER_GLOW_RELEASE = 3
};

/// The Hammer attachment's glow and light, the player's persistent weapon effect (`GameActor::weaponEffectTask`):
/// requested by `M4A1_HAMMER_GLOW_*` - off, a flickering idle glow, a charged burst of sparks that falls back to idle,
/// or release.
void m4a1HammerGlowTask(Task* task);

/// The Hammer's flash where its discharge hits, with a short beam back to the attachment; a nonzero `spawnArg1`
/// attaches it to the contact.
void m4a1HammerImpactFlashTask(Task* task);

/// The M4A1 Hammer's attack: a three-round burst on the primary button, stopping when the magazine runs out, and a
/// discharge of the attachment on the secondary; a held action input ends its recovery early.
void m4a1HammerAttackState(Task* playerTask);

#endif // WEAPONS_M4A1_HAMMER_H
