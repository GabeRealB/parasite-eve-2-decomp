#ifndef ACTORS_SHARED_801652A0_H
#define ACTORS_SHARED_801652A0_H

#include "common.h"

#include "main/task.h"

#include "actors/actors_shared_80168d3c.h"

/// Count the frame; on frames 1 and 2 play sounds 0x402C0004 and 0x402C0003
/// (bank from the enemy's `field_8` high nibble) panned and attenuated from
/// the model root. Once the `flags_EC` test (`ActorsShared8016974c`) fires,
/// set the task state to 3 and switch to state 3, sub-state 0. Shared by
/// `actor_341700` and `actor_342400`, whose work blocks are the same
/// `ActorsShared80168d3cWork`.
void ActorsShared801652a0(Task* arg0);

#endif
