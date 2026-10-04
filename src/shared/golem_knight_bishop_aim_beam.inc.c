/* Part of the Knight and Bishop GOLEM library; see golem_knight_bishop.h. */

/// Draws the red aim beam between the two screen points
/// `golemKnightBishopAimFromPart` leaves in `beamScreenX`, `beamScreenY` and
/// `beamDepth`: eight segments, each skipped while its interpolated depth is
/// below 0x1E, and each drawn as two shaded quads offset along the screen
/// normal, a centre line and a tpage.
void golemKnightBishopDrawAimBeam(Task* arg0)
{
    GolemKnightBishopTrailScratch* sc;
    GolemKnightBishopWork*         work;
    POLY_G4*                       poly;
    LINE_F2*                       line;
    DR_TPAGE*                      tp;
    s32                            i;
    s32                            j;

    sc         = SCRATCH_STACK_RESERVE_BLOCK(GolemKnightBishopTrailScratch);
    work       = arg0->work;
    sc->dir.vx = work->beamScreenX[1] - work->beamScreenX[0];
    sc->dir.vy = work->beamScreenY[1] - work->beamScreenY[0];
    sc->dir.vz = 0;
    VectorNormalS(&sc->dir, &sc->norm);
    sc->norm.vy *= -1;
    sc->dx       = (work->beamScreenX[1] - work->beamScreenX[0]) / 8;
    sc->dy       = (work->beamScreenY[1] - work->beamScreenY[0]) / 8;
    sc->dz       = (work->beamDepth[1] - work->beamDepth[0]) / 8;
    for (i = 0; i < 8; i++) {
        sc->z = sc->dz * (i + 1) + work->beamDepth[0];
        if (sc->z < 0x1E) {
            continue;
        }
        sc->x[0] = work->beamScreenX[0] + sc->dx * i;
        sc->x[1] = work->beamScreenX[0] + sc->dx * (i + 1);
        sc->x[2] = sc->x[0] + ((-(sc->norm.vy * 0x600) >> 12) / sc->z);
        sc->x[3] = sc->x[1] + ((-(sc->norm.vy * 0x600) >> 12) / sc->z);
        sc->x[4] = sc->x[0] + (((sc->norm.vy * 3) >> 3) / sc->z);
        sc->x[5] = sc->x[1] + (((sc->norm.vy * 3) >> 3) / sc->z);
        sc->y[0] = work->beamScreenY[0] + sc->dy * i;
        sc->y[1] = work->beamScreenY[0] + sc->dy * (i + 1);
        sc->y[2] = sc->y[0] + ((-(sc->norm.vx * 0x600) >> 12) / sc->z);
        sc->y[3] = sc->y[1] + ((-(sc->norm.vx * 0x600) >> 12) / sc->z);
        sc->y[4] = sc->y[0] + (((sc->norm.vx * 3) >> 3) / sc->z);
        sc->y[5] = sc->y[1] + (((sc->norm.vx * 3) >> 3) / sc->z);
        for (j = 0; j < 2; j++) {
            poly           = gGpuPrimCursor;
            gGpuPrimCursor = poly + 1;
            setlen(poly, 8);
            poly->code = 0x3A;
            poly->x0   = sc->x[gGolemKnightBishopBeamQuadCorners[j][0]];
            poly->y0   = sc->y[gGolemKnightBishopBeamQuadCorners[j][0]];
            poly->x1   = sc->x[gGolemKnightBishopBeamQuadCorners[j][1]];
            poly->y1   = sc->y[gGolemKnightBishopBeamQuadCorners[j][1]];
            poly->x2   = sc->x[gGolemKnightBishopBeamQuadCorners[j][2]];
            poly->y2   = sc->y[gGolemKnightBishopBeamQuadCorners[j][2]];
            poly->x3   = sc->x[gGolemKnightBishopBeamQuadCorners[j][3]];
            poly->y3   = sc->y[gGolemKnightBishopBeamQuadCorners[j][3]];
            setRGB0(poly, 0xFF, 0, 0);
            setRGB1(poly, 0xFF, 0, 0);
            setRGB2(poly, 0, 0, 0);
            setRGB3(poly, 0, 0, 0);
            addPrim((&gGpuCurrentOt[((((u32)(sc->z << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*gGpuCurrentOt)]), poly);
        }
        line           = gGpuPrimCursor;
        gGpuPrimCursor = line + 1;
        setlen(line, 3);
        line->code = 0x42;
        line->x0   = sc->x[0];
        line->y0   = sc->y[0];
        line->x1   = sc->x[1];
        line->y1   = sc->y[1];
        setRGB0(line, 0xFF, 0, 0);
        addPrim((&gGpuCurrentOt[((((u32)(sc->z << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*gGpuCurrentOt)]), line);
        tp             = gGpuPrimCursor;
        gGpuPrimCursor = tp + 1;
        setlen(tp, 1);
        tp->code[0] = 0xE1000620;
        addPrim((&gGpuCurrentOt[((((u32)(sc->z << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*gGpuCurrentOt)]), tp);
    }
    SCRATCH_STACK_RELEASE_BLOCK(GolemKnightBishopTrailScratch);
}
