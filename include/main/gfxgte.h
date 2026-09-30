#ifndef MAIN_GFXGTE_H
#define MAIN_GFXGTE_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/gtemac.h>
#include <psyq/inline_c.h>

#include "gte.h"
#include "types.h"

#include "main/scratch.h"

/// Rotates `v` in place by the rotation part of `m`, with no translation.
static __inline__ void gfxRotateSv(MATRIX* m, SVECTOR* v)
{
    SVECTOR in;

    in = *v;
    gte_ApplyMatrixSV(m, &in, v);
}

/// Loads `src` into the GTE's V0 and `m`'s rotation into RT, through a local
/// copy of the vector, ready for a rotate command.
static __inline__ void gfxLoadRotSv(MATRIX* m, SVECTOR* src)
{
    SVECTOR sv;

    sv = *src;
    gte_SetRotMatrix(m);
    gte_ldv0(&sv);
}

/// Scales column `n` of `m`'s rotation by the matching component of `scale`
/// (4.12 fixed point) with the GTE's `gpf 12`. Each column is gathered into an
/// `SVECTOR` borrowed from the scratch-pad stack, scaled and written back; the
/// block is returned before leaving.
static __inline__ void gfxScaleMatrixColumns(MATRIX* m, VECTOR* scale)
{
    SVECTOR* sv;

    sv = SCRATCH_PUSH(SVECTOR);
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
