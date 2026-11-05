#ifndef NECROSIS_NECROSIS_H
#define NECROSIS_NECROSIS_H

#include <engine/task_types.h>

/// A puff of the Necrosis cloud's trail, sized by `spawnArg1`: it shrinks, shedding mist puffs, and pauses with the
/// room's PE.
void necrosisTrailPuffTask(Task* task);

/// A Necrosis mist puff, sized by `spawnArg1`: it drifts away through its strip, additive or subtractive, and pauses
/// with the room's PE.
void necrosisMistPuffTask(Task* task);

/// The Necrosis cast: a cloud launched along the caster's facing that speeds up, growing its damage sphere and shedding
/// a trail, until a wall stops it or its travel ends; then it waits for its trail. A held attachment or a room's PE
/// cancellation frees it.
void necrosisCastTask(Task* task);

#endif // NECROSIS_NECROSIS_H
