#ifndef ACTOR_104900_H
#define ACTOR_104900_H

#include "common.h"

#include "actors/actors_shared_80137fb8.h"
#include "gameplay/3FB8.h"
#include "main/session.h"
#include "main/task.h"

/// Player flag byte read by the 0x801339B0 handler: when it is 1 the handler
/// takes its `GameActor::field_958` shortcut instead of measuring distance.
extern u8 D_801153F2;

typedef ActorsShared80137fb8Work Actor104900Work;

#endif // ACTOR_104900_H
