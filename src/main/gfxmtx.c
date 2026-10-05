#include "main/gfx.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/gtemac.h>
#include <psyq/inline_c.h>
#include <psyq/memory.h>

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

/// Scratch workspace for composing a rotation from three Euler angles.
///
/// One axis rotation is written to `rotation` directly. Each further factor is
/// then applied on the right, one column at a time: the GTE transforms `column`
/// by the product so far and the result replaces that column of `rotation`. A
/// factor's column along its own axis is a unit vector, which leaves the
/// matching product column unchanged, so only two columns per factor pass
/// through `column`. Shared by the X * Y * Z and Y * X * Z orders.
/// Owned by one scratch-stack reservation and released before return.
typedef struct {
    MATRIX  rotation; // Product so far, complete after the third factor; only m is used, scaled by ONE
    s16     sinX;     // Signed sine of the X angle, scaled by ONE (4096)
    s16     cosX;     // Signed cosine of the X angle, scaled by ONE
    s16     sinY;     // Signed sine of the Y angle, scaled by ONE
    s16     cosY;     // Signed cosine of the Y angle, scaled by ONE
    s16     sinZ;     // Signed sine of the Z angle, scaled by ONE
    s16     cosZ;     // Signed cosine of the Z angle, scaled by ONE
    SVECTOR column;   // Column of the factor being applied, the GTE's operand; pad is unused
} _GfxEulerRotationScratch;
STATIC_ASSERT_SIZEOF(_GfxEulerRotationScratch, 0x34);

/// Scratch workspace for composing a rotation in the Z * Y * X Euler order.
///
/// The Z rotation is written to `rotation` directly. The Y and X factors are
/// then applied on the right, one column at a time: the GTE transforms a column
/// of the factor by the product so far and the result replaces the same column
/// of `rotation`. A factor's column along its own axis is a unit vector, which
/// leaves the matching product column unchanged, so only two columns per factor
/// are staged. Unlike `_GfxEulerRotationScratch`, which rewrites one operand,
/// each product column has its own operand vector.
/// Owned by one scratch-stack reservation and released before return.
typedef struct {
    MATRIX  rotation;   // Product so far, complete after the third factor; only m is used, scaled by ONE
    s16     sinX;       // Signed sine of the X angle, scaled by ONE (4096)
    s16     cosX;       // Signed cosine of the X angle, scaled by ONE
    s16     sinY;       // Signed sine of the Y angle, scaled by ONE
    s16     cosY;       // Signed cosine of the Y angle, scaled by ONE
    s16     sinZ;       // Signed sine of the Z angle, scaled by ONE
    s16     cosZ;       // Signed cosine of the Z angle, scaled by ONE
    SVECTOR columns[3]; // GTE operands, indexed by the product column each replaces: 0 from Y, 1 from X, 2 from Y then X; pads are unused
} _GfxZyxRotationScratch;
STATIC_ASSERT_SIZEOF(_GfxZyxRotationScratch, 0x44);

/// Builds a pure Z-axis rotation from precomputed sine and cosine.
///
/// `rotation` supplies a live, writable `MATRIX*`. `angleSin` and `angleCos`
/// are signed 16-bit expressions for the same angle, scaled by `ONE` (4096)
/// and in [-ONE, ONE]. The resulting 3x3 is
/// {{cos, -sin, 0}, {sin, cos, 0}, {0, 0, ONE}}.
///
/// Evaluates `rotation` nine times and each trigonometric expression twice.
/// Arguments must have no side effects and remain unchanged by the stores;
/// any sine/cosine storage must be disjoint from the nine destination elements.
/// Captures no caller identifiers. Writes only those nine signed 16-bit elements;
/// the alignment bytes and translation remain unchanged. Requires no initialized
/// destination rotation, scratch-stack reservation or GTE state; retains no pointer.
#define GRAPHICS_BUILD_Z_ROTATION(rotation, angleSin, angleCos) \
    do {                                                        \
        (rotation)->m[0][0] = (angleCos);                       \
        (rotation)->m[0][1] = -(angleSin);                      \
        (rotation)->m[0][2] = 0;                                \
        (rotation)->m[1][0] = (angleSin);                       \
        (rotation)->m[1][1] = (angleCos);                       \
        (rotation)->m[1][2] = 0;                                \
        (rotation)->m[2][0] = 0;                                \
        (rotation)->m[2][1] = 0;                                \
        (rotation)->m[2][2] = ONE;                              \
    } while (0)

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

/// Installs an Euler product or multiplies it into the current rotation.
///
/// The two word-aligned matrices must be disjoint, with the product initialized;
/// composition (zero replace) also requires an initialized destination rotation.
/// Writes only the nine rotation entries and changes GTE state on composition.
static __inline__ void _gfxApplyEulerRotation(MATRIX* matrix, const MATRIX* rotation, s32 replace)
{
    if (replace != 0) {
        MATRIX_PAIR(matrix, 0, 0) = MATRIX_PAIR(rotation, 0, 0);
        MATRIX_PAIR(matrix, 0, 2) = MATRIX_PAIR(rotation, 0, 2);
        MATRIX_PAIR(matrix, 1, 1) = MATRIX_PAIR(rotation, 1, 1);
        MATRIX_PAIR(matrix, 2, 0) = MATRIX_PAIR(rotation, 2, 0);
        matrix->m[2][2]           = rotation->m[2][2];
    } else {
        gte_MulMatrix0(matrix, rotation, matrix);
    }
}

/// Builds the first X factor of an Euler rotation in its scratch workspace.
///
/// Reads sinX/cosX scaled by ONE and writes only the nine rotation entries.
static __inline__ void _gfxBuildEulerXFactor(_GfxEulerRotationScratch* scratch)
{
    scratch->rotation.m[0][0] = ONE;
    scratch->rotation.m[0][1] = 0;
    scratch->rotation.m[0][2] = 0;
    scratch->rotation.m[1][0] = 0;
    scratch->rotation.m[1][1] = scratch->cosX;
    scratch->rotation.m[1][2] = -scratch->sinX;
    scratch->rotation.m[2][0] = 0;
    scratch->rotation.m[2][1] = scratch->sinX;
    scratch->rotation.m[2][2] = scratch->cosX;
}

/// Builds the first Y factor of an Euler rotation in its scratch workspace.
///
/// Reads sinY/cosY scaled by ONE and writes only the nine rotation entries.
static __inline__ void _gfxBuildEulerYFactor(_GfxEulerRotationScratch* scratch)
{
    scratch->rotation.m[0][0] = scratch->cosY;
    scratch->rotation.m[0][1] = 0;
    scratch->rotation.m[0][2] = scratch->sinY;
    scratch->rotation.m[1][0] = 0;
    scratch->rotation.m[1][1] = ONE;
    scratch->rotation.m[1][2] = 0;
    scratch->rotation.m[2][0] = -scratch->sinY;
    scratch->rotation.m[2][1] = 0;
    scratch->rotation.m[2][2] = scratch->cosY;
}

void gfxRotMatrixXYZ(MATRIX* matrix, const SVECTOR* angles, s32 replace)
{
    _GfxEulerRotationScratch* scratch;

    scratch = SCRATCH_STACK_RESERVE_BLOCK(_GfxEulerRotationScratch);

    scratch->sinX = rsin(angles->vx);
    scratch->sinY = rsin(angles->vy);
    scratch->sinZ = rsin(angles->vz);
    scratch->cosX = rcos(angles->vx);
    scratch->cosY = rcos(angles->vy);
    scratch->cosZ = rcos(angles->vz);

    // Build RX; the other factors are applied on the right by their columns.
    _gfxBuildEulerXFactor(scratch);

    // Apply RY, preserving the column along its own axis.
    scratch->column.vx = scratch->cosY;
    scratch->column.vy = 0;
    scratch->column.vz = -scratch->sinY;

    gte_SetRotMatrix(&scratch->rotation);
    gte_ldsv(&scratch->column);
    gte_rtir();
    scratch->column.vx = scratch->sinY;
    scratch->column.vz = scratch->cosY;
    gte_stclmv(&scratch->rotation.m[0][0]);

    gte_ldsv(&scratch->column);
    gte_rtir();
    // Stage RZ in the GTE pipeline gap, then transform its two columns.
    scratch->column.vx = scratch->cosZ;
    scratch->column.vy = scratch->sinZ;
    scratch->column.vz = 0;
    gte_stclmv(&scratch->rotation.m[0][2]);

    gte_SetRotMatrix(&scratch->rotation);
    gte_ldsv(&scratch->column);
    gte_rtir();
    scratch->column.vx = -scratch->sinZ;
    scratch->column.vy = scratch->cosZ;
    gte_stclmv(&scratch->rotation.m[0][0]);

    gte_ldsv(&scratch->column);
    gte_rtir();
    gte_stclmv(&scratch->rotation.m[0][1]);

    // Install only the 3x3, or right-multiply the caller's existing rotation.
    _gfxApplyEulerRotation(matrix, &scratch->rotation, replace);

    SCRATCH_STACK_RELEASE_BLOCK(_GfxEulerRotationScratch);
}

void gfxRotMatrixYXZ(MATRIX* matrix, const SVECTOR* angles, s32 replace)
{
    _GfxEulerRotationScratch* scratch;

    scratch = SCRATCH_STACK_RESERVE_BLOCK(_GfxEulerRotationScratch);

    scratch->sinX = rsin(angles->vx);
    scratch->sinY = rsin(angles->vy);
    scratch->sinZ = rsin(angles->vz);
    scratch->cosX = rcos(angles->vx);
    scratch->cosY = rcos(angles->vy);
    scratch->cosZ = rcos(angles->vz);

    // Build RY; the other factors are applied on the right by their columns.
    _gfxBuildEulerYFactor(scratch);

    // Apply RX, preserving the column along its own axis.
    scratch->column.vx = 0;
    scratch->column.vy = scratch->cosX;
    scratch->column.vz = scratch->sinX;

    gte_SetRotMatrix(&scratch->rotation);
    gte_ldsv(&scratch->column);
    gte_rtir();
    scratch->column.vx = 0;
    scratch->column.vy = -scratch->sinX;
    scratch->column.vz = scratch->cosX;
    gte_stclmv(&scratch->rotation.m[0][1]);

    gte_ldsv(&scratch->column);
    gte_rtir();
    // Stage RZ in the GTE pipeline gap, then transform its two columns.
    scratch->column.vx = scratch->cosZ;
    scratch->column.vy = scratch->sinZ;
    scratch->column.vz = 0;
    gte_stclmv(&scratch->rotation.m[0][2]);

    gte_SetRotMatrix(&scratch->rotation);
    gte_ldsv(&scratch->column);
    gte_rtir();
    scratch->column.vx = -scratch->sinZ;
    scratch->column.vy = scratch->cosZ;
    scratch->column.vz = 0;
    gte_stclmv(&scratch->rotation.m[0][0]);

    gte_ldsv(&scratch->column);
    gte_rtir();
    gte_stclmv(&scratch->rotation.m[0][1]);

    // Install only the 3x3, or right-multiply the caller's existing rotation.
    _gfxApplyEulerRotation(matrix, &scratch->rotation, replace);

    SCRATCH_STACK_RELEASE_BLOCK(_GfxEulerRotationScratch);
}

/// Builds the first Z factor of a ZYX rotation in its scratch workspace.
///
/// Reads sinZ/cosZ scaled by ONE and writes only the nine rotation entries.
static __inline__ void _gfxBuildZyxZFactor(_GfxZyxRotationScratch* scratch)
{
    scratch->rotation.m[0][0] = scratch->cosZ;
    scratch->rotation.m[0][1] = -scratch->sinZ;
    scratch->rotation.m[0][2] = 0;
    scratch->rotation.m[1][0] = scratch->sinZ;
    scratch->rotation.m[1][1] = scratch->cosZ;
    scratch->rotation.m[1][2] = 0;
    scratch->rotation.m[2][0] = 0;
    scratch->rotation.m[2][1] = 0;
    scratch->rotation.m[2][2] = ONE;
}

/// Builds Rz(z) * Ry(y) * Rx(x), preserving matrix translation and alignment bytes.
///
/// Signed xyz angles use 4096 units per turn; pad is not read. Nonzero `replace`
/// installs the product, zero right-multiplies the initialized current rotation.
/// Requires a writable word-aligned matrix, halfword-aligned readable angles,
/// and 0x44 free scratch-stack bytes disjoint from both objects. Releases that
/// reservation before return, changes GTE rotation and arithmetic state, and
/// retains no pointer.
static void _gfxRotMatrixZYX(MATRIX* matrix, const SVECTOR* angles, s32 replace)
{
    _GfxZyxRotationScratch* scratch;

    scratch = SCRATCH_STACK_RESERVE_BLOCK(_GfxZyxRotationScratch);

    scratch->sinX = rsin(angles->vx);
    scratch->sinY = rsin(angles->vy);
    scratch->sinZ = rsin(angles->vz);
    scratch->cosX = rcos(angles->vx);
    scratch->cosY = rcos(angles->vy);
    scratch->cosZ = rcos(angles->vz);

    // Build RZ; the other factors are applied on the right by their columns.
    _gfxBuildZyxZFactor(scratch);

    // Apply RY, preserving the column along its own axis.
    scratch->columns[0].vx = scratch->cosY;
    scratch->columns[0].vy = 0;
    scratch->columns[0].vz = -scratch->sinY;

    gte_SetRotMatrix(&scratch->rotation);
    gte_ldsv(&scratch->columns[0]);
    gte_rtir();
    scratch->columns[2].vx = scratch->sinY;
    scratch->columns[2].vy = 0;
    scratch->columns[2].vz = scratch->cosY;
    gte_stclmv(&scratch->rotation.m[0][0]);

    gte_ldsv(&scratch->columns[2]);
    gte_rtir();
    // Stage RX in the GTE pipeline gap, then transform its two columns.
    scratch->columns[1].vx = 0;
    scratch->columns[1].vy = scratch->cosX;
    scratch->columns[1].vz = scratch->sinX;
    gte_stclmv(&scratch->rotation.m[0][2]);

    gte_SetRotMatrix(&scratch->rotation);
    gte_ldsv(&scratch->columns[1]);
    gte_rtir();
    scratch->columns[2].vx = 0;
    scratch->columns[2].vy = -scratch->sinX;
    scratch->columns[2].vz = scratch->cosX;
    gte_stclmv(&scratch->rotation.m[0][1]);

    gte_ldsv(&scratch->columns[2]);
    gte_rtir();
    gte_stclmv(&scratch->rotation.m[0][2]);

    // Install only the 3x3, or right-multiply the caller's existing rotation.
    _gfxApplyEulerRotation(matrix, &scratch->rotation, replace);

    SCRATCH_STACK_RELEASE_BLOCK(_GfxZyxRotationScratch);
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

/// Transposes the nine rotation entries, preserving destination translation.
///
/// Source rotation must be initialized and destination rotation writable, both
/// halfword-aligned and disjoint: columns are copied one at a time, so this is
/// not an in-place transpose. Alignment bytes are untouched. Borrows the matrices
/// only for the call and uses no scratch space or GTE state.
static void _gfxTransposeRotation(const MATRIX* source, MATRIX* destination)
{
    gte_TransposeMatrix(source, destination);
}

void gfxReadMatrixXAxis(const MATRIX* matrix, SVECTOR* xAxis)
{
    gte_ReadMatrixColumn(matrix, 0, xAxis);
}

void gfxReadMatrixYAxis(const MATRIX* matrix, SVECTOR* yAxis)
{
    gte_ReadMatrixColumn(matrix, 1, yAxis);
}

void gfxReadMatrixZAxis(const MATRIX* matrix, SVECTOR* zAxis)
{
    gte_ReadMatrixColumn(matrix, 2, zAxis);
}

/// Builds a pure X-axis rotation from precomputed sine and cosine.
///
/// `scratch` must hold `angleSin` and `angleCos` for the same angle, scaled by
/// `ONE` (4096) and in [-ONE, ONE]. Only these two fields are read; its matrix
/// need not be initialized. `rotation` is a live, writable `MATRIX`, either
/// disjoint from `scratch` or exactly `&scratch->rotation`.
///
/// Writes the nine signed 16-bit rotation elements, preserving the alignment
/// bytes and translation. The caller owns both objects; no scratch-stack
/// reservation or GTE state is changed, and no pointer is retained.
static __inline__ void _gfxBuildXRotation(MATRIX* rotation, const _GfxAxisRotationScratch* scratch)
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

void gfxRotMatrixZ(MATRIX* matrix, s32 angle, s32 replace)
{
    _GfxAxisRotationScratch* scratch;

    scratch = SCRATCH_STACK_RESERVE_BLOCK(_GfxAxisRotationScratch);

    scratch->angleSin = rsin(angle);
    scratch->angleCos = rcos(angle);

    // Build Rz directly for replacement, or in scratch for matrix * Rz.
    // Composition reads only the 3x3, so scratch translation stays unset.
    if (replace != 0) {
        GRAPHICS_BUILD_Z_ROTATION(matrix, scratch->angleSin, scratch->angleCos);
    } else {
        GRAPHICS_BUILD_Z_ROTATION(&scratch->rotation, scratch->angleSin, scratch->angleCos);
        gte_MulMatrix0(matrix, &scratch->rotation, matrix);
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

void gfxBuildOrthonormalBasis(MATRIX* matrix, const SVECTOR* zAxis, const SVECTOR* yHint)
{
    MATRIX*   basisRows;
    SVECTOR3* zRow;

    // The eight-byte hint copy temporarily covers row 2's first halfword.
    // The following Z stores replace it before any row is used.
    basisRows = SCRATCH_STACK_RESERVE_BLOCK(MATRIX);
    memcpy(basisRows->m[1], yHint, sizeof(*yHint));
    zRow     = (SVECTOR3*)basisRows->m[2];
    zRow->vx = zAxis->vx;
    zRow->vy = zAxis->vy;
    zRow->vz = zAxis->vz;

    // Seed X from Y cross Z. The GTE helpers access only each row's xyz.
    gte_ldopv1SV(basisRows->m[1]);
    gte_ldopv2SV(zRow);
    gte_op12();
    gte_stsv(basisRows->m[0]);

    // Rebuild Y from Z cross X, then X from Y cross Z, and normalize all rows.
    MatrixNormal_2(basisRows, basisRows);

    gte_TransposeMatrix(basisRows, matrix);

    SCRATCH_STACK_RELEASE_BLOCK(MATRIX);
}

s32 gfxDotProduct(const SVECTOR* left, const SVECTOR* right)
{
    s32 dotProduct;

    gte_ldsvrtrow0(left);
    gte_ldv0(right);
    gte_rtv0_sf0();
    gte_stlvnl0(&dotProduct);
    return dotProduct;
}
