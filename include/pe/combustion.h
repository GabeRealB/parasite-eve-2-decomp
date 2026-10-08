#ifndef INCLUDE_PE_COMBUSTION_H
#define INCLUDE_PE_COMBUSTION_H

#include "main/task_types.h"

/// Lays one descending flame line to a side of the Combustion caster.
///
/// Requires a counted coordinate-body effect with cleared `EffectWork` in
/// `spawnArg2.pointer` and an active PE level of 1..3. `spawnArg1.value` is the
/// side multiplier (+1 or -1); zero becomes +1. The coordinate borrows the
/// player's root, 1024 units above it, yawed 512 angle units per side
/// (4096 per turn). Each emission advances a signed-halfword local Y/Z offset
/// by the level's drop/reach steps and makes the flame a child of this task.
///
/// After the first wide flame, requests stat application and stops emitting;
/// the remaining ticks unwind yaw by 80 angle units per side. A held attachment,
/// PE cancellation or the level's duration releases the task and its children.
/// The player and Combustion overlay must stay live while it runs.
void combustionFlameEmitterTask(Task* task);

/// Animates one flame at an emission position in a Combustion line.
///
/// Requires a counted coordinate-body effect with cleared `EffectWork` and
/// active PE level 1..3. Borrows `work->parent`, placing the coordinate at the
/// copied spawn offset. `spawnArg1.value` is the emitter's emission age: it
/// sets sprite size to age * 32 + 512 and selects the wide tip after the
/// level's narrow-flame interval. Animation age starts randomly in 0..15;
/// age 33 ends it, so this is a phase cutoff rather than 33 elapsed ticks.
///
/// Draws every active tick, using a larger sprite at level three, and has a
/// one-in-four chance to spawn a child `combustionEmberTask`. A held attachment
/// or PE cancellation also releases it. Its borrowed parent and overlay must
/// remain live until teardown.
void combustionFlameTask(Task* task);

/// Animates one rising ember shed by a Combustion flame.
///
/// Requires a spawned coordinate-body effect task with its `EffectWork` in
/// `spawnArg2.pointer`, age and index initially zero, state 0, and
/// `spawnArg1.value` of 0 or 1. The active Combustion PE level must be 1..3.
/// Higher levels increase upward displacement and sprite size; level 3 also
/// randomizes the later animation choice.
///
/// Initialization always moves and draws pyro-flame frame zero. Later ticks
/// either flash successive pyro-flame cells on odd ages, advance the eight-cell
/// ember strip, or advance the six-cell small-flame strip. Exhausting the chosen
/// animation releases the owned work and task; parent teardown can end it sooner.
void combustionEmberTask(Task* task);

/// Starts the two opposite flame lines of a Combustion cast.
///
/// Requires a counted coordinate-body effect with owned `EffectWork` in
/// `spawnArg2.pointer`. The first tick composes its placement and spawns two
/// unlimited emitters with side multipliers +1 and -1; the next tick retires
/// this task. The emitters run independently and are not its teardown children.
void combustionCastTask(Task* task);

#endif // INCLUDE_PE_COMBUSTION_H
