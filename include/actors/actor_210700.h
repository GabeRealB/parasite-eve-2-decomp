#ifndef INCLUDE_ACTORS_ACTOR_210700_H
#define INCLUDE_ACTORS_ACTOR_210700_H

#include "main/task_types.h"

#include "gameplay/animation.h"

// Native animation sets referenced by room script banks.
extern AnimationSet gAkropolisSanctuaryPlayerSanctuaryArrival7Clip;

extern AnimationSet gAkropolisSanctuaryPlayerSanctuaryArrival11Clip;

extern AnimationSet gAkropolisSanctuaryPlayerSanctuaryTalkClip;

extern AnimationSet gAkropolisSanctuaryPlayerSanctuaryTalkEndClip;

extern AnimationSet gAkropolisSanctuaryPlayerSanctuaryReplyClip;

extern AnimationSet gAkropolisSanctuaryPlayerSanctuaryListenClip;

extern AnimationSet gAkropolisSanctuaryPlayerSanctuaryArrival26Clip;

extern AnimationSet gAkropolisSanctuaryPlayerSanctuaryArrival27Clip;

/// Rupert Broderick spawn recipe for the sanctuary scene.
///
/// Priority 192. Spawn at index 0.
/// TMD bodies use automatic primitive buffers.
/// Body `Task::spawnArg2` borrows its live `Enemy` for placement and teardown.
/// Keep actor_210700 callbacks and borrowed model/animation resources loaded
/// while the spawned tasks use them.
extern TaskDesc gActor210700RupertBroderickTaskDesc;

#endif // INCLUDE_ACTORS_ACTOR_210700_H
