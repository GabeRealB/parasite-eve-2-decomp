#ifndef ACTOR_400100_MOVE_H
#define ACTOR_400100_MOVE_H

#include "actors/actor_400100.h"

typedef struct Actor00100MoveWork {
    /* 0x00 */ u8  pad_0[0xC];
    /* 0x0C */ s16 pos[2][2];
    /* 0x14 */ s16 index;
} Actor00100MoveWork;

typedef struct Actor00100MoveScratch {
    /* 0x00 */ SVECTOR vec;
    /* 0x08 */ SVECTOR target;
    /* 0x10 */ MATRIX  matrix;
    /* 0x30 */ s16     delta;
    /* 0x32 */ s16     original;
    /* 0x34 */ s16     yaw;
    /* 0x36 */ s16     playerYaw;
} Actor00100MoveScratch;
STATIC_ASSERT_SIZEOF(Actor00100MoveScratch, 0x38);

extern u32 D_80070F70;
extern u8  D_801153F2;

#endif
