/* Part of the Knight and Bishop GOLEM library; see golem_knight_bishop.h. */

/// Rebuilds the root part's rotation from the saved attach matrix
/// `field_674`, scaled per axis by `scale`: the saved matrix is
/// copied into the root coordinate, and an identity scaled in a scratchpad
/// matrix is multiplied into it.
void golemKnightBishopApplyScale(Task* arg0)
{
    void**           scratch;
    OverlayMat*      head;
    OverlayMat*      m;
    GfxCoord*        coord;
    Actor402200Work* work;

    scratch                              = SCRATCH_HEAD_ADDR;
    head                                 = SCRATCH_HEAD_AT(scratch, OverlayMat);
    m                                    = head - 1;
    SCRATCH_HEAD_AT(scratch, OverlayMat) = m;
    coord                                = &arg0->extra.tmd->coords[0];
    work                                 = arg0->work;

    coord->coord     = work->field_674;
    m->ident.m00_m01 = 0x1000;
    m->ident.m02_m10 = 0;
    m->ident.m11_m12 = 0x1000;
    m->ident.m20_m21 = 0;
    m->ident.m22     = 0x1000;
    ScaleMatrix(&m->mat, &work->scale);
    MulMatrix(&coord->coord, &m->mat);
    SCRATCH_POP_AT(scratch, OverlayMat);
}
