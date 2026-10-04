/* Part of the glow drawing library; see glow_draw.h. */

/// Draws a pulsing light shaft at a point in `arg0`'s space. `arg1` is rotated
/// by the coordinate's `workm` and offset by its translation, then projected
/// through `GsWSMATRIX` into a `RoomGlowSpriteScratch` block; nothing is
/// drawn when `otz` is 0x10 or less. Two gouraud `POLY_G4` halves of half width
/// `(s16)arg3 * 32 / otz` and two `LINE_G3` diagonals meet at the projected
/// point, whose vertex pulses cyan as `rsin(animFrame * arg2) / 34 + 0x78`.
void glowDrawStarLocal(GfxCoord* arg0, SVECTOR* arg1, s32 arg2, s32 arg3)
{
    RoomGlowSpriteScratch* block;
    POLY_G4*               prim;
    LINE_G3*               line;
    s32                    i;
    s32                    color;
    s32                    pulse;
    s32                    twice;
    s32                    t;
    s32                    t2;

    actorRenderComposeCoord(arg0);
    block = SCRATCH_STACK_RESERVE_BLOCK(RoomGlowSpriteScratch);

    gte_SetRotMatrix(&arg0->workm);
    gte_ldv0(arg1);
    gte_rtv0();
    gte_stsv(&block->worldPos);
    block->worldPos.vx += arg0->workm.t[0];
    block->worldPos.vy += arg0->workm.t[1];
    block->worldPos.vz += arg0->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->worldPos);
    gte_rtps();
    gte_stsxy(&block->screenPos);
    gte_stszotz(&block->otz);
    if (block->otz >= 0x11) {
        pulse             = rsin(gDisplayState.animFrame * (s16)arg2);
        i                 = 0;
        block->halfExtent = ((s16)arg3 << 5) / block->otz;
        color             = pulse / 34 + 0x78;
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, 0, color, color);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->screenPos.vx - block->halfExtent;
            prim->x1 = prim->x2 = block->screenPos.vx;
            prim->x3            = block->screenPos.vx + block->halfExtent;
            prim->y0 = prim->y2 = prim->y3 = block->screenPos.vy;
            twice                          = i << 1;
            prim->y1                       = (block->screenPos.vy - block->halfExtent) + block->halfExtent * twice;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->otz);
            i++;
        } while (i < 2);

        i = 0;
        do {
            line           = gGpuPrimCursor;
            gGpuPrimCursor = line + 1;
            setLineG3(line);
            setRGB0(line, 0, 0, 0);
            setRGB1(line, 0, color, color);
            setRGB2(line, 0, 0, 0);
            t        = i * 3 - 1;
            t2       = i + 1;
            line->x0 = block->screenPos.vx + (block->halfExtent * t);
            line->y0 = block->screenPos.vy - (block->halfExtent * t2);
            line->x1 = block->screenPos.vx;
            line->y1 = block->screenPos.vy;
            line->x2 = block->screenPos.vx - (block->halfExtent * t);
            line->y2 = block->screenPos.vy + (block->halfExtent * t2);
            addPrim(((u_long*)((((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) + (uintptr)gGpuCurrentOt)),
                    line);
            gpuSetPrimitiveBlendMode(line, GPU_BLEND_ADD, block->otz);
            i = t2;
        } while (i < 2);
    }
    SCRATCH_STACK_RELEASE_BLOCK(RoomGlowSpriteScratch);
}
