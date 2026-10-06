#ifndef INCLUDE_PE_COMBUSTION_H
#define INCLUDE_PE_COMBUSTION_H

#include "main/task_types.h"

void func_combustion_8012EF34(Task* arg0);

void func_combustion_8012F2BC(Task* arg0);

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

void func_combustion_801308E0(Task* arg0);

#endif // INCLUDE_PE_COMBUSTION_H
