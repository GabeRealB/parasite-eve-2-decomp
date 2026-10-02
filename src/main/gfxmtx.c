#include "main/gfx.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/gtemac.h>
#include <psyq/inline_c.h>

#include "gte.h"
#include "types.h"

#include "main/scratch.h"

/// Scratch workspace for scaling a light direction before normalization.
///
/// The input copy preserves the caller's direction while `VectorNormalS` uses
/// its reduced components. GTE counts include the sign bit: leading zeros for
/// nonnegative components and leading ones for negative components.
/// Owned by one scratch-stack reservation and released after normalization.
typedef struct {
    VECTOR direction;         // Input direction, uniformly reduced if needed; fourth word copied but unused
    s32    scaleBits;         // Minimum leading sign bits (1..32), reused as the common right shift (1..17)
    s32    componentSignBits; // Leading sign bits of the Y or Z component just counted (1..32)
} _GfxLightDirectionScratch;
STATIC_ASSERT_SIZEOF(_GfxLightDirectionScratch, 0x18);

/// Temporary matrix and trigonometric values for a rotation about one axis.
///
/// Owned by the current scratch-stack reservation and released before return.
/// Axis rotations reserve sizeof this block; Euler extraction reserves a larger
/// byte span and reuses the matrix for the rotation with its X angle removed.
typedef struct {
    MATRIX rotation; // Axis rotation or Euler intermediate; only m is used, scaled by ONE
    s16    angleSin; // Signed sine of the axis angle, scaled by ONE (4096)
    s16    angleCos; // Signed cosine of the axis angle, scaled by ONE (4096)
} _GfxAxisRotationScratch;
STATIC_ASSERT_SIZEOF(_GfxAxisRotationScratch, 0x24);

typedef struct {
    /* 0x00 */ MATRIX  mat;
    /* 0x20 */ s16     sin_x;
    /* 0x22 */ s16     cos_x;
    /* 0x24 */ s16     sin_y;
    /* 0x26 */ s16     cos_y;
    /* 0x28 */ s16     sin_z;
    /* 0x2A */ s16     cos_z;
    /* 0x2C */ SVECTOR vec;
} ScratchRotXYZ;

typedef struct {
    /* 0x00 */ MATRIX  mat;
    /* 0x20 */ s16     sin_x;
    /* 0x22 */ s16     cos_x;
    /* 0x24 */ s16     sin_y;
    /* 0x26 */ s16     cos_y;
    /* 0x28 */ s16     sin_z;
    /* 0x2A */ s16     cos_z;
    /* 0x2C */ SVECTOR vec;
    /* 0x34 */ SVECTOR vec2;
    /* 0x3C */ SVECTOR vec3;
} ScratchRotZYX;

static void Gfx_RotMatrixZYX(MATRIX* out, SVECTOR* angles, s32 flag);

static void Gfx_TransposeRot(MATRIX* arg0, MATRIX* arg1);

/* 0x34 */

/* 0x44 */

/// Reduces a copied light direction for safe signed sum-of-squares normalization.
///
/// `scratch` is a live, writable workspace with `direction`'s xyz initialized. Its
/// xyz components end in [-16384, 16383]. Components outside that range cause a
/// common arithmetic right shift of 1..17 bits, rounding negative values down.
/// Already bounded directions, including zero, and the fourth word are unchanged.
///
/// Overwrites both count fields: `scaleBits` holds the minimum input sign-bit
/// count (18..32) if no shift was needed, or the shift amount (1..17) otherwise.
/// `componentSignBits` holds the input Z count (1..32). Changes the GTE leading
/// sign-bit-count state. The caller owns and releases the workspace.
static __inline__ void _gfxReduceLightDirection(_GfxLightDirectionScratch* scratch)
{
    /// Minimum leading sign-bit count for safe three-component normalization.
    ///
    /// The GTE count includes the sign bit. Eighteen bounds each component to
    /// [-16384, 16383], so the squared sum is at most 805306368 and fits the
    /// signed 32-bit additions in `VectorNormalS`. Seventeen would permit
    /// three -32768 components, whose squared sum overflows those additions.
    enum { GRAPHICS_NORMALIZE_MIN_SIGN_BITS = 18 };

    // Select the least sign-extension headroom across xyz.
    gte_Lzc(scratch->direction.vx, &scratch->scaleBits);
    gte_Lzc(scratch->direction.vy, &scratch->componentSignBits);

    if (scratch->scaleBits > scratch->componentSignBits) {
        scratch->scaleBits = scratch->componentSignBits;
    }

    gte_Lzc(scratch->direction.vz, &scratch->componentSignBits);

    if (scratch->scaleBits > scratch->componentSignBits) {
        scratch->scaleBits = scratch->componentSignBits;
    }

    // Retain the count when already bounded; otherwise reuse it for the shift.
    if (scratch->scaleBits < GRAPHICS_NORMALIZE_MIN_SIGN_BITS) {
        scratch->scaleBits      = GRAPHICS_NORMALIZE_MIN_SIGN_BITS - scratch->scaleBits;
        scratch->direction.vx >>= scratch->scaleBits;
        scratch->direction.vy >>= scratch->scaleBits;
        scratch->direction.vz >>= scratch->scaleBits;
    }
}

void Gfx_RotMatrixXYZ(MATRIX* out, SVECTOR* angles, s32 flag)
{
    ScratchRotXYZ* block;

    block = SCRATCH_STACK_RESERVE_BLOCK(ScratchRotXYZ);

    block->sin_x = rsin(angles->vx);
    block->sin_y = rsin(angles->vy);
    block->sin_z = rsin(angles->vz);
    block->cos_x = rcos(angles->vx);
    block->cos_y = rcos(angles->vy);
    block->cos_z = rcos(angles->vz);

    block->mat.m[0][0] = ONE;
    block->mat.m[0][1] = 0;
    block->mat.m[0][2] = 0;
    block->mat.m[1][0] = 0;
    block->mat.m[1][1] = block->cos_x;
    block->mat.m[1][2] = -block->sin_x;
    block->mat.m[2][0] = 0;
    block->mat.m[2][1] = block->sin_x;
    block->mat.m[2][2] = block->cos_x;

    block->vec.vx = block->cos_y;
    block->vec.vy = 0;
    block->vec.vz = -block->sin_y;

    gte_SetRotMatrix(&block->mat);
    gte_ldsv(&block->vec);
    gte_rtir();
    block->vec.vx = block->sin_y;
    block->vec.vz = block->cos_y;
    gte_stclmv(&block->mat.m[0][0]);

    gte_ldsv(&block->vec);
    gte_rtir();
    block->vec.vx = block->cos_z;
    block->vec.vy = block->sin_z;
    block->vec.vz = 0;
    gte_stclmv(&block->mat.m[0][2]);

    gte_SetRotMatrix(&block->mat);
    gte_ldsv(&block->vec);
    gte_rtir();
    block->vec.vx = -block->sin_z;
    block->vec.vy = block->cos_z;
    gte_stclmv(&block->mat.m[0][0]);

    gte_ldsv(&block->vec);
    gte_rtir();
    gte_stclmv(&block->mat.m[0][1]);

    if (flag != 0) {
        MATRIX_PAIR(out, 0, 0) = MATRIX_PAIR(&block->mat, 0, 0);
        MATRIX_PAIR(out, 0, 2) = MATRIX_PAIR(&block->mat, 0, 2);
        MATRIX_PAIR(out, 1, 1) = MATRIX_PAIR(&block->mat, 1, 1);
        MATRIX_PAIR(out, 2, 0) = MATRIX_PAIR(&block->mat, 2, 0);
        out->m[2][2]           = block->mat.m[2][2];
    } else {
        gte_MulMatrix0(out, &block->mat, out);
    }

    SCRATCH_STACK_RELEASE_BLOCK(ScratchRotXYZ);
}

void Gfx_RotMatrixYXZ(MATRIX* out, SVECTOR* angles, s32 flag)
{
    ScratchRotXYZ* block;

    block = SCRATCH_STACK_RESERVE_BLOCK(ScratchRotXYZ);

    block->sin_x = rsin(angles->vx);
    block->sin_y = rsin(angles->vy);
    block->sin_z = rsin(angles->vz);
    block->cos_x = rcos(angles->vx);
    block->cos_y = rcos(angles->vy);
    block->cos_z = rcos(angles->vz);

    block->mat.m[0][0] = block->cos_y;
    block->mat.m[0][1] = 0;
    block->mat.m[0][2] = block->sin_y;
    block->mat.m[1][0] = 0;
    block->mat.m[1][1] = ONE;
    block->mat.m[1][2] = 0;
    block->mat.m[2][0] = -block->sin_y;
    block->mat.m[2][1] = 0;
    block->mat.m[2][2] = block->cos_y;

    block->vec.vx = 0;
    block->vec.vy = block->cos_x;
    block->vec.vz = block->sin_x;

    gte_SetRotMatrix(&block->mat);
    gte_ldsv(&block->vec);
    gte_rtir();
    block->vec.vx = 0;
    block->vec.vy = -block->sin_x;
    block->vec.vz = block->cos_x;
    gte_stclmv(&block->mat.m[0][1]);

    gte_ldsv(&block->vec);
    gte_rtir();
    block->vec.vx = block->cos_z;
    block->vec.vy = block->sin_z;
    block->vec.vz = 0;
    gte_stclmv(&block->mat.m[0][2]);

    gte_SetRotMatrix(&block->mat);
    gte_ldsv(&block->vec);
    gte_rtir();
    block->vec.vx = -block->sin_z;
    block->vec.vy = block->cos_z;
    block->vec.vz = 0;
    gte_stclmv(&block->mat.m[0][0]);

    gte_ldsv(&block->vec);
    gte_rtir();
    gte_stclmv(&block->mat.m[0][1]);

    if (flag != 0) {
        MATRIX_PAIR(out, 0, 0) = MATRIX_PAIR(&block->mat, 0, 0);
        MATRIX_PAIR(out, 0, 2) = MATRIX_PAIR(&block->mat, 0, 2);
        MATRIX_PAIR(out, 1, 1) = MATRIX_PAIR(&block->mat, 1, 1);
        MATRIX_PAIR(out, 2, 0) = MATRIX_PAIR(&block->mat, 2, 0);
        out->m[2][2]           = block->mat.m[2][2];
    } else {
        gte_MulMatrix0(out, &block->mat, out);
    }

    SCRATCH_STACK_RELEASE_BLOCK(ScratchRotXYZ);
}

static void Gfx_RotMatrixZYX(MATRIX* out, SVECTOR* angles, s32 flag)
{
    ScratchRotZYX* block;

    block = SCRATCH_STACK_RESERVE_BLOCK(ScratchRotZYX);

    block->sin_x = rsin(angles->vx);
    block->sin_y = rsin(angles->vy);
    block->sin_z = rsin(angles->vz);
    block->cos_x = rcos(angles->vx);
    block->cos_y = rcos(angles->vy);
    block->cos_z = rcos(angles->vz);

    block->mat.m[0][0] = block->cos_z;
    block->mat.m[0][1] = -block->sin_z;
    block->mat.m[0][2] = 0;
    block->mat.m[1][0] = block->sin_z;
    block->mat.m[1][1] = block->cos_z;
    block->mat.m[1][2] = 0;
    block->mat.m[2][0] = 0;
    block->mat.m[2][1] = 0;
    block->mat.m[2][2] = ONE;

    block->vec.vx = block->cos_y;
    block->vec.vy = 0;
    block->vec.vz = -block->sin_y;

    gte_SetRotMatrix(&block->mat);
    gte_ldsv(&block->vec);
    gte_rtir();
    block->vec3.vx = block->sin_y;
    block->vec3.vy = 0;
    block->vec3.vz = block->cos_y;
    gte_stclmv(&block->mat.m[0][0]);

    gte_ldsv(&block->vec3);
    gte_rtir();
    block->vec2.vx = 0;
    block->vec2.vy = block->cos_x;
    block->vec2.vz = block->sin_x;
    gte_stclmv(&block->mat.m[0][2]);

    gte_SetRotMatrix(&block->mat);
    gte_ldsv(&block->vec2);
    gte_rtir();
    block->vec3.vx = 0;
    block->vec3.vy = -block->sin_x;
    block->vec3.vz = block->cos_x;
    gte_stclmv(&block->mat.m[0][1]);

    gte_ldsv(&block->vec3);
    gte_rtir();
    gte_stclmv(&block->mat.m[0][2]);

    if (flag != 0) {
        MATRIX_PAIR(out, 0, 0) = MATRIX_PAIR(&block->mat, 0, 0);
        MATRIX_PAIR(out, 0, 2) = MATRIX_PAIR(&block->mat, 0, 2);
        MATRIX_PAIR(out, 1, 1) = MATRIX_PAIR(&block->mat, 1, 1);
        MATRIX_PAIR(out, 2, 0) = MATRIX_PAIR(&block->mat, 2, 0);
        out->m[2][2]           = block->mat.m[2][2];
    } else {
        gte_MulMatrix0(out, &block->mat, out);
    }

    SCRATCH_STACK_RELEASE_BLOCK(ScratchRotZYX);
}

void gfxMatrixToEuler(MATRIX* matrix, SVECTOR* angles)
{
    // The inverse X rotation and its sine and cosine occupy 0x24 bytes.
    // The reservation is 0x30; the tail is not read or written.
    enum { GRAPHICS_EULER_SCRATCH_BYTES = 0x30 };

    _GfxAxisRotationScratch* block;
    s16                      angleX;

    block = SCRATCH_STACK_RESERVE_BYTES(GRAPHICS_EULER_SCRATCH_BYTES);

    // X cancels m[1][2] against m[2][2]. Removing that Rx leaves Ry * Rz:
    // Y is read from the product's column 2 and Z from its first row.
    angleX          = -ratan2(matrix->m[1][2], matrix->m[2][2]);
    angles->vx      = angleX;
    block->angleSin = rsin(angleX);
    block->angleCos = rcos(angles->vx);

    block->rotation.m[0][0] = ONE;
    block->rotation.m[0][1] = 0;
    block->rotation.m[0][2] = 0;
    block->rotation.m[1][0] = 0;
    block->rotation.m[1][1] = block->angleCos;
    block->rotation.m[1][2] = block->angleSin;
    block->rotation.m[2][0] = 0;
    block->rotation.m[2][1] = -block->angleSin;
    block->rotation.m[2][2] = block->angleCos;

    gte_MulMatrix0(&block->rotation, matrix, &block->rotation);

    angles->vy = ratan2(block->rotation.m[0][2], block->rotation.m[2][2]);
    angles->vz = ratan2(block->rotation.m[1][0], block->rotation.m[1][1]);

    SCRATCH_STACK_RELEASE_BYTES(GRAPHICS_EULER_SCRATCH_BYTES);
}

static void Gfx_TransposeRot(MATRIX* arg0, MATRIX* arg1)
{
    gte_TransposeMatrix(arg0, arg1);
}

void Gfx_MatrixCol0(MATRIX* matrix, SVECTOR* vector)
{
    gte_ReadMatrixColumn(matrix, 0, vector);
}

void Gfx_MatrixCol1(MATRIX* matrix, SVECTOR* vector)
{
    gte_ReadMatrixColumn(matrix, 1, vector);
}

void Gfx_MatrixCol2(MATRIX* matrix, SVECTOR* vector)
{
    gte_ReadMatrixColumn(matrix, 2, vector);
}

/// Installs a pure X rotation using the scratch block's initialized sine and cosine.
///
/// Only the nine rotation elements are written; translation remains untouched.
/// `rotation` may be the matrix embedded in `scratch`.
static __inline__ void _gfxBuildXRotation(MATRIX* rotation, _GfxAxisRotationScratch* scratch)
{
    rotation->m[0][0] = ONE;
    rotation->m[0][1] = 0;
    rotation->m[0][2] = 0;
    rotation->m[1][0] = 0;
    rotation->m[1][1] = scratch->angleCos;
    rotation->m[1][2] = -scratch->angleSin;
    rotation->m[2][0] = 0;
    rotation->m[2][1] = scratch->angleSin;
    rotation->m[2][2] = scratch->angleCos;
}

void gfxRotMatrixX(MATRIX* matrix, s32 angle, s32 replace)
{
    _GfxAxisRotationScratch* scratch;

    scratch = SCRATCH_STACK_RESERVE_BLOCK(_GfxAxisRotationScratch);

    scratch->angleSin = rsin(angle);
    scratch->angleCos = rcos(angle);

    // Build Rx directly for replacement, or in scratch for matrix * Rx.
    // Composition uses only the 3x3, so scratch translation stays unset.
    if (replace != 0) {
        _gfxBuildXRotation(matrix, scratch);
    } else {
        _gfxBuildXRotation(&scratch->rotation, scratch);
        gte_MulMatrix0(matrix, &scratch->rotation, matrix);
    }

    SCRATCH_STACK_RELEASE_BLOCK(_GfxAxisRotationScratch);
}

void gfxRotMatrixY(MATRIX* matrix, s32 angle, s32 replace)
{
    _GfxAxisRotationScratch* block;

    block = SCRATCH_STACK_RESERVE_BLOCK(_GfxAxisRotationScratch);

    block->angleSin = rsin(angle);
    block->angleCos = rcos(angle);

    // Nonzero replace writes the caller's rotation in place. Zero builds the
    // same rotation in scratch and right-multiplies; its translation stays
    // unset because the multiply reads only the 3x3.
    if (replace != 0) {
        matrix->m[0][0] = block->angleCos;
        matrix->m[0][1] = 0;
        matrix->m[0][2] = block->angleSin;
        matrix->m[1][0] = 0;
        matrix->m[1][1] = ONE;
        matrix->m[1][2] = 0;
        matrix->m[2][0] = -block->angleSin;
        matrix->m[2][1] = 0;
        matrix->m[2][2] = block->angleCos;
    } else {
        block->rotation.m[0][0] = block->angleCos;
        block->rotation.m[0][1] = 0;
        block->rotation.m[0][2] = block->angleSin;
        block->rotation.m[1][0] = 0;
        block->rotation.m[1][1] = ONE;
        block->rotation.m[1][2] = 0;
        block->rotation.m[2][0] = -block->angleSin;
        block->rotation.m[2][1] = 0;
        block->rotation.m[2][2] = block->angleCos;

        gte_MulMatrix0(matrix, &block->rotation, matrix);
    }

    SCRATCH_STACK_RELEASE_BLOCK(_GfxAxisRotationScratch);
}

void Gfx_RotMatrixZ(MATRIX* matrix, s32 angle, s32 flag)
{
    _GfxAxisRotationScratch* block;

    block = SCRATCH_STACK_RESERVE_BLOCK(_GfxAxisRotationScratch);

    block->angleSin = rsin(angle);
    block->angleCos = rcos(angle);

    if (flag != 0) {
        matrix->m[0][0] = block->angleCos;
        matrix->m[0][1] = -block->angleSin;
        matrix->m[0][2] = 0;
        matrix->m[1][0] = block->angleSin;
        matrix->m[1][1] = block->angleCos;
        matrix->m[1][2] = 0;
        matrix->m[2][0] = 0;
        matrix->m[2][1] = 0;
        matrix->m[2][2] = ONE;
    } else {
        block->rotation.m[0][0] = block->angleCos;
        block->rotation.m[0][1] = -block->angleSin;
        block->rotation.m[0][2] = 0;
        block->rotation.m[1][0] = block->angleSin;
        block->rotation.m[1][1] = block->angleCos;
        block->rotation.m[1][2] = 0;
        block->rotation.m[2][0] = 0;
        block->rotation.m[2][1] = 0;
        block->rotation.m[2][2] = ONE;

        gte_MulMatrix0(matrix, &block->rotation, matrix);
    }

    SCRATCH_STACK_RELEASE_BLOCK(_GfxAxisRotationScratch);
}

void gfxNormalizeLightDirection(const void* direction, SVECTOR* normalizedDirection)
{
    _GfxLightDirectionScratch* scratch;

    scratch = SCRATCH_STACK_RESERVE_BLOCK(_GfxLightDirectionScratch);

    // The aligned span includes one unused word after the three components.
    scratch->direction = *(const VECTOR*)direction;
    _gfxReduceLightDirection(scratch);
    VectorNormalS(&scratch->direction, normalizedDirection);

    SCRATCH_STACK_RELEASE_BLOCK(_GfxLightDirectionScratch);
}

/// Builds a rotation from two axes: `arg2` and `arg1` become rows 1 and 2 of a
/// scratch matrix, their cross product row 0, and the transposed, normalized
/// result is written to `out`.
void Gfx_OrthonormalBasis(MATRIX* out, SVECTOR* arg1, SVECTOR* arg2)
{
    MATRIX*  mat;
    SVECTOR* row;

    mat                  = SCRATCH_STACK_RESERVE_BLOCK(MATRIX);
    *(SVECTOR*)mat->m[1] = *arg2;
    row                  = (SVECTOR*)mat->m[2];
    row->vx              = arg1->vx;
    row->vy              = arg1->vy;
    row->vz              = arg1->vz;

    gte_ldopv1SV(mat->m[1]);
    gte_ldopv2SV(row);
    gte_op12();
    gte_stsv(mat->m[0]);

    MatrixNormal_2(mat, mat);

    gte_TransposeMatrix(mat, out);

    SCRATCH_STACK_RELEASE_BLOCK(MATRIX);
}

s32 Gfx_ApplyMatrixNoSf(SVECTOR* arg0, SVECTOR* arg1)
{
    s32 result;

    gte_ldsvrtrow0(arg0);
    gte_ldv0(arg1);
    gte_rtv0_sf0();
    gte_stlvnl0(&result);
    return result;
}
