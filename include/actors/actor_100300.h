#ifndef ACTOR_100300_H
#define ACTOR_100300_H

#include "common.h"
#include "gameplay/1BC.h"
#include "gameplay/3FB8.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

extern s32 D_80070F70;

typedef struct Actor00300GroundScratch {
    /* 0x00 */ SVECTOR vec[4];
    /* 0x20 */ DVECTOR sxy0;
    /* 0x24 */ DVECTOR sxy1;
    /* 0x28 */ DVECTOR sxy2;
    /* 0x2C */ DVECTOR sxy3;
} Actor00300GroundScratch;
STATIC_ASSERT_SIZEOF(Actor00300GroundScratch, 0x30);

extern MATRIX Gfx_ViewWorldMtx;

#endif
