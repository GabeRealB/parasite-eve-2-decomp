/* Part of the sprite quad library; see sprite_quad.h. */

/// Draws one cell of the overlay's sprite texture at `coord`'s world position.
static void spriteQuadDraw(GfxCoord* coord, SPRITE_QUAD_FRAME_T frame, s16 size, s16 angle)
{
    u8*              head;
    GpFxQuadScratch* block;
    POLY_FT4*        prim;
    SVECTOR*         vec;
    s32              u0;
    s32              u1;
    s32              ang2;
    u16              vz;

    head                                      = SCRATCH_STACK_CURSOR(u8);
    ((GpFxQuadScratch*)(head - 0x1C))->vec.vx = (u16)coord->workm.t[0];
    block                                     = (GpFxQuadScratch*)(head - 0x1C);
    block->vec.vy                             = (u16)coord->workm.t[1];
    vz                                        = (u16)coord->workm.t[2];
    block->vec.vz                             = vz;
    SCRATCH_STACK_CURSOR(GpFxQuadScratch)     = block;
    vec                                       = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&((GpFxQuadScratch*)(head - 0x1C))->sx);
    gte_stflg(&((GpFxQuadScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpFxQuadScratch*)(head - 0x1C))->otz);
        block->otz++;
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setPolyFT4(prim);
        setSemiTrans(prim, 1);
        setShadeTex(prim, 1);
        prim->tpage = SPRITE_QUAD_TPAGE;
        prim->clut  = SPRITE_QUAD_CLUT;
        u0          = frame * SPRITE_QUAD_CELL_W;
        u1          = u0 + (SPRITE_QUAD_CELL_W - 1);
        setUV4(prim, u0, SPRITE_QUAD_V0, u1, SPRITE_QUAD_V0, u0, SPRITE_QUAD_V1, u1, SPRITE_QUAD_V1);
        block->dx = (((size * SPRITE_QUAD_SCALE) / block->otz) * rsin(angle)) >> 12;
        block->dy = (((size * SPRITE_QUAD_SCALE) / block->otz) * rcos(angle)) >> 12;
        prim->x0  = block->sx + (u16)block->dx;
        prim->x3  = block->sx - (u16)block->dx;
        prim->y0  = block->sy - (u16)block->dy;
        prim->y3  = block->sy + (u16)block->dy;
        ang2      = angle + 0x400;
        block->dx = (((size * SPRITE_QUAD_SCALE) / block->otz) * rsin(ang2)) >> 12;
        block->dy = (((size * SPRITE_QUAD_SCALE) / block->otz) * rcos(ang2)) >> 12;
        prim->x1  = block->sx + (u16)block->dx;
        prim->x2  = block->sx - (u16)block->dx;
        prim->y1  = block->sy - (u16)block->dy;
        prim->y2  = block->sy + (u16)block->dy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BYTES(0x1C);
}
