#ifndef INCLUDE_ACTORS_ACTOR_310600_H
#define INCLUDE_ACTORS_ACTOR_310600_H

#include "gameplay/animation.h"

#include "main/task_types.h"

// Native animation sets referenced by room script banks.
extern AnimationSet gAkropolisCafeteriaPlayerCafeteriaFollowUpStartClip;

/// Rupert Broderick spawn recipes: 0 twenty-part body, 1 one-part Mongoose.
///
/// Both create TMD tasks at priority 192. The body skips automatic model buffers
/// and requires a live `Enemy*` in spawn argument 2, normally supplied by the
/// scene spawner. It creates the Mongoose on body coordinate 8. Direct attachment
/// spawns take a parent coordinate index in argument 1 and its live `Task*` in
/// argument 2. The child borrows the parent's coordinate and lighting and joins
/// its teardown tree. Indices are 0..1; there is no terminator.
/// Keep actor_310600's callbacks, model and animation data loaded while either
/// task or any effect borrowing its offsets is live.
extern TaskDesc gActor310600RupertBroderickTasks[2];

#endif // INCLUDE_ACTORS_ACTOR_310600_H
