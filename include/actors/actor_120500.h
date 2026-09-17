#ifndef ACTOR_120500_H
#define ACTOR_120500_H

#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

/// Work block this overlay hangs off `Actor120500.field_1C`; the pairs at
/// 0x4B8, 0x4C0 and 0x4C8 are each a request code plus its phase counter,
/// reset together.
///
/// `func_actor_120500_801322A0` allocates it with `Mem_Malloc(0x4CC, 0)` and
/// zeroes it with `Mem_Set`.  It opens with the animation prefix the other
/// model actors carry: the 0x14-byte `GpAnimCtx` `func_800B3F84` is handed as
/// its `arg0`, the twenty 0x28-byte `GpAnimSlot`s `Gp_AnimResetSlot` walks,
/// and the pose buffer at 0x334.  The two `MATRIX`es at 0x474 / 0x494 are the
/// model's light and colour matrices, published through `TmdObject::field_1C`
/// / `field_20`.
typedef struct Actor120500Work {
    /* 0x000 */ GpAnimCtx  anim;             // `func_800B3F84` arg0
    /* 0x014 */ GpAnimSlot slots[0x14];
    /* 0x334 */ byte       field_334[0x140]; // pose buffer, `func_800B3F84` arg3
    /* 0x474 */ MATRIX     field_474;        // light matrix, into TmdObject::field_1C
    /* 0x494 */ MATRIX     field_494;        // colour matrix, into TmdObject::field_20
    /* 0x4B4 */ Task*      field_4B4;        // Gp_DispatchMsg target for msgs 0x3F4/0x3F3/0x3E9
    /* 0x4B8 */ s16        field_4B8;
    /* 0x4BA */ s16        field_4BA;
    /* 0x4BC */ byte       pad_4BC[0x4];
    /* 0x4C0 */ s16        field_4C0;
    /* 0x4C2 */ s16        field_4C2;
    /* 0x4C4 */ byte       pad_4C4[0x4];
    /* 0x4C8 */ s16        field_4C8;
    /* 0x4CA */ s16        field_4CA;
} Actor120500Work;
STATIC_ASSERT_SIZEOF(Actor120500Work, 0x4CC);

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
