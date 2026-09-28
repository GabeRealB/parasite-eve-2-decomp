#ifndef MAIN_GFX_TYPES_H
#define MAIN_GFX_TYPES_H

#include <psyq/sys/types.h>

#include "common.h"

/// 8-byte VRAM/heap slot: pointer + size. Tables selected via Gfx_ImageSlotTables.
typedef struct _GfxImageSlot {
    /* 0x0 */ u_long* pixels;
    /* 0x4 */ s32     size;
} GfxImageSlot;
STATIC_ASSERT_SIZEOF(GfxImageSlot, 0x8);

/// The leading rotation entries of a `MATRIX`, paired into words, as code
/// resets and copies a rotation.
typedef struct _GpMtxWords {
    s32 m00_m01;
    s32 m02_m10;
    s32 m11_m12;
    s32 m20_m21;
    s16 m22;
} GpMtxWords;

#endif // MAIN_GFX_TYPES_H
