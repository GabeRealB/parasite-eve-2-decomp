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
    EffectShapeScratch* block;
    POLY_FT4*           prim;
    s32                 ang;

    block                = SCRATCH_STACK_RESERVE_BLOCK(EffectShapeScratch);
    block->worldPoint.vx = coord->workm.t[0];
    block->worldPoint.vy = coord->workm.t[1];
    block->worldPoint.vz = coord->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->worldPoint);
    gte_rtps();
#if SPRITE_QUAD_PRIM_FIRST
    /* taken while the GTE projects, before its flag is read: a dropped sprite still uses its slot */
    prim           = gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setPolyFT4(prim);
#endif
    gte_stsxy(&block->screenX);
    gte_stflg(&block->projectionFlags);
    if (block->projectionFlags >= 0) {
        gte_stszotz(&block->depth);
        block->depth++;
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
        ang                    = angle;
        block->extent.corner.x = (((size * SPRITE_QUAD_SCALE) / block->depth) * rsin(ang)) >> 12;
        block->extent.corner.y = (((size * SPRITE_QUAD_SCALE) / block->depth) * rcos(ang)) >> 12;
        prim->x0               = block->screenX + (u16)block->extent.corner.x;
        prim->x3               = block->screenX - (u16)block->extent.corner.x;
        prim->y0               = block->screenY - (u16)block->extent.corner.y;
        prim->y3               = block->screenY + (u16)block->extent.corner.y;
        ang                    = ang + 0x400;
        block->extent.corner.x = (((size * SPRITE_QUAD_SCALE) / block->depth) * rsin(ang)) >> 12;
        block->extent.corner.y = (((size * SPRITE_QUAD_SCALE) / block->depth) * rcos(ang)) >> 12;
        prim->x1               = block->screenX + (u16)block->extent.corner.x;
        prim->x2               = block->screenX - (u16)block->extent.corner.x;
        prim->y1               = block->screenY - (u16)block->extent.corner.y;
        prim->y2               = block->screenY + (u16)block->extent.corner.y;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
}

#undef SPRITE_QUAD_ODD_LOOK
#undef SPRITE_QUAD_EVEN_LOOK
#undef SPRITE_QUAD_PRIM_FIRST
#undef SPRITE_QUAD_SCALE
