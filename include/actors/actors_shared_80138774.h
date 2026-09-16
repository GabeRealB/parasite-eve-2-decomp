#ifndef ACTORS_SHARED_80138774_H
#define ACTORS_SHARED_80138774_H

#include "common.h"

#include "gameplay/3FB8.h"

/// Yaw, in `arg0`'s own frame, from `arg0`'s coordinate to the coordinate of
/// the actor held in `Gp_ActorSlots[arg1]`, or 0 when that slot is empty.
///
/// Five actor overlays carry this body, each in its `_2` unit right after the
/// 0x...85E0 / 0x...05E0 spawn state: `actor_101100` and `actor_104900` at
/// 0x80138774 (byte-identical), and `actor_201100`, `actor_204900` and
/// `actor_301100` at 0x80150774, 0x80150774 and 0x80168774.
s32 ActorsShared80138774(GsCOORDINATE2* arg0, s32 arg1);

#endif // ACTORS_SHARED_80138774_H
