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

/// Scales column `n` of `m`'s rotation by the matching component of `scale`
/// (4.12 fixed point) with the GTE's `gpf 12`. Each column is gathered into an
/// `SVECTOR` borrowed from the scratch-pad stack, scaled and written back; the
/// block is returned before leaving.
static __inline__ void gfxScaleMatrixColumns(MATRIX* m, VECTOR* scale)
{
    SVECTOR* sv;

    sv = SCRATCH_STACK_RESERVE_BLOCK(SVECTOR);
    gte_ReadMatrixColumn(m, 0, sv);
    gte_lddp(scale->vx);
    gte_ldsv(sv);
    gte_gpf12();
    gte_stsv(sv);
    gte_WriteMatrixColumn(sv, m, 0);
    gte_ReadMatrixColumn(m, 1, sv);
    gte_lddp(scale->vy);
    gte_ldsv(sv);
    gte_gpf12();
    gte_stsv(sv);
    gte_WriteMatrixColumn(sv, m, 1);
    gte_ReadMatrixColumn(m, 2, sv);
    gte_lddp(scale->vz);
    gte_ldsv(sv);
    gte_gpf12();
    gte_stsv(sv);
    gte_WriteMatrixColumn(sv, m, 2);
    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
}

#endif // MAIN_GFXGTE_H
