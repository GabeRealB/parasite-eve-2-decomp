/* Part of the Sucklerceph library; see sucklerceph.h. */

/// Scales the rotation of `arg1`'s matrix by the first enemy's scale factor
/// `swellScale`, clamped first to 0x1000..0x13E8: each of the matrix's three
/// columns is copied into an `SVECTOR` on the scratch stack, multiplied by the
/// factor on the GTE and written back.
void sucklercephScalePart(Task* arg0, GfxCoord* arg1)
{
    ScratchStackCursor* scratch;
    SVECTOR*            vec;
    MATRIX*             matrix;
    SucklercephWork*    work;

    scratch = (ScratchStackCursor*)SCRATCH_STACK_CURSOR_SLOT;
    vec     = scratch->top;
    work    = arg0->work;
    vec--;
    scratch->top = vec;
    if (work->swellScale >= 0x13E8) {
        work->swellScale = 0x13E8;
    }
    if (work->swellScale < 0x1001) {
        work->swellScale = 0x1000;
    }
    matrix = &arg1->coord;

    gte_ReadMatrixColumn(matrix, 0, vec);
    gte_lddp(work->swellScale);
    gte_ldsv(vec);
    gte_gpf12();
    gte_stsv(vec);
    gte_WriteMatrixColumn(vec, matrix, 0);

    gte_ReadMatrixColumn(matrix, 1, vec);
    gte_lddp(work->swellScale);
    gte_ldsv(vec);
    gte_gpf12();
    gte_stsv(vec);
    gte_WriteMatrixColumn(vec, matrix, 1);

    gte_ReadMatrixColumn(matrix, 2, vec);
    gte_lddp(work->swellScale);
    gte_ldsv(vec);
    gte_gpf12();
    gte_stsv(vec);
    gte_WriteMatrixColumn(vec, matrix, 2);

    SCRATCH_POP_AT(&scratch->top, SVECTOR);
}
