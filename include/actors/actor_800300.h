#ifndef INCLUDE_ACTORS_ACTOR_800300_H
#define INCLUDE_ACTORS_ACTOR_800300_H

#include "main/task_types.h"
#include "main/tmd_types.h"

/// Models those descriptors attach.
extern TmdSource gActor800300Model02CF4;

/// Task entries the resident task descriptor tables name. A table in main or
/// gameplay reaches each of these by name, so they are the family's interface
/// to the resident code.
void func_actor_800300_801625F4(Task* task);

#endif // INCLUDE_ACTORS_ACTOR_800300_H
