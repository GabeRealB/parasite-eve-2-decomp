#ifndef INCLUDE_PE_NECROSIS_H
#define INCLUDE_PE_NECROSIS_H

#include "main/task_types.h"

/// Shrinks one Necrosis trail puff and emits drifting mist children.
///
/// Bank-6 slot 0x19 requires a coordinate body and cleared owned `EffectWork`
/// in `spawnArg2.pointer`. Low twelve bits of `spawnArg1.value` give size
/// 0..4095; initialization chooses a fixed 0..4095 screen bearing in 4096
/// units per turn. `scale` holds size, `angle` bearing, `step` the initial
/// size divided by sixteen, and `period` the initial size minus 256.
/// Each running update draws frame `age % 6`, then subtracts the fixed step.
/// If the remaining size is below one step, it releases; otherwise every
/// third age spawns/reparents a mist child with the saved `period` size.
/// PE control other than running pauses drawing, shrinkage and cleanup.
/// An initial size below 16 gives a zero step and no self-expiry; the cast
/// supplies larger values, and parent teardown can end the puff sooner.
void necrosisTrailPuffTask(Task* task);

/// Advances and draws one drifting Necrosis mist puff from effect slot 0x01A.
///
/// Requires a coordinate body and the counted `EffectWork` created by the
/// effect spawner. The low 12 bits of `task->spawnArg1.value` supply its size
/// in game-coordinate units. Initialization chooses a fixed random bearing,
/// displacement and blend mode from the current PE level (1..3).
/// Draws frames 1..7 of the small strip, or 1..5 of the large strip at level 3
/// when subtractive blending is selected, then releases the work and task.
/// Updates, drawing and expiry stop while PE effect control is nonzero.
void necrosisMistPuffTask(Task* task);

/// Launches an accelerating Necrosis cloud that grows and emits trail puffs.
///
/// Bank-6 slot 0x18 requires cleared owned `EffectWork` in `spawnArg2.pointer`
/// and a coordinate body; the attachment level digit must be 1..3. Borrows the
/// player rotation and launches along its forward axis at 144 coordinate units
/// per running update. Velocity in `move` gains a Q12 factor of 4352 (1.0625)
/// before each movement, including the launch update. Task-owned collision
/// storage holds a growing damage sphere and a radius-128 room-grid probe.
/// A class-zero grid contact stops movement and unlinks only the probe.
///
/// Each travelling update spawns a parented trail puff sized from the level
/// radius plus 96 times age, and grows the damage radius by 32. Both bodies
/// are unlinked once age exceeds the level travel limit. The task then waits
/// until age exceeds that limit plus 16 before releasing work and children.
/// Non-running PE control pauses initialization/travel, but the final wait
/// still advances age. A held attachment or cancellation ends the cast.
/// Both collision bodies are unlinked before their owned storage is released.
void necrosisCastTask(Task* task);

#endif // INCLUDE_PE_NECROSIS_H
