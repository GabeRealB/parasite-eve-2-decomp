/* Part of the Knight and Bishop GOLEM library; see golem_knight_bishop.h. */

/// Rebuilds the root part's rotation from the saved attach matrix
/// `field_674`, scaled per axis by `scale`: the saved matrix is
/// copied into the root coordinate, and an identity scaled in a scratchpad
/// matrix is multiplied into it.
void golemKnightBishopApplyScale(Task* arg0)
{
    void**                 scratch;
    OverlayMat*            head;
    OverlayMat*            m;
    GfxCoord*              coord;
    GolemKnightBishopWork* work;

    scratch                              = SCRATCH_HEAD_ADDR;
    head                                 = SCRATCH_HEAD_AT(scratch, OverlayMat);
    m                                    = head - 1;
    SCRATCH_HEAD_AT(scratch, OverlayMat) = m;
    coord                                = &arg0->extra.tmd->coords[0];
    work                                 = arg0->work;

    coord->coord    = work->field_674;
    m->ident.m00M01 = ONE;
    m->ident.m02M10 = 0;
    m->ident.m11M12 = ONE;
    m->ident.m20M21 = 0;
    m->ident.m22    = ONE;
    ScaleMatrix(&m->mat, &work->scale);
    MulMatrix(&coord->coord, &m->mat);
    SCRATCH_POP_AT(scratch, OverlayMat);
}
