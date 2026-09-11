#ifndef ACTORS_SHARED_8016B1C8_H
#define ACTORS_SHARED_8016B1C8_H

#include "common.h"

#include "main/task.h"

#include "actors/actors_shared_80168d3c.h"

/// Jumps the work block's state machine to state 1, sub-state 0, when bit 0
/// of the `flags_EC` halfword or any of bits 0x102 of the whole word is set -
/// the same test as `ActorsShared8016974c`, inlined. Shared by `actor_341700`
/// and `actor_342400`, whose work blocks are the same
/// `ActorsShared80168d3cWork`.
void ActorsShared8016b1c8(Task* arg0);

#endif
