#ifndef ACTOR_311500_H
#define ACTOR_311500_H

#include "common.h"

#include "gameplay/1BC.h"
#include "main/session.h"
#include "main/tmd.h"

typedef struct Actor311500Work {
    /* 0x000 */ byte pad_0[0x45C];
    /// Collision table set up by `Gp_InitRec18Table(rec18, 1, 0)` in
    /// `func_actor_311500_801629D8`; `field_4CC` below holds the `field_4` of
    /// the entry the damage check picked out.
    /* 0x45C */ GpRec18 rec18[1];
    /* 0x474 */ byte    pad_474[0x58];
    /* 0x4CC */ s32     field_4CC;
    /* 0x4D0 */ s32     field_4D0;
    /* 0x4D4 */ u16     field_4D4;
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
