#ifndef MAIN_GFXGTE_H
#define MAIN_GFXGTE_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/gtemac.h>
#include <psyq/inline_c.h>

#include "gte.h"
#include "types.h"

#include "main/scratch.h"

/// Applies a matrix's 3x3 part to a signed short vector in place.
///
/// `rotationMatrix` has signed Q12 coefficients (`ONE` is 1.0); `vector`
/// keeps its caller's coordinate units. Products are shifted right by 12 and
/// saturated to -32768..32767. Scaling and reflection in the matrix are retained;
/// the result is not normalized. Matrix translation is not read or added.
///
/// Requires a word-aligned matrix with its first 20 bytes readable, including
/// the two bytes after the nine coefficients, and a halfword-aligned, readable
/// full `SVECTOR` with writable xyz. The fourth halfword is copied but has no
/// effect and is not written. Borrows both pointers only until return; uses a
/// local snapshot and no scratch-stack storage. Overwrites GTE RT, V0, MAC1..3,
/// IR1..3 and FLAG; translation registers are unchanged.
static __inline__ void _gfxRotateSv(const MATRIX* rotationMatrix, SVECTOR* vector)
{
    SVECTOR input;

    // Snapshot the complete input before the GTE loads and in-place stores.
    input = *vector;
    gte_ApplyMatrixSV(rotationMatrix, &input, vector);
}

/// Stages a rotation matrix and signed short vector for the next GTE command.
///
/// `rotationMatrix` supplies 4.12 coefficients; `source` keeps its caller's
/// coordinate units. Both inputs are read-only and must remain valid through
/// the call. The full `SVECTOR` is copied to a local snapshot before loading
/// RT and V0. Translation registers are unchanged.
/// Issue `gte_rtv0()` and read its result before replacing RT or V0.
static __inline__ void _gfxLoadRotSv(const MATRIX* rotationMatrix, const SVECTOR* source)
{
    SVECTOR input;

    input = *source;
    gte_SetRotMatrix(rotationMatrix);
    gte_ldv0(&input);
}

/// Scales a matrix's three basis columns in place by per-axis Q12 factors.
///
/// Column 0 uses `scale->vx`, column 1 `vy`, and column 2 `vz`: this applies
/// scale along the matrix's local axes. Each factor's low signed 16 bits are
/// used (`ONE` is unit scale). Coefficients are signed Q12; products are shifted
/// right by 12 and saturated to -32768..32767. Translation is unchanged, and
/// the result is not normalized. The scale vector's fourth word is not read.
///
/// Borrows both inputs until return: the matrix's nine halfword coefficients
/// must be halfword-aligned, readable and writable. The scale's three words
/// must be readable and word-aligned. Keep both inputs disjoint from each other
/// and from the scratch reservation. Requires an initialized stack with room for one
/// aligned `SVECTOR` (8 bytes); releases it before return without clearing it.
/// Overwrites GTE IR0..3, MAC1..3, RGB0..2 and FLAG. GTE rotation and translation
/// registers are unchanged; callers invalidate coordinate caches themselves.
static __inline__ void _gfxScaleMatrixColumns(MATRIX* matrix, const VECTOR* scale)
{
    SVECTOR* column;

    /// Gathers, Q12-scales and scatters one basis column using an `SVECTOR`.
    ///
    /// `axis` is a constant in 0..2. Pointer arguments are evaluated repeatedly
    /// and must have no side effects; `factor` is evaluated once after gathering
    /// the column. Captures no caller identifiers; used only in this function.
#define GRAPHICS_SCALE_MATRIX_COLUMN(matrix, column, axis, factor) \
    {                                                              \
        gte_ReadMatrixColumn((matrix), (axis), (column));          \
        gte_lddp(factor);                                          \
        gte_ldsv(column);                                          \
        gte_gpf12();                                               \
        gte_stsv(column);                                          \
        gte_WriteMatrixColumn((column), (matrix), (axis));         \
    }

    // Reuse one scratch column while preserving the gather/scale/store order.
    column = SCRATCH_STACK_RESERVE_BLOCK(SVECTOR);
    GRAPHICS_SCALE_MATRIX_COLUMN(matrix, column, 0, scale->vx);
    GRAPHICS_SCALE_MATRIX_COLUMN(matrix, column, 1, scale->vy);
    GRAPHICS_SCALE_MATRIX_COLUMN(matrix, column, 2, scale->vz);
#undef GRAPHICS_SCALE_MATRIX_COLUMN
    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
}

#endif // MAIN_GFXGTE_H
