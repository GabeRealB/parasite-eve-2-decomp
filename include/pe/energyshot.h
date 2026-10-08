#ifndef INCLUDE_PE_ENERGYSHOT_H
#define INCLUDE_PE_ENERGYSHOT_H

#include "main/task_types.h"

/// Advances the rising additive billboard in effect-bank slot 0xDD.
///
/// Requires a counted coordinate-body effect task with an owned, cleared
/// `EffectWork` in `spawnArg2.pointer`, as `effectSpawn` supplies. Its coordinate
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

/// Grows and fades Energy Shot's cast discs, wedges and vertical beam bands.
///
/// Requires a counted coordinate-body effect with cleared, owned `EffectWork`
/// in `spawnArg2.pointer`, a live borrowed parent and active PE level 1..3.
/// Initialization requests stats, resets the previous shot-aura flags, seeds
/// all sixteen beam texture phases and 8/12/16 wedge yaws, and plays the
/// level's cue. The package tables are shared by casts, so a later cast
/// replaces their phases. Angles use 4096 units per turn.
///
/// Brightness also controls radius. Growth draws three discs above the origin,
/// wedges and one to three beam bands, and emits rising energy sparks with
/// randomized palettes. Passing the level's brightness cap starts the shot
/// aura and fading: discs keep their capped radius, while level-three wedges
/// and beams continue expanding. A held attachment, PE cancellation or the
/// fade floor releases the task. The parent and Energy Shot overlay stay live.
void energyshotCastTask(Task* task);

#endif // INCLUDE_PE_ENERGYSHOT_H
