#ifndef ACTOR_113000_H
#define ACTOR_113000_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/1BC.h"
#include "gameplay/3FB8.h"
#include "main/task.h"

/// 0x14-byte animation preset `func_actor_113000_80132208` reads: `field_0` is
/// the animation bank index it compares against `Actor113000Work::field_47C`,
/// `field_4` the animation id it stores in `field_478`, and `field_8` selects
/// between `func_800B4114` and `Gp_AnimResetSlot`. The same record shape as
/// `Actor113100AnimPreset` / `Actor323300AnimPreset`.
typedef struct Actor113000AnimPreset {
    /* 0x00 */ s32 field_0;
    /* 0x04 */ s32 field_4;
    /* 0x08 */ s32 field_8;
    /* 0x0C */ s32 field_C;
    /* 0x10 */ s32 field_10;
} Actor113000AnimPreset;
STATIC_ASSERT_SIZEOF(Actor113000AnimPreset, 0x14);

/// Animation source table `func_actor_113000_80132208` indexes by the preset's
/// bank index and hands `func_800B3F84` as its data argument.
extern void* D_actor_113000_8013ABB0[];

/// Work block this actor parks in `Task::idMap`. The block is fronted by a
/// `GpAnimCtx` -- the start handler passes the block itself, its `slots` array
/// and its `GpAnimMtxRec` table at 0x334 to `func_800B3F84`, the same three
/// addresses `Actor113100Work` hands over. Twenty 0x28-byte slots fit exactly
/// between 0x14 and 0x334, and the tick loop walks indices 1..0x13.
/// The per-frame state fields (`0x474`..`0x4C8`) are the animation context its
/// handlers walk; `light` / `color` are the matrices the TMD object's
/// `field_1C` / `field_20` are republished from. The spawn handler seeds the
/// two -1 words at 0x478 / 0x47C and the halfword pair at 0x4C6 / 0x4C8.
typedef struct Actor113000Work {
    /* 0x000 */ GpAnimCtx  anim;
    /* 0x014 */ GpAnimSlot slots[20];
    /// The `GpAnimMtxRec` table `func_800B3F84` fills, immediately after the
    /// slots.
    /* 0x334 */ byte field_334[0x140];
    /// Raised once the slots have been started, so the next preset takes the
    /// `func_800B4114` path rather than `Gp_AnimResetSlot`.
    /* 0x474 */ s32    field_474;
    /* 0x478 */ s32    field_478; ///< -1 out of the spawn handler
    /* 0x47C */ s32    field_47C; ///< -1 out of the spawn handler
    /* 0x480 */ MATRIX light;
    /* 0x4A0 */ MATRIX color;
    /* 0x4C0 */ s16    field_4C0; ///< upload countdown reload; set to 1 alongside `field_4C4` by mode 3
    /* 0x4C2 */ u16    field_4C2; ///< upload countdown the per-frame state runs down, reloaded from `field_4C0` on underflow
    /* 0x4C4 */ s16    field_4C4; ///< upload step 1..3; set to 1 alongside `field_4C0` by mode 3
    /* 0x4C6 */ s16    field_4C6; ///< cleared by the spawn handler
    /* 0x4C8 */ s16    field_4C8; ///< -1 out of the spawn handler
    /* 0x4CA */ byte   pad_4CA[0x2];
} Actor113000Work;
STATIC_ASSERT_SIZEOF(Actor113000Work, 0x4CC);

/// Texture-upload state the per-frame tick runs at the end of its body: steps
/// the countdown at `field_4C2` down while `field_4C4` names the upload step
/// in progress, and on underflow posts that step's image, reloading the
/// countdown for steps 1 and 2 or clearing the step for step 3.
void func_actor_113000_80131E30(GpActorWork* arg0);

#endif // ACTOR_113000_H
