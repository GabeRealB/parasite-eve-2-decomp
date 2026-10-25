#ifndef INCLUDE_ACTORS_ACTOR_202900_H
#define INCLUDE_ACTORS_ACTOR_202900_H

#include "main/task_types.h"

#include "gameplay/animation.h"

// Native animation sets referenced by room script banks.
extern AnimationSet gAkropolisCafeteriaPlayerOpeningClip;

/// Cafeteria woman spawn recipes: 0 animated body, 1 carried model.
///
/// Priority 192. Indices 0..1 have no terminator.
/// TMD bodies use automatic primitive buffers.
/// Body `Task::spawnArg2` borrows its live `Enemy`. Entry 1 ignores its arguments
/// and follows part 4 of the published singleton body task; that body must
/// remain live while the carried model runs.
/// Keep actor_202900 callbacks and borrowed model/animation resources loaded
/// while the spawned tasks use them.
extern TaskDesc gActor202900WomanTaskDescriptors[2];

#endif // INCLUDE_ACTORS_ACTOR_202900_H
