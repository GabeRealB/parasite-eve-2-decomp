#ifndef ACTORS_SHARED_8016B294_H
#define ACTORS_SHARED_8016B294_H

#include "common.h"

#include "main/task.h"

#include "actors/actors_shared_80168d3c.h"

/// On the same `flags_EC` test as `ActorsShared8016b21c` (bit 0 of the
/// halfword or any of bits 0x102 of the whole word), requests animation 0xE
/// at step 0x10, rolls a 0xB0..0xEF frame hold into `field_446` from the
/// global LCG, clears the frame counter and `field_42C`, and advances the
/// sub-state. Shared by `actor_341700` and `actor_342400`, whose work blocks
/// are the same `ActorsShared80168d3cWork`.
void ActorsShared8016b294(Task* arg0);

#endif
