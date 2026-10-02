/* Part of the effect sprite library; see effect_sprite.h. */

/// Draws one cell of a 5-column, 48-texel sprite sheet (tpage 0x2B) as a
/// semi-transparent `POLY_FT4` centred on the coordinate's projected position.
/// `arg1`'s low 12 bits are the cell index and its top nibble the palette
/// bank, `arg2` the half-extent (scaled by 47 over depth) and `arg3` the
/// quad's rotation. Nothing is drawn when the projection fails.
void effectSpriteDrawBanked(GfxCoord* arg0, u16 arg1, s16 arg2, s16 arg3)
{
    EffectShapeScratch* head;
    EffectShapeScratch* block;
    POLY_FT4*           prim;
    u16                 col;
    u16                 row;
    s32                 u0;
    s32                 v0;
    s32                 ang;
    s32                 ang2;
    u16                 bank;
    u32                 idx;

    head                       = SCRATCH_STACK_CURSOR(EffectShapeScratch);
    (head - 1)->worldPoint.vx  = arg0->workm.t[0];
    SCRATCH_STACK_CURSOR(void) = head - 1;
    block                      = SCRATCH_STACK_CURSOR(EffectShapeScratch);
    block->worldPoint.vy       = arg0->workm.t[1];
    block->worldPoint.vz       = arg0->workm.t[2];
    idx                        = arg1;
    idx                       &= 0xFFF;
    bank                       = arg1 >> 12;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->worldPoint);
    gte_rtps();
    gte_stsxy(&(head - 1)->screenX);
    gte_stflg(&(head - 1)->projectionFlags);
    if (block->projectionFlags >= 0) {
        gte_stszotz(&(head - 1)->depth);
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
        block->extent.corner.x = (((arg2 * 47) / block->depth) * rsin(ang)) >> 12;
        block->extent.corner.y = (((arg2 * 47) / block->depth) * rcos(ang)) >> 12;
        prim->x0               = block->screenX + (u16)block->extent.corner.x;
        prim->x3               = block->screenX - (u16)block->extent.corner.x;
        prim->y0               = block->screenY - (u16)block->extent.corner.y;
        prim->y3               = block->screenY + (u16)block->extent.corner.y;
        ang2                   = ang + 0x400;
        block->extent.corner.x = (((arg2 * 47) / block->depth) * rsin(ang2)) >> 12;
        block->extent.corner.y = (((arg2 * 47) / block->depth) * rcos(ang2)) >> 12;
        prim->x1               = block->screenX + (u16)block->extent.corner.x;
        prim->x2               = block->screenX - (u16)block->extent.corner.x;
        prim->y1               = block->screenY - (u16)block->extent.corner.y;
        prim->y2               = block->screenY + (u16)block->extent.corner.y;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
}
