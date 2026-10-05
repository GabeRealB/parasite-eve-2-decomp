/* Part of the glow drawing library; see glow_draw.h. */

/// Prepares a red disc wedge with a red centre and a black rim.
///
/// Borrows one writable `POLY_G4`, setting its opaque Gouraud command and
/// packet length. Vertex 2 has the low byte of `redIntensity` in red and zero
/// green and blue; vertices 0, 1 and 3 are black. The caller supplies
/// coordinates, DMA linkage and semitransparency; its flicker supplies 32 or 40.
static inline void _glowInitRedDiscWedge(POLY_G4* wedge, s32 redIntensity)
{
    setPolyG4(wedge);
    setRGB0(wedge, 0, 0, 0);
    setRGB1(wedge, 0, 0, 0);
    setRGB2(wedge, redIntensity, 0, 0);
    setRGB3(wedge, 0, 0, 0);
}

void glowDrawRedDisc(const SVECTOR* worldPoint, s16 radiusScale)
{
    GlowCentreRadiusScratch* block;
    POLY_G4*                 prim;
    s32                      angle;
    s32                      halfStepAngle;
    s32                      nextAngle;
    s32                      redIntensity;
    s32                      screenRadius;

    block = SCRATCH_STACK_RESERVE_BLOCK(GlowCentreRadiusScratch);

    // Project into the current view before deriving screen radii and sorting depths.
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(worldPoint);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stszotz(&block->otz);
    if (block->otz >= GLOW_MIN_DEPTH) {
        screenRadius  = (radiusScale * GLOW_RADIUS_SCALE) / block->otz;
        redIntensity  = (((u8)gDisplayState.animFrame & 1) * GLOW_FLICKER_INTENSITY_STEP) | GLOW_FLICKER_BASE_INTENSITY;
        angle         = 0;
        block->radius = screenRadius;
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            _glowInitRedDiscWedge(prim, redIntensity);
            prim->x0      = block->sx + ((block->radius * rsin(angle)) >> GLOW_TRIG_SHIFT);
            halfStepAngle = angle + GLOW_EIGHTH_TURN;
            prim->y0      = block->sy + ((block->radius * rcos(angle)) >> GLOW_TRIG_SHIFT);
            prim->x1      = block->sx + ((block->radius * rsin(halfStepAngle)) >> GLOW_TRIG_SHIFT);
            prim->y1      = block->sy + ((block->radius * rcos(halfStepAngle)) >> GLOW_TRIG_SHIFT);
            nextAngle     = angle + GLOW_QUARTER_TURN;
            prim->x2      = block->sx;
            prim->y2      = block->sy;
            prim->x3      = block->sx + ((block->radius * rsin(nextAngle)) >> GLOW_TRIG_SHIFT);
            prim->y3      = block->sy + ((block->radius * rcos(nextAngle)) >> GLOW_TRIG_SHIFT);
            angle         = nextAngle;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->otz);
        } while (angle < GLOW_FULL_TURN);
    }
    SCRATCH_STACK_RELEASE_BLOCK(GlowCentreRadiusScratch);
}
