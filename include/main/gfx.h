#ifndef MAIN_GFX_H
#define MAIN_GFX_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "types.h"

#include "main/coord.h"
#include "main/gfx_types.h"

/// Borrows a matrix's three translation words as an unpadded XYZ vector.
///
/// Evaluates `matrix` once and returns a `VECTOR3*` aliasing `MATRIX::t`;
/// the pointer expression itself reads no matrix data and makes no copy.
/// The SDK stores three signed 32-bit longs here, exactly the 12 bytes of
/// `VECTOR3`. A four-word `VECTOR` view would extend beyond the matrix.
/// Units and coordinate frame belong to the matrix; no transform is applied.
///
/// The matrix must remain live and word-aligned while the view is used.
/// The cast returns an unqualified pointer: use it only for reads when the
/// matrix is read-only. Writes change the matrix directly; callers manage
/// any containing coordinate's composition stamp. Captures no caller locals.
#define GRAPHICS_MATRIX_TRANSLATION(matrix) ((VECTOR3*)((matrix)->t))

/// Resident world-to-view rotation node above the view-origin translation.
///
/// `gGfxViewCoord` is its child and `gGfxViewOffsetCoord` its parent. Its local
/// 3x3 coefficients use 12 fractional bits (4096 for one); camera application
/// transposes a camera pose or copies an already inverted camera rotation.
/// Translation stays zero. Writers invalidate `composeStamp` after changing
/// the rotation; composed matrices are rebuilt on demand. Storage has resident
/// lifetime and is shared by camera application and model-coordinate chains.
extern GfxCoord gGfxViewRotCoord;

/// Resident root translation applied after the view rotation.
///
/// Parentless root above `gGfxViewRotCoord`, with identity local rotation and
/// an outer XYZ offset in signed game-coordinate units. Graphics startup sets
/// Z to 32768; camera records clear XYZ, explicit camera offsets replace them,
/// and the gameplay default view sets Z to 4096. Writers invalidate
/// `composeStamp`; the coordinate and its cached matrix have resident lifetime.
extern GfxCoord gGfxViewOffsetCoord;

/// View coordinate, and the parent of nodes stored in world space.
///
/// A node parented here keeps a world-space local matrix. Composing that node
/// includes this coordinate's `workm`, the view matrix the world is drawn and
/// projected through, so the result is in view space. This node is parented to
/// `gGfxViewRotCoord`; the view rotation and `gGfxViewOffsetCoord` are the two
/// coordinates above it. Its own `coord` keeps the identity rotation from
/// initialization and carries the view translation in `t`. View updates store
/// that translation and clear `composeStamp` so the next composition rebuilds
/// `workm`. `composeStamp & GRAPHICS_COORD_STAMP_MASK` is that rebuild's
/// generation; bit 31 is visit parity.
extern GfxCoord gGfxViewCoord;

/// Installs one flat directional light in a direction/colour matrix pair.
///
/// `lightIndex` is 0..2, selecting a row of `directionMatrix->m` and a column
/// of `colorMatrix->m`. The input's signed xyz may use any common scale; its
/// normalized, negated direction is stored with length approximately `ONE`
/// (4096), in the input's coordinate frame. Zero xyz produces a zero row.
/// Each RGB byte is shifted left by four into the matching colour column,
/// giving Q12 coefficients in 0..4080. Other entries, translations (including
/// ambient RGB in `colorMatrix->t`) and alignment bytes are retained.
///
/// Requires a readable, word-aligned, complete `GsF_LIGHT`, writable matrices,
/// and an initialized, word-aligned scratch stack with 48 free bytes: 24 for
/// the direction and 24 more during normalization. Caller objects must be
/// disjoint from that reservation, released before return. Changes GTE
/// arithmetic and leading-sign-bit-count state; retains no caller pointer.
void gfxSetFlatLight(s32 lightIndex, const GsF_LIGHT* light, MATRIX* directionMatrix, MATRIX* colorMatrix);

/// Builds Rx(x) * Ry(y) * Rz(z) and replaces or composes `matrix`'s rotation.
///
/// `angles` contains signed X, Y and Z angles in 4096 units per turn; its
/// fourth halfword is not read. Matrix elements use `ONE` (4096) for 1.0.
/// Nonzero `replace` (`GRAPHICS_ROTATION_REPLACE`) installs the product;
/// zero (`GRAPHICS_ROTATION_COMPOSE`) computes matrix * product. Only the
/// nine rotation elements are written; alignment bytes and translation stay.
/// Composition requires the current rotation to be initialized.
///
/// Requires a writable, word-aligned matrix, halfword-aligned readable angles,
/// and an initialized scratch stack with 0x34 free bytes. Inputs and output
/// must be disjoint from that reservation, released before return. Changes
/// GTE rotation and arithmetic state; retains no pointer. The caller manages
/// any containing coordinate's dirty stamp.
void gfxRotMatrixXYZ(MATRIX* matrix, const SVECTOR* angles, s32 replace);

/// Builds Ry(y) * Rx(x) * Rz(z) and replaces or composes `matrix`'s rotation.
///
/// X, Y and Z angles are `angles`' signed xyz, in 4096 units per turn; its
/// fourth halfword is not read. The product's elements use `ONE` for 1.0.
/// Nonzero `replace` installs the product, zero computes matrix * product;
/// composition requires initialized rotation elements. Alignment bytes and
/// translation are preserved.
///
/// Requires a writable, word-aligned matrix, halfword-aligned readable angles,
/// and 0x34 free bytes on the initialized scratch stack, disjoint from both
/// objects and released before return. Changes GTE rotation and arithmetic
/// state; retains no pointer. The caller manages the coordinate's dirty stamp.
void gfxRotMatrixYXZ(MATRIX* matrix, const SVECTOR* angles, s32 replace);

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
/// `gfxRotMatrixXYZ`, `RotMatrix` and `RotMatrix_gte`. The fourth halfword
/// is not written. Translation is not read.
void gfxMatrixToEuler(MATRIX* matrix, SVECTOR* angles);

/// Copies the matrix's local X axis (its first column) into `xAxis`.
///
/// Copies m[0][0], m[1][0] and m[2][0] to signed xyz without normalization;
/// the matrix's scale and destination frame are retained (`ONE` = 1.0 for
/// conventional rotations). Translation and the vector's pad are untouched.
/// Requires three readable matrix elements and writable vector xyz, both
/// halfword-aligned. Overlap is allowed: all reads precede all stores.
/// Borrows both objects for the call; uses no scratch space or GTE state.
void gfxReadMatrixXAxis(const MATRIX* matrix, SVECTOR* xAxis);

/// Copies the matrix's local Y axis (its second column) into `yAxis`.
///
/// Copies m[0][1], m[1][1] and m[2][1] to signed xyz without normalization;
/// the matrix's scale and destination frame are retained (`ONE` = 1.0 for
/// conventional rotations). Translation and the vector's pad are untouched.
/// Requires three readable matrix elements and writable vector xyz, both
/// halfword-aligned. Overlap is allowed: all reads precede all stores.
/// Borrows both objects for the call; uses no scratch space or GTE state.
void gfxReadMatrixYAxis(const MATRIX* matrix, SVECTOR* yAxis);

/// Copies the matrix's local Z axis (its third column) into `zAxis`.
///
/// Copies `m[0][2]`, `m[1][2]` and `m[2][2]` into `vx`, `vy` and `vz`
/// without normalization or translation. The signed 16-bit components retain
/// the matrix's scale and destination coordinate frame; conventional rotation
/// matrices use `ONE` (4096) for 1.0. A scaled matrix need not yield a unit axis.
///
/// The three matrix elements must be readable and initialized, and the vector's
/// xyz writable; both require halfword alignment. The vector's `pad` is untouched.
/// Source and destination may overlap: all three reads precede the stores.
/// Borrows both objects only for the call; allocates no scratch space and changes
/// no GTE state.
void gfxReadMatrixZAxis(const MATRIX* matrix, SVECTOR* zAxis);

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

/// Builds an orthonormal rotation around a Z direction and a Y-axis hint.
///
/// `zAxis` supplies the Z direction and `yHint` the preferred Y direction in
/// the same coordinate frame, conventionally scaled by `ONE` (4096). X starts
/// as yHint cross zAxis; X and Y are rebuilt perpendicular to Z and all three
/// axes normalized to approximately `ONE`, then written as matrix columns.
/// A useful frame requires nonzero Z and a nonparallel hint with cross products
/// that survive the GTE's 12-bit shift and signed-halfword saturation. Degenerate
/// inputs have no fallback. Matrix alignment bytes and translation are preserved.
///
/// Both inputs and the writable matrix require halfword alignment. Reads xyz
/// from `zAxis` and all eight bytes of `yHint`, including its ignored pad; a
/// six-byte hint is insufficient. Inputs may overlap each other or the output.
/// Requires an initialized scratch stack with sizeof(MATRIX) free bytes,
/// disjoint from all caller objects and released before return. Changes GTE
/// arithmetic and rotation state; retains no pointer.
void gfxBuildOrthonormalBasis(MATRIX* matrix, const SVECTOR* zAxis, const SVECTOR* yHint);

/// Returns the signed xyz dot product without a fixed-point fraction shift.
///
/// Each component is a signed halfword; the result has the product of the input
/// units (squared coordinate units when both arguments are the same vector).
/// Returns the low signed 32 bits of the GTE accumulator without saturation;
/// callers needing a nonnegative squared distance must keep that sum in range.
/// Both inputs must be word-aligned, readable eight-byte SVECTORs. Their pads
/// are loaded but do not affect the returned component. Inputs may alias.
/// Changes GTE rotation, vector, accumulator, IR and flag state; uses no scratch
/// stack and retains no pointer.
s32 gfxDotProduct(const SVECTOR* left, const SVECTOR* right);

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

/// Copies the nine rotation coefficients of `source` into `destination`, one halfword each.
///
/// Writes `m[0][0]` through `m[2][2]` in row order and leaves the alignment
/// halfword and the translation of `destination` unchanged. Both matrices must
/// be live; they may not partially overlap.
static __inline__ void gfxCopyRotation(MATRIX* destination, const MATRIX* source)
{
    destination->m[0][0] = source->m[0][0];
    destination->m[0][1] = source->m[0][1];
    destination->m[0][2] = source->m[0][2];
    destination->m[1][0] = source->m[1][0];
    destination->m[1][1] = source->m[1][1];
    destination->m[1][2] = source->m[1][2];
    destination->m[2][0] = source->m[2][0];
    destination->m[2][1] = source->m[2][1];
    destination->m[2][2] = source->m[2][2];
}

/// Copies the same 18 coefficient bytes as `gfxCopyRotation`, as four words and a halfword.
///
/// Both matrices must be word-aligned. The alignment halfword and the
/// translation of `destination` are unchanged; a whole `GfxRotationWords`
/// assignment would also copy the alignment bytes.
static __inline__ void gfxCopyRotationWords(MATRIX* destination, const MATRIX* source)
{
    GfxRotationWords*       destinationWords = (GfxRotationWords*)destination;
    const GfxRotationWords* sourceWords      = (const GfxRotationWords*)source;

    destinationWords->m00M01 = sourceWords->m00M01;
    destinationWords->m02M10 = sourceWords->m02M10;
    destinationWords->m11M12 = sourceWords->m11M12;
    destinationWords->m20M21 = sourceWords->m20M21;
    destinationWords->m22    = sourceWords->m22;
}

#endif // MAIN_GFX_H
