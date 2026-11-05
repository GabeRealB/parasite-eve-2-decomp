#ifndef PYROKINESIS_PYROKINESIS_H
#define PYROKINESIS_PYROKINESIS_H

#include <engine/task_types.h>

/// A Pyrokinesis flame, the volley's `spawnArg1`th: it launches along the caster's facing, shedding a trail and lit,
/// with jet cones behind the second and third. Hitting an enemy bursts it in three rings; a wall or the end of its range
/// stops it, shrinking. It holds while the room's PE pauses, and a held attachment or its cancellation frees it.
void pyrokinesisCastTask(Task* task);

/// The cone at a Pyrokinesis flame's launch point, growing and fading; it holds while the room's PE pauses.
void pyrokinesisLaunchConeTask(Task* task);

/// A puff of a Pyrokinesis flame's trail, rising and spinning through its cells; it holds while the room's PE pauses.
void pyrokinesisFlamePuffTask(Task* task);

/// A ring from a Pyrokinesis flame's burst, tilted by `spawnArg1`, growing and fading; it holds while the room's PE
/// pauses.
void pyrokinesisFlameRingTask(Task* task);

/// The Pyrokinesis volley: launches three flames, a few frames apart, each on its own; it holds while the room's PE
/// pauses, and leaving battle, a held attachment or its cancellation ends it.
void pyrokinesisVolleyTask(Task* task);

#endif // PYROKINESIS_PYROKINESIS_H
