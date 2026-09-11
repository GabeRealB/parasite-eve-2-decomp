#ifndef ACTORS_SHARED_8016B370_H
#define ACTORS_SHARED_8016B370_H

#include "common.h"

#include "main/task.h"

#include "actors/actors_shared_80168d3c.h"

/// Resets the work block's state machine to state 0, sub-state 0, on the same
/// `flags_EC` test as `ActorsShared8016b1c8` (bit 0 of the halfword or any of
/// bits 0x102 of the whole word). Shared by `actor_341700` and
/// `actor_342400`, whose work blocks are the same `ActorsShared80168d3cWork`.
void ActorsShared8016b370(Task* arg0);

#endif
