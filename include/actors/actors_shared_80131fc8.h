#ifndef ACTORS_SHARED_80131FC8_H
#define ACTORS_SHARED_80131FC8_H

#include "common.h"
#include "psyq/libgte.h"
#include "psyq/libgpu.h"

typedef struct ActorsDrawScratch {
    /* 0x00 */ s32     otz;
    /* 0x04 */ u_short ofs[2];
    /* 0x08 */ RECT    rect;
    /* 0x10 */ u32     pad;
} ActorsDrawScratch;
STATIC_ASSERT_SIZEOF(ActorsDrawScratch, 0x14);

/// Byte view of gDisplayState.drawBuffer at 0x80070F87.
extern u8 D_80070F87[];

void ActorsShared80131fc8(s32 otz);

#endif
