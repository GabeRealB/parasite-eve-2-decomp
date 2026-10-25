#ifndef INCLUDE_ACTORS_ACTOR_400500_H
#define INCLUDE_ACTORS_ACTOR_400500_H

#include "main/task_types.h"

/// Gray Stalker body-task spawn recipe exported to Dryfield underpass resources.
///
/// Creates a TMD body at execution priority 96. The `actor_400500` package and
/// its geometry must remain loaded throughout the body and child-arm lifetimes.
extern TaskDesc gActor400500TaskDesc;

#endif // INCLUDE_ACTORS_ACTOR_400500_H
