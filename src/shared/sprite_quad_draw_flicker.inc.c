/* Part of the sprite quad library; see sprite_quad.h.
 *
 * The flicker form: the frame's low bit alternates two looks of the flame
 * strip (tpage 0x29, v 0xC8-0xFF) - SPRITE_QUAD_ODD_LOOK(prim) and
 * SPRITE_QUAD_EVEN_LOOK(prim), statements the including unit defines from
 * SPRITE_QUAD_CORE_CELL / SPRITE_QUAD_RIM_CELL plus its own tint and blend
 * flags. */

#ifndef SPRITE_QUAD_RESERVE_BEFORE_PROJECTION_CHECK
/// Selects when a flicker draw reserves and initializes its `POLY_FT4` packet.
///
/// Bind to an integer preprocessor expression before including this fragment:
/// 0 (default) reserves only after the GTE FLAG word accepts the projection;
/// nonzero reserves before reading FLAG, so even a rejected projection advances
/// `gGpuPrimCursor` by one `POLY_FT4` and initializes the packet header. Rejected
/// packets are never linked into the ordering table or reclaimed by this draw.
/// The frame's packet arena must have room for every reservation.
///
/// `combustion` and `energyball` use the default; `dryfield_dilapidated_house`
/// binds 1. This fragment undefines the binding after defining its drawer.
#define SPRITE_QUAD_RESERVE_BEFORE_PROJECTION_CHECK 0
#endif

/// Places the flicker quad's opposite corner pairs around its projected centre.
///
/// Borrows live scratch and packet storage. Requires a positive depth and the
/// carrier's signed-integer SPRITE_QUAD_SCALE binding. sizeFactor controls the
/// perspective half-diagonal; spinAngle uses 4096 units per turn. GPU corner
/// sums retain their low 16 bits. No packet is reserved or queued here.
static inline void _spriteQuadSetFlickerCorners(EffectShapeScratch* block, POLY_FT4* prim, s16 sizeFactor, s16 spinAngle)
{
    enum { SPRITE_QUAD_FLICKER_TRIG_FRACTION_BITS = 12,
           SPRITE_QUAD_FLICKER_QUARTER_TURN       = 1024 };
    s32 cornerAngle;
    // Opposite corner pairs use directions a quarter turn apart.
    cornerAngle            = spinAngle;
    block->extent.corner.x = (((sizeFactor * SPRITE_QUAD_SCALE) / block->depth) * rsin(cornerAngle)) >> SPRITE_QUAD_FLICKER_TRIG_FRACTION_BITS;
    block->extent.corner.y = (((sizeFactor * SPRITE_QUAD_SCALE) / block->depth) * rcos(cornerAngle)) >> SPRITE_QUAD_FLICKER_TRIG_FRACTION_BITS;
    prim->x0               = block->screenX + (u16)block->extent.corner.x;
    prim->x3               = block->screenX - (u16)block->extent.corner.x;
    prim->y0               = block->screenY - (u16)block->extent.corner.y;
    prim->y3               = block->screenY + (u16)block->extent.corner.y;
    cornerAngle            = cornerAngle + SPRITE_QUAD_FLICKER_QUARTER_TURN;
    block->extent.corner.x = (((sizeFactor * SPRITE_QUAD_SCALE) / block->depth) * rsin(cornerAngle)) >> SPRITE_QUAD_FLICKER_TRIG_FRACTION_BITS;
    block->extent.corner.y = (((sizeFactor * SPRITE_QUAD_SCALE) / block->depth) * rcos(cornerAngle)) >> SPRITE_QUAD_FLICKER_TRIG_FRACTION_BITS;
    prim->x1               = block->screenX + (u16)block->extent.corner.x;
    prim->x2               = block->screenX - (u16)block->extent.corner.x;
    prim->y1               = block->screenY - (u16)block->extent.corner.y;
    prim->y2               = block->screenY + (u16)block->extent.corner.y;
}

/// Draws a spinning billboard alternating the carrier's two flame looks.
///
/// Borrows the coordinate's already-composed translation in GsWSMATRIX input
/// space, narrowed to s16 for projection. frame parity selects the look;
/// sizeFactor * SPRITE_QUAD_SCALE / (SZ3/4+1) is the pixel half-diagonal.
/// spinAngle uses 4096 units per turn. Rejected projections draw nothing;
/// the reservation binding decides whether they still consume a packet.
/// Accepted quads sort one depth unit behind the centre. Borrows scratch for
/// this call and requires room for one FT4 in the frame's live packet arena.
static void _spriteQuadDrawFlicker(const GfxCoord* coord, s16 frame, s16 sizeFactor, s16 spinAngle)
{
    EffectShapeScratch* block;
    POLY_FT4*           prim;

    block                = SCRATCH_STACK_RESERVE_BLOCK(EffectShapeScratch);
    block->worldPoint.vx = coord->workm.t[0];
    block->worldPoint.vy = coord->workm.t[1];
    block->worldPoint.vz = coord->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->worldPoint);
    gte_rtps();
#if SPRITE_QUAD_RESERVE_BEFORE_PROJECTION_CHECK
    // Rejected projections still consume a packet; only accepted ones are linked.
    prim           = gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setPolyFT4(prim);
#endif
    gte_stsxy(&block->screenX);
    gte_stflg(&block->projectionFlags);
    if (block->projectionFlags >= 0) {
        gte_stszotz(&block->depth);
        block->depth++;
#if !SPRITE_QUAD_RESERVE_BEFORE_PROJECTION_CHECK
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setPolyFT4(prim);
#endif
        if (frame & 1) {
            SPRITE_QUAD_ODD_LOOK(prim);
        } else {
            SPRITE_QUAD_EVEN_LOOK(prim);
        }
        _spriteQuadSetFlickerCorners(block, prim, sizeFactor, spinAngle);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
}

#undef SPRITE_QUAD_ODD_LOOK
#undef SPRITE_QUAD_EVEN_LOOK
#undef SPRITE_QUAD_RESERVE_BEFORE_PROJECTION_CHECK
#undef SPRITE_QUAD_SCALE
