#ifndef ACTORS_SHARED_80132C8C_H
#define ACTORS_SHARED_80132C8C_H

#include "common.h"

#include "actors/actors_shared_80132b88.h"

/// Same script command as `ActorsShared80132b88` - latch the payload angle into
/// the work block's `field_4EE` - except the angle is only written when it is
/// exactly 1, the "reset facing" command; every other payload is ignored. The
/// work block is still read unconditionally, so it is fetched before the test.
s32 ActorsShared80132c8c(ActorShared80132b88* arg0, s32 arg1, ActorShared80132b88Cmd* arg2);

#endif
