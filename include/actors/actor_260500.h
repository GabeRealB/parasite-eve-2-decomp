#ifndef ACTOR_260500_H
#define ACTOR_260500_H

#include "common.h"

#include "actors/actors_shared_801326b4.h"
#include "gameplay/1BC.h"
#include "main/session.h"
#include "main/task.h"

/// Per-actor work block for the `actor_260500` overlay.
///
/// The overlay's `ActorsShared80131f9cSub0` (here at 0x80149FB0) allocates it
/// with `memCalloc(0x4B8, 0)` and stores the pointer both in
/// `ActorsShared80131f9cWork` and in the task's 0x1C slot, so the size below is
/// the allocation and not a guess. Every other function in the overlay reaches
/// the block through the global.
///
/// It opens with the light and colour matrices the actor's model is drawn
/// under, then the animation context with nineteen slots and one 0x10-byte
/// pose record per slot.
typedef struct Actor260500Work {
    /* 0x000 */ MATRIX     light;
    /* 0x020 */ MATRIX     color;
    /* 0x040 */ GpAnimCtx  anim;
    /* 0x054 */ GpAnimSlot slots[0x13];
    /* 0x34C */ byte       poses[0x13][0x10];
    /* 0x47C */ s16        field_47C; // animation reset mode `func_actor_260500_8014A110` dispatches on (1 reseeds via `func_actor_260500_8014A644`, 2 via `ActorsShared80132428`, 3 after)
    /* 0x47E */ byte       pad_47E[0x2];
    /* 0x480 */ s16        field_480; // animation id the reset is seeded with, latched from the preset's `field_4`
    /* 0x482 */ s16        field_482; // cleared before the reset is handed to `func_actor_260500_8014A110`
    /* 0x484 */ byte       pad_484[0x2A];
    /* 0x4AE */ s16        field_4AE; // yaw the model is turned to face the target
    /* 0x4B0 */ byte       pad_4B0[0x2];
    /* 0x4B2 */ s16        field_4B2; // planar distance over the step count below
    /* 0x4B4 */ s16        field_4B4; // Role unproven: the state-0 handler clears it
    /* 0x4B6 */ byte       pad_4B6[0x2];
} Actor260500Work;
STATIC_ASSERT_SIZEOF(Actor260500Work, 0x4B8);

/// The block above, published by `ActorsShared80131f9c` from the task's
/// `Task::work`. Declared here with the type the overlay reads it through,
/// the same way `include/actors/actor_260400.h` does; the shared header
/// (`actors_shared_80131f9c.h`) publishes the bare `void*`.
extern Actor260500Work* ActorsShared80131f9cWork;

/// Reset argument the reseed forwards. The play-animation handler latches the
/// preset's `field_C` here and `func_actor_260500_8014A110`'s reseed path reads
/// it back.
extern s16 D_actor_260500_80159D7C;

/// Animation preset the overlay's "play animation" message handler applies to
/// the work block: `field_4` is the animation id, `field_8` picks the reset
/// path -- non-zero seeds `field_47C` with 1 and takes `field_C` as the reset
/// argument, zero seeds it with 2 -- and the id range is the handler's own:
/// `func_actor_260500_8014A6C4` accepts the first 0x24.
typedef struct Actor260500AnimPreset {
    /* 0x00 */ s32 field_0;
    /* 0x04 */ s32 field_4;
    /* 0x08 */ s32 field_8;
    /* 0x0C */ s32 field_C;
} Actor260500AnimPreset;
STATIC_ASSERT_SIZEOF(Actor260500AnimPreset, 0x10);

/// Per-frame update handed the overlay's published task once the reset above
/// has been latched.
void func_actor_260500_8014A110(Task* task);

s32 func_actor_260500_8014A6C4(Task* task, s32 arg1, Actor260500AnimPreset* preset);

#endif
