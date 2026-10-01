/* Part of the glow drawing library; see glow_draw.h. */

/// Draws a pulsing star at `point` in `coord`'s space, projected through
/// `GsWSMATRIX`; nothing is drawn when its `otz` is 16 or less. Around the
/// projected centre it lays a fan of additive gouraud wedges of radius
/// `rOuter` (`(s16)arg3 * 64 / otz`), each paired with a brighter one of half
/// that radius, then four rays reaching from `rInner` out to twice `rOuter`.
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
    u8*              head;
    RoomGlowScratch* block;
    POLY_G4*         prim;
    s32              pulse;
    s32              color;
    s32              half;
    s32              size;
    s32              ang;
    s32              t;
    s32              t2;
    s32              u;
#if GLOW_DRAW_RAY_STAR_HALF_FIRST
    s32 work;
#endif

    Gp_UpdateCoord(coord);
    {
        void** scratch;
        u8*    tmp;

        scratch = SCRATCH_STACK_CURSOR_SLOT;
        head    = *scratch;
        tmp     = (*scratch = head - 0x18);
        block   = (RoomGlowScratch*)tmp;
    }

    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(point);
    gte_rtv0();
    gte_stsv(&((RoomGlowScratch*)(head - 0x18))->vec);
    block->vec.vx += coord->workm.t[0];
    block->vec.vy += coord->workm.t[1];
    block->vec.vz += coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((RoomGlowScratch*)(head - 0x18))->vec);
    gte_rtps();
    gte_stsxy(&((RoomGlowScratch*)(head - 0x18))->sx);
    gte_stszotz(&block->otz);
    if (((RoomGlowScratch*)(head - 0x18))->otz > 16) {
        pulse         = rsin(gDisplayState.animFrame * (s16)rate);
        ang           = 0;
        size          = (s16)arg3;
        block->rOuter = (size * 64) / ((RoomGlowScratch*)(head - 0x18))->otz;
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
        block->rInner = (size * 8) / ((RoomGlowScratch*)(head - 0x18))->otz;
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
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            t        = ang + 0x100;
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->rOuter * rsin(t)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(t)) >> 12);
            t2       = ang + 0x200;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rOuter * rsin(t2)) >> 12);
            prim->y3 = block->sy + ((block->rOuter * rcos(t2)) >> 12);
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
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 13);
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 13);
            prim->x1 = block->sx + ((block->rOuter * rsin(t)) >> 13);
            prim->y1 = block->sy + ((block->rOuter * rcos(t)) >> 13);
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rOuter * rsin(t2)) >> 13);
            prim->y3 = block->sy + ((block->rOuter * rcos(t2)) >> 13);
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
            prim->x0 = block->sx + ((block->rInner * rsin(u)) >> 13);
            prim->y0 = block->sy + ((block->rInner * rcos(u)) >> 13);
            prim->x1 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            u        = ang + 0x400;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rInner * rsin(u)) >> 13);
            prim->y3 = block->sy + ((block->rInner * rcos(u)) >> 13);
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
            prim->x0 = block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->rOuter * rsin(u)) >> 11);
            prim->y1 = block->sy + ((block->rOuter * rcos(u)) >> 11);
            u        = ang + 0x800;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rInner * rsin(u)) >> 12);
            prim->y3 = block->sy + ((block->rInner * rcos(u)) >> 12);
            ang      = u;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->otz);
        } while (ang < 0x1000);
    }
#if GLOW_DRAW_RAY_STAR_HALF_FIRST
    work = (s32)SCRATCH_STACK_CURSOR_SLOT;
    SCRATCH_POP_BYTES_AT(work, sizeof(RoomGlowScratch));
#else
    SCRATCH_STACK_RELEASE_BYTES(0x18);
#endif
}

#undef GLOW_DRAW_RAY_STAR_OUTER
#undef GLOW_DRAW_RAY_STAR_RAY_HALFWORD
#undef GLOW_DRAW_RAY_STAR_HALF_FIRST
#undef GLOW_DRAW_RAY_STAR_INNER
#undef GLOW_DRAW_RAY_STAR_RAY
