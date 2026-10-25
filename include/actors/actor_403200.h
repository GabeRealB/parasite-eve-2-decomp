#ifndef INCLUDE_ACTORS_ACTOR_403200_H
#define INCLUDE_ACTORS_ACTOR_403200_H

#include "gameplay/animation.h"

#include "main/task_types.h"

/// Task descriptor for the Shelter B3 dumping-hole Glutton host.
///
/// A single TMD-body task at priority `TASK_PRIORITY_ACTOR` owns the host model, combat work and
/// lifecycle dispatcher. Room resource rows borrow this descriptor from
/// actor_403200 after loading its package.
extern TaskDesc gActor403200GluttonHostTaskDesc;

// Native animation sets referenced by room script banks.
extern AnimationSet gActor403200Animation2CDE0;

extern AnimationSet gActor403200Animation2CF64;

extern AnimationSet gActor403200Animation2D1D0;

extern AnimationSet gActor403200Animation2D928;

#endif // INCLUDE_ACTORS_ACTOR_403200_H
