/* Part of the glow drawing library; see glow_draw.h. */

/// Sets a tinted wedge's coloured centre and black rim in an allocated quad.
static inline void _glowInitTintedDiscWedge(POLY_G4* prim, s32 red, s32 green, s32 blue)
{
    setPolyG4(prim);
    setRGB0(prim, 0, 0, 0);
    setRGB1(prim, 0, 0, 0);
    setRGB2(prim, red, green, blue);
    setRGB3(prim, 0, 0, 0);
}

void glowDrawTintedDiscNoBias(const SVECTOR* worldPoint, s32 radiusScale, s32 packedColor)
{
    enum {
        GLOW_TINTED_DISC_TRIG_SHIFT   = 12,
        GLOW_TINTED_DISC_FULL_TURN    = 0x1000,
        GLOW_TINTED_DISC_WEDGE_ANGLE  = 0x200,
        GLOW_TINTED_DISC_QUARTER_TURN = 0x400,
        GLOW_TINTED_DISC_HALF_TURN    = 0x800,
        GLOW_TINTED_DISC_OUTER_SCALE  = 64,
        GLOW_TINTED_DISC_INNER_SCALE  = 8,
    };

    GlowCentreRadiiScratch* block;
    POLY_G4*                prim;
    s32                     angle;
    s32                     halfStepAngle;
    s32                     nextAngle;
    s32                     previousArmAngle;
    s32                     nextArmAngle;
    s32                     oppositeArmAngle;
    s32                     frameParity;
    s32                     shiftedColor;
    s32                     flicker;
    s32                     red;
    s32                     green;
    s32                     blue;
    s32                     outerRadius;
    s32                     innerRadius;
    s32                     halfRed;
    s32                     halfGreen;
    s32                     halfBlue;

    block = SCRATCH_STACK_RESERVE_BLOCK(GlowCentreRadiiScratch);

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(worldPoint);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        // Preserve the signed low halfword before computing both screen radii.
        radiusScale      <<= 16;
        radiusScale      >>= 16;
        outerRadius        = (radiusScale * GLOW_TINTED_DISC_OUTER_SCALE) / block->otz;
        frameParity        = gDisplayState.animFrame;
        block->outerRadius = outerRadius;
        innerRadius        = (radiusScale * GLOW_TINTED_DISC_INNER_SCALE) / block->otz;
        angle              = 0;
        shiftedColor       = packedColor << 16;
        flicker            = (frameParity & 1) << (shiftedColor >> 28);
        red                = flicker + ((shiftedColor >> 20) & 0xF0);
        green              = flicker + ((shiftedColor >> 16) & 0xF0);
        blue               = flicker + ((packedColor & 0xF) << 4);
        block->innerRadius = innerRadius;
        // Layer half-bright outer wedges with full-bright wedges at half radius.
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            halfRed = (u8)red >> 1;
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            halfGreen = (u8)green >> 1;
            halfBlue  = (u8)blue >> 1;
            setRGB2(prim, halfRed, halfGreen, halfBlue);
            setRGB3(prim, 0, 0, 0);
            prim->x0      = block->sx + ((block->outerRadius * rsin(angle)) >> GLOW_TINTED_DISC_TRIG_SHIFT);
            halfStepAngle = angle + GLOW_TINTED_DISC_WEDGE_ANGLE / 2;
            prim->y0      = block->sy + ((block->outerRadius * rcos(angle)) >> GLOW_TINTED_DISC_TRIG_SHIFT);
            prim->x1      = block->sx + ((block->outerRadius * rsin(halfStepAngle)) >> GLOW_TINTED_DISC_TRIG_SHIFT);
            prim->y1      = block->sy + ((block->outerRadius * rcos(halfStepAngle)) >> GLOW_TINTED_DISC_TRIG_SHIFT);
            nextAngle     = angle + GLOW_TINTED_DISC_WEDGE_ANGLE;
            prim->x2      = block->sx;
            prim->y2      = block->sy;
            prim->x3      = block->sx + ((block->outerRadius * rsin(nextAngle)) >> GLOW_TINTED_DISC_TRIG_SHIFT);
            prim->y3      = block->sy + ((block->outerRadius * rcos(nextAngle)) >> GLOW_TINTED_DISC_TRIG_SHIFT);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->otz);

            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            _glowInitTintedDiscWedge(prim, red, green, blue);
            prim->x0 = block->sx + ((block->outerRadius * rsin(angle)) >> (GLOW_TINTED_DISC_TRIG_SHIFT + 1));
            prim->y0 = block->sy + ((block->outerRadius * rcos(angle)) >> (GLOW_TINTED_DISC_TRIG_SHIFT + 1));
            prim->x1 = block->sx + ((block->outerRadius * rsin(halfStepAngle)) >> (GLOW_TINTED_DISC_TRIG_SHIFT + 1));
            prim->y1 = block->sy + ((block->outerRadius * rcos(halfStepAngle)) >> (GLOW_TINTED_DISC_TRIG_SHIFT + 1));
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->outerRadius * rsin(nextAngle)) >> (GLOW_TINTED_DISC_TRIG_SHIFT + 1));
            prim->y3 = block->sy + ((block->outerRadius * rcos(nextAngle)) >> (GLOW_TINTED_DISC_TRIG_SHIFT + 1));
            angle    = nextAngle;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->otz);
        } while (angle < GLOW_TINTED_DISC_FULL_TURN);

        // Overlay four blades; alternating tips use the radius and twice the radius.
        angle = GLOW_TINTED_DISC_WEDGE_ANGLE;
        red   = (u8)halfRed;
        green = (u8)halfGreen;
        blue  = (u8)halfBlue;
        do {
            previousArmAngle = angle - GLOW_TINTED_DISC_QUARTER_TURN;
            prim             = gGpuPrimCursor;
            gGpuPrimCursor   = prim + 1;
            _glowInitTintedDiscWedge(prim, red, green, blue);
            prim->x0     = block->sx + ((block->innerRadius * rsin(previousArmAngle)) >> (GLOW_TINTED_DISC_TRIG_SHIFT + 1));
            prim->y0     = block->sy + ((block->innerRadius * rcos(previousArmAngle)) >> (GLOW_TINTED_DISC_TRIG_SHIFT + 1));
            prim->x1     = block->sx + ((block->outerRadius * rsin(angle)) >> GLOW_TINTED_DISC_TRIG_SHIFT);
            prim->y1     = block->sy + ((block->outerRadius * rcos(angle)) >> GLOW_TINTED_DISC_TRIG_SHIFT);
            nextArmAngle = angle + GLOW_TINTED_DISC_QUARTER_TURN;
            prim->x2     = block->sx;
            prim->y2     = block->sy;
            prim->x3     = block->sx + ((block->innerRadius * rsin(nextArmAngle)) >> (GLOW_TINTED_DISC_TRIG_SHIFT + 1));
            prim->y3     = block->sy + ((block->innerRadius * rcos(nextArmAngle)) >> (GLOW_TINTED_DISC_TRIG_SHIFT + 1));
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->otz);

            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            _glowInitTintedDiscWedge(prim, red, green, blue);
            prim->x0         = block->sx + ((block->innerRadius * rsin(angle)) >> GLOW_TINTED_DISC_TRIG_SHIFT);
            prim->y0         = block->sy + ((block->innerRadius * rcos(angle)) >> GLOW_TINTED_DISC_TRIG_SHIFT);
            prim->x1         = block->sx + ((block->outerRadius * rsin(nextArmAngle)) >> (GLOW_TINTED_DISC_TRIG_SHIFT - 1));
            prim->y1         = block->sy + ((block->outerRadius * rcos(nextArmAngle)) >> (GLOW_TINTED_DISC_TRIG_SHIFT - 1));
            oppositeArmAngle = angle + GLOW_TINTED_DISC_HALF_TURN;
            prim->x2         = block->sx;
            prim->y2         = block->sy;
            prim->x3         = block->sx + ((block->innerRadius * rsin(oppositeArmAngle)) >> GLOW_TINTED_DISC_TRIG_SHIFT);
            prim->y3         = block->sy + ((block->innerRadius * rcos(oppositeArmAngle)) >> GLOW_TINTED_DISC_TRIG_SHIFT);
            angle            = oppositeArmAngle;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->otz);
        } while (angle < GLOW_TINTED_DISC_FULL_TURN);
    }
    SCRATCH_STACK_RELEASE_BLOCK(GlowCentreRadiiScratch);
}
