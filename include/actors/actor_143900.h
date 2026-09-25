#ifndef ACTOR_143900_H
#define ACTOR_143900_H

#include "common.h"
#include "actors/actors_shared_8013411c.h"
#include "gameplay/1BC.h"
#include "main/task.h"

/// Work block of the overlay's first actor variant.
///
/// That variant's spawn routine, `func_actor_143900_80131E70`, allocates it
/// with `memCalloc(0x4F0, 0)` and stores the pointer both in
/// `D_actor_143900_801496B8` and in the task's `Task::work` slot, so the size
/// below is the allocation and not a guess. Every other function of the
/// variant reaches the block through the global.
///
/// `light` and `color` are the two matrices the block supplies to the model:
/// the spawn routine points the object's `lightMtx` / `colorMtx` at them.
/// `anim` is the animation context the tick and reseed loops walk, and `slots`
/// and `pad_374` are what `func_800B3F84` fills in beside it.
typedef struct Actor143900Work {
    /* 0x000 */ MATRIX     light;
    /* 0x020 */ MATRIX     color;
    /* 0x040 */ GpAnimCtx  anim;
    /* 0x054 */ GpAnimSlot slots[0x14];
    /* 0x374 */ byte       pad_374[0x140];
    /* 0x4B4 */ s16        field_4B4; // reset mode `func_actor_143900_80132624` selects (1 or 2)
    /* 0x4B6 */ s16        field_4B6; // copy of `field_4B8`, kept for change detection
    /* 0x4B8 */ s16        field_4B8; // animation id the slots are seeded with
    /* 0x4BA */ s16        field_4BA; // cleared by `func_actor_143900_80132624` before the reseed
    /* 0x4BC */ byte       pad_4BC[0x2A];
    /* 0x4E6 */ u16        yaw;       // last yaw handed to `Gfx_RotMatrixY`
    /* 0x4E8 */ byte       pad_4E8[2];
    /* 0x4EA */ s16        field_4EA; // steps left in the walk the update performs
    /* 0x4EC */ s16        field_4EC; // turn steps left, latched by the 0x7DB handler
    /* 0x4EE */ byte       pad_4EE[2];
} Actor143900Work;
STATIC_ASSERT_SIZEOF(Actor143900Work, 0x4F0);

/// The first variant's work block under the name the library copies of the
/// tick and placement bodies (`src/lib/actors_shared_80132840.c`,
/// `src/lib/actors_shared_80132a98.c`) still reach it by in the actors that
/// link them. This overlay links neither and names the block
/// `D_actor_143900_801496B8`; the declaration stays until those files are
/// deleted.
extern Actor143900Work* ActorsShared80131f9cWork;

#endif
