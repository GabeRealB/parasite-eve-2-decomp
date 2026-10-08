#ifndef INCLUDE_PE_METABOLISM_H
#define INCLUDE_PE_METABOLISM_H

#include "main/task_types.h"

/// Animates one short-lived sparkle emitted by the metabolism cast.
///
/// Requires a counted effect task with a coordinate body and owned, cleared
/// `EffectWork` in `spawnArg2.pointer`. Bits 0..11 of `spawnArg1.value` supply
/// the sprite sizing numerator (0..4095); higher bits are ignored. The current
/// attachment's level digit must be 1..3. Levels 1 and 2 select a fixed random
/// screen rotation with palette 1; level 3 has a one-in-three chance to select
/// an axis-aligned sprite with palette 3 and brightness initially 192.
///
/// Initialization only advances age. Active ticks add eight parent-coordinate
/// units to local Y, advance the animation index on even ages, and draw on odd
/// ages. From cleared work, indices 1..7 draw at ages 3..15; the fading path
/// wraps its four texture frames and loses 24 brightness units per draw.
/// Age 16 moves once more, then releases the work and task. Parent teardown
/// may end them sooner; the coordinate's parent chain must stay live meanwhile.
void metabolismSparkleTask(Task* task);

/// Grows and fades the Metabolism cast around the player while emitting sparkles.
///
/// Bank-6 slot 0x12 requires a coordinate body and cleared owned `EffectWork`
/// in `spawnArg2.pointer`. The current attachment level digit must be 1..3.
/// Initialization parents the coordinate to the player, lifts it 1024 local
/// units, seeds fixed fan bearings and applies the attachment stats. The work
/// keeps brightness in `scale`, radius in `angle`, and the level row in `index`.
/// Growth adds the row radius step and up to 16 brightness units per callback,
/// emitting three parented sparkles after the retained transform/RNG sequence.
/// At the row radius limit it fades by 16 per callback while radius still grows;
/// a brightness below 17 selects release on the next callback.
///
/// Both drawing phases render fan wedges, two coincident filled discs and
/// concentric complete bands; levels 2 and 3 add the outer band. A held
/// attachment or cancellation releases the work and task immediately.
/// Other PE-control values do not pause this task. The player parent must
/// stay live until task teardown.
void metabolismCastTask(Task* task);

#endif // INCLUDE_PE_METABOLISM_H
