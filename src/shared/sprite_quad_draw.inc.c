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
    EffectShapeScratch* head;
    EffectShapeScratch* block;
    POLY_FT4*           prim;
    SVECTOR*            vec;
    s32                 u0;
    s32                 u1;
#ifdef SPRITE_QUAD_UV_TABLE
    GpEffUv8* rec;
#endif
#ifdef SPRITE_QUAD_CELL_H
    s32 v0;
#endif
    s32 ang2;
    u16 vz;

    head                                     = SCRATCH_STACK_CURSOR(EffectShapeScratch);
    (head - 1)->worldPoint.vx                = (u16)SPRITE_QUAD_POS(pos, 0);
    block                                    = head - 1;
    block->worldPoint.vy                     = (u16)SPRITE_QUAD_POS(pos, 1);
    vz                                       = (u16)SPRITE_QUAD_POS(pos, 2);
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
#if SPRITE_QUAD_OTZ_BIAS
        block->depth++;
#endif
#ifdef SPRITE_QUAD_MIN_OTZ
        /* nearer than this the sprite is not drawn */
        if (block->depth >= SPRITE_QUAD_MIN_OTZ)
#endif
        {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyFT4(prim);
            setSemiTrans(prim, 1);
            setShadeTex(prim, 1);
            prim->tpage = SPRITE_QUAD_TPAGE;
            prim->clut  = SPRITE_QUAD_CLUT;
#if defined(SPRITE_QUAD_UV_TABLE)
            /* each frame names its own square cell */
            rec = &SPRITE_QUAD_UV_TABLE[frame];
            setUV4(prim, rec->u, rec->v, rec->u + (SPRITE_QUAD_CELL_W - 1), rec->v, rec->u, rec->v + (SPRITE_QUAD_CELL_W - 1),
                   rec->u + (SPRITE_QUAD_CELL_W - 1), rec->v + (SPRITE_QUAD_CELL_W - 1));
#else
#if defined(SPRITE_QUAD_CELLS_PER_ROW)
#ifdef SPRITE_QUAD_CELL_H
            u0 = (s16)(frame % SPRITE_QUAD_CELLS_PER_ROW) * SPRITE_QUAD_CELL_W;
#else
            u0 = (frame % SPRITE_QUAD_CELLS_PER_ROW) * SPRITE_QUAD_CELL_W;
#endif
#elif defined(SPRITE_QUAD_CELL_MASK)
            u0 = (frame & SPRITE_QUAD_CELL_MASK) * SPRITE_QUAD_CELL_W;
#else
            u0 = frame * SPRITE_QUAD_CELL_W;
#endif
#ifdef SPRITE_QUAD_U_BASE
            /* both edges from the cell's offset; the right edge wraps past 0xFF */
            u1 = u0 + (s8)(SPRITE_QUAD_U_BASE + SPRITE_QUAD_CELL_W - 1);
            u0 = u0 + SPRITE_QUAD_U_BASE;
#else
            u1 = u0 + (SPRITE_QUAD_CELL_W - 1);
#endif
#ifdef SPRITE_QUAD_CELL_H
            /* a grid of cells: the row comes from the frame too */
            v0 = (s16)(frame / SPRITE_QUAD_CELLS_PER_ROW) * SPRITE_QUAD_CELL_H;
            setUV4(prim, u0, v0 + SPRITE_QUAD_V0, u0 + (SPRITE_QUAD_CELL_W - 1), v0 + SPRITE_QUAD_V0, u0, v0 + SPRITE_QUAD_V1,
                   u0 + (SPRITE_QUAD_CELL_W - 1), v0 + SPRITE_QUAD_V1);
#else
            setUV4(prim, u0, SPRITE_QUAD_V0, u1, SPRITE_QUAD_V0, u0, SPRITE_QUAD_V1, u1, SPRITE_QUAD_V1);
#endif
#endif
            block->extent.corner.x = (((size * SPRITE_QUAD_SCALE) / block->depth) * rsin(angle)) >> 12;
            block->extent.corner.y = (((size * SPRITE_QUAD_SCALE) / block->depth) * rcos(angle)) >> 12;
            prim->x0               = block->screenX + (u16)block->extent.corner.x;
            prim->x3               = block->screenX - (u16)block->extent.corner.x;
            prim->y0               = block->screenY - (u16)block->extent.corner.y;
            prim->y3               = block->screenY + (u16)block->extent.corner.y;
            ang2                   = angle + 0x400;
            block->extent.corner.x = (((size * SPRITE_QUAD_SCALE) / block->depth) * rsin(ang2)) >> 12;
            block->extent.corner.y = (((size * SPRITE_QUAD_SCALE) / block->depth) * rcos(ang2)) >> 12;
            prim->x1               = block->screenX + (u16)block->extent.corner.x;
            prim->x2               = block->screenX - (u16)block->extent.corner.x;
            prim->y1               = block->screenY - (u16)block->extent.corner.y;
            prim->y2               = block->screenY + (u16)block->extent.corner.y;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
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
#undef SPRITE_QUAD_CELL_H
#undef SPRITE_QUAD_UV_TABLE
