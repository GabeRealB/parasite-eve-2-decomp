#ifndef ACTORS_SHARED_8016784C_H
#define ACTORS_SHARED_8016784C_H

#include "common.h"

#include "main/task.h"

#include "actors/actors_shared_80168d3c.h"

/// Count the frame; on the first frame play sound 0x402C0009 (bank from the
/// enemy's `field_8` high nibble) panned and attenuated from the model root.
/// Every frame, push the root 0x50 along the heading `field_7A` and fall:
/// `field_428` grows by 4 into the velocity `field_42A` added to `coord.t[1]`.
/// Once the root passes above y = 0 it is pinned at -0x3C, the frame counter
/// clears and the state advances. Shared by `actor_341700` and
/// `actor_342400`, whose work blocks are the same `ActorsShared80168d3cWork`.
void ActorsShared8016784c(Task* arg0);

#endif
