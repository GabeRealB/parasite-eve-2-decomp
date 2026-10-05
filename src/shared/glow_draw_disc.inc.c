/* Part of the glow drawing library; see glow_draw.h. */

/// Reserves a disc wedge packet with one coloured centre vertex and a black rim.
///
/// Advances the current frame's packet cursor by one `POLY_G4`. The returned
/// quad has its command and four RGB groups initialized, with colour at vertex 2.
/// Coordinates, ordering-table linkage and semitransparency remain for the caller.
/// The arena must have room; the packet lives until the frame's GPU work finishes.
static inline POLY_G4* _glowAllocateDiscWedge(u8 red, u8 green, u8 blue)
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

void glowDrawDisc(const SVECTOR* worldPoint, s32 radiusScale, s32 packedColor)
{
    enum {
        GLOW_DISC_TRIG_SHIFT     = 12,
        GLOW_DISC_FULL_TURN      = 0x1000,
        GLOW_DISC_WEDGE_ANGLE    = 0x400,
        GLOW_DISC_RADIUS_SCALE   = 64,
        GLOW_DISC_FLICKER_LEVEL  = 8,
        GLOW_DISC_PULL_THRESHOLD = 0x50,
    };

    GLOW_DRAW_DISC_SCRATCH* block;
    POLY_G4*                prim;
    s32                     angle;
    s32                     halfStepAngle;
    s32                     nextAngle;
    s32                     shiftedColor;
    s32                     flicker;
    s32                     redNibble;
    s32                     greenNibble;
    u8                      red;
    u8                      green;
    u8                      blue;
#ifdef GLOW_DRAW_DISC_PULL
    s32 originalDepth;
#endif

    block = SCRATCH_STACK_RESERVE_BLOCK(GLOW_DRAW_DISC_SCRATCH);
    // Project once; all four wedges share the centre and sorting depth.
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(worldPoint);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
#ifdef GLOW_DRAW_DISC_PULL
        originalDepth = block->otz;
        if (originalDepth > GLOW_DISC_PULL_THRESHOLD) {
            block->otz = originalDepth - GLOW_DRAW_DISC_PULL;
        }
        // Preserve the signed low-halfword scaling before the depth divide.
        radiusScale = radiusScale << 16;
        radiusScale = radiusScale >> 10;
        radiusScale = radiusScale / block->otz;
#else
        radiusScale = ((s16)radiusScale * GLOW_DISC_RADIUS_SCALE) / block->otz;
#endif
        angle = 0;
#if GLOW_DRAW_DISC_SHIFTED_FLICKER
        // This variant adds the encoded flicker level; the default inserts bit 3.
        shiftedColor = packedColor << 16;
        flicker      = (gDisplayState.animFrame & 1) << (shiftedColor >> 28);
#else
        flicker      = ((u8)gDisplayState.animFrame & 1) * GLOW_DISC_FLICKER_LEVEL;
        shiftedColor = packedColor << 16;
#endif
        redNibble   = (shiftedColor >> 20) & 0xF0;
        greenNibble = (shiftedColor >> 16) & 0xF0;
#if GLOW_DRAW_DISC_SHIFTED_FLICKER
        red   = flicker + redNibble;
        green = flicker + greenNibble;
        blue  = flicker + ((packedColor & 0xF) << 4);
#else
        red   = flicker | redNibble;
        green = flicker | greenNibble;
        blue  = flicker | ((packedColor & 0xF) << 4);
#endif
        block->radius = radiusScale;
        do {
            prim          = _glowAllocateDiscWedge(red, green, blue);
            prim->x0      = block->sx + ((block->radius * rsin(angle)) >> GLOW_DISC_TRIG_SHIFT);
            halfStepAngle = angle + GLOW_DISC_WEDGE_ANGLE / 2;
            prim->y0      = block->sy + ((block->radius * rcos(angle)) >> GLOW_DISC_TRIG_SHIFT);
            prim->x1      = block->sx + ((block->radius * rsin(halfStepAngle)) >> GLOW_DISC_TRIG_SHIFT);
            prim->y1      = block->sy + ((block->radius * rcos(halfStepAngle)) >> GLOW_DISC_TRIG_SHIFT);
            nextAngle     = angle + GLOW_DISC_WEDGE_ANGLE;
            prim->x2      = block->sx;
            prim->y2      = block->sy;
            prim->x3      = block->sx + ((block->radius * rsin(nextAngle)) >> GLOW_DISC_TRIG_SHIFT);
            prim->y3      = block->sy + ((block->radius * rcos(nextAngle)) >> GLOW_DISC_TRIG_SHIFT);
            angle         = nextAngle;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->otz);
        } while (angle < GLOW_DISC_FULL_TURN);
    }
    SCRATCH_STACK_RELEASE_BLOCK(GLOW_DRAW_DISC_SCRATCH);
}

#undef GLOW_DRAW_DISC_PULL
#undef GLOW_DRAW_DISC_SHIFTED_FLICKER
