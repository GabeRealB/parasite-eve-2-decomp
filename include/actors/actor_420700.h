#ifndef ACTOR_420700_H
#define ACTOR_420700_H

#include "common.h"
#include "gameplay/1BC.h"

/// Per-actor work block for the `actor_420700` overlay.
///
/// The overlay's state-0 handler (`ActorsShared80131f9cSub0`, here at
/// 0x80131E24) allocates it with `Mem_Calloc(0x5A0, 0)` and stores the pointer
/// both in `ActorsShared80131f9cWork` and in the task's 0x1C slot, so the size
/// below is the allocation and not a guess. Every other function in the
/// overlay reaches the block through the global.
///
/// `anim` is the animation context `Gp_AnimTickIndex` and friends walk; the
/// same layout as `Actor143900Work` and `Actor461800Work`.
typedef struct Actor420700Work {
    /* 0x000 */ byte       pad_0[0x40];
    /* 0x040 */ GpAnimCtx  anim;
    /* 0x054 */ GpAnimSlot slots[0x14];
    /* 0x374 */ byte       pad_374[0x140];
    /* 0x4B4 */ s16        field_4B4; // actor step: 1 and 2 select the body to run, which then advances it to 3
    /* 0x4B6 */ s16        field_4B6; // copy of `field_4B8`, kept for change detection
    /* 0x4B8 */ s16        field_4B8; // animation id the slots are seeded with
    /* 0x4BA */ s16        field_4BA; // ramp mode message 0x7DB selected: 1 and 3 rise, 2 falls, 0 leaves it alone
    /* 0x4BC */ s16        field_4BC; // ramp value `ActorsShared80131f9cSub1` walks by 0x80, clamped to 0..0x1000
    /* 0x4BE */ byte       pad_4BE[0xE2];
} Actor420700Work;
STATIC_ASSERT_SIZEOF(Actor420700Work, 0x5A0);

extern Actor420700Work* ActorsShared80131f9cWork;

void func_actor_420700_801324EC(void);
void func_actor_420700_801325C8(void);

/// Argument block of message 0x7DB, which arms the `field_4BC` ramp: the ramp
/// starts at the end the mode walks away from, 0 for the rising modes 1 and 3
/// and 0x1000 for the falling mode 2. Mode 0 is taken as a no-op, and a block
/// whose leading id is not 0x1B02 is rejected.
typedef struct Actor420700ModeArgs {
    /* 0x0 */ u16 id;
    /* 0x2 */ u16 mode;
} Actor420700ModeArgs;

s32 func_actor_420700_80132784(Task* task, s32 arg1, Actor420700ModeArgs* args);

/// The shared reset body `src/actors/lib/actors_shared_80132538.c`. Declared
/// here rather than through `actors_shared_80132538.h` because that header
/// publishes `ActorsShared80131f9cWork` with its own work type.
void ActorsShared80132538(void);

#endif
