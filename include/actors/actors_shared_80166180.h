#ifndef ACTORS_SHARED_80166180_H
#define ACTORS_SHARED_80166180_H

#include "common.h"

#include "main/task.h"

#include "actors/actors_shared_8016bd98.h"

/// `ActorsShared8016bd98` with a spark burst: the model root takes `mat_0`
/// squashed vertically by the shrinking `field_430`, frame 4 spawns effect
/// 0x600A5 on the root coord, frame 16 rotates the light mode to 2, and after
/// frame 32 the model is flagged 0x80 and the state advances without
/// resetting the frame counter. Shared by `actor_341700` and `actor_342400`.
void ActorsShared80166180(Task* arg0);

#endif
