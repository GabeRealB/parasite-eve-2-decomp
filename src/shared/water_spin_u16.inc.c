/* Part of the water effects library; see water_effects.h. */

/// Draws a spinning sprite at the coordinate's world position, projected
/// through `GsWSMATRIX`. If the projection is valid, one semi-transparent
/// `POLY_FT4` (tpage 0x2B, clut 0x43D3) is queued, its texture the 32-texel
/// column `arg1` of the strip at v 0xE0..0xFF. Its corners sit at
/// `(s16)arg2 * 31 / otz` from the projected point, rotated by the angle
/// `arg3`. The work block lives on the scratchpad stack.
void waterDrawSpinU16(GfxCoord* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    void**           scratch;
    u8*              head;
    GpFxQuadScratch* block;
    POLY_FT4*        prim;
    SVECTOR*         vec;
    s32              u0;
    s32              ang;
    s32              ang2;
    u16              vz;

    scratch = SCRATCH_STACK_CURSOR_SLOT;
    TOUCH_REG_USE(arg2, scratch);
    head                                      = *scratch;
    ((GpFxQuadScratch*)(head - 0x1C))->vec.vx = (u16)arg0->workm.t[0];
    block                                     = (GpFxQuadScratch*)(head - 0x1C);
    block->vec.vy                             = (u16)arg0->workm.t[1];
    vz                                        = (u16)arg0->workm.t[2];
    *scratch                                  = block;
    block->vec.vz                             = vz;
    vec                                       = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&((GpFxQuadScratch*)(head - 0x1C))->sx);
    gte_stflg(&((GpFxQuadScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpFxQuadScratch*)(head - 0x1C))->otz);
        prim           = gGpuPrimCursor;
        ang            = (s16)arg3;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x2B;
        prim->clut  = 0x43D3;
        u0          = (arg1 & 0xFFFF) << 5;
        setUV4(prim, u0, 0xE0, u0 + 0x1F, 0xE0, u0, 0xFF, u0 + 0x1F, 0xFF);
        block->dx = ((((s16)arg2 * 31) / block->otz) * rsin(ang)) >> 12;
        block->dy = ((((s16)arg2 * 31) / block->otz) * rcos(ang)) >> 12;
        prim->x0  = block->sx + (u16)block->dx;
        prim->x3  = block->sx - (u16)block->dx;
        prim->y0  = block->sy - (u16)block->dy;
        prim->y3  = block->sy + (u16)block->dy;
        ang2      = ang + 0x400;
        block->dx = ((((s16)arg2 * 31) / block->otz) * rsin(ang2)) >> 12;
        block->dy = ((((s16)arg2 * 31) / block->otz) * rcos(ang2)) >> 12;
        prim->x1  = block->sx + (u16)block->dx;
        prim->x2  = block->sx - (u16)block->dx;
        prim->y1  = block->sy - (u16)block->dy;
        prim->y2  = block->sy + (u16)block->dy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_POP_BYTES_AT(scratch, 0x1C);
}
