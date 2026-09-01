#ifndef ACTOR_107600_H
#define ACTOR_107600_H

#include "common.h"

/// Work block this overlay hangs off `Actor107600.field_1C`. The state pair at
/// +0x158/+0x15A is what `func_actor_107600_80134B98` writes: the new state in
/// `field_158` and its sub-state counter cleared.
typedef struct Actor107600Work {
    /* 0x000 */ byte pad_0[0x158];
    /* 0x158 */ s16  field_158;
    /* 0x15A */ s16  field_15A;
} Actor107600Work;

typedef struct Actor107600 {
    /* 0x00 */ byte             pad_0[0x1C];
    /* 0x1C */ Actor107600Work* field_1C;
} Actor107600;

void func_actor_107600_80134B98(Actor107600* arg0, s16 arg1);
void func_actor_107600_80134D10(Actor107600* arg0);
void func_actor_107600_80134D30(Actor107600* arg0);
void func_actor_107600_80134D50(Actor107600* arg0);

#endif
