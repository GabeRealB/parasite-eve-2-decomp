#include "main/gfx.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/gtemac.h>
#include <psyq/inline_c.h>

#include "gte.h"
#include "types.h"

#include "main/scratch.h"

/// Gfx_NormalizeLightDir's scratch-pad block: the direction being scaled down
/// to fit VectorNormalS, and the leading-zero counts of its components.
typedef struct {
    VECTOR v;
    s32    lzc_min; // fewest leading zeros among the components, then the shift applied
    s32    lzc_tmp; // leading zeros of the component just counted
} ScratchNormBlock;

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

void Gfx_MatrixToEuler(MATRIX* matrix, SVECTOR* vector)
{
    /// Scratch-stack reservation in bytes for the inverse-X Euler intermediate.
    ///
    /// The 0x30-byte span holds a 0x24-byte `_GfxAxisRotationScratch`; the final
    /// 0x0C bytes are unused. Allocation and release both use the full span.
    enum { GRAPHICS_EULER_SCRATCH_BYTES = 0x30 };

    _GfxAxisRotationScratch* block;
    s16                      angle;

    block = SCRATCH_STACK_RESERVE_BYTES(GRAPHICS_EULER_SCRATCH_BYTES);

    angle           = -ratan2(matrix->m[1][2], matrix->m[2][2]);
    vector->vx      = angle;
    block->angleSin = rsin(angle);
    block->angleCos = rcos(vector->vx);

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

    vector->vy = ratan2(block->rotation.m[0][2], block->rotation.m[2][2]);
    vector->vz = ratan2(block->rotation.m[1][0], block->rotation.m[1][1]);

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

void Gfx_RotMatrixX(MATRIX* matrix, s32 angle, s32 flag)
{
    _GfxAxisRotationScratch* block;

    block = SCRATCH_STACK_RESERVE_BLOCK(_GfxAxisRotationScratch);

    block->angleSin = rsin(angle);
    block->angleCos = rcos(angle);

    if (flag != 0) {
        matrix->m[0][0] = ONE;
        matrix->m[0][1] = 0;
        matrix->m[0][2] = 0;
        matrix->m[1][0] = 0;
        matrix->m[1][1] = block->angleCos;
        matrix->m[1][2] = -block->angleSin;
        matrix->m[2][0] = 0;
        matrix->m[2][1] = block->angleSin;
        matrix->m[2][2] = block->angleCos;
    } else {
        block->rotation.m[0][0] = ONE;
        block->rotation.m[0][1] = 0;
        block->rotation.m[0][2] = 0;
        block->rotation.m[1][0] = 0;
        block->rotation.m[1][1] = block->angleCos;
        block->rotation.m[1][2] = -block->angleSin;
        block->rotation.m[2][0] = 0;
        block->rotation.m[2][1] = block->angleSin;
        block->rotation.m[2][2] = block->angleCos;

        gte_MulMatrix0(matrix, &block->rotation, matrix);
    }

    SCRATCH_STACK_RELEASE_BLOCK(_GfxAxisRotationScratch);
}

void Gfx_RotMatrixY(MATRIX* matrix, s32 angle, s32 flag)
{
    _GfxAxisRotationScratch* block;

    block = SCRATCH_STACK_RESERVE_BLOCK(_GfxAxisRotationScratch);

    block->angleSin = rsin(angle);
    block->angleCos = rcos(angle);

    if (flag != 0) {
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

void Gfx_NormalizeLightDir(VECTOR* light, SVECTOR* out)
{
    ScratchNormBlock* block;

    block    = SCRATCH_STACK_RESERVE_BLOCK(ScratchNormBlock);
    block->v = *light;

    gte_Lzc(block->v.vx, &block->lzc_min);
    gte_Lzc(block->v.vy, &block->lzc_tmp);

    if (block->lzc_min > block->lzc_tmp) {
        block->lzc_min = block->lzc_tmp;
    }

    gte_Lzc(block->v.vz, &block->lzc_tmp);

    if (block->lzc_min > block->lzc_tmp) {
        block->lzc_min = block->lzc_tmp;
    }

    if (block->lzc_min < 18) {
        block->lzc_min = 18 - block->lzc_min;
        block->v.vx  >>= block->lzc_min;
        block->v.vy  >>= block->lzc_min;
        block->v.vz  >>= block->lzc_min;
    }

    VectorNormalS(&block->v, out);

    SCRATCH_STACK_RELEASE_BLOCK(ScratchNormBlock);
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
