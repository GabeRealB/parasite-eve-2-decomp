#ifndef INCLUDE_PE_FLARE_H
#define INCLUDE_PE_FLARE_H

#include "main/task_types.h"

/// Advances and draws one spark from the Flare Parasite Energy effect.
///
/// Requires a live coordinate-body task and the zero-initialized, counted
/// `EffectWork` allocated by `effectSpawn` in `spawnArg2.pointer`.
/// `spawnArg1.value` supplies a perspective-size numerator in its low 12 bits;
/// the Flare emitter supplies 1664..2175. Initialization preserves the spawn
/// translation and borrows the live player's root rotation to orient a constant
/// velocity: forward speed 256 coordinate units per update and transverse speed
/// size / 32, with a random bearing in 4096 units per turn. The same bearing
/// rotates the camera-facing sprite.
///
/// Motion starts on the first update. Texture cells 0..7 advance every second
/// update; the sixteenth update releases the counted work and retires the task.
/// The Flare overlay must remain loaded while this callback can run.
void flareSparkTask(Task* task);

/// Runs the Flare item's sound cue and shower of child sparks.
///
/// Requires a counted coordinate-body effect with cleared, owned `EffectWork`
/// in `spawnArg2.pointer`. Ages 1..19 each attempt one spark with random size
/// 1664..2175, and age 8 requests attachment stat application. Each successful
/// spark becomes a child of this task. Age 36 retires the task; the normal
/// completion path leaves the sound script playing out.
///
/// A held attachment or any nonzero PE effect control stops the cue and
/// retires the task immediately. Its coordinate chain, player and Flare overlay
/// must remain live until teardown.
void flareEffectTask(Task* task);

#endif // INCLUDE_PE_FLARE_H
