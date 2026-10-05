/* Part of the effect sprite library; see effect_sprite.h. */

/// Draws an upright raw-texture debris billboard at a composed coordinate's translation.
///
/// `coord` is borrowed read-only with `workm` in `GsWSMATRIX`'s input space; translation
/// is narrowed to signed 16-bit coordinates. `frame` narrows to `u16` and its low
/// three bits select a 4x2 grid of 56x56 cells starting at V=0. `size` narrows to
/// `s16`: that signed value * 55 / (SZ3/4) is the pixel half-side. The anchor lies
/// three quarters down the square. Accepted projections require nonzero depth,
/// without a check here. Screen-coordinate and UV stores retain low bits.
///
/// A nonnegative GTE FLAG allocates one raw-texture, additive semi-transparent
/// `POLY_FT4`. Requires a live packet arena and initialized scratch stack; releases
/// one `EffectCentreScratch` on every path. GTE state changes; no pointer is retained.
static void _effectSpriteDrawBillboard(const GfxCoord* coord, s32 frame, s32 size)
{
    enum {
        EFFECT_SPRITE_BILLBOARD_CELL_PITCH_TEXELS        = 56,
        EFFECT_SPRITE_BILLBOARD_UV_SPAN_TEXELS           = EFFECT_SPRITE_BILLBOARD_CELL_PITCH_TEXELS - 1,
        EFFECT_SPRITE_BILLBOARD_COLUMN_MASK              = 3,
        EFFECT_SPRITE_BILLBOARD_FRAME_MASK               = 7,
        EFFECT_SPRITE_BILLBOARD_ROW_SHIFT                = 2,
        EFFECT_SPRITE_BILLBOARD_RAW_SEMITRANSPARENT_CODE = 0x2F
    };
    EffectCentreScratch* block;
    POLY_FT4*            quad;
    u16                  frameIndex;
    u32                  cellBits;
    s32                  rowV;
    u8                   uLeft;
    u8                   uRight;
    u8                   vTop;
    u8                   vBottom;

    frameIndex           = frame;
    block                = SCRATCH_STACK_RESERVE_BLOCK(EffectCentreScratch);
    block->worldPoint.vx = coord->workm.t[0];
    block->worldPoint.vy = coord->workm.t[1];
    block->worldPoint.vz = coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->worldPoint);
    gte_rtps();
    gte_stsxy(&block->screenX);
    gte_stflg(&block->projectionFlags);
    if (block->projectionFlags >= 0) {
        gte_stszotz(&block->depth);
        quad           = gGpuPrimCursor;
        gGpuPrimCursor = quad + 1;
        setlen(quad, sizeof(*quad) / sizeof(u32) - 1);
        setcode(quad, EFFECT_SPRITE_BILLBOARD_RAW_SEMITRANSPARENT_CODE);
        quad->tpage = getTPage(0, GPU_BLEND_ADD, 704, 0);
        quad->clut  = getClut(304, 270);
        cellBits    = frameIndex;
        uLeft       = (cellBits & EFFECT_SPRITE_BILLBOARD_COLUMN_MASK) * EFFECT_SPRITE_BILLBOARD_CELL_PITCH_TEXELS;
        rowV        = ((cellBits & EFFECT_SPRITE_BILLBOARD_FRAME_MASK) >> EFFECT_SPRITE_BILLBOARD_ROW_SHIFT) * EFFECT_SPRITE_BILLBOARD_CELL_PITCH_TEXELS;
        vTop        = rowV;
        vBottom     = rowV + EFFECT_SPRITE_BILLBOARD_UV_SPAN_TEXELS;
        uRight      = uLeft + EFFECT_SPRITE_BILLBOARD_UV_SPAN_TEXELS;
        setUV4(quad, uLeft, vTop, uRight, vTop, uLeft, vBottom, uRight, vBottom);
        block->screenExtent = ((s16)size * EFFECT_SPRITE_BILLBOARD_UV_SPAN_TEXELS) / block->depth;
        quad->x0 = quad->x2 = block->screenX - block->screenExtent;
        quad->x1 = quad->x3 = block->screenX + block->screenExtent;
        quad->y0 = quad->y1 = block->screenY - block->screenExtent - (block->screenExtent >> 1);
        quad->y2 = quad->y3 = block->screenY + (block->screenExtent >> 1);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectCentreScratch);
}
