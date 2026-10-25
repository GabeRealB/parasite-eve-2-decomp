#ifndef INCLUDE_ACTORS_ACTOR_105300_H
#define INCLUDE_ACTORS_ACTOR_105300_H

#include "main/task_types.h"

#include "actors/generator.h"

/// Beta Generator body and Life Support spawn recipes exported by actor_105300.
///
/// Entry `GENERATOR_BODY_TASK_INDEX` creates the TMD body; entry
/// `GENERATOR_LIFE_SUPPORT_TASK_INDEX` creates its coordinate-only child.
/// Both execute at priority `TASK_PRIORITY_ACTOR` and receive a live `Enemy` in spawn argument 2
/// through `enemySpawnFromTable`. The child requires its body task as parent.
/// The actor overlay and the body's borrowed model data must remain loaded.
extern TaskDesc gActor105300GeneratorTasks[GENERATOR_TASK_DESC_COUNT];

#endif // INCLUDE_ACTORS_ACTOR_105300_H
