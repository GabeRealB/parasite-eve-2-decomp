#ifndef MAIN_GFX_TYPES_H
#define MAIN_GFX_TYPES_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

/// RAM region one area uses for a captured frame and the auxiliary heap.
///
/// `regionBase` is that region's first byte. The captured frame is one
/// 320×240 16-bit image, the same length as `FsImgBuffers`, stored at this
/// address; image transfer reads and writes it as a `u_long*`. Heap
/// configuration uses the same address as the auxiliary region's base.
/// `byteExtent` counts the bytes from `regionBase` up to, but not including,
/// the resident image workspace `Fs_ImgBuffers`. It is not the captured
/// frame's length. Every nonempty slot's base and extent meet at that
/// workspace.
///
/// An empty slot has a null `regionBase` and a zero `byteExtent`. Frame
/// transfer and heap configuration dereference the base, so both require a
/// nonempty slot.
typedef struct {
    u8* regionBase; // First byte of this area's image memory; NULL when the slot is empty
    s32 byteExtent; // Bytes from `regionBase` to `Fs_ImgBuffers`; 0 when the slot is empty
} GfxImageSlot;
STATIC_ASSERT_SIZEOF(GfxImageSlot, 0x8);

/// A nonempty slot, by its extent: the area's image memory ends where the fixed
/// image buffers begin, so its base is that many bytes below them. Needs
/// `main/fs_types.h` for `D_801D7000`.
#define GFX_IMAGE_SLOT(bytes) { (u8*)&D_801D7000 - (bytes), (bytes) }

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

/// Graphics matrix with SDK element access and packed access to its 3x3 coefficients.
///
/// The same coefficients describe rotation, scale, or light/colour transforms,
/// with 12 fractional bits (`ONE` represents 1.0). `mat.t` holds three signed
/// translation values whose units belong to the caller.
///
/// `rotationWords` accesses exactly 18 bytes with four words and one halfword;
/// its field stores leave the alignment halfword and translation untouched.
/// Copy those fields individually when transferring only the 3x3: copying the
/// whole word-view struct includes its tail alignment bytes. A whole `GfxMatrix`
/// copy includes the translation. A borrowed SDK `MATRIX` may be accessed through
/// this view while it remains live and word-aligned; borrowing does not transfer ownership.
typedef union {
    MATRIX           mat;           // SDK coefficients and translation
    GfxRotationWords rotationWords; // Packed signed 12-fractional-bit coefficients; excludes translation
} GfxMatrix;
STATIC_ASSERT_SIZEOF(GfxMatrix, 0x20);

#endif // MAIN_GFX_TYPES_H
