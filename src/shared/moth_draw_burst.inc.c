/* Part of the Moth library; see moth.h. */

/// Writes the four corners of a square centered at zero in the screen plane.
///
/// Borrows a reserved quad scratch block. halfSize is measured in screen pixels;
/// each component narrows to s16 and all four Z values are zero.
static __inline__ void _mothSetBurstCorners(ActorScreenQuadScratch* scratch, s32 halfSize)
{
    scratch->corners[0].vx = -halfSize;
    scratch->corners[0].vy = -halfSize;
    scratch->corners[0].vz = 0;
    scratch->corners[1].vx = halfSize;
    scratch->corners[1].vy = -halfSize;
    scratch->corners[1].vz = 0;
    scratch->corners[2].vx = -halfSize;
    scratch->corners[2].vy = halfSize;
    scratch->corners[2].vz = 0;
    scratch->corners[3].vx = halfSize;
    scratch->corners[3].vy = halfSize;
    scratch->corners[3].vz = 0;
}

/// Queues the moth's death burst with a fixed random screen roll and subtractive blend.
///
/// Requires a composed root, timer in 1..23, eight UV cells and live primitive/
/// ordering-table storage. Projects the cached root through GsWSMATRIX and
/// skips OT depth below 20; half-size is 30720/depth screen pixels. Timer 1
/// picks the retained screen roll only if projection passes the depth test.
/// Cells are 32 pixels square, selected by timer/3. Uses the model's signed
/// texture-page/CLUT-row offsets and releases its scratch quad before return.
static void _mothDrawBurst(Task* task)
{
    enum {
        MOTH_BURST_MIN_OT_DEPTH       = 20,
        MOTH_BURST_SIZE_NUMERATOR     = 0x7800,
        MOTH_BURST_CELL_PIXELS        = 32,
        MOTH_BURST_TEXTURE_X          = 384,
        MOTH_BURST_TEXTURE_Y          = 256,
        MOTH_BURST_CLUT_ROW           = 245,
        MOTH_BURST_TEXTURE_PAGE_WORDS = 64,
        MOTH_BURST_SCREEN_X_MASK      = 0xFFFF,
        MOTH_BURST_NEUTRAL_MODULATION = 128
    };

    ActorScreenQuadScratch* scratch;
    MothWork*               work;
    TmdObject*              model;
    GfxCoord*               rootCoord;
    s32                     halfSize, screenX, screenY;
    s16                     cornerIndex;
    SVECTOR*                corner;
    POLY_FT4*               quad;
    ActorSpriteUv*          cell;
    model                  = task->extra.tmd;
    scratch                = SCRATCH_STACK_RESERVE_BLOCK(ActorScreenQuadScratch);
    rootCoord              = model->coords;
    work                   = task->work;
    scratch->corners[0].vx = rootCoord->workm.t[0];
    scratch->corners[0].vy = rootCoord->workm.t[1];
    scratch->corners[0].vz = rootCoord->workm.t[2];
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_ldv0(&scratch->corners[0]);
    gte_rtps();
    gte_stsxy(&scratch->screenCentre);
    gte_stszotz(&scratch->otz);
    if (scratch->otz < MOTH_BURST_MIN_OT_DEPTH) {
        SCRATCH_STACK_RELEASE_BLOCK(ActorScreenQuadScratch);
        return;
    }
    if (work->timer == 1) {
        scratch->corners[0].vx = 0;
        scratch->corners[0].vy = 0;
        scratch->corners[0].vz = ((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & ACTOR_TRANSFORM_ANGLE_MASK;
        RotMatrix(&scratch->corners[0], &work->burstRollMtx);
    }
    // Rotate a camera-facing quad in screen space, then bind its animation cell.
    halfSize = MOTH_BURST_SIZE_NUMERATOR / scratch->otz;
    screenX  = scratch->screenCentre & MOTH_BURST_SCREEN_X_MASK;
    screenY  = scratch->screenCentre >> 16;
    _mothSetBurstCorners(scratch, halfSize);
    for (cornerIndex = 0; cornerIndex < (s32)ARRAY_SIZE(scratch->corners); cornerIndex++) {
        gte_SetRotMatrix(&work->burstRollMtx);
        corner = &scratch->corners[cornerIndex];
        gte_ldv0(corner);
        gte_rtv0();
        gte_stsv(corner);
        corner->vx += screenX;
        corner->vy += screenY;
    }
    quad           = gGpuPrimCursor;
    gGpuPrimCursor = quad + 1;
    setPolyFT4(quad);
    setSemiTrans(quad, true);
    setRGB0(quad, MOTH_BURST_NEUTRAL_MODULATION, MOTH_BURST_NEUTRAL_MODULATION, MOTH_BURST_NEUTRAL_MODULATION);
    setShadeTex(quad, true);
    quad->tpage = getTPage(1, GPU_BLEND_SUBTRACT, model->texturePageOffset * MOTH_BURST_TEXTURE_PAGE_WORDS + MOTH_BURST_TEXTURE_X, MOTH_BURST_TEXTURE_Y);
    quad->clut  = (model->clutRowOffset << 6) + getClut(0, MOTH_BURST_CLUT_ROW);
    cell        = &gMothBurstUvs[(s16)(work->timer / MOTH_BURST_TICKS_PER_CELL)];
    quad->u0    = cell->u;
    quad->v0    = cell->v;
    quad->u1    = cell->u + MOTH_BURST_CELL_PIXELS - 1;
    quad->v1    = cell->v;
    quad->u2    = cell->u;
    quad->v2    = cell->v + MOTH_BURST_CELL_PIXELS - 1;
    quad->u3    = cell->u + MOTH_BURST_CELL_PIXELS - 1;
    quad->v3    = cell->v + MOTH_BURST_CELL_PIXELS - 1;
    quad->x0    = scratch->corners[0].vx;
    quad->y0    = scratch->corners[0].vy;
    quad->x1    = scratch->corners[1].vx;
    quad->y1    = scratch->corners[1].vy;
    quad->x2    = scratch->corners[2].vx;
    quad->y2    = scratch->corners[2].vy;
    quad->x3    = scratch->corners[3].vx;
    quad->y3    = scratch->corners[3].vy;
    addPrim((&gGpuCurrentOt[(((((u32)scratch->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*gGpuCurrentOt)]), quad);
    SCRATCH_STACK_RELEASE_BLOCK(ActorScreenQuadScratch);
}
