/* Part of the pyro flame library; see pyro_flame.h. */

/// Flame sprite, identical in Pyrokinesis and Combustion. Links one frame of the flame
/// at `arg0`'s world position: the position is projected through `GsWSMATRIX`
/// by a single `RTPS` and the quad is dropped when that sets a negative
/// `gte_stflg`. `arg1` picks one of the 0x20-wide texture frames on tpage
/// 0x2A, `arg3` spins the quad and `arg2` sizes it: the corners sit
/// `arg2 * 31 / otz` from the projected centre along `arg3` and `arg3 + 0x400`,
/// so the sprite shrinks with depth. Same shape as the gameplay
/// `effectDrawSpinningBillboard`, with the CLUT fixed at 0x42C2 instead of picked from
/// `Gp_QuadClutX`.
void pyroFlameDrawSprite(GfxCoord* arg0, s16 arg1, s16 arg2, s16 arg3)
{
    EffectShapeScratch* head;
    EffectShapeScratch* block;
    POLY_FT4*           prim;
    SVECTOR*            vec;
    s32                 u0;
    s32                 u1;
    s32                 ang2;
    u16                 vz;

    head                                     = SCRATCH_STACK_CURSOR(EffectShapeScratch);
    (head - 1)->worldPoint.vx                = (u16)arg0->workm.t[0];
    block                                    = head - 1;
    block->worldPoint.vy                     = (u16)arg0->workm.t[1];
    vz                                       = (u16)arg0->workm.t[2];
    block->worldPoint.vz                     = vz;
    SCRATCH_STACK_CURSOR(EffectShapeScratch) = block;
    vec                                      = &block->worldPoint;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&(head - 1)->screenX);
    gte_stflg(&(head - 1)->projectionFlags);
    if (block->projectionFlags >= 0) {
        gte_stszotz(&(head - 1)->depth);
        block->depth++;
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setPolyFT4(prim);
        setSemiTrans(prim, 1);
        setShadeTex(prim, 1);
        prim->tpage = 0x2A;
        prim->clut  = 0x42C2;
        u0          = arg1 << 5;
        u1          = u0 + 0x1F;
        setUV4(prim, u0, 0x18, u1, 0x18, u0, 0x37, u1, 0x37);
        block->extent.corner.x = (((arg2 * 31) / block->depth) * rsin(arg3)) >> 12;
        block->extent.corner.y = (((arg2 * 31) / block->depth) * rcos(arg3)) >> 12;
        prim->x0               = block->screenX + (u16)block->extent.corner.x;
        prim->x3               = block->screenX - (u16)block->extent.corner.x;
        prim->y0               = block->screenY - (u16)block->extent.corner.y;
        prim->y3               = block->screenY + (u16)block->extent.corner.y;
        ang2                   = arg3 + 0x400;
        block->extent.corner.x = (((arg2 * 31) / block->depth) * rsin(ang2)) >> 12;
        block->extent.corner.y = (((arg2 * 31) / block->depth) * rcos(ang2)) >> 12;
        prim->x1               = block->screenX + (u16)block->extent.corner.x;
        prim->x2               = block->screenX - (u16)block->extent.corner.x;
        prim->y1               = block->screenY - (u16)block->extent.corner.y;
        prim->y2               = block->screenY + (u16)block->extent.corner.y;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
}
