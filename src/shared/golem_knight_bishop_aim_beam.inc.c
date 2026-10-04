/* Part of the Knight and Bishop GOLEM library; see golem_knight_bishop.h. */

/// Scratch-stack block the aim beam is drawn from.
///
/// The beam is cut into eight segments between its two projected ends. Each
/// segment has three pairs of vertices: its start and end on the centre line,
/// and the same two points set off to either side by a width that shrinks
/// with the segment's depth. Reserved for the length of the call.
typedef struct {
    VECTOR  span;       // screen vector from the beam's far end to the point on part 4; z is zero
    SVECTOR direction;  // `span` normalised (0x1000 for 1), with y negated; its components, swapped, give the offsets across the beam
    s32     depth;      // ordering depth at the end of the segment being drawn; the segment is skipped below 0x1E
    s32     depthStep;  // an eighth of the depth difference between the beam's ends
    u16     vertexX[6]; // screen x of the segment's vertices: [0], [1] its start and end on the centre line; [2], [3] and [4], [5] the same points on either edge
    u16     vertexY[6]; // screen y of the same vertices
    s16     stepX;      // an eighth of the beam's extent in screen x: one segment's length
    s16     stepY;      // the same in screen y
} _GolemKnightBishopAimBeamScratch;
STATIC_ASSERT_SIZEOF(_GolemKnightBishopAimBeamScratch, 0x3C);

/// Draws the red aim beam between the two screen points
/// `golemKnightBishopAimFromPart` leaves in `beamScreenX`, `beamScreenY` and
/// `beamDepth`: eight segments, each skipped while its interpolated depth is
/// below 0x1E, and each drawn as two shaded quads offset along the screen
/// normal, a centre line and a tpage.
void golemKnightBishopDrawAimBeam(Task* arg0)
{
    _GolemKnightBishopAimBeamScratch* sc;
    GolemKnightBishopWork*            work;
    POLY_G4*                          poly;
    LINE_F2*                          line;
    DR_TPAGE*                         tp;
    s32                               i;
    s32                               j;

    sc          = SCRATCH_STACK_RESERVE_BLOCK(_GolemKnightBishopAimBeamScratch);
    work        = arg0->work;
    sc->span.vx = work->beamScreenX[1] - work->beamScreenX[0];
    sc->span.vy = work->beamScreenY[1] - work->beamScreenY[0];
    sc->span.vz = 0;
    VectorNormalS(&sc->span, &sc->direction);
    sc->direction.vy *= -1;
    sc->stepX         = (work->beamScreenX[1] - work->beamScreenX[0]) / 8;
    sc->stepY         = (work->beamScreenY[1] - work->beamScreenY[0]) / 8;
    sc->depthStep     = (work->beamDepth[1] - work->beamDepth[0]) / 8;
    for (i = 0; i < 8; i++) {
        sc->depth = sc->depthStep * (i + 1) + work->beamDepth[0];
        if (sc->depth < 0x1E) {
            continue;
        }
        sc->vertexX[0] = work->beamScreenX[0] + sc->stepX * i;
        sc->vertexX[1] = work->beamScreenX[0] + sc->stepX * (i + 1);
        sc->vertexX[2] = sc->vertexX[0] + ((-(sc->direction.vy * 0x600) >> 12) / sc->depth);
        sc->vertexX[3] = sc->vertexX[1] + ((-(sc->direction.vy * 0x600) >> 12) / sc->depth);
        sc->vertexX[4] = sc->vertexX[0] + (((sc->direction.vy * 3) >> 3) / sc->depth);
        sc->vertexX[5] = sc->vertexX[1] + (((sc->direction.vy * 3) >> 3) / sc->depth);
        sc->vertexY[0] = work->beamScreenY[0] + sc->stepY * i;
        sc->vertexY[1] = work->beamScreenY[0] + sc->stepY * (i + 1);
        sc->vertexY[2] = sc->vertexY[0] + ((-(sc->direction.vx * 0x600) >> 12) / sc->depth);
        sc->vertexY[3] = sc->vertexY[1] + ((-(sc->direction.vx * 0x600) >> 12) / sc->depth);
        sc->vertexY[4] = sc->vertexY[0] + (((sc->direction.vx * 3) >> 3) / sc->depth);
        sc->vertexY[5] = sc->vertexY[1] + (((sc->direction.vx * 3) >> 3) / sc->depth);
        for (j = 0; j < 2; j++) {
            poly           = gGpuPrimCursor;
            gGpuPrimCursor = poly + 1;
            setlen(poly, 8);
            poly->code = 0x3A;
            poly->x0   = sc->vertexX[gGolemKnightBishopBeamQuadCorners[j][0]];
            poly->y0   = sc->vertexY[gGolemKnightBishopBeamQuadCorners[j][0]];
            poly->x1   = sc->vertexX[gGolemKnightBishopBeamQuadCorners[j][1]];
            poly->y1   = sc->vertexY[gGolemKnightBishopBeamQuadCorners[j][1]];
            poly->x2   = sc->vertexX[gGolemKnightBishopBeamQuadCorners[j][2]];
            poly->y2   = sc->vertexY[gGolemKnightBishopBeamQuadCorners[j][2]];
            poly->x3   = sc->vertexX[gGolemKnightBishopBeamQuadCorners[j][3]];
            poly->y3   = sc->vertexY[gGolemKnightBishopBeamQuadCorners[j][3]];
            setRGB0(poly, 0xFF, 0, 0);
            setRGB1(poly, 0xFF, 0, 0);
            setRGB2(poly, 0, 0, 0);
            setRGB3(poly, 0, 0, 0);
            addPrim((&gGpuCurrentOt[((((u32)(sc->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*gGpuCurrentOt)]), poly);
        }
        line           = gGpuPrimCursor;
        gGpuPrimCursor = line + 1;
        setlen(line, 3);
        line->code = 0x42;
        line->x0   = sc->vertexX[0];
        line->y0   = sc->vertexY[0];
        line->x1   = sc->vertexX[1];
        line->y1   = sc->vertexY[1];
        setRGB0(line, 0xFF, 0, 0);
        addPrim((&gGpuCurrentOt[((((u32)(sc->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*gGpuCurrentOt)]), line);
        tp             = gGpuPrimCursor;
        gGpuPrimCursor = tp + 1;
        setlen(tp, 1);
        tp->code[0] = 0xE1000620;
        addPrim((&gGpuCurrentOt[((((u32)(sc->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*gGpuCurrentOt)]), tp);
    }
    SCRATCH_STACK_RELEASE_BLOCK(_GolemKnightBishopAimBeamScratch);
}
