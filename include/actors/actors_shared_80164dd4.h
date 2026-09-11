#ifndef ACTORS_SHARED_80164DD4_H
#define ACTORS_SHARED_80164DD4_H

#include "common.h"

#include "main/task.h"

#include "actors/actors_shared_80168d3c.h"

/// Push the model root 0xC8 along the heading `field_40C` and fall under an
/// accelerating drop (`field_428` the acceleration, grown by 0xE a frame;
/// `field_42A` the speed, also added to `obj_2CC.field_12`). Once the root
/// reaches the floor `field_92` it is pinned there, animation 0x13 (kind 1,
/// speed 0x10) is requested and the sub-state advances. Shared by
/// `actor_341700` and `actor_342400`.
void ActorsShared80164dd4(Task* arg0);

#endif
