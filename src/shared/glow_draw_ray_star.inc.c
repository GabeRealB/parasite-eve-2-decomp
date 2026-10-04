/* Part of the glow drawing library; see glow_draw.h. */

/// Draws a pulsing star at `point` in `coord`'s space, projected through
/// `GsWSMATRIX`; nothing is drawn when its `otz` is 16 or less. Around the
/// projected centre it lays a fan of additive gouraud wedges of radius
/// `outerRadius` (`(s16)arg3 * 64 / otz`), each paired with a brighter one of
/// half that radius, then four rays whose shoulders sit at `innerRadius` and
/// whose tips reach `outerRadius` and twice it.
/// The centre's intensity `color` is `rsin(animFrame * rate) / 34 + 0x78`,
/// `half` half of it. The including unit tints the three layers:
///   GLOW_DRAW_RAY_STAR_OUTER(prim, color, half)  the wide wedges
///   GLOW_DRAW_RAY_STAR_INNER(prim, color, half)  the narrow wedges
///   GLOW_DRAW_RAY_STAR_RAY(prim, color)          the rays (`color` is halved)
/// each a `setRGB2` of the centre vertex. GLOW_DRAW_RAY_STAR_RAY_HALFWORD 1
/// reads the rays' level back as a `u16` (the Breezeway);
/// GLOW_DRAW_RAY_STAR_HALF_FIRST 1 halves the intensity once, before the fan
/// (the Trailer Coach).
void glowDrawRayStar(GfxCoord* coord, SVECTOR* point, s32 rate, s32 arg3)
{
    RoomGlowRadiiScratch* block;
    POLY_G4*              prim;
    s32                   pulse;
    s32                   color;
    s32                   half;
    s32                   size;
    s32                   ang;
    s32                   t;
    s32                   t2;
    s32                   u;
#if GLOW_DRAW_RAY_STAR_HALF_FIRST
    s32 work;
#endif

    actorRenderComposeCoord(coord);
    block = SCRATCH_STACK_RESERVE_BLOCK(RoomGlowRadiiScratch);

    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(point);
    gte_rtv0();
    gte_stsv(&block->worldPos);
    block->worldPos.vx += coord->workm.t[0];
    block->worldPos.vy += coord->workm.t[1];
    block->worldPos.vz += coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->worldPos);
    gte_rtps();
    gte_stsxy(&block->screenPos);
    gte_stszotz(&block->otz);
    if (block->otz > 16) {
        pulse              = rsin(gDisplayState.animFrame * (s16)rate);
        ang                = 0;
        size               = (s16)arg3;
        block->outerRadius = (size * 64) / block->otz;
#if GLOW_DRAW_RAY_STAR_HALF_FIRST
        /* `work` carries the intensity and later the scratch-head address;
           sharing it keeps the halving shift on the intensity's own register */
        work   = pulse / 34 + 0x78;
        color  = work;
        work <<= 16;
        half   = work >> 17;
#else
        color = pulse / 34 + 0x78;
#endif
        block->innerRadius = (size * 8) / block->otz;
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
#if !GLOW_DRAW_RAY_STAR_HALF_FIRST
            half = (s16)color >> 1;
#endif
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            GLOW_DRAW_RAY_STAR_OUTER(prim, color, half);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->screenPos.vx + ((block->outerRadius * rsin(ang)) >> 12);
            t        = ang + 0x100;
            prim->y0 = block->screenPos.vy + ((block->outerRadius * rcos(ang)) >> 12);
            prim->x1 = block->screenPos.vx + ((block->outerRadius * rsin(t)) >> 12);
            prim->y1 = block->screenPos.vy + ((block->outerRadius * rcos(t)) >> 12);
            t2       = ang + 0x200;
            prim->x2 = block->screenPos.vx;
            prim->y2 = block->screenPos.vy;
            prim->x3 = block->screenPos.vx + ((block->outerRadius * rsin(t2)) >> 12);
            prim->y3 = block->screenPos.vy + ((block->outerRadius * rcos(t2)) >> 12);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->otz);

            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            GLOW_DRAW_RAY_STAR_INNER(prim, color, half);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->screenPos.vx + ((block->outerRadius * rsin(ang)) >> 13);
            prim->y0 = block->screenPos.vy + ((block->outerRadius * rcos(ang)) >> 13);
            prim->x1 = block->screenPos.vx + ((block->outerRadius * rsin(t)) >> 13);
            prim->y1 = block->screenPos.vy + ((block->outerRadius * rcos(t)) >> 13);
            prim->x2 = block->screenPos.vx;
            prim->y2 = block->screenPos.vy;
            prim->x3 = block->screenPos.vx + ((block->outerRadius * rsin(t2)) >> 13);
            prim->y3 = block->screenPos.vy + ((block->outerRadius * rcos(t2)) >> 13);
            ang      = t2;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->otz);
        } while (ang < 0x1000);

#if GLOW_DRAW_RAY_STAR_HALF_FIRST
        color = (s16)color >> 1;
#elif GLOW_DRAW_RAY_STAR_RAY_HALFWORD
        /* this build reloads the rays' level as an unsigned halfword */
        color = (u16)half;
#else
        color = half;
#endif
        ang = 0x200;
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            GLOW_DRAW_RAY_STAR_RAY(prim, color);
            setRGB3(prim, 0, 0, 0);
            u        = ang - 0x400;
            prim->x0 = block->screenPos.vx + ((block->innerRadius * rsin(u)) >> 13);
            prim->y0 = block->screenPos.vy + ((block->innerRadius * rcos(u)) >> 13);
            prim->x1 = block->screenPos.vx + ((block->outerRadius * rsin(ang)) >> 12);
            prim->y1 = block->screenPos.vy + ((block->outerRadius * rcos(ang)) >> 12);
            u        = ang + 0x400;
            prim->x2 = block->screenPos.vx;
            prim->y2 = block->screenPos.vy;
            prim->x3 = block->screenPos.vx + ((block->innerRadius * rsin(u)) >> 13);
            prim->y3 = block->screenPos.vy + ((block->innerRadius * rcos(u)) >> 13);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->otz);

            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            GLOW_DRAW_RAY_STAR_RAY(prim, color);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->screenPos.vx + ((block->innerRadius * rsin(ang)) >> 12);
            prim->y0 = block->screenPos.vy + ((block->innerRadius * rcos(ang)) >> 12);
            prim->x1 = block->screenPos.vx + ((block->outerRadius * rsin(u)) >> 11);
            prim->y1 = block->screenPos.vy + ((block->outerRadius * rcos(u)) >> 11);
            u        = ang + 0x800;
            prim->x2 = block->screenPos.vx;
            prim->y2 = block->screenPos.vy;
            prim->x3 = block->screenPos.vx + ((block->innerRadius * rsin(u)) >> 12);
            prim->y3 = block->screenPos.vy + ((block->innerRadius * rcos(u)) >> 12);
            ang      = u;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->otz);
        } while (ang < 0x1000);
    }
#if GLOW_DRAW_RAY_STAR_HALF_FIRST
    work = (s32)SCRATCH_STACK_CURSOR_SLOT;
    SCRATCH_POP_BYTES_AT(work, sizeof(RoomGlowRadiiScratch));
#else
    SCRATCH_STACK_RELEASE_BLOCK(RoomGlowRadiiScratch);
#endif
}

#undef GLOW_DRAW_RAY_STAR_OUTER
#undef GLOW_DRAW_RAY_STAR_RAY_HALFWORD
#undef GLOW_DRAW_RAY_STAR_HALF_FIRST
#undef GLOW_DRAW_RAY_STAR_INNER
#undef GLOW_DRAW_RAY_STAR_RAY
