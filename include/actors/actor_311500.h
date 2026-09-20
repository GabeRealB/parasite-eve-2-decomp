#ifndef ACTOR_311500_H
#define ACTOR_311500_H

#include "common.h"

#include "gameplay/1BC.h"
#include "main/session.h"
#include "main/tmd.h"

/// 0x28-byte animation stride view of the work block, the same shape
/// `Actor206100AnimStride` and `Actor400500AnimStride` describe: the byte at
/// 0x1D is the per-slot step scale `func_actor_311500_80162C34` rewrites to
/// 0x20 while re-arming the slots.
typedef struct Actor311500AnimStride {
    /* 0x00 */ byte pad[0x1D];
    /* 0x1D */ u8   field_1D;
    /* 0x1E */ byte pad_1E[0xA];
} Actor311500AnimStride;
STATIC_ASSERT_SIZEOF(Actor311500AnimStride, 0x28);

typedef struct Actor311500Work {
    /// Animation context the block itself begins with: `func_actor_311500_80162F28`
    /// hands the block straight to `func_800B4114` / `Gp_AnimTickIndex`.
    /* 0x000 */ GpAnimCtx  anim;
    /* 0x014 */ GpAnimSlot slots[19];
    /* 0x30C */ byte       pad_30C[0x130];
    /// List node `func_actor_311500_801630A4` unlinks on the first step of
    /// state 1. Sits directly in front of the collision table.
    /* 0x43C */ GpObj field_43C;
    /// Collision table set up by `Gp_InitRec18Table(rec18, 1, 0)` in
    /// `func_actor_311500_801629D8`; `field_4CC` below holds the `field_4` of
    /// the entry the damage check picked out.
    /* 0x45C */ GpRec18 rec18[1];
    /* 0x474 */ MATRIX  light;
    /* 0x494 */ MATRIX  color;
    /* 0x4B4 */ Task*   field_4B4;
    /* 0x4B8 */ MATRIX* field_4B8;
    /// Saved copy of the model object's `field_C`, swapped with it when the
    /// mode byte changes: `func_actor_311500_80163334` restores it into the
    /// object on entry to mode 0 and captures it again on entry to mode 2.
    /// Read and written as a word, but only its low half is meaningful, which
    /// is why the restore loads a halfword.
    /* 0x4BC */ u32 field_4BC;
    /// State stepped by `func_actor_311500_80162F28`: 0 arms every slot and
    /// spawns the hit effect, 1 only ticks them.
    /* 0x4C0 */ s16  field_4C0;
    /* 0x4C2 */ byte pad_4C2[0x2];
    /// Delay counter the state-1 step runs up to 0x1F before it drops back to
    /// state 0; `func_actor_311500_80162C34` reads it as an unsigned halfword
    /// and sign-extends it for the limit test, `func_actor_311500_801630A4`
    /// reads it signed.
    /* 0x4C4 */ s16  field_4C4;
    /* 0x4C6 */ byte pad_4C6[0x2];
    /// Repeat count the state-2 step raises once the animation context's
    /// `slots[1].flags` enable bit comes back set; state 0 picks its re-arm branch
    /// off it (below 2 re-arms every slot, at or above 2 just steps the state).
    /* 0x4C8 */ s16  field_4C8;
    /* 0x4CA */ byte pad_4CA[0x2];
    /* 0x4CC */ s32  field_4CC;
    /* 0x4D0 */ s32  field_4D0;
    /* 0x4D4 */ u16  field_4D4;
    /// Last `D_801153F4` mode this handler recorded. Mode 0 restores the
    /// object's `field_C` only while it is still zero, mode 2 captures it back
    /// once the mode byte has moved on, and the shared tail stores the mode.
    /* 0x4D6 */ u16 field_4D6;
} Actor311500Work;
STATIC_ASSERT_SIZEOF(Actor311500Work, 0x4D8);

typedef struct Actor311500 {
    /* 0x00 */ byte             pad_0[0x1C];
    /* 0x1C */ Actor311500Work* field_1C;
    /* 0x20 */ GpEnemy*         field_20;
    /* 0x24 */ void*            field_24;
    /* 0x28 */ byte             pad_28[4];
    /* 0x2C */ TmdObject*       field_2C;
    /// Step within the current mode, driven by `func_actor_311500_80163334`:
    /// 0 re-arms the animation slots, 1 cues the entry sound and clears the
    /// collision table, 2 back-steps once the animation flag settles, 3 waits
    /// for the pose step, 4 returns without touching the shared tail.
    /* 0x30 */ u32 field_30;
} Actor311500;

/// Which of the alternate modes the current frame runs, keyed on
/// `gGameSession`'s scenario id. `func_actor_311500_80163334` dispatches on it
/// and records the value it saw in the work block's `field_4D6`.
extern u8 D_801153F4;

extern GpPairSrcE D_actor_311500_801692C0;
extern u8         D_actor_311500_801692F4[];
extern u32        D_actor_311500_80169330;

/// The array access preserves the dependency on the preceding work-field
/// store, as in actor_111800's initializer.
extern MATRIX* D_80073B8C[1];

void func_actor_311500_801636A0(Actor311500* arg0, s32 arg1, s32 arg2, u32* arg3);

#endif
