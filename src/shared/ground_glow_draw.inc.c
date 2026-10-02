/* Part of the ground glow library; see ground_glow.h. */

/// `u` is latched before each pair of stores on purpose: writing the `POLY_FT4`
/// byte straight from the expression lets GCC fold the store's truncation back
/// into the `gDisplayState.animFrame` load and the `+ 0xC0` / `+ 0xDF`, which the
/// ROM does not do.
static void groundGlowDraw(GfxCoord* ground, s32 size)
{
    OverlayGroundScratch* sc;
    POLY_FT4*             prim;
    s32                   i;
    s32                   otz;
    s32                   flag;
    s32                   u;

    sc = SCRATCH_STACK_RESERVE_BLOCK(OverlayGroundScratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    for (i = 0; i < ARRAY_SIZE(D_80111E38); i++) {
        sc->vec[i].vx = (u16)D_80111E38[i].axis0Sign * size;
        sc->vec[i].vy = 0;
        sc->vec[i].vz = (u16)D_80111E38[i].axis1Sign * size;
        gte_SetRotMatrix(&gGfxViewCoord.workm);
        gte_ldv0(&sc->vec[i]);
        gte_rtv0();
        gte_stsv(&sc->vec[i]);
        sc->vec[i].vx += ground->workm.t[0];
        sc->vec[i].vy += ground->workm.t[1];
        sc->vec[i].vz += ground->workm.t[2];
    }

    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&sc->vec[0]);
    gte_rtps();
    gte_stsxy(&sc->sxy0);
    gte_ldv3(&sc->vec[1], &sc->vec[2], &sc->vec[3]);
    gte_rtpt();
    gte_stsxy3(&sc->sxy1, &sc->sxy2, &sc->sxy3);
    gte_stflg(&flag);
    if (flag >= 0) {
        gte_stszotz(&otz);
        otz++;
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2E);
        prim->r0    = GROUND_GLOW_R;
        prim->g0    = GROUND_GLOW_G;
        prim->b0    = GROUND_GLOW_B;
        prim->tpage = 0x28;
        prim->clut  = GROUND_GLOW_CLUT;
        u           = ((gDisplayState.animFrame & 1) << 5) + 0xC0;
        prim->v0    = 0x38;
        prim->u0    = u;
        u           = ((gDisplayState.animFrame & 1) << 5) + 0xDF;
        prim->v1    = 0x38;
        prim->u1    = u;
        u           = ((gDisplayState.animFrame & 1) << 5) + 0xC0;
        prim->v2    = 0x57;
        prim->u2    = u;
        u           = ((gDisplayState.animFrame & 1) << 5) + 0xDF;
        prim->v3    = 0x57;
        prim->u3    = u;
        prim->x0    = sc->sxy0.vx;
        prim->y0    = sc->sxy0.vy;
        prim->x1    = sc->sxy1.vx;
        prim->y1    = sc->sxy1.vy;
        prim->x2    = sc->sxy2.vx;
        prim->y2    = sc->sxy2.vy;
        prim->x3    = sc->sxy3.vx;
        prim->y3    = sc->sxy3.vy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), prim);
    }
    SCRATCH_STACK_RELEASE_BLOCK(OverlayGroundScratch);
}

#undef GROUND_GLOW_R
#undef GROUND_GLOW_G
#undef GROUND_GLOW_B
#undef GROUND_GLOW_CLUT
