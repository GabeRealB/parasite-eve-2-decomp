/* Part of the Dryfield main street library; see main_street.h. */

/// Stores the rotated displacement from the puff's centre to one corner.
///
/// `projection` borrows the live scratch block, with positive SZ3 / 4 depth.
/// The signed half-diagonal is truncated to integer pixels before Q12 rotation;
/// the angle uses 4096 units per turn and rotation products must fit s32.
/// Only the two corner offsets change; no storage or pointer is retained.
static inline void _mainStreetComputePuffCornerOffset(EffectBillboardScratch* projection, s16 sizeFactor, s32 cornerAngle)
{
    q19_12 sine;
    s32    scaledSize;

    sine                      = rsin(cornerAngle);
    scaledSize                = sizeFactor * MAIN_STREET_PUFF_UV_SPAN;
    projection->cornerOffsetX = ((scaledSize / projection->depth) * sine) >> MAIN_STREET_PUFF_TRIG_FRACTION_BITS;
    projection->cornerOffsetY = ((scaledSize / projection->depth) * rcos(cornerAngle)) >> MAIN_STREET_PUFF_TRIG_FRACTION_BITS;
}

/// Queues one additive, unmodulated frame of the main street's rotating puff billboard.
///
/// `coord` borrows an already composed translation in the input space of
/// `GsWSMATRIX`; only each component's low 16 bits reach the signed projection
/// vector. `frame` is 0..9, selecting a 48-by-48 cell in the five-column sheet.
/// The signed V origin wraps to byte 128 for row zero and 176 for row one.
/// `sizeFactor * 47 / (SZ3 / 4)` is the signed screen half-diagonal in pixels,
/// truncated toward zero before rotation. `angle` uses 4096 units per turn;
/// for a positive size factor, zero puts the first corner above the centre
/// and a quarter turn puts it to the right.
///
/// Requires initialized GTE projection settings, an initialized, word-aligned
/// scratch-stack cursor with room for one `EffectBillboardScratch`, and room
/// for one `POLY_FT4` in the current primitive arena. The packet is reserved
/// even if a negative GTE FLAG or depth below 65 rejects it. Scratch storage
/// is released before returning; queued packets remain borrowed by the GPU until the arena is
/// reused. No coordinate or scratch pointer is retained.
static void _mainStreetDrawPuff(const GfxCoord* coord, u16 frame, s16 sizeFactor, s16 angle)
{
    EffectBillboardScratch* projection;
    POLY_FT4*               quad;
    s32                     cornerAngle;
    s32                     perpendicularAngle;
    s32                     leftU;
    s32                     topV;
    s32                     rightU;
    s32                     bottomV;

    projection                = SCRATCH_STACK_RESERVE_BLOCK(EffectBillboardScratch);
    projection->worldPoint.vx = coord->workm.t[0];
    projection->worldPoint.vy = coord->workm.t[1];
    projection->worldPoint.vz = coord->workm.t[2];
    // Project the composed centre; the puff rotates in screen space.
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&projection->worldPoint);
    gte_rtps();
    quad           = gGpuPrimCursor;
    gGpuPrimCursor = quad + 1;
    setPolyFT4(quad);
    gte_stsxy(&projection->screenX);
    gte_stflg(&projection->projectionFlags);
    if (projection->projectionFlags >= 0) {
        gte_stszotz(&projection->depth);
        if (projection->depth >= MAIN_STREET_PUFF_MIN_DEPTH) {
            cornerAngle = angle;
            quad->tpage = getTPage(0, GPU_BLEND_ADD, 704, 0);
            quad->clut  = getClut(48, 270);
            setSemiTrans(quad, 1);
            setShadeTex(quad, 1);
            leftU   = (frame % MAIN_STREET_PUFF_CELLS_PER_ROW) * MAIN_STREET_PUFF_CELL_WIDTH;
            topV    = (frame / MAIN_STREET_PUFF_CELLS_PER_ROW) * MAIN_STREET_PUFF_CELL_WIDTH;
            rightU  = leftU + MAIN_STREET_PUFF_UV_SPAN;
            bottomV = topV + MAIN_STREET_PUFF_TOP_V + MAIN_STREET_PUFF_UV_SPAN;
            topV   += MAIN_STREET_PUFF_TOP_V;
            setUV4(quad, leftU, topV, rightU, topV, leftU, bottomV, rightU, bottomV);
            // Two perpendicular half-diagonals supply the opposite corner pairs.
            _mainStreetComputePuffCornerOffset(projection, sizeFactor, cornerAngle);
            quad->x0           = projection->screenX + projection->cornerOffsetX;
            quad->x3           = projection->screenX - projection->cornerOffsetX;
            quad->y0           = projection->screenY - projection->cornerOffsetY;
            quad->y3           = projection->screenY + projection->cornerOffsetY;
            perpendicularAngle = cornerAngle + MAIN_STREET_PUFF_QUARTER_TURN;
            _mainStreetComputePuffCornerOffset(projection, sizeFactor, perpendicularAngle);
            quad->x1 = projection->screenX + projection->cornerOffsetX;
            quad->x2 = projection->screenX - projection->cornerOffsetX;
            quad->y1 = projection->screenY - projection->cornerOffsetY;
            quad->y2 = projection->screenY + projection->cornerOffsetY;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)projection->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    quad);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectBillboardScratch);
}
