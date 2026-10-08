#ifndef INCLUDE_PE_HEALING_H
#define INCLUDE_PE_HEALING_H

#include "main/task_types.h"

/// Advances the additive rising spark emitted by a Healing sparkle.
///
/// Requires a counted effect task with a coordinate body and an owned, cleared
/// `EffectWork` in `spawnArg2.pointer`, as `effectSpawn` supplies. The parent
/// coordinate chain must remain live until teardown. `spawnArg1.value` bits
/// 0..11 give the signed-size drawer a nonnegative sizing numerator (0..4095).
/// The initialization tick chooses a fixed random screen rotation without
/// moving or drawing. Active ticks rise four parent-coordinate units along Y;
/// ages 3, 5, ... 15 draw texture frames 1..7. Age 16 releases the work and task,
/// unless parent-task teardown ends them earlier. Performs no room-control check.
void healingRisingSparkTask(Task* task);

/// Animates and fades one sparkle shed by the Healing aura.
///
/// Requires a counted coordinate-body effect with cleared `EffectWork` in
/// `spawnArg2.pointer`, a live borrowed parent, and active PE level 1..3.
/// `spawnArg1.value` bits 0..11 give the sprite size numerator. Initialization
/// places it at the copied local offset and captures the level's brightness.
/// Every tick, including initialization, moves local Y by +4 coordinate units.
///
/// Odd ages below 30 advance and draw the animation; from age 16 those draws
/// reduce brightness. Levels one and two draw a modulated billboard, level
/// three a core and outer glow. Ages 1 modulo 8 spawn a child rising spark.
/// Age 30 releases the owned work and task. Parent teardown may end it earlier;
/// this callback performs no attachment or room-control cancellation check.
void healingSparkleTask(Task* task);

/// Grows and fades the Healing cast's blue aura while shedding sparkles.
///
/// Requires a counted coordinate-body effect with cleared, owned `EffectWork`
/// in `spawnArg2.pointer` and active PE level 1..3. Initialization borrows the
/// player's root at local Y=-1024, requests stat application and plays the
/// level's cue. Brightness grows in steps of 16 while the radius grows from
/// 128 coordinate units and the aura spins; each growing tick attempts a
/// child sparkle. Draws a doubled centre disc and surrounding glow bands.
///
/// At the level's radius limit, brightness fades in steps of 16, followed by
/// a 31-tick hold before teardown. A held attachment or PE cancellation ends
/// it immediately; cancellation before initialization still requests stats.
/// The player, parent coordinate chain and Healing overlay must remain live.
void healingAuraTask(Task* task);

#endif // INCLUDE_PE_HEALING_H
