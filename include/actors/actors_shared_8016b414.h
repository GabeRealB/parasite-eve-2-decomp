#ifndef ACTORS_SHARED_8016B414_H
#define ACTORS_SHARED_8016B414_H

#include "common.h"

#include "main/task.h"

#include "actors/actors_shared_80168d3c.h"

/// On the same `flags_EC` test as `ActorsShared8016b370` (bit 0 of the
/// halfword or any of bits 0x102 of the whole word), clears the per-state
/// frame counter, sets `field_438`, requests animation 4 at step 0x10 and
/// advances the sub-state. Shared by `actor_341700` and `actor_342400`, whose
/// work blocks are the same `ActorsShared80168d3cWork`.
void ActorsShared8016b414(Task* arg0);

#endif
