/* Part of the sprite quad library; see sprite_quad.h.
 *
 * The flicker form: the frame's low bit alternates two looks of the flame
 * strip (tpage 0x29, v 0xC8-0xFF) - SPRITE_QUAD_CONFIGURE_FLICKER_ODD_PACKET(prim) and
 * SPRITE_QUAD_CONFIGURE_FLICKER_EVEN_PACKET(prim), statements the including
 * unit defines from SPRITE_QUAD_SET_FLICKER_ODD_CELL /
 * SPRITE_QUAD_SET_FLICKER_EVEN_CELL plus its own tint and blend flags. */

#ifndef SPRITE_QUAD_SCALE
#error Define SPRITE_QUAD_SCALE before including the flicker sprite quad drawer
#endif

#ifndef SPRITE_QUAD_CONFIGURE_FLICKER_ODD_PACKET
#error Define SPRITE_QUAD_CONFIGURE_FLICKER_ODD_PACKET before including the flicker sprite quad drawer
#endif

#ifndef SPRITE_QUAD_CONFIGURE_FLICKER_EVEN_PACKET
#error Define SPRITE_QUAD_CONFIGURE_FLICKER_EVEN_PACKET before including the flicker sprite quad drawer
#endif

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

/// Rotates the flicker quad's perspective half-diagonal into a pixel offset.
///
/// Borrows live scratch with positive depth; writes only its corner offsets.
/// `sizeFactor * SPRITE_QUAD_SCALE / depth` truncates toward zero before
/// multiplying Q12 trigonometric samples. Products must fit s32; angle uses
/// 4096 units per turn, zero upward and a quarter turn rightward.
static inline void _spriteQuadComputeFlickerCornerOffset(EffectShapeScratch* scratch, s16 sizeFactor, s32 cornerAngle)
{
    enum { SPRITE_QUAD_FLICKER_TRIG_FRACTION_BITS = 12 };

    // Check in C so a carrier may bind an enum constant as well as a literal.
    STATIC_ASSERT((SPRITE_QUAD_SCALE) > 0, sprite_quad_flicker_scale_is_positive);
    STATIC_ASSERT(sizeof(SPRITE_QUAD_SCALE) <= sizeof(s32) && (SPRITE_QUAD_SCALE) * -1 < 0, sprite_quad_flicker_scale_uses_signed_word_arithmetic);

    scratch->extent.corner.x = (((sizeFactor * (SPRITE_QUAD_SCALE)) / scratch->depth) * rsin(cornerAngle)) >> SPRITE_QUAD_FLICKER_TRIG_FRACTION_BITS;
    scratch->extent.corner.y = (((sizeFactor * (SPRITE_QUAD_SCALE)) / scratch->depth) * rcos(cornerAngle)) >> SPRITE_QUAD_FLICKER_TRIG_FRACTION_BITS;
}

/// Places the flicker quad's opposite corner pairs around its projected centre.
///
/// Requires live, disjoint scratch and packet storage with a projected centre
/// and positive depth. `sizeFactor * SPRITE_QUAD_SCALE / depth` is the signed
/// pixel half-diagonal, truncated before Q12 rotation; products must fit s32.
/// The carrier supplies the signed-integer scale binding. `spinAngle` uses
/// 4096 units per turn, zero upward and a quarter turn rightward. Writes all
/// four packet XY pairs and scratch corner offsets; GPU sums retain their low
/// 16 bits. No packet is reserved or queued here and no pointer is retained.
static inline void _spriteQuadSetFlickerCorners(EffectShapeScratch* scratch, POLY_FT4* prim, s16 sizeFactor, s16 spinAngle)
{
    enum { SPRITE_QUAD_FLICKER_QUARTER_TURN = 1024 };
    s32 cornerAngle;
    // Opposite corner pairs use directions a quarter turn apart.
    cornerAngle = spinAngle;
    _spriteQuadComputeFlickerCornerOffset(scratch, sizeFactor, cornerAngle);
    prim->x0    = scratch->screenX + (u16)scratch->extent.corner.x;
    prim->x3    = scratch->screenX - (u16)scratch->extent.corner.x;
    prim->y0    = scratch->screenY - (u16)scratch->extent.corner.y;
    prim->y3    = scratch->screenY + (u16)scratch->extent.corner.y;
    cornerAngle = cornerAngle + SPRITE_QUAD_FLICKER_QUARTER_TURN;
    _spriteQuadComputeFlickerCornerOffset(scratch, sizeFactor, cornerAngle);
    prim->x1 = scratch->screenX + (u16)scratch->extent.corner.x;
    prim->x2 = scratch->screenX - (u16)scratch->extent.corner.x;
    prim->y1 = scratch->screenY - (u16)scratch->extent.corner.y;
    prim->y2 = scratch->screenY + (u16)scratch->extent.corner.y;
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
            SPRITE_QUAD_CONFIGURE_FLICKER_ODD_PACKET(prim);
        } else {
            SPRITE_QUAD_CONFIGURE_FLICKER_EVEN_PACKET(prim);
        }
        _spriteQuadSetFlickerCorners(block, prim, sizeFactor, spinAngle);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(block->depth),
                prim);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
}

#undef SPRITE_QUAD_CONFIGURE_FLICKER_ODD_PACKET
#undef SPRITE_QUAD_CONFIGURE_FLICKER_EVEN_PACKET
#undef SPRITE_QUAD_RESERVE_BEFORE_PROJECTION_CHECK
#undef SPRITE_QUAD_SCALE
