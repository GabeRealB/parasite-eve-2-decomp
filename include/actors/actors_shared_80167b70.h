#ifndef ACTORS_SHARED_80167B70_H
#define ACTORS_SHARED_80167B70_H

#include "common.h"

#include "main/task.h"

#include "actors/actors_shared_80168d3c.h"

/// The fall of `ActorsShared80168010` with a different landing: push the model
/// root 0x8C back against the heading `field_7A`, ease `field_78` towards
/// 0x200 and drop under the `field_428` / `field_42A` acceleration. Once the
/// root passes above y = 0 it is pinned at -0x3C, `field_78` / `field_7C` are
/// cleared, the fall rearms at speed -0x6E, animation 12 (kind 2, speed 0x10)
/// is requested and the state advances. Shared by `actor_341700` and
/// `actor_342400`.
void ActorsShared80167b70(Task* arg0);

#endif
