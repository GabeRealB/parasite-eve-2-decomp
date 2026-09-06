#ifndef ACTOR_120500_H
#define ACTOR_120500_H

#include "common.h"

/// Work block this overlay hangs off `Actor120500.field_1C`; the pairs at
/// 0x4B8, 0x4C0 and 0x4C8 are each a request code plus its phase counter,
/// reset together.
typedef struct Actor120500Work {
    /* 0x000 */ byte pad_0[0x4B8];
    /* 0x4B8 */ s16  field_4B8;
    /* 0x4BA */ s16  field_4BA;
    /* 0x4BC */ byte pad_4BC[0x4];
    /* 0x4C0 */ s16  field_4C0;
    /* 0x4C2 */ s16  field_4C2;
    /* 0x4C4 */ byte pad_4C4[0x4];
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

void func_actor_120500_801328C0(s16 arg0);
void func_actor_120500_801328E0(s16 arg0);
void func_actor_120500_80132900(s16 arg0);

#endif
