#ifndef COMBUSTION_COMBUSTION_H
#define COMBUSTION_COMBUSTION_H

#include <engine/task_types.h>

/// Lays a Combustion flame line to one side of the player (`spawnArg1`, +1 or -1), a flame each frame, reaching out
/// and down; its last flame is wide and applies the spell, then it turns back and goes with its flames.
void combustionFlameEmitterTask(Task* task);

/// A flame of a Combustion line, placed where its emitter laid it and sized by when (`spawnArg1`); it burns from a
/// random frame, shedding embers, and is wide if it ends the line.
void combustionFlameTask(Task* task);

/// An ember rising from a Combustion flame, playing one of its animations (`spawnArg1`, or at random at the large
/// level) once.
void combustionEmberTask(Task* task);

/// The Combustion cast: spawns the two lines, one to each side, and goes the next frame.
void combustionCastTask(Task* task);

#endif // COMBUSTION_COMBUSTION_H
