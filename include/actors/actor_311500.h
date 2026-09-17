#ifndef ACTOR_311500_H
#define ACTOR_311500_H

#include "common.h"

#include "gameplay/1BC.h"
#include "main/session.h"
#include "main/tmd.h"

typedef struct Actor311500Work {
    /// Animation context the block itself begins with: `func_actor_311500_80162F28`
    /// hands the block straight to `func_800B4114` / `Gp_AnimTickIndex`.
    /* 0x000 */ GpAnimCtx anim;
    /* 0x014 */ byte      pad_14[0x38];
    /// Enable flag `func_actor_311500_80162F28` polls in bit 0.
    /* 0x04C */ u16  field_4C;
    /* 0x04E */ byte pad_4E[0x40E];
    /// Collision table set up by `Gp_InitRec18Table(rec18, 1, 0)` in
    /// `func_actor_311500_801629D8`; `field_4CC` below holds the `field_4` of
    /// the entry the damage check picked out.
    /* 0x45C */ GpRec18 rec18[1];
    /* 0x474 */ byte    pad_474[0x4C];
    /// State stepped by `func_actor_311500_80162F28`: 0 arms every slot and
    /// spawns the hit effect, 1 only ticks them.
    /* 0x4C0 */ s16  field_4C0;
    /* 0x4C2 */ byte pad_4C2[0xA];
    /* 0x4CC */ s32  field_4CC;
    /* 0x4D0 */ s32  field_4D0;
    /* 0x4D4 */ u16  field_4D4;
} Actor311500Work;

typedef struct Actor311500 {
    /* 0x00 */ byte             pad_0[0x1C];
    /* 0x1C */ Actor311500Work* field_1C;
    /* 0x20 */ GpEnemy*         field_20;
    /* 0x24 */ byte             pad_24[0x8];
    /* 0x2C */ TmdObject*       field_2C;
} Actor311500;

void func_actor_311500_801636A0(Actor311500* arg0, s32 arg1, s32 arg2, u32* arg3);

#endif
