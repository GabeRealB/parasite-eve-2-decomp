#ifndef ACTOR_SPECIMEN_INIT_H
#define ACTOR_SPECIMEN_INIT_H

#include "gameplay/1BC.h"

/// Each specimen overlay supplies its own parameter record and animation bank.
extern GpPairSrcE ActorSpecimenInitParams;
extern u8         ActorSpecimenInitAnimBank[];

/// Initializes the specimen's animation slots, three collision nodes and task.
void ActorSpecimenInit(GpEnemy* enemy, Task* task);

#endif
