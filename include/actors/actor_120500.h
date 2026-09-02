#ifndef ACTOR_120500_H
#define ACTOR_120500_H

#include "common.h"

/// Work block this overlay hangs off `Actor120500.field_1C`; the pair at
/// 0x4C8 is a request code plus its phase counter, reset together.
typedef struct Actor120500Work {
    /* 0x000 */ byte pad_0[0x4C8];
    /* 0x4C8 */ s16  field_4C8;
    /* 0x4CA */ s16  field_4CA;
} Actor120500Work;

/// `Task` as this overlay uses it: only the slot at 0x1C is retyped, holding
/// the actor's own work block rather than a `TaskIdMap`.
typedef struct Actor120500 {
    /* 0x00 */ byte             pad_0[0x1C];
    /* 0x1C */ Actor120500Work* field_1C;
} Actor120500;

extern Actor120500* D_actor_120500_80138454;

void func_actor_120500_80132900(s16 arg0);

#endif
