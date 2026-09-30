/* Part of the effect sprite library; see effect_sprite.h. */

/// Draws one cell of a 5-column, 48-texel sprite sheet (tpage 0x2B) as a
/// semi-transparent `POLY_FT4` centred on the coordinate's projected position.
/// `arg1`'s low 12 bits are the cell index and its top nibble the palette
/// bank, `arg2` the half-extent (scaled by 47 over depth) and `arg3` the
/// quad's rotation. Nothing is drawn when the projection fails.
void effectSpriteDrawBanked(GfxCoord* arg0, u16 arg1, s16 arg2, s16 arg3)
{
    u8*              head;
    GpFxQuadScratch* block;
    POLY_FT4*        prim;
    u16              col;
    u16              row;
    s32              u0;
    s32              v0;
    s32              ang;
    s32              ang2;
    u16              bank;
    u32              idx;

    head                                      = SCRATCH_STACK_CURSOR(u8);
    ((GpFxQuadScratch*)(head - 0x1C))->vec.vx = arg0->workm.t[0];
    SCRATCH_STACK_CURSOR(void)                = head - 0x1C;
    block                                     = SCRATCH_STACK_CURSOR(GpFxQuadScratch);
    block->vec.vy                             = arg0->workm.t[1];
    block->vec.vz                             = arg0->workm.t[2];
    idx                                       = arg1;
    idx                                      &= 0xFFF;
    bank                                      = arg1 >> 12;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(block);
    gte_rtps();
    gte_stsxy(&((GpFxQuadScratch*)(head - 0x1C))->sx);
    gte_stflg(&((GpFxQuadScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpFxQuadScratch*)(head - 0x1C))->otz);
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x2B;
        if (bank >= 2) {
            prim->clut = 0x428F;
        } else {
            prim->clut = ((bank + 0x10E) << 6) | (idx & 0x3F);
        }
        col = (u16)idx % 5;
        row = (u16)idx / 5;
        ang = arg3;
        u0  = col * 0x30;
        v0  = row * 0x30;
        setUV4(prim, u0, v0 + 0x68, u0 + 0x2F, v0 + 0x68, u0, v0 - 0x69, u0 + 0x2F, v0 - 0x69);
        block->dx = (((arg2 * 47) / block->otz) * rsin(ang)) >> 12;
        block->dy = (((arg2 * 47) / block->otz) * rcos(ang)) >> 12;
        prim->x0  = block->sx + (u16)block->dx;
        prim->x3  = block->sx - (u16)block->dx;
        prim->y0  = block->sy - (u16)block->dy;
        prim->y3  = block->sy + (u16)block->dy;
        ang2      = ang + 0x400;
        block->dx = (((arg2 * 47) / block->otz) * rsin(ang2)) >> 12;
        block->dy = (((arg2 * 47) / block->otz) * rcos(ang2)) >> 12;
        prim->x1  = block->sx + (u16)block->dx;
        prim->x2  = block->sx - (u16)block->dx;
        prim->y1  = block->sy - (u16)block->dy;
        prim->y2  = block->sy + (u16)block->dy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BYTES(0x1C);
}
