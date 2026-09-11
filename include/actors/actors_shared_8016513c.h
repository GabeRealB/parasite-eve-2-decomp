#ifndef ACTORS_SHARED_8016513C_H
#define ACTORS_SHARED_8016513C_H

#include "common.h"

#include "main/task.h"

#include "actors/actors_shared_8016a538.h"

/// Ease the pitch `field_434` a quarter of the way back to zero, rebuild the
/// model root's rotation from it and the heading `field_7A`, then drop the
/// root under an accelerating fall (`field_428` the acceleration, `field_42A`
/// the speed). Once it reaches the floor (Y 0) it stops there, requests
/// animation 12 (kind 2, speed 0x20) and advances `field_422`. Shared by
/// `actor_341700` and `actor_342400`.
void ActorsShared8016513c(Task* arg0);

#endif
