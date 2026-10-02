/* Part of the water effects library; see water_effects.h. */

/// Draws a spinning sprite at the coordinate's world position, projected
/// through `GsWSMATRIX`. If the projection is valid, one semi-transparent
/// `POLY_FT4` (tpage 0x2B, clut 0x43D3) is queued, its texture the 32-texel
/// column `arg1` of the strip at v 0xE0..0xFF. Its corners sit at
/// `(s16)arg2 * 31 / otz` from the projected point, rotated by the angle
/// `arg3`. The work block lives on the scratchpad stack.
void waterDrawSpinU16(GfxCoord* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    void**              scratch;
    EffectShapeScratch* head;
    EffectShapeScratch* block;
    POLY_FT4*           prim;
    SVECTOR*            vec;
    s32                 u0;
    s32                 ang;
    s32                 ang2;
    u16                 vz;

    scratch = SCRATCH_STACK_CURSOR_SLOT;
    TOUCH_REG_USE(arg2, scratch);
    head                      = *scratch;
    (head - 1)->worldPoint.vx = (u16)arg0->workm.t[0];
    block                     = head - 1;
    block->worldPoint.vy      = (u16)arg0->workm.t[1];
    vz                        = (u16)arg0->workm.t[2];
    *scratch                  = block;
    block->worldPoint.vz      = vz;
    vec                       = &block->worldPoint;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&(head - 1)->screenX);
    gte_stflg(&(head - 1)->projectionFlags);
    if (block->projectionFlags >= 0) {
        gte_stszotz(&(head - 1)->depth);
        prim           = gGpuPrimCursor;
        ang            = (s16)arg3;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x2B;
        prim->clut  = 0x43D3;
        u0          = (arg1 & 0xFFFF) << 5;
        setUV4(prim, u0, 0xE0, u0 + 0x1F, 0xE0, u0, 0xFF, u0 + 0x1F, 0xFF);
        block->extent.corner.x = ((((s16)arg2 * 31) / block->depth) * rsin(ang)) >> 12;
        block->extent.corner.y = ((((s16)arg2 * 31) / block->depth) * rcos(ang)) >> 12;
        prim->x0               = block->screenX + (u16)block->extent.corner.x;
        prim->x3               = block->screenX - (u16)block->extent.corner.x;
        prim->y0               = block->screenY - (u16)block->extent.corner.y;
        prim->y3               = block->screenY + (u16)block->extent.corner.y;
        ang2                   = ang + 0x400;
        block->extent.corner.x = ((((s16)arg2 * 31) / block->depth) * rsin(ang2)) >> 12;
        block->extent.corner.y = ((((s16)arg2 * 31) / block->depth) * rcos(ang2)) >> 12;
        prim->x1               = block->screenX + (u16)block->extent.corner.x;
        prim->x2               = block->screenX - (u16)block->extent.corner.x;
        prim->y1               = block->screenY - (u16)block->extent.corner.y;
        prim->y2               = block->screenY + (u16)block->extent.corner.y;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_POP_AT(scratch, EffectShapeScratch);
}
