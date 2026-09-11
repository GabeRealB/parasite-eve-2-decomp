#ifndef ACTORS_SHARED_801698D4_H
#define ACTORS_SHARED_801698D4_H

#include "common.h"

#include "main/task.h"

#include "actors/actors_shared_80168d3c.h"

/// Turn the heading `field_7A` by `step` towards the reverse of the XZ vector
/// at `field_88` / `field_8C`, leaving it alone while the wrapped 12-bit
/// difference is within 0x100 either way. Shared by `actor_341700` and
/// `actor_342400`.
void ActorsShared801698d4(Task* arg0, s32 step);

#endif
