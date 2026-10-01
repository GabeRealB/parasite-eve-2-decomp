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
extern GfxCoord gGfxViewRotCoord;

/// The root of the view chain, whose `coord` offsets the view along z.
extern GfxCoord Gfx_ViewOffsetCoord;

/// View coordinate, and the parent of nodes stored in world space.
///
/// A node parented here keeps a world-space local matrix. Composing that node
/// includes this coordinate's `workm`, the view matrix the world is drawn and
/// projected through, so the result is in view space. This node is parented to
/// `gGfxViewRotCoord`; the view rotation and `Gfx_ViewOffsetCoord` are the two
/// coordinates above it. Its own `coord` keeps the identity rotation from
/// initialization and carries the view translation in `t`. View updates store
/// that translation and clear `composeStamp` so the next composition rebuilds
/// `workm`. `composeStamp & GRAPHICS_COORD_STAMP_MASK` is that rebuild's
/// generation; bit 31 is visit parity.
extern GfxCoord gGfxViewCoord;

void Gfx_SetFlatLight(s32 id, GsF_LIGHT* light, MATRIX* dirMtx, MATRIX* colorMtx);

void Gfx_RotMatrixXYZ(MATRIX* out, SVECTOR* angles, s32 flag);

void Gfx_RotMatrixYXZ(MATRIX* out, SVECTOR* angles, s32 flag);

void Gfx_RotMatrixX(MATRIX* matrix, s32 angle, s32 flag);

/// Rotates `matrix` about Y and leaves its translation unchanged.
///
/// `angle` is signed, with 4096 units per turn, and is passed to `rsin` and
/// `rcos`. Those results are stored directly as matrix elements. The pure
/// rotation has `cos` at `m[0][0]` and `m[2][2]`, `sin` at `m[0][2]`, `-sin`
/// at `m[2][0]`, and the identity along Y.
///
/// Nonzero `replace` overwrites the nine rotation elements with that rotation.
/// Zero right-multiplies the current rotation by it. For a local-to-parent
/// matrix, overwriting yaws in the parent frame and right-multiplying turns
/// about the matrix's own Y.
void gfxRotMatrixY(MATRIX* matrix, s32 angle, s32 replace);

void Gfx_RotMatrixZ(MATRIX* matrix, s32 angle, s32 flag);

/// Decomposes a rotation matrix into XYZ Euler angles.
///
/// `angles->vx`, `vy` and `vz` are the X, Y and Z angles, signed, with 4096
/// units per turn, for the product Rx(x) * Ry(y) * Rz(z) built by
/// `Gfx_RotMatrixXYZ`, `RotMatrix` and `RotMatrix_gte`. The fourth halfword
/// is not written. Translation is not read.
void gfxMatrixToEuler(MATRIX* matrix, SVECTOR* angles);

void Gfx_MatrixCol0(MATRIX* matrix, SVECTOR* vector);

void Gfx_MatrixCol1(MATRIX* matrix, SVECTOR* vector);

void Gfx_MatrixCol2(MATRIX* matrix, SVECTOR* vector);

/// Normalizes a light direction from a readable VECTOR-sized span.
///
/// The input's first three signed words are the direction. All 16 bytes must
/// be readable, including the unused fourth word; translation views must also
/// provide the following word. `out` receives the normalized short vector.
void Gfx_NormalizeLightDir(VECTOR* light, SVECTOR* out);

void Gfx_OrthonormalBasis(MATRIX* out, SVECTOR* arg1, SVECTOR* arg2);

s32 Gfx_ApplyMatrixNoSf(SVECTOR* arg0, SVECTOR* arg1);

/// Sets the nine rotation entries of `matrix` to the GTE fixed-point identity.
///
/// Diagonal entries are `ONE` (4096, representing 1.0); all others are zero.
/// Four word stores and a final halfword store write exactly 18 bytes, leaving
/// the two alignment bytes before `t` and all three translation words unchanged.
/// The caller manages any containing `GfxCoord`'s `composeStamp` separately.
static __inline__ void gfxSetRotIdentity(MATRIX* matrix)
{
    GpMtxWords* rotationWords = (GpMtxWords*)matrix;

    rotationWords->m00_m01 = ONE;
    rotationWords->m02_m10 = 0;
    rotationWords->m11_m12 = ONE;
    rotationWords->m20_m21 = 0;
    rotationWords->m22     = ONE;
}

#endif // MAIN_GFX_H
