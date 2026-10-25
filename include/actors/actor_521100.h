#ifndef INCLUDE_ACTORS_ACTOR_521100_H
#define INCLUDE_ACTORS_ACTOR_521100_H

#include "main/task_types.h"

#include "gameplay/animation.h"

// Native animation sets referenced by room script banks.
extern AnimationSet gGolemNo9PlayerEvent51Clip;

extern AnimationSet gGolemNo9PlayerEvent53Clip;

extern AnimationSet gGolemNo9PlayerEvent55Clip;

extern AnimationSet gGolemNo9PlayerEvent57Clip;

extern AnimationSet gGolemNo9PlayerEvent48Clip;

extern AnimationSet gGolemNo9PlayerEvent58Clip;

extern AnimationSet gGolemNo9PlayerEvent59Clip;

extern AnimationSet gGolemNo9PlayerEvent60Clip;

extern AnimationSet gGolemNo9PlayerEvent61Clip;

/// Dryfield No. 9 GOLEM spawn recipes (0 body, 1 attached gunblade).
///
/// Both are TMD bodies at priority `TASK_PRIORITY_ACTOR` with automatic model buffers; no terminator.
/// Area placement selects the body; its initializer spawns the gunblade with the
/// parent Enemy in spawnArg2. The child borrows the body's work and lighting.
/// Keep actor_521100 and its model/animation resources loaded until both are gone.
extern TaskDesc gActor521100No9GolemTaskDescs[2];

/// Scripted ANMC-woman spawn recipes: 0 nineteen-part model, 1 periodic splatter emitter.
///
/// Both run at priority `TASK_PRIORITY_CONTROL`, with no terminator. Entry 0 skips automatic model
/// buffers and borrows its Enemy through spawn argument 2. Entry 1 has no body;
/// argument 1 selects the woman (0) or player (1), and argument 2 borrows the
/// live woman task. Keep actor_521100 and the selected models loaded through use.
extern TaskDesc gActor521100AnmcWomanTaskDescs[2];

#endif // INCLUDE_ACTORS_ACTOR_521100_H
