#ifndef ACTOR_02400_SPAWN_H
#define ACTOR_02400_SPAWN_H

#include "common.h"
#include "gameplay/1BC.h"
#include "gameplay/3FB8.h"

/// Spawn-time view of the 0x154-byte work block shared by actor_102400 and
/// actor_202400. The two collision bodies own four contact records and one
/// contact record; the matrix at 0x100 preserves the unscaled model pose.
typedef struct Actor02400SpawnWork {
    /* 0x000 */ MATRIX         color;
    /* 0x020 */ MATRIX         light;
    /* 0x040 */ GpObj          obj40;
    /* 0x060 */ GpRec18        rec60[4];
    /* 0x0C0 */ GpObj          objC0;
    /* 0x0E0 */ GpRec18        recE0;
    /* 0x0F8 */ GsCOORDINATE2* field_F8;
    /* 0x0FC */ s16            field_FC;
    /* 0x0FE */ s16            field_FE;
    /* 0x100 */ MATRIX         field_100;
    /* 0x120 */ byte           pad_120[8];
    /* 0x128 */ s16            field_128;
    /* 0x12A */ s16            field_12A;
    /* 0x12C */ s16            field_12C;
    /* 0x12E */ byte           pad_12E[0x12];
    /* 0x140 */ s16            field_140;
    /* 0x142 */ byte           pad_142[0xC];
    /* 0x14E */ s16            variant;
    /* 0x150 */ byte           pad_150[4];
} Actor02400SpawnWork;
STATIC_ASSERT_SIZEOF(Actor02400SpawnWork, 0x154);

#endif
