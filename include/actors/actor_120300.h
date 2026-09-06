#ifndef ACTOR_120300_H
#define ACTOR_120300_H

#include "common.h"

/// Work block this overlay hangs off `Actor120300.field_1C`; each pair at
/// 0x4C0 and 0x4C8 is a request code plus its phase counter, reset together.
typedef struct Actor120300Work {
    /* 0x000 */ byte pad_0[0x4C0];
    /* 0x4C0 */ s16  field_4C0;
    /* 0x4C2 */ s16  field_4C2;
    /* 0x4C4 */ byte pad_4C4[0x4];
    /* 0x4C8 */ s16  field_4C8;
    /* 0x4CA */ s16  field_4CA;
} Actor120300Work;

/// `Task` as this overlay uses it: only the slot at 0x1C is retyped, holding
/// the actor's own work block rather than a `TaskIdMap`.
typedef struct Actor120300 {
    /* 0x00 */ byte             pad_0[0x1C];
    /* 0x1C */ Actor120300Work* field_1C;
} Actor120300;

extern Actor120300* D_actor_120300_80141BA8;

void func_actor_120300_80133E14(s16 arg0);
void func_actor_120300_80133E34(s16 arg0);

#endif
