/* Part of the glow drawing library; see glow_draw.h. */

/// Projects the world-space point `arg0` through `gGfxViewCoord.workm` and, when
/// the GTE flag is non-negative, queues a sixteen-wedge gouraud disc plus two
/// inner cross wedges around the projected centre. `arg2` is a signed
/// half-extent; on-screen radii are `(s16)arg2 * 64 / otz` (outer) and
/// `(s16)arg2 * 8 / otz` (inner). `arg1` scales `gDisplayState.animFrame` into
/// `rsin` so the lit vertex pulses as `rsin(...) / 34 + 0x78` on green and
/// blue.
void glowDrawPulsingDisc(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    GlowCentreRadiiScratch* block;
    POLY_G4*                prim;
    s32                     pulse;
    s32                     color;
    s32                     half;
    s32                     size;
    s32                     ang;
    s32                     t;
    s32                     t2;
    s32                     u;

    block = SCRATCH_STACK_RESERVE_BLOCK(GlowCentreRadiiScratch);

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        pulse              = rsin(gDisplayState.animFrame * (s16)arg1);
        ang                = 0;
        size               = (s16)arg2;
        block->outerRadius = (size * 64) / block->otz;
        color              = pulse / 34 + 0x78;
        block->innerRadius = (size * 8) / block->otz;
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            half = (s16)color >> 1;
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, 0, half, half);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->outerRadius * rsin(ang)) >> 12);
            t        = ang + 0x100;
            prim->y0 = block->sy + ((block->outerRadius * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->outerRadius * rsin(t)) >> 12);
            prim->y1 = block->sy + ((block->outerRadius * rcos(t)) >> 12);
            t2       = ang + 0x200;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->outerRadius * rsin(t2)) >> 12);
            prim->y3 = block->sy + ((block->outerRadius * rcos(t2)) >> 12);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->otz);

            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, 0, color, color);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->outerRadius * rsin(ang)) >> 13);
            prim->y0 = block->sy + ((block->outerRadius * rcos(ang)) >> 13);
            prim->x1 = block->sx + ((block->outerRadius * rsin(t)) >> 13);
            prim->y1 = block->sy + ((block->outerRadius * rcos(t)) >> 13);
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->outerRadius * rsin(t2)) >> 13);
            prim->y3 = block->sy + ((block->outerRadius * rcos(t2)) >> 13);
            ang      = t2;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->otz);
        } while (ang < 0x1000);

        color = half;
        ang   = 0x200;
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, 0, color, color);
            setRGB3(prim, 0, 0, 0);
            u        = ang - 0x400;
            prim->x0 = block->sx + ((block->innerRadius * rsin(u)) >> 13);
            prim->y0 = block->sy + ((block->innerRadius * rcos(u)) >> 13);
            prim->x1 = block->sx + ((block->outerRadius * rsin(ang)) >> 12);
            prim->y1 = block->sy + ((block->outerRadius * rcos(ang)) >> 12);
            u        = ang + 0x400;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->innerRadius * rsin(u)) >> 13);
            prim->y3 = block->sy + ((block->innerRadius * rcos(u)) >> 13);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->otz);

            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, 0, color, color);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->innerRadius * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->innerRadius * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->outerRadius * rsin(u)) >> 11);
            prim->y1 = block->sy + ((block->outerRadius * rcos(u)) >> 11);
            u        = ang + 0x800;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->innerRadius * rsin(u)) >> 12);
            prim->y3 = block->sy + ((block->innerRadius * rcos(u)) >> 12);
            ang      = u;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_STACK_RELEASE_BLOCK(GlowCentreRadiiScratch);
}
