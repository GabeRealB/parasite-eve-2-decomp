#ifndef ACTORS_SHARED_801511C8_H
#define ACTORS_SHARED_801511C8_H

#include "common.h"
#include "main/task.h"
#include "gameplay/3A34.h"

/// Work block of the task served by this shared body. Only the `GpObj` display
/// node at 0x8 is reached from here -- `ActorsShared801511c8` is the exit
/// callback that unlinks it -- so the type stops there; whatever each overlay
/// keeps after it differs per actor.
typedef struct ActorShared801511c8Work {
    /* 0x00 */ byte  pad_0[0x8];
    /* 0x08 */ GpObj obj;
} ActorShared801511c8Work;

void ActorsShared801511c8(Task* arg0);

#endif
