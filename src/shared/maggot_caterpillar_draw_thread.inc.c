/* Part of the Maggot and Caterpillar library; see maggot_caterpillar.h. */

/// Draws the fading vertical thread of a hanging or falling actor.
///
/// Endpoints lie on the Y axis of `baseMatrix`, with the upper endpoint offset
/// from the lower by `vertical.threadRise`. Both must project to Z / 4 >= 30;
/// the lower endpoint supplies ordering depth. Zero fade draws full brightness;
/// other values count down from 45 and stay at one. Releases its scratch block
/// on every path and queues a semitransparent Gouraud line and draw mode.
static void _maggotCaterpillarDrawThread(Task* actor)
{
    enum { MAGGOT_CATERPILLAR_THREAD_NEAR_DEPTH = 30,
           MAGGOT_CATERPILLAR_THREAD_LOWER_Y    = -0x352 };
    MaggotCaterpillarLineScratch* scratch;
    MaggotCaterpillarWork*        work;
    LINE_G2*                      line;
    DR_TPAGE*                     drawMode;
    s32                           upperX;
    s32                           upperY;
    s32                           upperScreenPos;
    s32                           lowerScreenPos;

    // Project one Y-axis endpoint into the reserved scratch block.
    // Standalone statement sequence: scratch/work are stable locals, height
    // is evaluated once. Call only where all nine statements belong.
#define MAGGOT_CATERPILLAR_PROJECT_THREAD_END(scratch, work, height) \
    (scratch)->position.vx = 0;                                      \
    (scratch)->position.vy = (height);                               \
    (scratch)->position.vz = 0;                                      \
    gte_SetRotMatrix(&(work)->baseMatrix);                           \
    gte_SetTransMatrix(&(work)->baseMatrix);                         \
    gte_ldv0(&(scratch)->position);                                  \
    gte_rtps();                                                      \
    gte_stsxy(&(scratch)->screenPos);                                \
    gte_stszotz(&(scratch)->depth)

    scratch = SCRATCH_STACK_RESERVE_BLOCK(MaggotCaterpillarLineScratch);
    work    = actor->work;
    MAGGOT_CATERPILLAR_PROJECT_THREAD_END(scratch, work, work->vertical.threadRise + MAGGOT_CATERPILLAR_THREAD_LOWER_Y);
    if (scratch->depth < MAGGOT_CATERPILLAR_THREAD_NEAR_DEPTH) {
        SCRATCH_STACK_RELEASE_BLOCK(MaggotCaterpillarLineScratch);
        return;
    }
    upperScreenPos = scratch->screenPos;
    upperX         = upperScreenPos & 0xFFFF;
    upperY         = upperScreenPos >> 16;
    MAGGOT_CATERPILLAR_PROJECT_THREAD_END(scratch, work, MAGGOT_CATERPILLAR_THREAD_LOWER_Y);
    if (scratch->depth < MAGGOT_CATERPILLAR_THREAD_NEAR_DEPTH) {
        SCRATCH_STACK_RELEASE_BLOCK(MaggotCaterpillarLineScratch);
        return;
    }
    line           = gGpuPrimCursor;
    gGpuPrimCursor = line + 1;
    lowerScreenPos = scratch->screenPos;
    setLineG2(line);
    setSemiTrans(line, 1);
    line->x0 = upperX;
    line->y0 = upperY;
    line->x1 = lowerScreenPos;
    line->y1 = lowerScreenPos >> 16;
    if (work->threadFade == 0) {
        line->b0 = line->g0 = line->r0 = 0x80;
        line->b1 = line->g1 = line->r1 = 0xC0;
    } else {
        if (--work->threadFade <= 0) {
            work->threadFade = 1;
        }
        line->r0 = (work->threadFade * 0x80) / MAGGOT_CATERPILLAR_THREAD_FADE_FRAMES;
        line->g0 = line->r0;
        line->b0 = line->r0;
        line->r1 = (work->threadFade * 0xC0) / MAGGOT_CATERPILLAR_THREAD_FADE_FRAMES;
        line->g1 = line->r1;
        line->b1 = line->r1;
    }
    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((u32)(scratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK), line);
    // The draw mode is linked last so the GPU executes it before the line.
    drawMode       = gGpuPrimCursor;
    gGpuPrimCursor = drawMode + 1;
    setDrawTPage(drawMode, 1, 1, getTPage(0, 1, 0, 0));
    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((u32)(scratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK), drawMode);
    SCRATCH_STACK_RELEASE_BLOCK(MaggotCaterpillarLineScratch);
#undef MAGGOT_CATERPILLAR_PROJECT_THREAD_END
}
