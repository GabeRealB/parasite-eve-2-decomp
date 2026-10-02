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

/// Canonical values for replacing a rotation or composing it on the right.
///
/// The axis-rotation routines accept any nonzero value as replacement.
enum {
    GRAPHICS_ROTATION_COMPOSE = 0,
    GRAPHICS_ROTATION_REPLACE = 1,
};

/// Rotates `matrix` about X and preserves its translation.
///
/// `matrix` must be a live, writable, word-aligned `MATRIX`. `angle` is signed,
/// with 4096 units per turn; `rsin` and `rcos` supply signed matrix elements
/// scaled by `ONE` (4096). The pure Rx has the identity along X, cosine at
/// `m[1][1]` and `m[2][2]`, negative sine at `m[1][2]`, and sine at `m[2][1]`.
///
/// Nonzero `replace` (`GRAPHICS_ROTATION_REPLACE`) installs Rx in the nine
/// rotation elements. Zero (`GRAPHICS_ROTATION_COMPOSE`) right-multiplies the
/// current rotation by Rx. For a local-to-parent matrix, replacement sets the
/// orientation in the parent frame; composition turns about the current local X.
/// Composition requires initialized rotation elements and overwrites GTE
/// rotation, product and flag registers. The initialized scratch stack must have
/// room for 0x24 bytes, reserved only until this call returns.
void gfxRotMatrixX(MATRIX* matrix, s32 angle, s32 replace);

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

/// Rotates `matrix` about Z and preserves its translation.
///
/// `matrix` must be a live, writable, word-aligned `MATRIX`. `angle` is signed,
/// with 4096 units per turn; `rsin` and `rcos` supply signed matrix elements
/// scaled by `ONE` (4096). The pure Rz has cosine at `m[0][0]` and `m[1][1]`,
/// negative sine at `m[0][1]`, sine at `m[1][0]`, and the identity along Z.
///
/// Nonzero `replace` (`GRAPHICS_ROTATION_REPLACE`) installs Rz in the nine
/// rotation elements. Zero (`GRAPHICS_ROTATION_COMPOSE`) right-multiplies the
/// current rotation by Rz. For a local-to-parent matrix, replacement sets the
/// orientation in the parent frame; composition turns about the current local Z.
/// Composition requires initialized rotation elements and overwrites GTE
/// rotation, product and flag registers. The initialized scratch stack must have
/// room for a word-aligned 0x24-byte reservation, disjoint from `matrix` and
/// released before return. No caller pointer is retained; the caller manages
/// any containing coordinate's dirty stamp.
void gfxRotMatrixZ(MATRIX* matrix, s32 angle, s32 replace);

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

/// Converts a light direction to a short vector with length approximately `ONE`.
///
/// `direction` is a word-aligned, readable 16-byte span whose first three
/// signed 32-bit words are xyz in any common scale and coordinate frame. The
/// fourth word is copied but has no effect on the result. This accepts a
/// `VECTOR`, a `GsF_LIGHT`, or a matrix translation with a readable following
/// word; a standalone three-word translation is insufficient.
///
/// Large components are uniformly right-shifted before normalization, rounding
/// negative values down. The result uses the input's coordinate frame and
/// 4096 for 1.0, and writes only `normalizedDirection`'s xyz; its final halfword
/// is untouched. A zero direction produces zero components.
///
/// Requires an initialized scratch stack with 24 free bytes, released before
/// return. Input and output must be disjoint from that reservation. Changes
/// GTE arithmetic and leading-sign-bit-count state; retains no caller pointer.
void gfxNormalizeLightDirection(const void* direction, SVECTOR* normalizedDirection);

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
    GfxRotationWords* rotationWords = (GfxRotationWords*)matrix;

    rotationWords->m00M01 = ONE;
    rotationWords->m02M10 = 0;
    rotationWords->m11M12 = ONE;
    rotationWords->m20M21 = 0;
    rotationWords->m22    = ONE;
}

#endif // MAIN_GFX_H
