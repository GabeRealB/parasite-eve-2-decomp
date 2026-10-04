/* Part of the Pawn and Rook GOLEM library; see golem_pawn_rook.h. */

/// Scratch-stack block the laser beam is drawn from.
///
/// The beam is walked in eight steps from its near end, each point projected
/// in turn, and a segment is drawn between each point and the one before it.
/// A segment has three pairs of vertices: its start and end on the centre
/// line, and the same two points set off to either side by a width that
/// shrinks with the segment's depth. Reserved for the length of the call.
typedef struct {
    VECTOR  span;       // screen vector from the segment's start to its end; z is zero
    SVECTOR point;      // the beam point being projected, in root space; then `span` normalised (0x1000 for 1), with y negated, whose components, swapped, give the offsets across the beam
    SVECTOR step;       // an eighth of the beam, in root space
    s32     prevScreen; // screen position of the segment's start, x in the low halfword and y in the high
    s32     screen;     // screen position of the segment's end, packed the same way
    s32     prevDepth;  // ordering depth of the segment's start, a quarter of its screen z
    s32     depth;      // ordering depth of the segment's end
    s16     vertexX[6]; // screen x of the segment's vertices: [0], [1] its start and end on the centre line; [2], [3] and [4], [5] the same points on either edge
    s16     vertexY[6]; // screen y of the same vertices
} _GolemPawnRookLaserBeamScratch;
STATIC_ASSERT_SIZEOF(_GolemPawnRookLaserBeamScratch, 0x48);

/// Draws the aim beam from `arg2` to `arg1` in eight projected steps. Each
/// step nearer than OTZ 30 is skipped; otherwise the segment's screen normal
/// (`VectorNormalS`) offsets the ends by a depth-scaled width into two
/// semi-transparent red-to-black `POLY_G4`s (corner order from
/// `gGolemPawnRookBeamRibbonCorners`), a red `LINE_F2` core and a blend `DR_TPAGE`.
void golemPawnRookDrawLaserBeam(Task* arg0, SVECTOR* arg1, SVECTOR* arg2)
{
    _GolemPawnRookLaserBeamScratch* s;
    GfxCoord*                       self;
    POLY_G4*                        poly;
    LINE_F2*                        line;
    DR_TPAGE*                       page;
    s32                             i;
    s32                             j;
    s32                             depth;

    s          = SCRATCH_STACK_RESERVE_BLOCK(_GolemPawnRookLaserBeamScratch);
    self       = arg0->extra.tmd->coords;
    s->step.vx = (arg1->vx - arg2->vx) / 8;
    s->step.vy = (arg1->vy - arg2->vy) / 8;
    s->step.vz = (arg1->vz - arg2->vz) / 8;
    gte_SetRotMatrix(&self->workm);
    gte_SetTransMatrix(&self->workm);
    gte_ldv0(arg2);
    gte_rtps();
    gte_stsxy(&s->prevScreen);
    gte_stszotz(&s->prevDepth);
    for (i = 1; i < 8; i++) {
        s->point.vx = arg2->vx + s->step.vx * i;
        s->point.vy = arg2->vy + s->step.vy * i;
        s->point.vz = arg2->vz + s->step.vz * i;
        gte_SetRotMatrix(&self->workm);
        gte_SetTransMatrix(&self->workm);
        gte_ldv0(&s->point);
        gte_rtps();
        gte_stsxy(&s->screen);
        gte_stszotz(&s->depth);
        depth = (s->prevDepth + s->depth) / 2;
        if (depth < 30) {
            s->prevScreen = s->screen;
            s->prevDepth  = s->depth;
            continue;
        }
        s->span.vz    = 0;
        s->vertexX[0] = s->prevScreen;
        s->vertexY[0] = s->prevScreen >> 16;
        s->vertexX[1] = s->screen;
        s->vertexY[1] = s->screen >> 16;
        s->span.vx    = s->vertexX[1] - s->vertexX[0];
        s->span.vy    = s->vertexY[1] - s->vertexY[0];
        VectorNormalS(&s->span, &s->point);
        s->point.vy  *= -1;
        s->vertexX[2] = s->vertexX[0] + (-(s->point.vy * 0x900) >> 12) / depth;
        s->vertexX[3] = s->vertexX[1] + (-(s->point.vy * 0x900) >> 12) / depth;
        s->vertexX[4] = s->vertexX[0] + ((s->point.vy * 9) >> 4) / depth;
        s->vertexY[2] = s->vertexY[0] + (-(s->point.vx * 0x900) >> 12) / depth;
        s->vertexY[3] = s->vertexY[1] + (-(s->point.vx * 0x900) >> 12) / depth;
        s->vertexY[4] = s->vertexY[0] + ((s->point.vx * 9) >> 4) / depth;
        s->vertexY[5] = s->vertexY[1] + ((s->point.vx * 9) >> 4) / depth;
        s->vertexX[5] = s->vertexX[1] + ((s->point.vy * 9) >> 4) / depth;
        for (j = 0; j < 2; j++) {
            poly           = gGpuPrimCursor;
            gGpuPrimCursor = poly + 1;
            setPolyG4(poly);
            setSemiTrans(poly, 1);
            poly->x0 = s->vertexX[gGolemPawnRookBeamRibbonCorners[j][0]];
            poly->y0 = s->vertexY[gGolemPawnRookBeamRibbonCorners[j][0]];
            poly->x1 = s->vertexX[gGolemPawnRookBeamRibbonCorners[j][1]];
            poly->y1 = s->vertexY[gGolemPawnRookBeamRibbonCorners[j][1]];
            poly->x2 = s->vertexX[gGolemPawnRookBeamRibbonCorners[j][2]];
            poly->y2 = s->vertexY[gGolemPawnRookBeamRibbonCorners[j][2]];
            poly->x3 = s->vertexX[gGolemPawnRookBeamRibbonCorners[j][3]];
            poly->y3 = s->vertexY[gGolemPawnRookBeamRibbonCorners[j][3]];
            poly->r0 = 0xFF;
            poly->g0 = 0;
            poly->b0 = 0;
            poly->r1 = 0xFF;
            poly->g1 = 0;
            poly->b1 = 0;
            poly->r2 = 0;
            poly->g2 = 0;
            poly->b2 = 0;
            poly->r3 = 0;
            poly->g3 = 0;
            poly->b3 = 0;
            addPrim((&gGpuCurrentOt[((((u32)(depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*gGpuCurrentOt)]), poly);
        }
        line           = gGpuPrimCursor;
        gGpuPrimCursor = line + 1;
        setLineF2(line);
        setSemiTrans(line, 1);
        line->x0 = s->prevScreen;
        line->y0 = s->prevScreen >> 16;
        line->x1 = s->screen;
        line->y1 = s->screen >> 16;
        line->r0 = 0xFF;
        line->g0 = 0;
        line->b0 = 0;
        addPrim((&gGpuCurrentOt[((((u32)(depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*gGpuCurrentOt)]), line);
        page           = gGpuPrimCursor;
        gGpuPrimCursor = page + 1;
        setlen(page, 1);
        page->code[0] = 0xE1000620;
        addPrim((&gGpuCurrentOt[((((u32)(depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*gGpuCurrentOt)]), page);
        s->prevScreen = s->screen;
        s->prevDepth  = s->depth;
    }
    SCRATCH_STACK_RELEASE_BLOCK(_GolemPawnRookLaserBeamScratch);
}
