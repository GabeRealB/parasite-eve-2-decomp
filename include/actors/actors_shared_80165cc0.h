#ifndef ACTORS_SHARED_80165CC0_H
#define ACTORS_SHARED_80165CC0_H

#include "common.h"

#include "main/task.h"

#include "actors/actors_shared_80168d3c.h"

/// Applies the animation request in `field_414` to slots 1..8: 1 seeks every
/// slot to `field_418` (clearing `field_426` afterwards when the id changed),
/// 2 resets them to it, and both then settle on 3, which counts frames in
/// `field_41A`. Every call ticks the slots at step `field_41C`.
/// Shared by `actor_341700` and `actor_342400`.
void ActorsShared80165cc0(Task* arg0);

#endif
