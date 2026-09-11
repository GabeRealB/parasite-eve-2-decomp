#ifndef ACTORS_SHARED_80167CDC_H
#define ACTORS_SHARED_80167CDC_H

#include "common.h"

#include "main/task.h"

#include "actors/actors_shared_80168d3c.h"

/// Count the frame and decay `field_78` by a thirty-second towards 0. On the
/// first frame play sound 0x402C0009 (bank from the enemy's `field_8` high
/// nibble) panned and attenuated from the model root. Every frame, push the
/// root back 0x50 against the heading `field_7A` and fall as
/// `ActorsShared8016784c` does. Once the root passes above y = 0 it is pinned
/// at -0x3C, the frame counter clears and the state advances. Shared by
/// `actor_341700` and `actor_342400`.
void ActorsShared80167cdc(Task* arg0);

#endif
