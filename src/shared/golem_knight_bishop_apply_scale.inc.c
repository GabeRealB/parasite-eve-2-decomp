/* Part of the Knight and Bishop GOLEM library; see golem_knight_bishop.h. */

/// Rebuilds the root part's rotation from `unscaledRootMtx`, scaled per axis
/// by `scale`: the saved matrix is copied into the root coordinate, and an
/// identity scaled in a scratchpad matrix is multiplied into it.
void golemKnightBishopApplyScale(Task* arg0)
{
    void**                 scratch;
    MATRIX*                head;
    MATRIX*                m;
    GfxCoord*              coord;
    GolemKnightBishopWork* work;

    scratch                             = SCRATCH_HEAD_ADDR;
    head                                = SCRATCH_HEAD_AT(scratch, MATRIX);
    m                                   = head - 1;
    SCRATCH_HEAD_AT(scratch, MATRIX) = m;
    coord                               = &arg0->extra.tmd->coords[0];
    work                                = arg0->work;

    coord->coord = work->unscaledRootMtx;
    gfxSetRotIdentity(m);
    ScaleMatrix(m, &work->scale);
    MulMatrix(&coord->coord, m);
    SCRATCH_POP_AT(scratch, MATRIX);
}
