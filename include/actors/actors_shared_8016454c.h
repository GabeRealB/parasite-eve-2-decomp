#ifndef ACTORS_SHARED_8016454C_H
#define ACTORS_SHARED_8016454C_H

#include "common.h"

#include "main/task.h"

/// Request animation 7 with sound 0x402C0001 panned to the model, draw a
/// random 0..0x7FF `field_410`, and pick the animation speed and `field_436`
/// step from the band `field_43A` (distance to the nearer player actor) falls
/// in: below 1000, then every 1000 up to 5000.
/// Shared by `actor_341700` and `actor_342400`.
void ActorsShared8016454c(Task* arg0);

#endif
