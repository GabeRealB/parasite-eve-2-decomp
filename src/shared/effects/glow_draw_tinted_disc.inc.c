/* Part of the glow drawing library; see glow_draw.h. */

/// Prepares a tinted disc wedge or blade with an RGB centre and a black rim.
///
/// Borrows one writable `POLY_G4`, setting its opaque Gouraud command and
/// packet length. Vertex 2 receives the low byte of each colour argument;
/// vertices 0, 1 and 3 are black. The caller supplies coordinates, DMA linkage
/// and semitransparency for both the inner disc and its overlaid blades.
static inline void _glowInitBiasedTintedDiscWedge(POLY_G4* wedge, s32 red, s32 green, s32 blue)
{
    setPolyG4(wedge);
    setRGB0(wedge, 0, 0, 0);
    setRGB1(wedge, 0, 0, 0);
    setRGB2(wedge, red, green, blue);
    setRGB3(wedge, 0, 0, 0);
}

/// Draws layered tinted discs and four glow blades at a biased world-point depth.
///
/// The view projection's camera Z / 4 is incremented by one before sizing and
/// sorting. The signed low halfword of `radiusScale` gives outer and inner pixel
/// radii of `radiusScale * 64 / depth` and `radiusScale * 8 / depth`.
/// Eight half-bright outer wedges receive full-bright copies at half radius.
/// Four half-bright blades overlay them, with alternating tips at the outer
/// radius and twice it, with shoulders at half the inner radius on shorter blades.
/// `packedColor` bits 8..11, 4..7 and 0..3 are RGB nibbles scaled by 16;
/// bits 12..15 select an odd-frame addition of `1 << shift` to each byte.
/// Shift must be 0..7; colour bytes wrap before halving. Rejects negative GTE
/// flags. Borrows `worldPoint` for the call and queues twenty additive quads
/// plus blend commands in the current frame.
static void _glowDrawTintedDisc(const SVECTOR* worldPoint, s32 radiusScale, s32 packedColor)
{
    GlowCentreRadiiScratch* block;
    POLY_G4*                prim;
    const DisplayState*     display;
    s32                     shiftedColor;
    s32                     flicker;
    s32                     signedRadiusScale;
    s32                     biasedDepth;
    s32                     outerRadius;
    s32                     innerRadius;
    s32                     angle;
    s32                     halfStepAngle;
    s32                     nextAngle;
    s32                     red;
    s32                     green;
    s32                     blue;
    s32                     halfRed;
    s32                     halfGreen;
    s32                     halfBlue;

    block = SCRATCH_STACK_RESERVE_BLOCK(GlowCentreRadiiScratch);

    // Project into the current view before deriving screen radii and sorting depths.
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(worldPoint);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        signedRadiusScale  = (s16)radiusScale;
        biasedDepth        = block->otz + 1;
        outerRadius        = (signedRadiusScale * GLOW_RADIUS_SCALE) / biasedDepth;
        block->otz         = biasedDepth;
        display            = &gDisplayState;
        flicker            = display->animFrame;
        block->outerRadius = outerRadius;
        innerRadius        = (signedRadiusScale * GLOW_INNER_RADIUS_SCALE) / block->otz;
        shiftedColor       = packedColor << 16;
        flicker            = flicker & 1;
        flicker            = flicker << (shiftedColor >> 28);
        red                = flicker + ((shiftedColor >> 20) & 0xF0);
        green              = flicker + ((shiftedColor >> 16) & 0xF0);
        blue               = flicker + ((packedColor & 0xF) << 4);
        block->innerRadius = innerRadius;
        angle              = 0;
        // Layer eight outer wedges with full-bright copies at half radius.
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            halfRed   = (u8)red >> 1;
            halfGreen = (u8)green >> 1;
            halfBlue  = (u8)blue >> 1;
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, halfRed, halfGreen, halfBlue);
            setRGB3(prim, 0, 0, 0);
            prim->x0      = block->sx + ((block->outerRadius * rsin(angle)) >> GLOW_TRIG_SHIFT);
            halfStepAngle = angle + GLOW_SIXTEENTH_TURN;
            prim->y0      = block->sy + ((block->outerRadius * rcos(angle)) >> GLOW_TRIG_SHIFT);
            prim->x1      = block->sx + ((block->outerRadius * rsin(halfStepAngle)) >> GLOW_TRIG_SHIFT);
            prim->y1      = block->sy + ((block->outerRadius * rcos(halfStepAngle)) >> GLOW_TRIG_SHIFT);
            nextAngle     = angle + GLOW_EIGHTH_TURN;
            prim->x2      = block->sx;
            prim->y2      = block->sy;
            prim->x3      = block->sx + ((block->outerRadius * rsin(nextAngle)) >> GLOW_TRIG_SHIFT);
            prim->y3      = block->sy + ((block->outerRadius * rcos(nextAngle)) >> GLOW_TRIG_SHIFT);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->otz);

            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            _glowInitBiasedTintedDiscWedge(prim, red, green, blue);
            prim->x0 = block->sx + ((block->outerRadius * rsin(angle)) >> (GLOW_TRIG_SHIFT + 1));
            prim->y0 = block->sy + ((block->outerRadius * rcos(angle)) >> (GLOW_TRIG_SHIFT + 1));
            prim->x1 = block->sx + ((block->outerRadius * rsin(halfStepAngle)) >> (GLOW_TRIG_SHIFT + 1));
            prim->y1 = block->sy + ((block->outerRadius * rcos(halfStepAngle)) >> (GLOW_TRIG_SHIFT + 1));
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->outerRadius * rsin(nextAngle)) >> (GLOW_TRIG_SHIFT + 1));
            prim->y3 = block->sy + ((block->outerRadius * rcos(nextAngle)) >> (GLOW_TRIG_SHIFT + 1));
            angle    = nextAngle;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->otz);
        } while (angle < GLOW_FULL_TURN);

        // Overlay four blades with alternating normal and doubled tips.
        red   = (u8)halfRed;
        green = (u8)halfGreen;
        blue  = (u8)halfBlue;
        angle = GLOW_EIGHTH_TURN;
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            _glowInitBiasedTintedDiscWedge(prim, red, green, blue);
            prim->x0 = block->sx + ((block->innerRadius * rsin(angle - GLOW_QUARTER_TURN)) >> (GLOW_TRIG_SHIFT + 1));
            prim->y0 = block->sy + ((block->innerRadius * rcos(angle - GLOW_QUARTER_TURN)) >> (GLOW_TRIG_SHIFT + 1));
            prim->x1 = block->sx + ((block->outerRadius * rsin(angle)) >> GLOW_TRIG_SHIFT);
            prim->y1 = block->sy + ((block->outerRadius * rcos(angle)) >> GLOW_TRIG_SHIFT);
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->innerRadius * rsin(angle + GLOW_QUARTER_TURN)) >> (GLOW_TRIG_SHIFT + 1));
            prim->y3 = block->sy + ((block->innerRadius * rcos(angle + GLOW_QUARTER_TURN)) >> (GLOW_TRIG_SHIFT + 1));
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->otz);

            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            _glowInitBiasedTintedDiscWedge(prim, red, green, blue);
            prim->x0 = block->sx + ((block->innerRadius * rsin(angle)) >> GLOW_TRIG_SHIFT);
            prim->y0 = block->sy + ((block->innerRadius * rcos(angle)) >> GLOW_TRIG_SHIFT);
            prim->x1 = block->sx + ((block->outerRadius * rsin(angle + GLOW_QUARTER_TURN)) >> (GLOW_TRIG_SHIFT - 1));
            prim->y1 = block->sy + ((block->outerRadius * rcos(angle + GLOW_QUARTER_TURN)) >> (GLOW_TRIG_SHIFT - 1));
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->innerRadius * rsin(angle + GLOW_HALF_TURN)) >> GLOW_TRIG_SHIFT);
            prim->y3 = block->sy + ((block->innerRadius * rcos(angle + GLOW_HALF_TURN)) >> GLOW_TRIG_SHIFT);
            angle   += GLOW_HALF_TURN;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->otz);
        } while (angle < GLOW_FULL_TURN);
    }
    SCRATCH_STACK_RELEASE_BLOCK(GlowCentreRadiiScratch);
}
