/* Part of the Maggot and Caterpillar library; see maggot_caterpillar.h. */

/// Draws a vertical semi-transparent gouraud line in the space of the matrix
/// `baseMatrix`, from height `vertical.threadRise - 0x352` to height -0x352; nothing is
/// drawn when either end projects nearer than depth 30. The line runs from grey
/// 0x80 to 0xC0; while `threadFade` counts down (never below 1) both ends are
/// scaled by `threadFade / 45`.
void maggotCaterpillarDrawThread(Task* actor)
{
    MaggotCaterpillarLineScratch* scratch;
    MaggotCaterpillarWork*        work;
    LINE_G2*                      line;
    DR_TPAGE*                     page;
    s32                           x;
    s32                           y;
    s32                           screen;
    s32                           screen1;

    scratch              = SCRATCH_STACK_RESERVE_BLOCK(MaggotCaterpillarLineScratch);
    work                 = actor->work;
    scratch->position.vx = 0;
    scratch->position.vy = work->vertical.threadRise - 0x352;
    scratch->position.vz = 0;
    gte_SetRotMatrix(&work->baseMatrix);
    gte_SetTransMatrix(&work->baseMatrix);
    gte_ldv0(&scratch->position);
    gte_rtps();
    gte_stsxy(&scratch->screenPos);
    gte_stszotz(&scratch->depth);
    if (scratch->depth < 30) {
        SCRATCH_STACK_RELEASE_BLOCK(MaggotCaterpillarLineScratch);
        return;
    }
    screen               = scratch->screenPos;
    x                    = screen & 0xFFFF;
    y                    = screen >> 16;
    scratch->position.vx = 0;
    scratch->position.vy = -0x352;
    scratch->position.vz = 0;
    gte_SetRotMatrix(&work->baseMatrix);
    gte_SetTransMatrix(&work->baseMatrix);
    gte_ldv0(&scratch->position);
    gte_rtps();
    gte_stsxy(&scratch->screenPos);
    gte_stszotz(&scratch->depth);
    if (scratch->depth < 30) {
        SCRATCH_STACK_RELEASE_BLOCK(MaggotCaterpillarLineScratch);
        return;
    }
    line           = gGpuPrimCursor;
    gGpuPrimCursor = line + 1;
    screen1        = scratch->screenPos;
    setLineG2(line);
    setSemiTrans(line, 1);
    line->x0 = x;
    line->y0 = y;
    line->x1 = screen1;
    line->y1 = screen1 >> 16;
    if (work->threadFade == 0) {
        line->b0 = line->g0 = line->r0 = 0x80;
        line->b1 = line->g1 = line->r1 = 0xC0;
    } else {
        if (--work->threadFade <= 0) {
            work->threadFade = 1;
        }
        line->r0 = (work->threadFade * 0x80) / 45;
        line->g0 = line->r0;
        line->b0 = line->r0;
        line->r1 = (work->threadFade * 0xC0) / 45;
        line->g1 = line->r1;
        line->b1 = line->r1;
    }
    addPrim((&gGpuCurrentOt[((((u32)(scratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*gGpuCurrentOt)]), line);
    page           = gGpuPrimCursor;
    gGpuPrimCursor = page + 1;
    setlen(page, 1);
    page->code[0] = 0xE1000620;
    addPrim((&gGpuCurrentOt[((((u32)(scratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*gGpuCurrentOt)]), page);
    SCRATCH_STACK_RELEASE_BLOCK(MaggotCaterpillarLineScratch);
}
