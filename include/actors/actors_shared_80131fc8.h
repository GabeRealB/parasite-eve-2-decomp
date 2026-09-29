#ifndef INCLUDE_ACTORS_ACTORS_SHARED_80131FC8_H
#define INCLUDE_ACTORS_ACTORS_SHARED_80131FC8_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

typedef struct ActorsDrawScratch {
    /* 0x00 */ s32     otz;
    /* 0x04 */ u_short ofs[2];
    /* 0x08 */ RECT    rect;
    /* 0x10 */ u32     pad;
} ActorsDrawScratch;
STATIC_ASSERT_SIZEOF(ActorsDrawScratch, 0x14);

#endif // INCLUDE_ACTORS_ACTORS_SHARED_80131FC8_H
