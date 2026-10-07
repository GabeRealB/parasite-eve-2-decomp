/* Part of the Maggot and Caterpillar library; see maggot_caterpillar.h. */

/// Draws the puff as a growing textured billboard using its parent actor atlas.
///
/// The task must own a live coordinate body and have a live TMD parent. Its
/// coordinate must already be composed. The frame is the puff age, 0..14; the
/// cell changes every two ticks. The radius storage must cover every age read.
/// Depth is projected Z / 4; values below 20 emit no primitive. One scratch
/// quad is reserved and released on every path.
static void _maggotCaterpillarDrawPuff(Task* actor, s32 frame)
{
    enum { MAGGOT_CATERPILLAR_PUFF_NEAR_DEPTH              = 20,
           MAGGOT_CATERPILLAR_PUFF_CELL_LAST_PIXEL         = 31,
           MAGGOT_CATERPILLAR_PUFF_RADIUS_PROJECTION_SCALE = 0x300 };
    POLY_FT4*               quad;
    GfxCoord*               coord;
    s32                     depth;
    s32                     screenCentre;
    s32                     y;
    s32                     radius;
    s32                     x;
    s32                     bottom;
    s32                     top;
    s32                     left;
    s32                     right;
    ActorScreenQuadScratch* scratchEnd;
    ActorScreenQuadScratch* scratch;
    TmdObject*              parentModel;
    ActorSpriteUv*          cell;
    SVECTOR*                projectedPoint;

    // Project the composed origin before reusing the vector storage as corners.
    scratchEnd                   = (ActorScreenQuadScratch*)SCRATCH_STACK_CURSOR(u8);
    coord                        = actor->extra.coordBody->coord;
    actor                        = actor->parent;
    parentModel                  = actor->extra.tmd;
    scratchEnd[-1].corners[0].vx = (u16)coord->workm.t[0];
    scratch                      = scratchEnd - 1;
    scratch->corners[0].vy       = (u16)coord->workm.t[1];
    SCRATCH_STACK_CURSOR(u8)     = (u8*)scratch;
    scratch->corners[0].vz       = (u16)coord->workm.t[2];
    projectedPoint               = &scratch->corners[0];
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_ldv0(projectedPoint);
    gte_rtps();
    gte_stsxy(&scratchEnd[-1].screenCentre);
    gte_stszotz(&scratchEnd[-1].otz);
    depth = scratch->otz;
    if (depth >= MAGGOT_CATERPILLAR_PUFF_NEAR_DEPTH) {
        radius                       = (s32)(gMaggotCaterpillarPuffRadius[frame] * MAGGOT_CATERPILLAR_PUFF_RADIUS_PROJECTION_SCALE) / depth;
        quad                         = gGpuPrimCursor;
        screenCentre                 = scratch->screenCentre;
        gGpuPrimCursor               = quad + 1;
        x                            = screenCentre & 0xFFFF;
        y                            = screenCentre >> 0x10;
        left                         = x - radius;
        top                          = y - radius;
        right                        = x + radius;
        bottom                       = y + radius;
        scratchEnd[-1].corners[0].vx = left;
        scratch->corners[0].vy       = top;
        scratch->corners[0].vz       = 0;
        scratch->corners[1].vx       = right;
        scratch->corners[1].vy       = top;
        scratch->corners[1].vz       = 0;
        scratch->corners[2].vx       = left;
        scratch->corners[2].vy       = bottom;
        scratch->corners[2].vz       = 0;
        scratch->corners[3].vx       = right;
        scratch->corners[3].vy       = bottom;
        scratch->corners[3].vz       = 0;
        setPolyFT4(quad);
        setSemiTrans(quad, 1);
        setRGB0(quad, 0x80, 0x80, 0x80);
        setShadeTex(quad, 1);
        quad->tpage = (s16)(((s32)(((parentModel->texturePageOffset << 6) + 0x180) & 0x3FF) >> 6) | 0xB0);
        quad->clut  = (s16)(((s32)((u8)parentModel->clutRowOffset << 0x18) >> 0x12) + 0x3D40);
        cell        = &gMaggotCaterpillarPuffCells[frame >> 1];
        quad->u0    = cell->u;
        quad->v0    = cell->v;
        quad->u1    = cell->u + MAGGOT_CATERPILLAR_PUFF_CELL_LAST_PIXEL;
        quad->v1    = cell->v;
        quad->u2    = cell->u;
        quad->v2    = cell->v + MAGGOT_CATERPILLAR_PUFF_CELL_LAST_PIXEL;
        quad->u3    = cell->u + MAGGOT_CATERPILLAR_PUFF_CELL_LAST_PIXEL;
        quad->v3    = cell->v + MAGGOT_CATERPILLAR_PUFF_CELL_LAST_PIXEL;
        quad->x0    = (u16)scratchEnd[-1].corners[0].vx;
        quad->y0    = (u16)scratch->corners[0].vy;
        quad->x1    = (u16)scratch->corners[1].vx;
        quad->y1    = (u16)scratch->corners[1].vy;
        quad->x2    = (u16)scratch->corners[2].vx;
        quad->y2    = (u16)scratch->corners[2].vy;
        quad->x3    = (u16)scratch->corners[3].vx;
        quad->y3    = (u16)scratch->corners[3].vy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((u32)(scratch->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK), quad);
    }
    SCRATCH_STACK_CURSOR(u8) += sizeof(ActorScreenQuadScratch);
}
