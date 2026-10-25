#ifndef INCLUDE_ACTORS_ACTOR_210600_H
#define INCLUDE_ACTORS_ACTOR_210600_H

#include "main/task_types.h"

#include "gameplay/animation.h"

// Native animation sets referenced by room script banks.
extern AnimationSet gAkropolisCafeteriaPlayerRupertTalkClip;

extern AnimationSet gAkropolisCafeteriaPlayerFollowUpEndClip;

extern AnimationSet gAkropolisCafeteriaBossStrangerCollapseClip;

extern AnimationSet gStrangerLesserIdleClip;

extern AnimationSet gAkropolisCafeteriaBossStrangerFollowUp3Clip;

extern AnimationSet gAkropolisCafeteriaExtraAnim7Clip;

extern AnimationSet gAkropolisCafeteriaExtraAnim6Clip;

/// Grinning Stranger spawn recipe for the cafeteria scene.
///
/// Priority 96. Spawn at index 0.
/// TMD bodies skip automatic primitive buffers.
/// Body `Task::spawnArg2` borrows its live `Enemy` for setup, update and teardown.
/// Keep actor_210600 callbacks and borrowed model/animation resources loaded
/// while the spawned tasks use them.
extern TaskDesc gActor210600GrinningStrangerTaskDesc;

#endif // INCLUDE_ACTORS_ACTOR_210600_H
