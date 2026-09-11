#ifndef ACTORS_SHARED_801662EC_H
#define ACTORS_SHARED_801662EC_H

#include "common.h"

#include "main/task.h"

#include "actors/actors_shared_80168d3c.h"

/// Latch the model root position into `field_60`, then aim at the nearer of
/// the two `Gp_ActorSlots` actors (distance measured in XZ): its offset goes
/// to `field_88`..`field_8C`, the distance to `field_43A`, and its heading
/// relative to `field_7A` to `field_444`. Nothing is written when slot 0 is
/// empty. Shared by `actor_341700` and `actor_342400`.
void ActorsShared801662ec(Task* arg0);

#endif
