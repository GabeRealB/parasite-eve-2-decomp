#ifndef INCLUDE_ACTORS_ACTOR_420700_H
#define INCLUDE_ACTORS_ACTOR_420700_H

#include "main/task_types.h"

#include "gameplay/animation.h"

// Animation sets used by the trailer-coach event scripts.
extern AnimationSet gDryfieldTrailerCoachPlayerEvent47Clip;

extern AnimationSet gDryfieldTrailerCoachPlayerEvent48Clip;

extern AnimationSet gDryfieldTrailerCoachPlayerEvent49Clip;

extern AnimationSet gDryfieldTrailerCoachPlayerEvent50Clip;

extern AnimationSet gDryfieldTrailerCoachPlayerConversationClip;

extern AnimationSet gDryfieldTrailerCoachPlayerEvent52Clip;

extern AnimationSet gDryfieldTrailerCoachPlayerEvent53Clip;

extern AnimationSet gDryfieldTrailerCoachPlayerEvent54Clip;

extern AnimationSet gDryfieldTrailerCoachPlayerEvent58Clip;

extern AnimationSet gDryfieldTrailerCoachPlayerEvent59Clip;

extern AnimationSet gDryfieldTrailerCoachPlayerEvent60Clip;

extern AnimationSet gDryfieldTrailerCoachPlayerEvent61Clip;

/// Trailer-coach Gary Douglas spawn recipes: 0 body, 1 head-and-hat, 2 shotgun.
///
/// All are TMD bodies at priority `TASK_PRIORITY_CONTROL`, without a terminator. Body placement
/// supplies a live Enemy in spawn argument 2; attachment spawns use zero
/// arguments and borrow the published singleton body. Keep actor_420700 loaded.
extern TaskDesc gActor420700GaryDouglasTaskDescriptors[3];

#endif // INCLUDE_ACTORS_ACTOR_420700_H
