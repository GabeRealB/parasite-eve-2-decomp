#ifndef INCLUDE_PE_ENERGYSHOT_H
#define INCLUDE_PE_ENERGYSHOT_H

#include "main/task_types.h"

/// Advances the rising additive billboard in effect-bank slot 0xDD.
///
/// Requires a counted coordinate-body effect task with an owned, cleared
/// `EffectWork` in `spawnArg2.pointer`, as `Gp_SpawnEff` supplies. Its coordinate
/// parent chain must stay live until teardown. Each tick, including initialization,
/// moves local Y by a fixed random -79..-16 parent-coordinate units. The screen
/// rotation is fixed at a random 0..4095 (4096 units per turn), with palette 0
/// and sizing numerator 0x400. `EffectWork::scale` stores that rotation.
///
/// `age` counts callback ticks and `index` selects the texture frame: frame 0
/// draws on ticks 1..3, frames 1..7 draw for four ticks each, and tick 32 moves
/// once more before releasing the work and task. External teardown may end it
/// earlier. Local Y additions must fit s32. The callback ignores `spawnArg1`
/// and room-effect pause/cancel gates.
void energyshotRisingBillboardTask(Task* task);

void func_energyshot_8012EF34(Task* arg0);

#endif // INCLUDE_PE_ENERGYSHOT_H
