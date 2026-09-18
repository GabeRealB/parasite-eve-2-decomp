#ifndef ACTORS_SHARED_801344AC_H
#define ACTORS_SHARED_801344AC_H

#include "common.h"

#include "main/task.h"

/// The 8-byte fade block the family's display-fade tasks allocate with
/// `Mem_Malloc(8, 0)` and park in `Task::work`: two bytes of padding, then the
/// three halfword colour channels.  `ActorsShared801344ac` seeds all three
/// saturated and walks them *down* by `Task::spawnArg1`; the fade-out task
/// beside it in `actor_136100` is the same shape walked up instead.
typedef struct ActorShared801344acWork {
    /* 0x0 */ u8  pad_0[0x2];
    /* 0x2 */ s16 r;
    /* 0x4 */ s16 g;
    /* 0x6 */ s16 b;
} ActorShared801344acWork;
STATIC_ASSERT_SIZEOF(ActorShared801344acWork, 0x8);

void ActorsShared801344ac(Task* arg0);

#endif
