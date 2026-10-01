/* Part of the sprite quad library; see sprite_quad.h.
 *
 * The flicker form: the frame's low bit alternates two looks of the flame
 * strip (tpage 0x29, v 0xC8-0xFF) - SPRITE_QUAD_ODD_LOOK(prim) and
 * SPRITE_QUAD_EVEN_LOOK(prim), statements the including unit defines from
 * SPRITE_QUAD_CORE_CELL / SPRITE_QUAD_RIM_CELL plus its own tint and blend
 * flags. SPRITE_QUAD_PRIM_FIRST 1 takes the primitive before the projection is
 * checked. */

#ifndef SPRITE_QUAD_PRIM_FIRST
#define SPRITE_QUAD_PRIM_FIRST 0
#endif

static void spriteQuadDrawFlicker(GfxCoord* coord, s16 frame, s16 size, s16 angle)
{
    GpFxQuadScratch* block;
    POLY_FT4*        prim;
    s32              ang;

    block         = SCRATCH_STACK_RESERVE_BLOCK(GpFxQuadScratch);
    block->vec.vx = coord->workm.t[0];
    block->vec.vy = coord->workm.t[1];
    block->vec.vz = coord->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
#if SPRITE_QUAD_PRIM_FIRST
    /* taken while the GTE projects, before its flag is read: a dropped sprite still uses its slot */
    prim           = gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setPolyFT4(prim);
#endif
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        block->otz++;
#if !SPRITE_QUAD_PRIM_FIRST
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setPolyFT4(prim);
#endif
        if (frame & 1) {
            SPRITE_QUAD_ODD_LOOK(prim);
        } else {
            SPRITE_QUAD_EVEN_LOOK(prim);
        }
        ang       = angle;
        block->dx = (((size * SPRITE_QUAD_SCALE) / block->otz) * rsin(ang)) >> 12;
        block->dy = (((size * SPRITE_QUAD_SCALE) / block->otz) * rcos(ang)) >> 12;
        prim->x0  = block->sx + (u16)block->dx;
        prim->x3  = block->sx - (u16)block->dx;
        prim->y0  = block->sy - (u16)block->dy;
        prim->y3  = block->sy + (u16)block->dy;
        ang       = ang + 0x400;
        block->dx = (((size * SPRITE_QUAD_SCALE) / block->otz) * rsin(ang)) >> 12;
        block->dy = (((size * SPRITE_QUAD_SCALE) / block->otz) * rcos(ang)) >> 12;
        prim->x1  = block->sx + (u16)block->dx;
        prim->x2  = block->sx - (u16)block->dx;
        prim->y1  = block->sy - (u16)block->dy;
        prim->y2  = block->sy + (u16)block->dy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BLOCK(GpFxQuadScratch);
}

#undef SPRITE_QUAD_ODD_LOOK
#undef SPRITE_QUAD_EVEN_LOOK
#undef SPRITE_QUAD_PRIM_FIRST
#undef SPRITE_QUAD_SCALE
