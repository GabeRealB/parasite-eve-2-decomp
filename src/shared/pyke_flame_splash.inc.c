/* Part of the Pyke flame library; see pyke_flame.h. */

#ifndef PYKE_FLAME_SPLASH_DEAD_BIAS
#define PYKE_FLAME_SPLASH_DEAD_BIAS 0
#endif

/// Draws the flame's ground splash: the unit quad `D_80111E38` scaled to
/// `width` half-size, laid flat by `gGfxViewCoord.workm` and moved to the traced
/// ground point `pos`, then projected through `GsWSMATRIX` into a
/// `PykeFlameSplashScratch`. The first corner goes through `rtps` and the other
/// three through one `rtpt`; a negative `gte_stflg` drops the quad.
static void pykeFlameDrawSplash(VECTOR3* pos, s32 width)
{
    u8*                     head;
    PykeFlameSplashScratch* block;
    POLY_FT4*               prim;
    EffectUnitQuadCorner*   corners;
    s32                     i;
    s32                     flag;
    s32                     otz;

    head = SCRATCH_STACK_CURSOR(u8) - sizeof(PykeFlameSplashScratch);
    /* The ROM stores the freshly computed head and keeps a *copy* of it in the
       register the rest of the function walks; without the barrier GCC folds
       the two together and stores the copy instead. */
    SCRATCH_STACK_CURSOR(u8) = head;
    block                    = (PykeFlameSplashScratch*)head;
    gte_SetTransMatrix(&GsWSMATRIX);
    i       = 0;
    corners = D_80111E38;
    do {
        block->vertices[i].vx = (u16)corners[i].axis0Sign * width;
        block->vertices[i].vy = 0;
        block->vertices[i].vz = (u16)corners[i].axis1Sign * width;
        gte_SetRotMatrix(&gGfxViewCoord.workm);
        gte_ldv0(&block->vertices[i]);
        gte_rtv0();
        gte_stsv(&block->vertices[i]);
        (u16) block->vertices[i].vx = (u16)block->vertices[i].vx + (u16)pos->vx;
        (u16) block->vertices[i].vy = (u16)block->vertices[i].vy + (u16)pos->vy;
        (u16) block->vertices[i].vz = (u16)block->vertices[i].vz + (u16)pos->vz;
        i++;
    } while (i < ARRAY_SIZE(D_80111E38));

    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vertices[0]);
    gte_rtps();
    gte_stsxy(&block->screenCorners[0]);
    gte_ldv3(&block->vertices[1], &block->vertices[2], &block->vertices[3]);
    gte_rtpt();
    gte_stsxy3(&block->screenCorners[1], &block->screenCorners[2], &block->screenCorners[3]);
    gte_stflg(&flag);
    if (flag >= 0) {
#if PYKE_FLAME_SPLASH_DEAD_BIAS
        /* Dead: `otz` is bumped before it is read back, so the increment lands
           on garbage and `gte_stszotz` immediately overwrites it. It still
           costs a `lw`/`addiu`/`sw` because the address escapes into the asm. */
        otz++;
        gte_stszotz(&otz);
#else
        gte_stszotz(&otz);
        otz++;
#endif
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2E);
        setRGB0(prim, 0x40, 0x40, 0x40);
        prim->tpage = 0x29;
        prim->clut  = 0x430F;
        setUV4(prim, 0xE0, 0xC8, 0xFF, 0xC8, 0xE0, 0xE7, 0xFF, 0xE7);
        prim->x0 = block->screenCorners[0].vx;
        prim->y0 = block->screenCorners[0].vy;
        prim->x1 = block->screenCorners[1].vx;
        prim->y1 = block->screenCorners[1].vy;
        prim->x2 = block->screenCorners[2].vx;
        prim->y2 = block->screenCorners[2].vy;
        prim->x3 = block->screenCorners[3].vx;
        prim->y3 = block->screenCorners[3].vy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BYTES(sizeof(PykeFlameSplashScratch));
}

#undef PYKE_FLAME_SPLASH_DEAD_BIAS
