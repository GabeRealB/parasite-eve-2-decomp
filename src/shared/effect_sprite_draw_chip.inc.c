/* Part of the effect sprite library; see effect_sprite.h. */

/// Draws a spinning textured sprite at the world position of `arg0`,
/// projected through `GsWSMATRIX`: one semi-transparent `POLY_FT4` whose
/// corners lie `(s16)arg2 * 31` over the depth from the centre, at the angle
/// `arg3` and a quarter turn past it. `arg1` picks the frame, a 32x32 cell
/// in a row of the texture page. Nothing is drawn when the projection flags
/// an error.
void effectSpriteDrawChip(GfxCoord* arg0, u16 arg1, s16 arg2, s16 arg3)
{
    void**              scratch;
    EffectShapeScratch* head;
    EffectShapeScratch* block;
    POLY_FT4*           prim;
    SVECTOR*            vec;
    s32                 u0;
    s32                 u1;
    s32                 v;
    s32                 ang;
    s32                 ang2;
    u16                 vz;

    scratch                   = SCRATCH_STACK_CURSOR_SLOT;
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
        ang            = arg3;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x2C;
        prim->clut  = 0x43D3;
        u0          = arg1 << 5;
        v           = 0xE0;
        u1          = u0 + 0x1F;
        setUV4(prim, u0, v, u1, v, u0, 0xFF, u1, 0xFF);
        block->extent.corner.x = (((arg2 * 31) / block->depth) * rsin(ang)) >> 12;
        block->extent.corner.y = (((arg2 * 31) / block->depth) * rcos(ang)) >> 12;
        prim->x0               = block->screenX + (u16)block->extent.corner.x;
        prim->x3               = block->screenX - (u16)block->extent.corner.x;
        prim->y0               = block->screenY - (u16)block->extent.corner.y;
        ang2                   = ang + 0x400;
        prim->y3               = block->screenY + (u16)block->extent.corner.y;
        block->extent.corner.x = (((arg2 * 31) / block->depth) * rsin(ang2)) >> 12;
        block->extent.corner.y = (((arg2 * 31) / block->depth) * rcos(ang2)) >> 12;
        prim->x1               = block->screenX + (u16)block->extent.corner.x;
        prim->x2               = block->screenX - (u16)block->extent.corner.x;
        prim->y1               = block->screenY - (u16)block->extent.corner.y;
        prim->y2               = block->screenY + (u16)block->extent.corner.y;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_POP_AT(scratch, EffectShapeScratch);
}
