#ifndef ENERGYSHOT_ENERGYSHOT_H
#define ENERGYSHOT_ENERGYSHOT_H

#include <engine/task_types.h>

/// A spinning additive billboard rising at its own speed through its eight cells, then going.
void energyshotRisingBillboardTask(Task* task);

/// The Energy Shot cast: rings and a fan of wedges above the caster and up to three beams grow while it sheds a spark
/// each frame, then an aura is spawned and they fade. Applies the spell and plays its cue on the first frame; a held
/// attachment or a room's PE cancellation frees it.
void energyshotCastTask(Task* task);

#endif // ENERGYSHOT_ENERGYSHOT_H
