#ifndef ACTORS_SHARED_8016A084_H
#define ACTORS_SHARED_8016A084_H

#include "common.h"

#include "main/task.h"

#include "actors/actors_shared_80168d3c.h"

/// Clear `field_438` and count the frame; on the first frame play sound
/// 0x402C0004 (bank from the enemy's `field_8` high nibble) panned and
/// attenuated from the model root. Once the `flags_EC` test
/// (`ActorsShared8016974c`) fires, roll `field_44A` from `Gp_LcgState` into
/// 0x5A..0xD9 and switch to state 3, sub-state 0. Shared by `actor_341700` and
/// `actor_342400`, whose work blocks are the same `ActorsShared80168d3cWork`.
void ActorsShared8016a084(Task* arg0);

#endif
