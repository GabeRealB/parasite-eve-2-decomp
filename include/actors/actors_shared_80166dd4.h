#ifndef ACTORS_SHARED_80166DD4_H
#define ACTORS_SHARED_80166DD4_H

#include "common.h"

#include "main/task.h"

#include "actors/actors_shared_80168d3c.h"

/// Counts the frame in `field_412` and, on frames 0x1D..0x29, pushes the model
/// root along the heading `field_7A` turned a quarter circle, by `field_41C`
/// scaled 30/16. Once `flags_EC` reports a hit (bit 0 as a halfword, or 0x102
/// as a word), clears `field_438`, requests animation 3 (kind 1, speed 0x10,
/// `field_426` 8) and advances to the next sub-state with a fresh frame count.
/// Shared by `actor_341700` and `actor_342400`.
void ActorsShared80166dd4(Task* arg0);

#endif
