#ifndef INCLUDE_STAGES_AKROPOLIS_ACTORS_HELICOPTER_LANDING_PAD_NO_9_H
#define INCLUDE_STAGES_AKROPOLIS_ACTORS_HELICOPTER_LANDING_PAD_NO_9_H

#include "main/task_types.h"

/// Emits the GOLEM's mode-controlled flame jet and orange transient light.
void helicopterLandingPadNo9FlameJetTask(Task* task);

/// Emits No. 9's muzzle flash, debris streaks, pixel sparks and orange light.
void helicopterLandingPadNo9MuzzleFlashTask(Task* task);

/// Draws No. 9's six-cell flame sprite with a fading tail and alternate-frame ground glow.
void helicopterLandingPadNo9GolemFlameTask(Task* task);

/// Draws No. 9's eight-frame additive flame-jet sprite with a palette per frame.
void helicopterLandingPadNo9FlameTongueTask(Task* task);

/// Draws No. 9's twelve-frame flame-jet sprite with optional narrowing and downward motion.
void helicopterLandingPadNo9FlameSpriteTask(Task* task);

/// Draws No. 9's rising, subtractive flame-jet sprite, holding each cell for two frames.
void helicopterLandingPadNo9GunfireParticleTask(Task* task);

/// Moves and draws one fading debris streak from No. 9's muzzle-flash burst.
void helicopterLandingPadNo9DebrisStreakTask(Task* task);

/// Draws an expanding twelve-frame explosion fireball and optional child copies.
void helicopterLandingPadNo9ExplosionFireballTask(Task* task);

/// Sequences the GOLEM grenade's fireball, main smoke and trailing smoke.
void helicopterLandingPadNo9ImpactBurstTask(Task* task);

/// Akropolis No. 9 spawn recipes: body, prop, weapon, chest, grenade, light and blast source.
extern TaskDesc gHelicopterLandingPadNo9TaskDescriptors[7];

#endif // INCLUDE_STAGES_AKROPOLIS_ACTORS_HELICOPTER_LANDING_PAD_NO_9_H
