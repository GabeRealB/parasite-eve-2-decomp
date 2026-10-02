/* Part of the effect sprite library; see effect_sprite.h. */

/// The same sprite drawer as `_effectSpriteDrawBanked` for
/// the sheet on tpage 0x2C, with one of two fixed palettes chosen by the top
/// nibble of `arg1`.
void effectSpriteDrawRotated(GfxCoord* arg0, u16 arg1, s16 arg2, s16 arg3)
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
    u16                 vz;

    bank                       = arg1 >> 12;
    arg1                      &= 0xFFF;
    head                       = SCRATCH_STACK_CURSOR(EffectShapeScratch);
    (head - 1)->worldPoint.vx  = arg0->workm.t[0];
    SCRATCH_STACK_CURSOR(void) = head - 1;
    block                      = SCRATCH_STACK_CURSOR(EffectShapeScratch);
    block->worldPoint.vy       = arg0->workm.t[1];
    block->worldPoint.vz       = arg0->workm.t[2];
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
        prim->tpage = 0x2C;
        prim->clut  = bank ? 0x428F : 0x43D0;
        col         = arg1 % 5;
        row         = arg1 / 5;
        ang         = arg3;
        u0          = col * 0x30;
        v0          = row * 0x30;
        setUV4(prim, u0, v0 - 0x80, u0 + 0x2F, v0 - 0x80, u0, v0 - 0x51, u0 + 0x2F, v0 - 0x51);
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
