#ifndef ACTOR_105400_H
#define ACTOR_105400_H

#include "common.h"
#include <psyq/libgte.h>

#include "gameplay/1BC.h"

/// Spawn offsets the state-0 setup reads into the enemy's `field_1C` vector
/// and into the second list node's halfword triple: the first entry of
/// `D_actor_105400_80133A30` is unused here, the second `field_8` is what both
/// reads take. Read through this view the components load signed (`lh`), which
/// is what the 32-bit `field_1C` store needs.
typedef struct Actor05400Pose {
    /* 0x0 */ SVECTOR field_0;
    /* 0x8 */ SVECTOR field_8;
} Actor05400Pose;
STATIC_ASSERT_SIZEOF(Actor05400Pose, 0x10);

/// 4-byte pan/volume row of the sound table `D_actor_105400_8013CE64`, indexed
/// by `Game_Session->field_4`; `field_0` and `field_2` are the second and third
/// `SndEvt_EnqueueType6` arguments.
typedef struct Actor05400SndRow {
    /* 0x0 */ s8 field_0;
    /* 0x1 */ s8 field_1;
    /* 0x2 */ s8 field_2;
    /* 0x3 */ s8 field_3;
} Actor05400SndRow;
STATIC_ASSERT_SIZEOF(Actor05400SndRow, 4);

/// The 0x340-byte block the state-0 setup allocates and stores at
/// `Task::idMap` (the same slot `Actor05400::field_1C` names). Its 0x14 prefix
/// is the `GpAnimCtx` handed to `func_800B3F84`; `slots`/`poses` are that
/// call's last two arguments. The two `MATRIX`es at 0x244 / 0x264 are the
/// model's colour and light matrices (`TmdObject::field_20` / `field_1C`),
/// `node0` / `node1` the `GpObj` list nodes linked onto list 2 with their two
/// `GpRec18` records, and `field_2FC` the working copy of the coordinate
/// matrix.
typedef struct Actor05400Work {
    /* 0x000 */ GpAnimCtx      anim;
    /* 0x014 */ GpAnimSlot     slots[10];
    /* 0x1A4 */ byte           poses[0xA0];
    /* 0x244 */ MATRIX         field_244;
    /* 0x264 */ MATRIX         field_264;
    /* 0x284 */ GpObj          node0;
    /* 0x2A4 */ GpObj          node1;
    /* 0x2C4 */ GpRec18        recs[2];
    /* 0x2F4 */ GsCOORDINATE2* coord;
    /* 0x2F8 */ u16            field_2F8;
    /* 0x2FA */ s16            field_2FA;
    /* 0x2FC */ MATRIX         field_2FC;
    /* 0x31C */ s32            field_31C;
    /* 0x320 */ byte           pad_320[6];
    /* 0x326 */ s16            field_326;
    /* 0x328 */ byte           pad_328[0xC];
    /* 0x334 */ s16            field_334;
    /* 0x336 */ byte           pad_336[2];
    /* 0x338 */ s16            field_338;
    /* 0x33A */ byte           pad_33A[2];
    /* 0x33C */ u16            field_33C;
    /* 0x33E */ byte           pad_33E[2];
} Actor05400Work;
STATIC_ASSERT_SIZEOF(Actor05400Work, 0x340);

typedef struct Actor05400 {
    /* 0x00 */ byte            pad_0[0x1C];
    /* 0x1C */ Actor05400Work* field_1C;
} Actor05400;

s16 Actor05400_Fn01B70(Actor05400* arg0);

void func_actor_105400_8013310C(GpEnemy* arg0, Task* arg1);

#endif
