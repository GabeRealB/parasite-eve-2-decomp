#ifndef ACTORS_SHARED_80168174_H
#define ACTORS_SHARED_80168174_H

#include "common.h"

#include "main/task.h"

#include "actors/actors_shared_80168d3c.h"

/// Count the frame and ease `field_78` an eighth of the way to 0x800. On the
/// first frame play sounds 0x402C0009 and 0x402C0003 (bank from the enemy's
/// `field_8` high nibble) panned and attenuated from the model root. Every
/// frame, push the root back 0x5A against the heading `field_7A` and fall as
/// `ActorsShared8016784c` does. Once the root passes above y = 0 it is pinned
/// at -0x3C, `field_78` / `field_7C` clear, the heading turns half a circle,
/// animation 0x11 is requested and the state advances. Shared by
/// `actor_341700` and `actor_342400`.
void ActorsShared80168174(Task* arg0);

#endif
