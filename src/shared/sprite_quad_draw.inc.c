/* Part of the sprite quad library; see sprite_quad.h. */

#ifndef SPRITE_QUAD_FUNC
#define SPRITE_QUAD_FUNC spriteQuadDraw
#endif
#ifndef SPRITE_QUAD_TPAGE
#define SPRITE_QUAD_TPAGE 0x2A
#endif
#ifndef SPRITE_QUAD_OTZ_BIAS
#define SPRITE_QUAD_OTZ_BIAS 1
#endif

/// Draws one cell of the overlay's sprite texture at `coord`'s world position.
static void SPRITE_QUAD_FUNC(SPRITE_QUAD_POS_T* pos, SPRITE_QUAD_FRAME_T frame, SPRITE_QUAD_SIZE_T size, s16 angle)
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
    ((GpFxQuadScratch*)(head - 0x1C))->vec.vx = (u16)SPRITE_QUAD_POS(pos, 0);
    block                                     = (GpFxQuadScratch*)(head - 0x1C);
    block->vec.vy                             = (u16)SPRITE_QUAD_POS(pos, 1);
    vz                                        = (u16)SPRITE_QUAD_POS(pos, 2);
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
#if SPRITE_QUAD_OTZ_BIAS
        block->otz++;
#endif
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setPolyFT4(prim);
        setSemiTrans(prim, 1);
        setShadeTex(prim, 1);
        prim->tpage = SPRITE_QUAD_TPAGE;
        prim->clut  = SPRITE_QUAD_CLUT;
#if defined(SPRITE_QUAD_CELLS_PER_ROW)
        u0 = (frame % SPRITE_QUAD_CELLS_PER_ROW) * SPRITE_QUAD_CELL_W;
#elif defined(SPRITE_QUAD_CELL_MASK)
        u0 = (frame & SPRITE_QUAD_CELL_MASK) * SPRITE_QUAD_CELL_W;
#else
        u0 = frame * SPRITE_QUAD_CELL_W;
#endif
#ifdef SPRITE_QUAD_U_BASE
        u0 = u0 + SPRITE_QUAD_U_BASE;
#endif
        u1 = u0 + (SPRITE_QUAD_CELL_W - 1);
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

#undef SPRITE_QUAD_FUNC
#undef SPRITE_QUAD_TPAGE
#undef SPRITE_QUAD_CLUT
#undef SPRITE_QUAD_CELL_W
#undef SPRITE_QUAD_CELLS_PER_ROW
#undef SPRITE_QUAD_V0
#undef SPRITE_QUAD_V1
#undef SPRITE_QUAD_SCALE
#undef SPRITE_QUAD_OTZ_BIAS
#undef SPRITE_QUAD_CELL_MASK
#undef SPRITE_QUAD_U_BASE
#undef SPRITE_QUAD_MIN_OTZ
