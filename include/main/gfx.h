#ifndef MAIN_GFX_H
#define MAIN_GFX_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "types.h"

#include "main/coord.h"
#include "main/gfx_types.h"

/// Two neighbouring elements of a matrix's rotation, `m[r][c]` and the one after
/// it, written or read as one word; code sets and copies rotations this way.
/// Build a value with `MATRIX_PAIR_VALUE`.
#define MATRIX_PAIR(mat, r, c) (*(s32*)&(mat)->m[r][c])

/// A matrix's translation as a vector.
#define MATRIX_TRANS(mat) ((VECTOR3*)(mat)->t)

/// The view rotation: the coordinate `gGfxViewCoord` hangs off, whose `coord`
/// holds the view's rotation, parented in turn to `Gfx_ViewOffsetCoord`.
extern GpCoord gGfxViewRotCoord;

/// The root of the view chain, whose `coord` offsets the view along z.
extern GpCoord Gfx_ViewOffsetCoord;

/// The view coordinate: every world-space object is parented to it, so a
/// coordinate composed against it comes out in view space.
///
/// Its `coord` carries the view translation and its `workm` the view matrix the
/// world is drawn and projected through. The view rotation and the view offset
/// are the two coordinates above it in the chain, which is why its own matrix
/// holds a translation alone.
extern GpCoord gGfxViewCoord;

void Gfx_SetFlatLight(s32 id, GsF_LIGHT* light, MATRIX* dirMtx, MATRIX* colorMtx);

void Gfx_RotMatrixXYZ(MATRIX* out, SVECTOR* angles, s32 flag);

void Gfx_RotMatrixYXZ(MATRIX* out, SVECTOR* angles, s32 flag);

void Gfx_RotMatrixX(MATRIX* matrix, s32 angle, s32 flag);

void Gfx_RotMatrixY(MATRIX* matrix, s32 angle, s32 flag);

void Gfx_RotMatrixZ(MATRIX* matrix, s32 angle, s32 flag);

void Gfx_MatrixToEuler(MATRIX* matrix, SVECTOR* vector);

void Gfx_MatrixCol0(MATRIX* matrix, SVECTOR* vector);

void Gfx_MatrixCol1(MATRIX* matrix, SVECTOR* vector);

void Gfx_MatrixCol2(MATRIX* matrix, SVECTOR* vector);

void Gfx_NormalizeLightDir(VECTOR* light, SVECTOR* out);

void Gfx_OrthonormalBasis(MATRIX* out, SVECTOR* arg1, SVECTOR* arg2);

s32 Gfx_ApplyMatrixNoSf(SVECTOR* arg0, SVECTOR* arg1);

/// Sets a matrix's rotation to identity in five word stores; the translation
/// is left alone.
static __inline__ void gfxSetRotIdentity(MATRIX* m)
{
    GpMtxWords* w = (GpMtxWords*)m;

    w->m00_m01 = ONE;
    w->m02_m10 = 0;
    w->m11_m12 = ONE;
    w->m20_m21 = 0;
    w->m22     = ONE;
}

#endif // MAIN_GFX_H
