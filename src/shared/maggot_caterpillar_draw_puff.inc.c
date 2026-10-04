/* Part of the Maggot and Caterpillar library; see maggot_caterpillar.h. */

/// Projects one frame's sprite into the scratch quad and, when the depth
/// clears the near plane, emits the semi-transparent `POLY_FT4` for it. The
/// model whose texture is drawn is the *parent* task's (`Task::parent`), not
/// this actor's, so the atlas and tpage come from whoever spawned it.
void maggotCaterpillarDrawPuff(Task* actor, s32 frame)
{
    POLY_FT4*               poly;
    GfxCoord*               coord;
    s32                     depth;
    s32                     screen;
    s32                     y;
    s32                     radius;
    s32                     x;
    s32                     bottom;
    s32                     top;
    s32                     left;
    s32                     right;
    ActorScreenQuadScratch* scratchEnd;
    ActorScreenQuadScratch* s;
    TmdObject*              texture;
    ActorSpriteUv*          uv;
    SVECTOR*                projection;

    scratchEnd                                                            = (ActorScreenQuadScratch*)*(u8**)PLAYSTATION_SCRATCHPAD_ADDRESS(SCRATCH_STACK_HEAD_BYTE_OFFSET);
    coord                                                                 = actor->extra.tmd->coords;
    actor                                                                 = actor->parent;
    texture                                                               = actor->extra.tmd;
    scratchEnd[-1].corners[0].vx                                          = (u16)coord->workm.t[0];
    s                                                                     = scratchEnd - 1;
    s->corners[0].vy                                                      = (u16)coord->workm.t[1];
    *(u8**)PLAYSTATION_SCRATCHPAD_ADDRESS(SCRATCH_STACK_HEAD_BYTE_OFFSET) = (u8*)s;
    s->corners[0].vz                                                      = (u16)coord->workm.t[2];
    projection                                                            = &s->corners[0];
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_ldv0(projection);
    gte_rtps();
    gte_stsxy(&scratchEnd[-1].screenCentre);
    gte_stszotz(&scratchEnd[-1].otz);
    depth = s->otz;
    if (depth >= 0x14) {
        radius                       = (s32)(gMaggotCaterpillarPuffRadius[frame] * 0x300) / depth;
        poly                         = gGpuPrimCursor;
        screen                       = s->screenCentre;
        gGpuPrimCursor               = poly + 1;
        x                            = screen & 0xFFFF;
        y                            = screen >> 0x10;
        left                         = x - radius;
        top                          = y - radius;
        right                        = x + radius;
        bottom                       = y + radius;
        scratchEnd[-1].corners[0].vx = left;
        s->corners[0].vy             = top;
        s->corners[0].vz             = 0;
        s->corners[1].vx             = right;
        s->corners[1].vy             = top;
        s->corners[1].vz             = 0;
        s->corners[2].vx             = left;
        s->corners[2].vy             = bottom;
        s->corners[2].vz             = 0;
        s->corners[3].vx             = right;
        s->corners[3].vy             = bottom;
        s->corners[3].vz             = 0;
        setPolyFT4(poly);
        setSemiTrans(poly, 1);
        setRGB0(poly, 0x80, 0x80, 0x80);
        setShadeTex(poly, 1);
        poly->tpage = (s16)(((s32)(((texture->texturePageOffset << 6) + 0x180) & 0x3FF) >> 6) | 0xB0);
        poly->clut  = (s16)(((s32)((u8)texture->clutRowOffset << 0x18) >> 0x12) + 0x3D40);
        uv          = &gMaggotCaterpillarPuffCells[frame >> 1];
        poly->u0    = (u8)uv->u;
        poly->v0    = (u8)uv->v;
        poly->u1    = (s8)(uv->u + 0x1F);
        poly->v1    = (u8)uv->v;
        poly->u2    = (u8)uv->u;
        poly->v2    = (s8)(uv->v + 0x1F);
        poly->u3    = (s8)(uv->u + 0x1F);
        poly->v3    = (s8)(uv->v + 0x1F);
        poly->x0    = (u16)scratchEnd[-1].corners[0].vx;
        poly->y0    = (u16)s->corners[0].vy;
        poly->x1    = (u16)s->corners[1].vx;
        poly->y1    = (u16)s->corners[1].vy;
        poly->x2    = (u16)s->corners[2].vx;
        poly->y2    = (u16)s->corners[2].vy;
        poly->x3    = (u16)s->corners[3].vx;
        poly->y3    = (u16)s->corners[3].vy;
        addPrim((&gGpuCurrentOt[((((u32)(s->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*gGpuCurrentOt)]), poly);
    }
    *(u8**)PLAYSTATION_SCRATCHPAD_ADDRESS(SCRATCH_STACK_HEAD_BYTE_OFFSET) += sizeof(ActorScreenQuadScratch);
}
