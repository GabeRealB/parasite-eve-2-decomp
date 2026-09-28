#ifndef GAMEPLAY_AREA_ENTRY_H
#define GAMEPLAY_AREA_ENTRY_H

#include "main/task.h"

/// The tasks of the two player-side actors, whose `work` is each actor's
/// `GameActor`. Slot 0 is the player's: `Gp_InitPlayerWork` claims it and
/// `Gp_TeardownSlot0` releases it. Slot 1 is the companion's, which that
/// actor's set-up claims the same way. The node and lock-on helpers walk
/// both slots.
extern Task* Gp_ActorSlots[2];

#endif // GAMEPLAY_AREA_ENTRY_H
