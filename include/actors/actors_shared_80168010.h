#ifndef ACTORS_SHARED_80168010_H
#define ACTORS_SHARED_80168010_H

#include "common.h"

#include "main/task.h"

#include "actors/actors_shared_80168d3c.h"

/// Push the model root 0x8C back against the heading `field_7A`, ease
/// `field_78` a 32nd of the way to 0x200, and fall under an accelerating drop
/// (`field_428` the acceleration, `field_42A` the speed). Once the root passes
/// above y = 0 it is pinned at -0x3C, the fall rearms at speed -0x12C,
/// animation 12 (kind 2, speed 0x10) is requested and the state advances.
/// Shared by `actor_341700` and `actor_342400`.
void ActorsShared80168010(Task* arg0);

#endif
