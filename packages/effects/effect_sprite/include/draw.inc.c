/* Part of the effect sprite library; see effect_sprite.h. */

/// Draws a spinning additive debris chip at the coordinate's translation: `frame` picks one of eight 32-texel cells,
/// `angle` spins it (4096 to a turn), and `size` times 31 over the depth is its half-diagonal in pixels.
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
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(block->depth),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK_AT(scratchSlot, EffectShapeScratch);
}

/// Draws an upright additive debris billboard at the coordinate's translation, anchored three quarters down: `frame`'s
/// low three bits pick one of eight 56-texel cells, four to a row, and `size` times 55 over the depth is its half-side.
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
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(block->depth),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectCentreScratch);
}
