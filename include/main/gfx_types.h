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

/// Word-access view of a `MATRIX`'s nine fixed-point rotation coefficients.
///
/// Each coefficient is signed with 12 fractional bits (`ONE` represents 1.0).
/// On the little-endian PlayStation, the first coefficient of each pair occupies
/// the low 16 bits and the second the high 16 bits. Storage must be word-aligned.
/// The four words and final halfword cover exactly 18 bytes; field accesses leave
/// the two alignment bytes before `MATRIX::t` and the translation unchanged.
/// Copy the fields individually: a whole-struct copy also includes tail alignment
/// bytes outside the rotation.
typedef struct {
    s32 m00M01; // Coefficients m[0][0] (low) and m[0][1] (high)
    s32 m02M10; // Coefficients m[0][2] (low) and m[1][0] (high)
    s32 m11M12; // Coefficients m[1][1] (low) and m[1][2] (high)
    s32 m20M21; // Coefficients m[2][0] (low) and m[2][1] (high)
    s16 m22;    // Signed coefficient m[2][2]; no access to the alignment halfword
} GfxRotationWords;

#endif // MAIN_GFX_TYPES_H
