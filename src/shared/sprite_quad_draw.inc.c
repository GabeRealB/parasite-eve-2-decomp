/* Part of the sprite quad library; see sprite_quad.h. */

#ifndef SPRITE_QUAD_CELL_WIDTH
#error Define SPRITE_QUAD_CELL_WIDTH before including the sprite quad drawer
#endif

#ifndef SPRITE_QUAD_SCALE
#error Define SPRITE_QUAD_SCALE before including the sprite quad drawer
#endif

#if !defined(SPRITE_QUAD_UV_TABLE)
#if defined(SPRITE_QUAD_CELLS_PER_ROW)
#if (SPRITE_QUAD_CELLS_PER_ROW) <= 0
#error SPRITE_QUAD_CELLS_PER_ROW must be a positive integer constant
#endif
#elif defined(SPRITE_QUAD_CELL_H)
#error A sprite cell grid requires SPRITE_QUAD_CELLS_PER_ROW
#endif
#endif

#ifndef SPRITE_QUAD_FUNC
#define SPRITE_QUAD_FUNC spriteQuadDraw
#endif
#ifndef SPRITE_QUAD_TEXTURE_PAGE
/// Packed GPU texture-page word for one included sprite-quad drawer.
///
/// Bind a 16-bit integer constant before each inclusion, using `getTPage`
/// or a named packed value. The default selects 4-bit indexed texels at VRAM
/// X=640 words, Y=0 scanlines with `GPU_BLEND_ADD`. The SDK `getTPage` macro
/// and `GPU_BLEND_ADD` must be in scope through `psyq/libgpu.h` and
/// `gameplay/room_effects.h` before this fragment is included.
///
/// The word selects the texel format, page origin and semitransparency mode;
/// UVs count texels relative to that origin and the CLUT is bound separately.
/// The drawer enables semitransparency; texture colours with bit 15 set blend.
/// This binding is assigned once to each emitted `POLY_FT4::tpage` and then
/// undefined after the drawer definition, so a later instance gets its own
/// binding or the default. Antibody's larger sprite, Flare, Necrosis, the
/// gallery, training room, Hammer's six-cell strip and Javelin use the default.
#define SPRITE_QUAD_TEXTURE_PAGE getTPage(0, GPU_BLEND_ADD, 640, 0)
#endif
#ifndef SPRITE_QUAD_OTZ_BIAS
#define SPRITE_QUAD_OTZ_BIAS 1
#endif

/// Draws one cell of the overlay's sprite texture at the translation supplied by `pos`.
static void SPRITE_QUAD_FUNC(SPRITE_QUAD_POSITION_SOURCE_TYPE* pos, SPRITE_QUAD_FRAME_T frame, SPRITE_QUAD_SIZE_T size, s16 angle)
{
    EffectShapeScratch* head;
    EffectShapeScratch* block;
    POLY_FT4*           prim;
    SVECTOR*            vec;
    s32                 u0;
    s32                 u1;
#ifdef SPRITE_QUAD_UV_TABLE
    const EffectSpriteTextureFrame* textureFrame;
#endif
#ifdef SPRITE_QUAD_CELL_H
    s32 v0;
#endif
    s32 ang2;
    u16 vz;

    // Check in C so bindings may refer to enum constants as well as literals.
    STATIC_ASSERT(SPRITE_QUAD_CELL_WIDTH >= 1 && SPRITE_QUAD_CELL_WIDTH <= 256, sprite_quad_cell_width_fits_uv_byte);
    STATIC_ASSERT(SPRITE_QUAD_TEXTURE_PAGE >= 0 && SPRITE_QUAD_TEXTURE_PAGE <= 0xFFFF, sprite_quad_texture_page_fits_packet);
    STATIC_ASSERT((SPRITE_QUAD_SCALE) > 0, sprite_quad_scale_is_positive);

    // Project the source translation's low 16 bits without changing its coordinate cache.
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
            prim->tpage = SPRITE_QUAD_TEXTURE_PAGE;
            prim->clut  = SPRITE_QUAD_CLUT;
#if defined(SPRITE_QUAD_UV_TABLE)
            // Cell widths count texels; both square-cell endpoints are inclusive.
            textureFrame = &(SPRITE_QUAD_UV_TABLE)[frame];
            setUV4(prim, textureFrame->u, textureFrame->v, textureFrame->u + (SPRITE_QUAD_CELL_WIDTH - 1), textureFrame->v,
                   textureFrame->u, textureFrame->v + (SPRITE_QUAD_CELL_WIDTH - 1),
                   textureFrame->u + (SPRITE_QUAD_CELL_WIDTH - 1), textureFrame->v + (SPRITE_QUAD_CELL_WIDTH - 1));
#else
#if defined(SPRITE_QUAD_CELLS_PER_ROW)
#ifdef SPRITE_QUAD_CELL_H
            u0 = (s16)(frame % (SPRITE_QUAD_CELLS_PER_ROW)) * SPRITE_QUAD_CELL_WIDTH;
#else
            // Repeat the strip's column while keeping its texel rows fixed.
            u0 = (frame % (SPRITE_QUAD_CELLS_PER_ROW)) * SPRITE_QUAD_CELL_WIDTH;
#endif
#elif defined(SPRITE_QUAD_CELL_MASK)
            u0 = (frame & SPRITE_QUAD_CELL_MASK) * SPRITE_QUAD_CELL_WIDTH;
#else
            u0 = frame * SPRITE_QUAD_CELL_WIDTH;
#endif
#ifdef SPRITE_QUAD_U_BASE
            // Sign-extend the first cell's right endpoint; GPU U fields retain its low byte.
            u1 = u0 + (s8)(SPRITE_QUAD_U_BASE + SPRITE_QUAD_CELL_WIDTH - 1);
            u0 = u0 + SPRITE_QUAD_U_BASE;
#else
            u1 = u0 + (SPRITE_QUAD_CELL_WIDTH - 1);
#endif
#ifdef SPRITE_QUAD_CELL_H
            // Advance through grid rows; the column count is not a frame limit.
            v0 = (s16)(frame / (SPRITE_QUAD_CELLS_PER_ROW)) * SPRITE_QUAD_CELL_H;
            setUV4(prim, u0, v0 + SPRITE_QUAD_V0, u0 + (SPRITE_QUAD_CELL_WIDTH - 1), v0 + SPRITE_QUAD_V0, u0, v0 + SPRITE_QUAD_V1,
                   u0 + (SPRITE_QUAD_CELL_WIDTH - 1), v0 + SPRITE_QUAD_V1);
#else
            setUV4(prim, u0, SPRITE_QUAD_V0, u1, SPRITE_QUAD_V0, u0, SPRITE_QUAD_V1, u1, SPRITE_QUAD_V1);
#endif
#endif
            // Rotate the perspective-scaled half-diagonal into two pairs of opposite corners.
            block->extent.corner.x = (((size * (SPRITE_QUAD_SCALE)) / block->depth) * rsin(angle)) >> 12;
            block->extent.corner.y = (((size * (SPRITE_QUAD_SCALE)) / block->depth) * rcos(angle)) >> 12;
            prim->x0               = block->screenX + (u16)block->extent.corner.x;
            prim->x3               = block->screenX - (u16)block->extent.corner.x;
            prim->y0               = block->screenY - (u16)block->extent.corner.y;
            prim->y3               = block->screenY + (u16)block->extent.corner.y;
            ang2                   = angle + 0x400;
            block->extent.corner.x = (((size * (SPRITE_QUAD_SCALE)) / block->depth) * rsin(ang2)) >> 12;
            block->extent.corner.y = (((size * (SPRITE_QUAD_SCALE)) / block->depth) * rcos(ang2)) >> 12;
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
#undef SPRITE_QUAD_TEXTURE_PAGE
#undef SPRITE_QUAD_CLUT
#undef SPRITE_QUAD_CELL_WIDTH
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
