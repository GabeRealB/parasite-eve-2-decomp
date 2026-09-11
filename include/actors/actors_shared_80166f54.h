#ifndef ACTORS_SHARED_80166F54_H
#define ACTORS_SHARED_80166F54_H

#include "common.h"

#include "main/task.h"

#include "actors/actors_shared_80168d3c.h"

/// Unless `ActorsShared8016945c` claims the frame: count the frame in
/// `field_412` and, on frames 0x17..0x23, push the model root along the
/// heading `field_7A` turned a quarter circle, by `field_41C` scaled -30/16.
/// Once `flags_EC` reports a hit (bit 0 as a halfword, or 0x102 as a word),
/// `field_438` is cleared and the state machine rewinds to state 0.
/// Shared by `actor_341700` and `actor_342400`.
void ActorsShared80166f54(Task* arg0);

#endif
