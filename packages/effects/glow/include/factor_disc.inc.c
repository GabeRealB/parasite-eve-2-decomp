/* Part of the glow drawing library; see glow.h. */

/// The drawer's name, `_glowDrawFactorDisc` by default, and the bits of its color's green and blue factors, 2 by
/// default; the bit discs bind 1.
#ifndef GLOW_FACTOR_DISC_FUNC
#define GLOW_FACTOR_DISC_FUNC _glowDrawFactorDisc
#endif
#ifndef GLOW_FACTOR_DISC_BITS
#define GLOW_FACTOR_DISC_BITS 2
#endif

/// Takes a disc wedge from the packet arena, colored at its center and black at the rim.
static inline POLY_G4* _glowAllocateFactorDiscWedge(u8 red, u8 green, u8 blue)
{
    POLY_G4* wedge;

    wedge          = gGpuPrimCursor;
    gGpuPrimCursor = wedge + 1;
    setPolyG4(wedge);
    setRGB0(wedge, 0, 0, 0);
    setRGB1(wedge, 0, 0, 0);
    setRGB2(wedge, red, green, blue);
    setRGB3(wedge, 0, 0, 0);
    return wedge;
}

/// Draws a flickering tinted disc of four wedges around a world point, sized by `radiusScale` in perspective; a point
/// too near draws nothing. `packedColor` multiplies the flicker by a red factor in bits 8..15 and two-bit green and
/// blue ones in bits 4..5 and 0..1.
static void GLOW_FACTOR_DISC_FUNC(const SVECTOR* worldPoint, s32 radiusScale, s32 packedColor)
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
        green         = brightness * ((shiftedColor >> 20) & ((1 << GLOW_FACTOR_DISC_BITS) - 1));
        blue          = brightness * (packedColor & ((1 << GLOW_FACTOR_DISC_BITS) - 1));
        angle         = 0;
        display       = displayState;
        block->radius = radius;
        // Build four glow wedges and quantize their shared camera depth.
        do {
            prim          = _glowAllocateFactorDiscWedge(red, green, blue);
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
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH_SCALED(block->otz, display->otDepthShift),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->otz);
        } while (angle < GLOW_FULL_TURN);
    }
    SCRATCH_STACK_RELEASE_BLOCK(GlowCentreRadiusScratch);
}

#undef GLOW_FACTOR_DISC_FUNC
#undef GLOW_FACTOR_DISC_BITS
