/* Part of the glow drawing library; see glow_draw.h. */

/// Draws a light shaft between the two world points `arg0[0]` and `arg0[1]`:
/// a fan of gouraud wedges around each projected point, joined by wedges
/// spanning the two, the sweep oriented along the screen-space line between
/// them. Each radius is `(s16)arg1 * 64` over that point's OTZ. Nothing is
/// drawn unless both points project. The lit vertices take a brightness that
/// flickers with the frame counter.
void glowDrawShaft(SVECTOR* arg0, s32 arg1)
{
    SVECTOR*                 p1;
    OverlayPointPairScratch* block;
    POLY_G4*                 prim;
    DisplayState*            ds;
    s32                      raw;
    s32                      ang;
    s32                      angEnd;
    s32                      limit;
    s32                      angStart;
    s32                      t;
    s32                      t2;
    s32                      t3;
    s32                      conn;
    s32                      scaled;
    s32                      blend;

    p1 = arg0 + 1;
    SCRATCH_STACK_RESERVE_BLOCK(OverlayPointPairScratch);
    block = SCRATCH_STACK_CURSOR(OverlayPointPairScratch);

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&block->sx0);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz0);
        gte_ldv0(p1);
        gte_rtps();
        gte_stsxy(&block->sx1);
        gte_stflg(&block->flag);
        if (block->flag >= 0) {
            gte_stszotz(&block->otz1);
            scaled         = (s16)arg1 * 64;
            block->radius0 = scaled / block->otz0;
            block->radius1 = scaled / block->otz1;
            raw            = ratan2((s16)block->sy1 - (s16)block->sy0, (s16)block->sx0 - (s16)block->sx1);
            ds             = &gDisplayState;
            ang            = (s16)raw;
            blend          = (((u8)ds->animFrame & 1) * 0x10) | 0x20;
            angEnd         = ang + 0x800;
            if (ang < angEnd) {
                angStart = ang;
                limit    = angEnd;
                do {
                    prim           = gGpuPrimCursor;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, blend, blend, blend);
                    setRGB3(prim, 0, 0, 0);
                    prim->x0 = block->sx0 + ((block->radius0 * rsin(ang)) >> 12);
                    t        = ang + 0x200;
                    prim->y0 = block->sy0 + ((block->radius0 * rcos(ang)) >> 12);
                    prim->x1 = block->sx0 + ((block->radius0 * rsin(t)) >> 12);
                    prim->y1 = block->sy0 + ((block->radius0 * rcos(t)) >> 12);
                    t2       = ang + 0x400;
                    prim->x2 = block->sx0;
                    prim->y2 = block->sy0;
                    prim->x3 = block->sx0 + ((block->radius0 * rsin(t2)) >> 12);
                    prim->y3 = block->sy0 + ((block->radius0 * rcos(t2)) >> 12);
                    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                            prim);
                    gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->otz0);

                    conn           = angStart + ((ang - angStart) * 2);
                    prim           = gGpuPrimCursor;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, blend, blend, blend);
                    setRGB3(prim, blend, blend, blend);
                    prim->x0 = block->sx0 + ((block->radius0 * rsin(conn)) >> 12);
                    prim->y0 = block->sy0 + ((block->radius0 * rcos(conn)) >> 12);
                    prim->x1 = block->sx1 + ((block->radius1 * rsin(conn)) >> 12);
                    prim->y1 = block->sy1 + ((block->radius1 * rcos(conn)) >> 12);
                    prim->x2 = block->sx0;
                    prim->y2 = block->sy0;
                    prim->x3 = block->sx1;
                    prim->y3 = block->sy1;
                    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)((block->otz1 + block->otz0) / 2) << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                            prim);
                    gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, (block->otz1 + block->otz0) / 2);

                    prim           = gGpuPrimCursor;
                    t3             = ang + 0x800;
                    t              = t3;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, blend, blend, blend);
                    setRGB3(prim, 0, 0, 0);
                    prim->x0 = block->sx1 + ((block->radius1 * rsin(t)) >> 12);
                    prim->y0 = block->sy1 + ((block->radius1 * rcos(t)) >> 12);
                    t        = ang + 0xA00;
                    prim->x1 = block->sx1 + ((block->radius1 * rsin(t)) >> 12);
                    prim->y1 = block->sy1 + ((block->radius1 * rcos(t)) >> 12);
                    t        = ang + 0xC00;
                    prim->x2 = block->sx1;
                    prim->y2 = block->sy1;
                    prim->x3 = block->sx1 + ((block->radius1 * rsin(t)) >> 12);
                    prim->y3 = block->sy1 + ((block->radius1 * rcos(t)) >> 12);
                    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                            prim);
                    gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->otz1);
                    ang = t2;
                } while (ang < limit);
            }
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(OverlayPointPairScratch);
}
