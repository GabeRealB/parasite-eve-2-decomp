#ifndef ACTOR_107600_H
#define ACTOR_107600_H

#include "common.h"

#include "main/task.h"

#include "gameplay/3A34.h"

/// Work block this overlay hangs off `Actor107600.field_1C` (the task's
/// `Task::idMap` slot, which is not a `TaskIdMap` here). The display node at
/// +0x60 is the one the exit callback `func_actor_107600_80134920` hands back
/// to `Gp_UnlinkObj`. The state pair at +0x158/+0x15A is what
/// `func_actor_107600_80134B98` writes: the new state in `field_158` and its
/// sub-state counter cleared.
typedef struct Actor107600Work {
    /* 0x000 */ byte  pad_0[0x60];
    /* 0x060 */ GpObj obj;
    /* 0x080 */ byte  pad_80[0xD8];
    /* 0x158 */ s16   field_158;
    /* 0x15A */ s16   field_15A;
    /* 0x15C */ byte  pad_15C[0xF];
    /* 0x16B */ u8    field_16B;
} Actor107600Work;

typedef struct Actor107600 {
    /* 0x00 */ byte             pad_0[0x1C];
    /* 0x1C */ Actor107600Work* field_1C;
} Actor107600;

void func_actor_107600_80134920(Task* arg0);
void func_actor_107600_80134B98(Actor107600* arg0, s16 arg1);
void func_actor_107600_80134D10(Actor107600* arg0);
void func_actor_107600_80134D30(Actor107600* arg0);
void func_actor_107600_80134D50(Actor107600* arg0);
void func_actor_107600_80134D70(Actor107600* arg0);

#endif
