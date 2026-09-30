/* Part of the burster library; see burster.h. */

/// Scales the rotation of `arg1`'s matrix by the first enemy's scale factor
/// `field_2AC`, clamped first to 0x1000..0x13E8: each of the matrix's three
/// columns is copied into an `SVECTOR` on the scratch stack, multiplied by the
/// factor on the GTE and written back.
void bursterScalePart(Task* arg0, GfxCoord* arg1)
{
    ActorScratchStack* scratch;
    SVECTOR*           vec;
    MATRIX*            matrix;
    Actor104600Work*   work;

    scratch = (ActorScratchStack*)SCRATCH_STACK_CURSOR_SLOT;
    vec     = scratch->head;
    work    = arg0->work;
    vec--;
    scratch->head = vec;
    if ((u32)work->field_2AC >= 0x13E8U) {
        work->field_2AC = 0x13E8;
    }
    if ((u32)work->field_2AC < 0x1001U) {
        work->field_2AC = 0x1000;
    }
    matrix = &arg1->coord;

    gte_ReadMatrixColumn(matrix, 0, vec);
    gte_lddp(work->field_2AC);
    gte_ldsv(vec);
    gte_gpf12();
    gte_stsv(vec);
    gte_WriteMatrixColumn(vec, matrix, 0);

    gte_ReadMatrixColumn(matrix, 1, vec);
    gte_lddp(work->field_2AC);
    gte_ldsv(vec);
    gte_gpf12();
    gte_stsv(vec);
    gte_WriteMatrixColumn(vec, matrix, 1);

    gte_ReadMatrixColumn(matrix, 2, vec);
    gte_lddp(work->field_2AC);
    gte_ldsv(vec);
    gte_gpf12();
    gte_stsv(vec);
    gte_WriteMatrixColumn(vec, matrix, 2);

    SCRATCH_POP_AT(&scratch->head, SVECTOR);
}
