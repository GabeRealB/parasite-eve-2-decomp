/* Part of the glow drawing library; see glow_draw.h. */

/// Initializes an allocated grey wedge with vertex 2 lit and the rim black.
///
/// Borrows one writable packet; brightness narrows to a byte. The caller
/// supplies allocation, geometry, ordering-table link and blend command.
static inline void _glowInitGreyCapsuleWedge(POLY_G4* prim, s32 brightness)
{
    setPolyG4(prim);
    setRGB0(prim, 0, 0, 0);
    setRGB1(prim, 0, 0, 0);
    prim->r2 = brightness;
    prim->g2 = brightness;
    prim->b2 = brightness;
    prim->r3 = 0;
    prim->g3 = 0;
    prim->b3 = 0;
}

/// Draws an additive grey capsule between two adjacent world points.
///
/// Borrows `worldPoints[0..1]` for view projection. The second camera Z / 4
/// depth must be at least 17; the first is clamped to 16. Projection flags
/// are not tested. Each pixel radius is the signed low halfword of
/// `radiusScale` times 64 divided by that endpoint's depth.
///
/// The signed low halfword of `startAngle` orients the two half-disc caps
/// in 4096 units per turn, with zero pointing down the screen. Two joining
/// bands sort at the first endpoint's depth. Lit vertices alternate grey
/// 32/48 with frame parity and the rim is black. Queues six quads plus
/// additive blend commands in the current frame packet arena.
static void _glowDrawGreyCapsule(const SVECTOR worldPoints[2], s32 radiusScale, s32 startAngle)
{
    enum {
        GLOW_GREY_CAPSULE_MIN_DEPTH        = 17,
        GLOW_GREY_CAPSULE_NEAR_DEPTH_CLAMP = 16,
        GLOW_GREY_CAPSULE_RADIUS_SCALE     = 64,
        GLOW_GREY_CAPSULE_BRIGHTNESS_BASE  = 32,
        GLOW_GREY_CAPSULE_BRIGHTNESS_STEP  = 16,
        GLOW_GREY_CAPSULE_TRIG_SHIFT       = 12,
        GLOW_GREY_CAPSULE_FULL_TURN        = 4096,
        GLOW_GREY_CAPSULE_HALF_TURN        = 2048,
        GLOW_GREY_CAPSULE_WEDGE_ANGLE      = 1024,
    };

    GlowPointPairScratch* block;
    POLY_G4*              prim;
    const SVECTOR*        endPoint;
    s32                   angle;
    s32                   rimAngle;
    s32                   nextAngle;
    s32                   endRimAngle;
    s32                   endMidAngle;
    s32                   endNextAngle;
    s32                   brightness;
    s32                   scaledRadius;
    s32                   startRadius;
    s32                   endRadius;
    s32                   angleOffset;

    endPoint = worldPoints + 1;
    block    = SCRATCH_STACK_RESERVE_BLOCK(GlowPointPairScratch);

    // Project both world endpoints; only the second depth rejects the capsule.
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(worldPoints);
    gte_rtps();
    gte_stsxy(&block->sx0);
    gte_stszotz(&block->otz0);
    gte_ldv0(endPoint);
    gte_rtps();
    gte_stsxy(&block->sx1);
    gte_stszotz(&block->otz1);
    if (block->otz1 >= GLOW_GREY_CAPSULE_MIN_DEPTH) {
        if (block->otz0 < GLOW_GREY_CAPSULE_NEAR_DEPTH_CLAMP) {
            block->otz0 = GLOW_GREY_CAPSULE_NEAR_DEPTH_CLAMP;
        }
        scaledRadius   = (s16)radiusScale * GLOW_GREY_CAPSULE_RADIUS_SCALE;
        startRadius    = scaledRadius / block->otz0;
        endRadius      = scaledRadius / block->otz1;
        angle          = 0;
        angleOffset    = (s16)startAngle;
        brightness     = (((u8)gDisplayState.animFrame & 1) * GLOW_GREY_CAPSULE_BRIGHTNESS_STEP) | GLOW_GREY_CAPSULE_BRIGHTNESS_BASE;
        block->radius0 = startRadius;
        block->radius1 = endRadius;
        // Join opposing half-disc caps with two bands.
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            _glowInitGreyCapsuleWedge(prim, brightness);
            prim->x0  = block->sx0 + ((block->radius0 * rsin(angleOffset + angle)) >> GLOW_GREY_CAPSULE_TRIG_SHIFT);
            prim->y0  = block->sy0 + ((block->radius0 * rcos(angleOffset + angle)) >> GLOW_GREY_CAPSULE_TRIG_SHIFT);
            rimAngle  = angle + GLOW_GREY_CAPSULE_WEDGE_ANGLE / 2;
            prim->x1  = block->sx0 + ((block->radius0 * rsin(angleOffset + rimAngle)) >> GLOW_GREY_CAPSULE_TRIG_SHIFT);
            prim->y1  = block->sy0 + ((block->radius0 * rcos(angleOffset + rimAngle)) >> GLOW_GREY_CAPSULE_TRIG_SHIFT);
            nextAngle = angle + GLOW_GREY_CAPSULE_WEDGE_ANGLE;
            prim->x2  = block->sx0;
            prim->y2  = block->sy0;
            prim->x3  = block->sx0 + ((block->radius0 * rsin(angleOffset + nextAngle)) >> GLOW_GREY_CAPSULE_TRIG_SHIFT);
            prim->y3  = block->sy0 + ((block->radius0 * rcos(angleOffset + nextAngle)) >> GLOW_GREY_CAPSULE_TRIG_SHIFT);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->otz0);

            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, brightness, brightness, brightness);
            setRGB3(prim, brightness, brightness, brightness);
            prim->x0 = block->sx0 + ((block->radius0 * rsin(angleOffset + (angle * 2))) >> GLOW_GREY_CAPSULE_TRIG_SHIFT);
            prim->y0 = block->sy0 + ((block->radius0 * rcos(angleOffset + (angle * 2))) >> GLOW_GREY_CAPSULE_TRIG_SHIFT);
            prim->x1 = block->sx1 + ((block->radius1 * rsin(angleOffset + (angle * 2))) >> GLOW_GREY_CAPSULE_TRIG_SHIFT);
            prim->y1 = block->sy1 + ((block->radius1 * rcos(angleOffset + (angle * 2))) >> GLOW_GREY_CAPSULE_TRIG_SHIFT);
            prim->x2 = block->sx0;
            prim->y2 = block->sy0;
            prim->x3 = block->sx1;
            prim->y3 = block->sy1;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->otz0);

            endRimAngle    = angle - GLOW_GREY_CAPSULE_FULL_TURN;
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            _glowInitGreyCapsuleWedge(prim, brightness);
            prim->x0     = block->sx1 + ((block->radius1 * rsin(angleOffset - endRimAngle)) >> GLOW_GREY_CAPSULE_TRIG_SHIFT);
            prim->y0     = block->sy1 + ((block->radius1 * rcos(angleOffset - endRimAngle)) >> GLOW_GREY_CAPSULE_TRIG_SHIFT);
            endMidAngle  = angle - (GLOW_GREY_CAPSULE_FULL_TURN - GLOW_GREY_CAPSULE_WEDGE_ANGLE / 2);
            prim->x1     = block->sx1 + ((block->radius1 * rsin(angleOffset - endMidAngle)) >> GLOW_GREY_CAPSULE_TRIG_SHIFT);
            prim->y1     = block->sy1 + ((block->radius1 * rcos(angleOffset - endMidAngle)) >> GLOW_GREY_CAPSULE_TRIG_SHIFT);
            endNextAngle = angle - (GLOW_GREY_CAPSULE_FULL_TURN - GLOW_GREY_CAPSULE_WEDGE_ANGLE);
            prim->x2     = block->sx1;
            prim->y2     = block->sy1;
            endNextAngle = angleOffset - endNextAngle;
            prim->x3     = block->sx1 + ((block->radius1 * rsin(endNextAngle)) >> GLOW_GREY_CAPSULE_TRIG_SHIFT);
            prim->y3     = block->sy1 + ((block->radius1 * rcos(endNextAngle)) >> GLOW_GREY_CAPSULE_TRIG_SHIFT);
            angle        = nextAngle;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->otz1);
        } while (angle < GLOW_GREY_CAPSULE_HALF_TURN);
    }
    SCRATCH_STACK_RELEASE_BLOCK(GlowPointPairScratch);
}
