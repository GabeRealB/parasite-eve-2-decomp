#ifndef ACTORS_SHARED_80164CA4_H
#define ACTORS_SHARED_80164CA4_H

#include "common.h"

#include "main/task.h"

#include "actors/actors_shared_80168d3c.h"

/// Count the frame; on the first frame play sound 0x402C0004 (bank from the
/// enemy's `field_8` high nibble) panned and attenuated from the model root.
/// Once the `flags_EC` test (`ActorsShared8016974c`) fires, clear `field_438`,
/// roll `field_44A` from `Gp_LcgState` into 0x5A..0xD9, turn the heading half
/// a revolution, request animation 0xD (kind 2, speed 0x10) and return to
/// state 0, sub-state 0. Shared by `actor_341700` and `actor_342400`, whose
/// work blocks are the same `ActorsShared80168d3cWork`.
void ActorsShared80164ca4(Task* arg0);

#endif
