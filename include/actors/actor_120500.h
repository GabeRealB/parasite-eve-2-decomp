#ifndef ACTOR_120500_H
#define ACTOR_120500_H

#include "common.h"

#include "actors/actors_shared_8013411c.h"
#include "gameplay/1BC.h"
#include "gameplay/3FB8.h"
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
/// model's light and colour matrices, published through `TmdObject::lightMtx`
/// / `field_20`.
typedef struct Actor120500Work {
    /* 0x000 */ GpAnimCtx  anim;             // `func_800B3F84` arg0
    /* 0x014 */ GpAnimSlot slots[0x14];
    /* 0x334 */ byte       field_334[0x140]; // pose buffer, `func_800B3F84` arg3
    /* 0x474 */ MATRIX     field_474;        // light matrix, into TmdObject::lightMtx
    /* 0x494 */ MATRIX     field_494;        // colour matrix, into TmdObject::colorMtx
    /* 0x4B4 */ Task*      field_4B4;        // Gp_DispatchMsg target for msgs 0x3F4/0x3F3/0x3E9
    /* 0x4B8 */ s16        field_4B8;
    /* 0x4BA */ s16        field_4BA;
    /* 0x4BC */ byte       pad_4BC[0x4];
    /* 0x4C0 */ u16        field_4C0;
    /* 0x4C2 */ s16        field_4C2;
    /* 0x4C4 */ byte       pad_4C4[0x4];
    /* 0x4C8 */ u16        field_4C8;
    /* 0x4CA */ s16        field_4CA;
} Actor120500Work;
STATIC_ASSERT_SIZEOF(Actor120500Work, 0x4CC);

/// Scratch buffer `func_actor_120500_8013241C` fills twice in one tick: as the
/// `GpAnimArg` it hands the player task with message 0x3E8, then as the model's
/// part-1 translation `func_800D7A9C` draws with. The two uses cannot overlap,
/// so retail keeps them in one 0x14-byte stack slot, as `Actor310100Vec` does.
typedef union Actor120500Args {
    /* 0x0 */ GpAnimArg msg; // message 0x3E8 payload
    /* 0x0 */ VECTOR    pos; // model part-1 translation
} Actor120500Args;

/// `Task` as this overlay uses it: only the slot at 0x1C is retyped, holding
/// the actor's own work block rather than a `TaskIdMap`.
typedef struct Actor120500 {
    /* 0x00 */ byte             pad_0[0x1C];
    /* 0x1C */ Actor120500Work* field_1C;
} Actor120500;

extern Actor120500* D_actor_120500_80138454;

/// Equipped-weapon id read by `func_actor_120500_8013241C`: `D_8007218A`
/// selects the alternate weapon block, moving the animation index from `+1`
/// to `+0x22`.
extern u8 D_80073BA9;
extern s8 D_8007218A;

/// Display flags the tick body reads before sending the animation change
/// (`D_80114C12` / `D_80071075`) and the one it raises with the view tasks
/// (`D_8007106B`).
extern s8 D_80114C12;
extern u8 D_80071075;
extern s8 D_8007106B;

/// Animation-set table this overlay hands the task at `field_4B4` as message
/// 0x3F4's `GpAnimArg::field_0`: three sets, the same shape
/// `D_actor_444000_8014430C` has.
extern GpAnimSet* D_actor_120500_8013807C[];

/// Placement records message 0x3E9 hands that same task, taken as raw
/// addresses the way `D_actor_120300_80140ACC` is.
extern s32 D_actor_120500_80138090;
extern s32 D_actor_120500_801380A8;

/// Pair of blocks `func_actor_120500_8013241C` passes to `func_800E8634`.
extern u8 D_actor_120500_801380D8[];
extern u8 D_actor_120500_80138318[];

/// Message 0x7D4's static payload, handed to `Gp_DispatchMsg` as the actor's
/// own placement record.
extern ActorShared8013411cPlacement D_actor_120500_801380C0;

/// Spawn handler this overlay's tick body steps every frame.
void func_actor_120500_80132028(Task* arg0);

void func_actor_120500_801328C0(s16 arg0);
void func_actor_120500_801328E0(s16 arg0);
void func_actor_120500_80132900(s16 arg0);

#endif
