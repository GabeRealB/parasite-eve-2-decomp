/* Part of the effect sprite library; see effect_sprite.h. */

/// Draws a spinning raw-texture debris cell at a composed coordinate's translation.
///
/// `coord` is borrowed read-only with `workm` in `GsWSMATRIX`'s input space; translation
/// is narrowed to signed 16-bit coordinates. `frame` selects a 32x32 cell in the
/// eight-cell row at V=224. Callers use 0..7; UV stores wrap to bytes without a
/// `frame` check. `size` is a signed numerator: `size` * 31 / (SZ3/4) is the pixel
/// half-diagonal. `angle` uses 4096 units per turn; Q12 products round down when
/// negative. Accepted projections require nonzero depth, without a check here.
///
/// A nonnegative GTE FLAG allocates one raw-texture, additive semi-transparent
/// `POLY_FT4`. Requires a live packet arena and initialized scratch stack; releases
/// one `EffectShapeScratch` on every path. GTE state changes; no pointer is retained.
static void _effectSpriteDrawChip(const GfxCoord* coord, u16 frame, s16 size, s16 angle)
{
    enum {
        EFFECT_SPRITE_CHIP_CELL_PITCH_TEXELS        = 32,
        EFFECT_SPRITE_CHIP_UV_SPAN_TEXELS           = EFFECT_SPRITE_CHIP_CELL_PITCH_TEXELS - 1,
        EFFECT_SPRITE_CHIP_FIRST_TEXEL_ROW          = 224,
        EFFECT_SPRITE_CHIP_LAST_TEXEL_ROW           = 255,
        EFFECT_SPRITE_CHIP_QUARTER_TURN             = ONE / 4,
        EFFECT_SPRITE_CHIP_TRIG_FRACTION_BITS       = 12,
        EFFECT_SPRITE_CHIP_RAW_SEMITRANSPARENT_CODE = 0x2F
    };
    void**              scratchSlot;
    EffectShapeScratch* scratchEnd;
    EffectShapeScratch* block;
    POLY_FT4*           quad;
    SVECTOR*            worldPoint;
    s32                 uLeft;
    s32                 uRight;
    s32                 vTop;
    s32                 cornerAngle;
    s32                 perpendicularAngle;
    u16                 zBits;

    scratchSlot                     = SCRATCH_STACK_CURSOR_SLOT;
    scratchEnd                      = *scratchSlot;
    (scratchEnd - 1)->worldPoint.vx = (u16)coord->workm.t[0];
    block                           = scratchEnd - 1;
    block->worldPoint.vy            = (u16)coord->workm.t[1];
    zBits                           = (u16)coord->workm.t[2];
    *scratchSlot                    = block;
    block->worldPoint.vz            = zBits;
    worldPoint                      = &block->worldPoint;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(worldPoint);
    gte_rtps();
    gte_stsxy(&(scratchEnd - 1)->screenX);
    gte_stflg(&(scratchEnd - 1)->projectionFlags);
    if (block->projectionFlags >= 0) {
        gte_stszotz(&(scratchEnd - 1)->depth);
        quad           = gGpuPrimCursor;
        cornerAngle    = angle;
        gGpuPrimCursor = quad + 1;
        setlen(quad, sizeof(*quad) / sizeof(u32) - 1);
        setcode(quad, EFFECT_SPRITE_CHIP_RAW_SEMITRANSPARENT_CODE);
        quad->tpage = getTPage(0, GPU_BLEND_ADD, 768, 0);
        quad->clut  = getClut(304, 271);
        uLeft       = frame * EFFECT_SPRITE_CHIP_CELL_PITCH_TEXELS;
        vTop        = EFFECT_SPRITE_CHIP_FIRST_TEXEL_ROW;
        uRight      = uLeft + EFFECT_SPRITE_CHIP_UV_SPAN_TEXELS;
        setUV4(quad, uLeft, vTop, uRight, vTop, uLeft, EFFECT_SPRITE_CHIP_LAST_TEXEL_ROW, uRight, EFFECT_SPRITE_CHIP_LAST_TEXEL_ROW);
        block->extent.corner.x = (((size * EFFECT_SPRITE_CHIP_UV_SPAN_TEXELS) / block->depth) * rsin(cornerAngle)) >> EFFECT_SPRITE_CHIP_TRIG_FRACTION_BITS;
        block->extent.corner.y = (((size * EFFECT_SPRITE_CHIP_UV_SPAN_TEXELS) / block->depth) * rcos(cornerAngle)) >> EFFECT_SPRITE_CHIP_TRIG_FRACTION_BITS;
        quad->x0               = block->screenX + (u16)block->extent.corner.x;
        quad->x3               = block->screenX - (u16)block->extent.corner.x;
        quad->y0               = block->screenY - (u16)block->extent.corner.y;
        perpendicularAngle     = cornerAngle + EFFECT_SPRITE_CHIP_QUARTER_TURN;
        quad->y3               = block->screenY + (u16)block->extent.corner.y;
        block->extent.corner.x = (((size * EFFECT_SPRITE_CHIP_UV_SPAN_TEXELS) / block->depth) * rsin(perpendicularAngle)) >> EFFECT_SPRITE_CHIP_TRIG_FRACTION_BITS;
        block->extent.corner.y = (((size * EFFECT_SPRITE_CHIP_UV_SPAN_TEXELS) / block->depth) * rcos(perpendicularAngle)) >> EFFECT_SPRITE_CHIP_TRIG_FRACTION_BITS;
        quad->x1               = block->screenX + (u16)block->extent.corner.x;
        quad->x2               = block->screenX - (u16)block->extent.corner.x;
        quad->y1               = block->screenY - (u16)block->extent.corner.y;
        quad->y2               = block->screenY + (u16)block->extent.corner.y;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                quad);
    }
    SCRATCH_POP_AT(scratchSlot, EffectShapeScratch);
}
