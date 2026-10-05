/* Part of the glow drawing library; see glow_draw.h. */

/// Allocates a centre-lit Gouraud quad in the current frame packet arena.
///
/// Requires one packet's space; colours narrow to bytes. The caller supplies
/// coordinates, ordering-table linkage and the blend command.
static inline POLY_G4* _glowAllocateBitDiscWedge(u8 red, u8 green, u8 blue)
{
    POLY_G4* prim;

    prim           = gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setPolyG4(prim);
    setRGB0(prim, 0, 0, 0);
    setRGB1(prim, 0, 0, 0);
    setRGB2(prim, red, green, blue);
    setRGB3(prim, 0, 0, 0);
    return prim;
}

/// Draws an additive disc with a red-byte factor and single-bit green/blue factors.
///
/// Borrows `worldPoint` for view projection. Depth is camera Z / 4 and must
/// be at least 17; projection flags are not tested. The signed low halfword
/// of `radiusScale` gives a pixel radius of `radiusScale * 64 / depth`.
/// Four centre-lit Gouraud wedges fade to a black rim.
///
/// `packedColor` bits 8..15 supply the red factor, bit 4 the green factor and
/// bit 0 the blue factor. Factors multiply the frame-parity intensity 32/40;
/// colour bytes wrap, and the signed red extraction is retained. Queues four
/// quads and additive blend commands in the current frame packet arena.
static void _glowDrawBitDisc(const SVECTOR* worldPoint, s32 radiusScale, s32 packedColor)
{
    enum {
        GLOW_MIN_DEPTH         = 17,
        GLOW_BRIGHTNESS_BASE   = 0x20,
        GLOW_BRIGHTNESS_STEP   = 8,
        GLOW_TRIG_SHIFT        = 12,
        GLOW_FULL_TURN         = 0x1000,
        GLOW_DISC_RADIUS_SCALE = 64,
        GLOW_DISC_WEDGE_ANGLE  = 0x400,
    };

    GlowCentreRadiusScratch* previousTop;
    GlowCentreRadiusScratch* block;
    POLY_G4*                 prim;
    DisplayState*            displayState;
    DisplayState*            display;
    s32                      radius;
    s32                      angle;
    s32                      halfStepAngle;
    s32                      nextAngle;
    s32                      shiftedColor;
    u8                       brightness;
    u8                       red;
    u8                       green;
    u8                       blue;

    previousTop = SCRATCH_STACK_CURSOR(GlowCentreRadiusScratch);
    block       = (SCRATCH_STACK_CURSOR(GlowCentreRadiusScratch) = previousTop - 1);

    // Project the world point before allocating its glow packets.
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(worldPoint);
    gte_rtps();
    gte_stsxy(&previousTop[-1].sx);
    gte_stszotz(&block->otz);
    if (previousTop[-1].otz >= GLOW_MIN_DEPTH) {
        radius        = ((s16)radiusScale * GLOW_DISC_RADIUS_SCALE) / previousTop[-1].otz;
        displayState  = &gDisplayState;
        shiftedColor  = packedColor << 16;
        brightness    = (((u8)displayState->animFrame & 1) * GLOW_BRIGHTNESS_STEP) | GLOW_BRIGHTNESS_BASE;
        red           = brightness * (shiftedColor >> 24);
        green         = brightness * ((shiftedColor >> 20) & 1);
        blue          = brightness * (packedColor & 1);
        angle         = 0;
        display       = displayState;
        block->radius = radius;
        // Build four glow wedges and quantize their shared camera depth.
        do {
            prim          = _glowAllocateBitDiscWedge(red, green, blue);
            prim->x0      = block->sx + ((block->radius * rsin(angle)) >> GLOW_TRIG_SHIFT);
            halfStepAngle = angle + GLOW_DISC_WEDGE_ANGLE / 2;
            prim->y0      = block->sy + ((block->radius * rcos(angle)) >> GLOW_TRIG_SHIFT);
            prim->x1      = block->sx + ((block->radius * rsin(halfStepAngle)) >> GLOW_TRIG_SHIFT);
            prim->y1      = block->sy + ((block->radius * rcos(halfStepAngle)) >> GLOW_TRIG_SHIFT);
            nextAngle     = angle + GLOW_DISC_WEDGE_ANGLE;
            prim->x2      = block->sx;
            prim->y2      = block->sy;
            prim->x3      = block->sx + ((block->radius * rsin(nextAngle)) >> GLOW_TRIG_SHIFT);
            prim->y3      = block->sy + ((block->radius * rcos(nextAngle)) >> GLOW_TRIG_SHIFT);
            angle         = nextAngle;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << display->otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->otz);
        } while (angle < GLOW_FULL_TURN);
    }
    SCRATCH_STACK_RELEASE_BLOCK(GlowCentreRadiusScratch);
}
