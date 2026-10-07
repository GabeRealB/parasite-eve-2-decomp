/* Part of the Sucklerceph library; see sucklerceph.h. */

/// Applies the Sucklerceph's swelling scale to a model part's local rotation.
///
/// Requires initialized work and a writable coordinate. Clamps the unsigned
/// Q12 `swellScale` to `ONE`..`ONE + 1000`, then multiplies all three columns
/// on the GTE with signed-halfword saturation. Translation is retained;
/// repeated calls compound scale unless animation restores the rotation first.
/// The caller invalidates composition. Reserves and releases one `SVECTOR`
/// with its fourth halfword untouched and changes GTE state.
static void _sucklercephScalePart(Task* task, GfxCoord* coord)
{
    enum { SUCKLERCEPH_SWELL_SCALE_MAX = ONE + 1000 };

    SVECTOR*         column;
    MATRIX*          matrix;
    SucklercephWork* work;

    column = SCRATCH_STACK_RESERVE_BLOCK(SVECTOR);
    work   = task->work;
    if (work->swellScale >= SUCKLERCEPH_SWELL_SCALE_MAX) {
        work->swellScale = SUCKLERCEPH_SWELL_SCALE_MAX;
    }
    if (work->swellScale < ONE + 1) {
        work->swellScale = ONE;
    }
    matrix = &coord->coord;

    // Column indices must be literal 0..2 for the GTE wrappers. Pointer arguments
    // must be stable and side-effect-free: each is evaluated more than once.
#define SUCKLERCEPH_SCALE_COLUMN(matrix, columnIndex, column, work) \
    do {                                                            \
        gte_ReadMatrixColumn((matrix), (columnIndex), (column));    \
        gte_lddp((work)->swellScale);                               \
        gte_ldsv((column));                                         \
        gte_gpf12();                                                \
        gte_stsv((column));                                         \
        gte_WriteMatrixColumn((column), (matrix), (columnIndex));   \
    } while (0)
    SUCKLERCEPH_SCALE_COLUMN(matrix, 0, column, work);
    SUCKLERCEPH_SCALE_COLUMN(matrix, 1, column, work);
    SUCKLERCEPH_SCALE_COLUMN(matrix, 2, column, work);

#undef SUCKLERCEPH_SCALE_COLUMN

    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
}
