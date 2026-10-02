/* Continue room_visual_effects.inc.c after the preceding overlay wrappers. */

/// Queues a gouraud ring of sixteen quads around the projected world position
/// of `arg0`: black at radius `arg1` and shaded `rgb` at radius `arg1 + arg2`,
/// both scaled by depth. Nothing is drawn when the projection overflows. The
/// same drawing as `RoomFx_DrawHaloRing`, with
/// its scratch block laid out differently.
static void RoomFx_DrawFlashRing(GfxCoord* arg0, s32 arg1, s32 arg2, u8* rgb)
{
    RoomFxFlashRingScratch* block;
    POLY_G4*                prim;
    s32                     ang;
    s32                     t;
    s16                     blackRadius = arg1;
    s16                     tintRadius  = arg1 + arg2;

    block                = SCRATCH_STACK_RESERVE_BLOCK(RoomFxFlashRingScratch);
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
        block->radii.black = (blackRadius * 64) / block->depth;
        block->radii.tint  = (tintRadius * 64) / block->depth;

        // One quad per sixteenth of a turn, joining the black edge to the tinted edge.
        ang = 0;
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
            setRGB3(prim, rgb[0], rgb[1], rgb[2]);
            prim->x0 = block->screenX + ((block->radii.black * rsin(ang)) >> 12);
            prim->y0 = block->screenY + ((block->radii.black * rcos(ang)) >> 12);
            t        = ang + 0x100;
            prim->x1 = block->screenX + ((block->radii.black * rsin(t)) >> 12);
            prim->y1 = block->screenY + ((block->radii.black * rcos(t)) >> 12);
            prim->x2 = block->screenX + ((block->radii.tint * rsin(ang)) >> 12);
            prim->y2 = block->screenY + ((block->radii.tint * rcos(ang)) >> 12);
            prim->x3 = block->screenX + ((block->radii.tint * rsin(t)) >> 12);
            prim->y3 = block->screenY + ((block->radii.tint * rcos(t)) >> 12);
            ang      = t;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->depth);
        } while (ang < 0x1000);
    }
    SCRATCH_STACK_RELEASE_BLOCK(RoomFxFlashRingScratch);
}

/// Queues a gouraud disc of eight wedges around the projected world position
/// of `arg0`, shaded `rgb` at the centre and black at the rim, of radius
/// `arg1` scaled by depth. Nothing is drawn when the projection overflows.
static void RoomFx_DrawFlashDisc(GfxCoord* arg0, s16 arg1, u8* rgb)
{
    RoomFxFanScratch* block;
    POLY_G4*          prim;
    s32               ang;
    s32               depth;

    block                = SCRATCH_STACK_RESERVE_BLOCK(RoomFxFanScratch);
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
        depth         = block->depth + 1;
        block->depth  = depth;
        block->radius = (arg1 * 64) / depth;

        for (ang = 0; ang < 0x1000; ang += 0x200) {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->screenX + ((block->radius * rsin(ang)) >> 12);
            prim->y0 = block->screenY + ((block->radius * rcos(ang)) >> 12);
            prim->x1 = block->screenX + ((block->radius * rsin(ang + 0x100)) >> 12);
            prim->y1 = block->screenY + ((block->radius * rcos(ang + 0x100)) >> 12);
            prim->x2 = block->screenX;
            prim->y2 = block->screenY;
            prim->x3 = block->screenX + ((block->radius * rsin(ang + 0x200)) >> 12);
            prim->y3 = block->screenY + ((block->radius * rcos(ang + 0x200)) >> 12);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->depth);
        }
    }
    SCRATCH_STACK_RELEASE_BYTES(sizeof(RoomFxFanScratch));
}
