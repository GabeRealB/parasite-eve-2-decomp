/* Part of the Moth library; see moth.h. */

/// Projects the model root and skips anything nearer than OT depth 20. Builds a
/// screen quad sized 0x7800/depth, rotated by a random roll chosen on the first
/// frame, and queues it as a semi-transparent textured POLY_FT4 using the
/// model's texture page and CLUT offsets and the gMothBurstUvs cell for
/// frame/3.
void mothDrawBurst(Task* arg0)
{
    ActorQuadScratch* sc;
    MothWork*         work;
    TmdObject*        obj;
    GfxCoord*         coord;
    s32               size, x, y;
    s16               i;
    SVECTOR*          v;
    POLY_FT4*         prim;
    ActorSpriteUv*    uv;
    obj         = arg0->extra.tmd;
    sc          = (ActorQuadScratch*)SCRATCH_STACK_RESERVE_BYTES(0x28);
    coord       = obj->coords;
    work        = arg0->work;
    sc->v[0].vx = coord->workm.t[0];
    sc->v[0].vy = coord->workm.t[1];
    sc->v[0].vz = coord->workm.t[2];
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_ldv0(&sc->v[0]);
    gte_rtps();
    gte_stsxy(&sc->sxy);
    gte_stszotz(&sc->otz);
    if (sc->otz < 20) {
        SCRATCH_STACK_RELEASE_BYTES(0x28);
        return;
    }
    if (work->timer == 1) {
        sc->v[0].vx = 0;
        sc->v[0].vy = 0;
        sc->v[0].vz = ((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xFFF;
        RotMatrix(&sc->v[0], &work->burstRollMtx);
    }
    size        = 0x7800 / sc->otz;
    x           = sc->sxy & 0xFFFF;
    y           = sc->sxy >> 16;
    sc->v[0].vx = -size;
    sc->v[0].vy = -size;
    sc->v[0].vz = 0;
    sc->v[1].vx = size;
    sc->v[1].vy = -size;
    sc->v[1].vz = 0;
    sc->v[2].vx = -size;
    sc->v[2].vy = size;
    sc->v[2].vz = 0;
    sc->v[3].vx = size;
    sc->v[3].vy = size;
    sc->v[3].vz = 0;
    for (i = 0; i < 4; i++) {
        gte_SetRotMatrix(&work->burstRollMtx);
        v = &sc->v[i];
        gte_ldv0(v);
        gte_rtv0();
        gte_stsv(v);
        v->vx += x;
        v->vy += y;
    }
    prim           = gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setlen(prim, 9);
    setcode(prim, 0x2E);
    setRGB0(prim, 0x80, 0x80, 0x80);
    setShadeTex(prim, 1);
    prim->tpage = (((obj->texturePageOffset * 64 + 0x180) & 0x3FF) >> 6) | 0xD0;
    prim->clut  = (obj->clutRowOffset << 6) + 0x3D40;
    uv          = &gMothBurstUvs[(s16)(work->timer / 3)];
    prim->u0    = uv->u;
    prim->v0    = uv->v;
    prim->u1    = uv->u + 31;
    prim->v1    = uv->v;
    prim->u2    = uv->u;
    prim->v2    = uv->v + 31;
    prim->u3    = uv->u + 31;
    prim->v3    = uv->v + 31;
    prim->x0    = sc->v[0].vx;
    prim->y0    = sc->v[0].vy;
    prim->x1    = sc->v[1].vx;
    prim->y1    = sc->v[1].vy;
    prim->x2    = sc->v[2].vx;
    prim->y2    = sc->v[2].vy;
    prim->x3    = sc->v[3].vx;
    prim->y3    = sc->v[3].vy;
    addPrim((&gGpuCurrentOt[(((((u32)sc->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*gGpuCurrentOt)]), prim);
    SCRATCH_STACK_RELEASE_BYTES(0x28);
}
