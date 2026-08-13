#ifndef GAMEPLAY_3FB8_H
#define GAMEPLAY_3FB8_H

#include "common.h"

#include "main/session.h"

/// Work object whose `actor` pointer sits at 0x1C (same slot as `Task::idMap`).
typedef struct _GpActorWork {
    /* 0x00 */ byte       pad_0[0x1C];
    /* 0x1C */ GameActor* actor;
} GpActorWork;

void func_80109170(GpActorWork* arg0);
void func_8010A42C(GpActorWork* arg0, s32 arg1);
void func_80103B5C(GpActorWork* arg0);
void func_8010B210(GpActorWork* arg0);

#endif // GAMEPLAY_3FB8_H
