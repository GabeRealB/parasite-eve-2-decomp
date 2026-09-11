#ifndef ACTORS_SHARED_80166C68_H
#define ACTORS_SHARED_80166C68_H

#include "common.h"

#include "main/task.h"

#include "actors/actors_shared_80168d3c.h"

/// Counts the frame in `field_412` and, on frames 0x1D..0x29, pushes the model
/// root along the heading `field_7A` turned a quarter circle, by `field_41C`
/// scaled 30/16. Once `flags_EC` reports a hit (bit 0 as a halfword, or 0x102
/// as a word), clears `field_438` and puts the task in state 3 with its work
/// block at state 3. Shared by `actor_341700` and `actor_342400`.
void ActorsShared80166c68(Task* arg0);

#endif
