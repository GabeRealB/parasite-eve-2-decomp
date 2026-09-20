#ifndef ACTOR_104900_H
#define ACTOR_104900_H

#include "common.h"

#include "actors/actors_shared_80137fb8.h"
#include "gameplay/3A34.h"
#include "gameplay/3FB8.h"
#include "main/session.h"
#include "main/task.h"

/// Player flag byte read by the 0x801339B0 handler: when it is 1 the handler
/// takes its `GameActor::field_958` shortcut instead of measuring distance.
extern u8 D_801153F2;

typedef ActorsShared80137fb8Work Actor104900Work;

/// Pair table the spawn state packs into the display node's `GpObj.key`.
extern GpU16Pair D_actor_104900_80139318;

/// Overlay-local spawn/setup state of `ActorsShared8013845c`: allocates the
/// 0x58-byte work block, plays the spawn cue, seeds the display node and
/// hands off to `ActorsShared8013845cSub1`.
void ActorsShared8013845cSub0(Task* task);
void ActorsShared8013845cSub1(Task* task);

#endif // ACTOR_104900_H
