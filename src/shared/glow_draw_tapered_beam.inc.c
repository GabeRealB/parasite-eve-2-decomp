/* Part of the glow drawing library; see glow_draw.h. */

/// Initializes an allocated grey wedge with vertex 2 lit and the rim black.
///
/// Borrows one writable packet; brightness narrows to a byte. The caller
/// supplies allocation, geometry, ordering-table link and blend command.
static inline void _glowInitTaperedBeamWedge(POLY_G4* prim, s32 brightness)
{
    setPolyG4(prim);
    setRGB0(prim, 0, 0, 0);
    setRGB1(prim, 0, 0, 0);
    setRGB2(prim, brightness, brightness, brightness);
    setRGB3(prim, 0, 0, 0);
}

/// Draws an additive grey beam between two local-space endpoints.
///
/// Borrows `coord`, `startPoint` and `endPoint`; `coord->workm` must already
/// map both endpoints into world space. Transformed components narrow to
/// signed 16 bits before view projection through `GsWSMATRIX`. The second
/// endpoint's camera Z / 4 depth must be at least 17; the first is clamped to
/// 16. Projection flags are not tested. Each pixel radius is the signed low
/// halfword of `radiusScale` times 64 divided by that endpoint's depth.
///
/// Opposing half-disc caps and two joining bands use grey 32/48 on frame
/// parity, fading to a black rim. Bands sort at the first endpoint's depth.
/// Endpoint order sets the screen-space orientation. Queues six quads and
/// additive blend commands in the current frame packet arena.
static void _glowDrawTaperedBeam(const GfxCoord* coord, const SVECTOR* startPoint, const SVECTOR* endPoint, s32 radiusScale)
{
    enum {
        GLOW_TAPERED_BEAM_MIN_DEPTH        = 17,
        GLOW_TAPERED_BEAM_NEAR_DEPTH_CLAMP = 16,
        GLOW_TAPERED_BEAM_RADIUS_SCALE     = 64,
        GLOW_TAPERED_BEAM_BRIGHTNESS_BASE  = 32,
        GLOW_TAPERED_BEAM_BRIGHTNESS_STEP  = 16,
        GLOW_TAPERED_BEAM_TRIG_SHIFT       = 12,
        GLOW_TAPERED_BEAM_FULL_TURN        = 4096,
        GLOW_TAPERED_BEAM_HALF_TURN        = 2048,
        GLOW_TAPERED_BEAM_WEDGE_ANGLE      = 1024,
    };

    GlowWorldPointPairScratch* block;
    POLY_G4*                   prim;
    s32                        angle;
    s32                        rimAngle;
    s32                        nextAngle;
    s32                        brightness;
    s32                        scaledRadius;

    block = SCRATCH_STACK_RESERVE_BLOCK(GlowWorldPointPairScratch);

    // Place both local endpoints in world space before projecting them.
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(startPoint);
    gte_rtv0();
    gte_stsv(&block->worldPoint0);
    block->worldPoint0.vx += coord->workm.t[0];
    block->worldPoint0.vy += coord->workm.t[1];
    block->worldPoint0.vz += coord->workm.t[2];

    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(endPoint);
    gte_rtv0();
    gte_stsv(&block->worldPoint1);
    block->worldPoint1.vx += coord->workm.t[0];
    block->worldPoint1.vy += coord->workm.t[1];
    block->worldPoint1.vz += coord->workm.t[2];

    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->worldPoint0);
    gte_rtps();
    gte_stsxy(&block->sx0);
    gte_stszotz(&block->otz0);
    gte_ldv0(&block->worldPoint1);
    gte_rtps();
    gte_stsxy(&block->sx1);
    gte_stszotz(&block->otz1);
    if (block->otz1 >= GLOW_TAPERED_BEAM_MIN_DEPTH) {
        if (block->otz0 < GLOW_TAPERED_BEAM_NEAR_DEPTH_CLAMP) {
            block->otz0 = GLOW_TAPERED_BEAM_NEAR_DEPTH_CLAMP;
        }
        scaledRadius   = (s16)radiusScale * GLOW_TAPERED_BEAM_RADIUS_SCALE;
        angle          = 0;
        brightness     = (((u8)gDisplayState.animFrame & 1) * GLOW_TAPERED_BEAM_BRIGHTNESS_STEP) | GLOW_TAPERED_BEAM_BRIGHTNESS_BASE;
        block->radius0 = scaledRadius / block->otz0;
        block->radius1 = scaledRadius / block->otz1;
        // Join opposing half-disc caps with two bands.
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            _glowInitTaperedBeamWedge(prim, brightness);
            prim->x0  = block->sx0 + ((block->radius0 * rsin(angle)) >> GLOW_TAPERED_BEAM_TRIG_SHIFT);
            rimAngle  = angle + GLOW_TAPERED_BEAM_WEDGE_ANGLE / 2;
            prim->y0  = block->sy0 + ((block->radius0 * rcos(angle)) >> GLOW_TAPERED_BEAM_TRIG_SHIFT);
            prim->x1  = block->sx0 + ((block->radius0 * rsin(rimAngle)) >> GLOW_TAPERED_BEAM_TRIG_SHIFT);
            prim->y1  = block->sy0 + ((block->radius0 * rcos(rimAngle)) >> GLOW_TAPERED_BEAM_TRIG_SHIFT);
            nextAngle = angle + GLOW_TAPERED_BEAM_WEDGE_ANGLE;
            prim->x2  = block->sx0;
            prim->y2  = block->sy0;
            prim->x3  = block->sx0 + ((block->radius0 * rsin(nextAngle)) >> GLOW_TAPERED_BEAM_TRIG_SHIFT);
            prim->y3  = block->sy0 + ((block->radius0 * rcos(nextAngle)) >> GLOW_TAPERED_BEAM_TRIG_SHIFT);
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
            prim->x0 = block->sx0 + ((block->radius0 * rsin(angle * 2)) >> GLOW_TAPERED_BEAM_TRIG_SHIFT);
            prim->y0 = block->sy0 + ((block->radius0 * rcos(angle * 2)) >> GLOW_TAPERED_BEAM_TRIG_SHIFT);
            prim->x1 = block->sx1 + ((block->radius1 * rsin(angle * 2)) >> GLOW_TAPERED_BEAM_TRIG_SHIFT);
            prim->y1 = block->sy1 + ((block->radius1 * rcos(angle * 2)) >> GLOW_TAPERED_BEAM_TRIG_SHIFT);
            prim->x2 = block->sx0;
            prim->y2 = block->sy0;
            prim->x3 = block->sx1;
            prim->y3 = block->sy1;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->otz0);

            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            _glowInitTaperedBeamWedge(prim, brightness);
            prim->x0 = block->sx1 + ((block->radius1 * rsin(GLOW_TAPERED_BEAM_FULL_TURN - angle)) >> GLOW_TAPERED_BEAM_TRIG_SHIFT);
            prim->y0 = block->sy1 + ((block->radius1 * rcos(GLOW_TAPERED_BEAM_FULL_TURN - angle)) >> GLOW_TAPERED_BEAM_TRIG_SHIFT);
            prim->x1 = block->sx1 + ((block->radius1 * rsin((GLOW_TAPERED_BEAM_FULL_TURN - GLOW_TAPERED_BEAM_WEDGE_ANGLE / 2) - angle)) >> GLOW_TAPERED_BEAM_TRIG_SHIFT);
            prim->y1 = block->sy1 + ((block->radius1 * rcos((GLOW_TAPERED_BEAM_FULL_TURN - GLOW_TAPERED_BEAM_WEDGE_ANGLE / 2) - angle)) >> GLOW_TAPERED_BEAM_TRIG_SHIFT);
            prim->x2 = block->sx1;
            prim->y2 = block->sy1;
            prim->x3 = block->sx1 + ((block->radius1 * rsin((GLOW_TAPERED_BEAM_FULL_TURN - GLOW_TAPERED_BEAM_WEDGE_ANGLE) - angle)) >> GLOW_TAPERED_BEAM_TRIG_SHIFT);
            prim->y3 = block->sy1 + ((block->radius1 * rcos((GLOW_TAPERED_BEAM_FULL_TURN - GLOW_TAPERED_BEAM_WEDGE_ANGLE) - angle)) >> GLOW_TAPERED_BEAM_TRIG_SHIFT);
            angle    = nextAngle;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->otz1);
        } while (angle < GLOW_TAPERED_BEAM_HALF_TURN);
    }
    SCRATCH_STACK_RELEASE_BLOCK(GlowWorldPointPairScratch);
}
