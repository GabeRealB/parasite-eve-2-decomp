#ifndef INCLUDE_PE_APOBIOSIS_H
#define INCLUDE_PE_APOBIOSIS_H

#include "main/task_types.h"

/// Runs the Apobiosis cast's expanding strips, screen flash and shard bursts.
///
/// Bank-6 slot 0xC0 requires a coordinate body, owned cleared `EffectWork` in
/// `spawnArg2.pointer`, active Apobiosis PE level 1..3, and a live player model
/// with at least two coordinates. Initialization borrows the spawn parent,
/// publishes the cast for shard teardown and plays the level's cue. `index`
/// selects the level, `age` counts callback ticks and `step` is flash brightness;
/// `scale` starts as the strip radius and later becomes a random spawn radius.
///
/// The ring phase requests stat application at age four and fades its flash
/// by 24 per tick. Later phases attempt a shard on one in four ticks through
/// age 20, one per tick through age 30, then two through age 40 at levels two
/// and three. Shards attach themselves to this cast on initialization.
/// The final flash fades by eight per tick. Cancellation or the end of that
/// fade releases the task and its children; the parent, player and overlay
/// must stay live. The ring retains an original secondary-angle access beyond
/// the seeded 16-halfword table; its storage semantics remain unresolved.
void apobiosisCastTask(Task* task);

/// Updates and draws one short-lived apobiosis shard (bank-6 effect slot 0xF7).
///
/// Requires a spawned coordinate body and owned `EffectWork` in
/// `spawnArg2.pointer`, a live apobiosis cast, and a PE level digit in 1-3.
/// A nonzero `spawnArg1.value` pins the coordinate to the spawn parent for
/// 25 ticks; zero leaves it drifting for 17. Both modes draw a rotating sprite
/// and a strip on odd ticks. `EffectWork::step` holds the zero-based PE level
/// row, `index` the texture frame, `angle` the fixed screen rotation in 4096
/// units per turn, and `pos` the strip's world-space endpoint offset.
/// Reparents the task under the cast and frees its counted effect work when
/// its lifetime expires, effects are held, or room PE effects are cancelled.
void apobiosisShardTask(Task* task);

#endif // INCLUDE_PE_APOBIOSIS_H
