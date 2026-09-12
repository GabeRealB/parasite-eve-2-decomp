#ifndef ACTORS_SHARED_80132CB8_H
#define ACTORS_SHARED_80132CB8_H

#include "common.h"

#include "main/task.h"

/// Rebuild collision face `index` from the actor's root coordinate: the face
/// normal becomes the model's forward axis scaled by `scale`, and the four
/// corners straddle the model's origin by its sideways axis at a fixed 5000.
/// Corners 0/2 sit one side and 1/3 the other, then 0 and 1 - the lower edge -
/// are dropped by `drop`. The normal is finally refreshed as the plain unit
/// forward axis, and the face record is re-stamped as a quad (kind 3), or kind
/// 2 outside area 0x27.
void ActorsShared80132cb8(Task* task, s16 scale, s16 drop, s16 index);

#endif
