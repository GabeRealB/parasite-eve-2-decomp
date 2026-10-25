#ifndef INCLUDE_ACTORS_ACTOR_02600_H
#define INCLUDE_ACTORS_ACTOR_02600_H

#include "main/task_types.h"

/// Slot-2 export of `gActor02600MaggotCaterpillarBodyTask` from actor_202600,
/// with the same body descriptor contract.
extern TaskDesc gActor202600MaggotCaterpillarBodyTask;

/// Maggot body spawn recipe with an eight-part TMD model and execution priority 96.
///
/// Spawn at index 0 with a live `Enemy*` in spawn argument 2. Its placement
/// record supplies the entrance mode; the body dispatches setup, active and
/// dying phases. Model primitive buffers use the normal automatic allocation
/// policy. Keep the actor package and borrowed model geometry loaded while the
/// body or its puff children remain live.
extern TaskDesc gActor02600MaggotCaterpillarBodyTask;

#endif // INCLUDE_ACTORS_ACTOR_02600_H
