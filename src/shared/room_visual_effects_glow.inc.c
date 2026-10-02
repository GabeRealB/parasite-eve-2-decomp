/* Continue room_visual_effects.inc.c after the preceding overlay wrappers. */

/// Queues a gouraud star around the projected world position of `arg0`,
/// shaded `arg2` at the centre and black at the tips: a disc of radius `arg1`
/// scaled by depth, at half brightness, with a full-brightness disc of half the
/// radius over it, plus four half-brightness spikes, two of them reaching twice
/// the disc's radius. Nothing is drawn when the projection overflows.
static void RoomFx_DrawBurstStar(GfxCoord* arg0, s16 arg1, u8* arg2)
{
    RoomFxRadialScratch* block;
    POLY_G4*             prim;
    s32                  ang;
    s32                  t;
    s32                  t2;
    s32                  u;

    block                = SCRATCH_STACK_RESERVE_BLOCK(RoomFxRadialScratch);
    block->worldPoint.vx = arg0->workm.t[0];
    block->worldPoint.vy = arg0->workm.t[1];
    block->worldPoint.vz = arg0->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->worldPoint);
    gte_rtps();
    gte_stsxy(&block->screenX);
    gte_stflg(&block->projectionFlags);
    if (block->projectionFlags >= 0) {
        gte_stszotz(&block->depth);
        block->depth++;
        block->radii.star.disc  = (arg1 * 64) / block->depth;
        block->radii.star.spike = (arg1 * 8) / block->depth;

        ang = 0;
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->screenX + ((block->radii.star.disc * rsin(ang)) >> 12);
            t        = ang + 0x100;
            prim->y0 = block->screenY + ((block->radii.star.disc * rcos(ang)) >> 12);
            prim->x1 = block->screenX + ((block->radii.star.disc * rsin(t)) >> 12);
            prim->y1 = block->screenY + ((block->radii.star.disc * rcos(t)) >> 12);
            t2       = ang + 0x200;
            prim->x2 = block->screenX;
            prim->y2 = block->screenY;
            prim->x3 = block->screenX + ((block->radii.star.disc * rsin(t2)) >> 12);
            prim->y3 = block->screenY + ((block->radii.star.disc * rcos(t2)) >> 12);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->depth);

            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0], arg2[1], arg2[2]);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->screenX + ((block->radii.star.disc * rsin(ang)) >> 13);
            prim->y0 = block->screenY + ((block->radii.star.disc * rcos(ang)) >> 13);
            prim->x1 = block->screenX + ((block->radii.star.disc * rsin(t)) >> 13);
            prim->y1 = block->screenY + ((block->radii.star.disc * rcos(t)) >> 13);
            prim->x2 = block->screenX;
            prim->y2 = block->screenY;
            prim->x3 = block->screenX + ((block->radii.star.disc * rsin(t2)) >> 13);
            prim->y3 = block->screenY + ((block->radii.star.disc * rcos(t2)) >> 13);
            ang      = t2;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->depth);
        } while (ang < 0x1000);

        ang = 0x200;
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
            setRGB3(prim, 0, 0, 0);
            u        = ang - 0x400;
            prim->x0 = block->screenX + ((block->radii.star.spike * rsin(u)) >> 13);
            prim->y0 = block->screenY + ((block->radii.star.spike * rcos(u)) >> 13);
            prim->x1 = block->screenX + ((block->radii.star.disc * rsin(ang)) >> 12);
            prim->y1 = block->screenY + ((block->radii.star.disc * rcos(ang)) >> 12);
            u        = ang + 0x400;
            prim->x2 = block->screenX;
            prim->y2 = block->screenY;
            prim->x3 = block->screenX + ((block->radii.star.spike * rsin(u)) >> 13);
            prim->y3 = block->screenY + ((block->radii.star.spike * rcos(u)) >> 13);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->depth);

            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->screenX + ((block->radii.star.spike * rsin(ang)) >> 12);
            prim->y0 = block->screenY + ((block->radii.star.spike * rcos(ang)) >> 12);
            prim->x1 = block->screenX + ((block->radii.star.disc * rsin(u)) >> 11);
            prim->y1 = block->screenY + ((block->radii.star.disc * rcos(u)) >> 11);
            u        = ang + 0x800;
            prim->x2 = block->screenX;
            prim->y2 = block->screenY;
            prim->x3 = block->screenX + ((block->radii.star.spike * rsin(u)) >> 12);
            prim->y3 = block->screenY + ((block->radii.star.spike * rcos(u)) >> 12);
            ang      = u;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->depth);
        } while (ang < 0x1000);
    }
    SCRATCH_STACK_RELEASE_BLOCK(RoomFxRadialScratch);
}
