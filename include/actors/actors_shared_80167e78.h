#ifndef ACTORS_SHARED_80167E78_H
#define ACTORS_SHARED_80167E78_H

#include "common.h"

#include "main/task.h"

#include "actors/actors_shared_80168d3c.h"

/// Count the frame and on the first one play sound 0x402C0009 (bank from the
/// enemy's `field_8` high nibble) panned and attenuated from the model root.
/// Every frame, push the root 0x14 back against the heading `field_7A`. Once
/// status bit 0 or bits 0x102 of `flags_EC` are set, flag `obj_2CC` with
/// 0x4000 and switch the task to state 3 with the work block's state 3,
/// sub-state 0. Shared by `actor_341700` and `actor_342400`.
void ActorsShared80167e78(Task* arg0);

#endif
