/* Part of the glow drawing library; see glow_draw.h. */

/// Draws a lit beam with rounded end caps and a halo fading to black.
///
/// Borrows 24 `screenPoints` for this call; only signed pixel X/Y are read.
/// Each six-point group has a centre followed by five rim points.
/// Groups 0..5 and 6..11 cap the two core ends; 12..17 and 18..23 supply the
/// corresponding enlarged halo caps. Halo centres 12 and 18 are unused.
/// The carrier's 16-row quad table supplies indices 0..23, and its 24-row
/// colour table supplies RGB bytes; fourth bytes are unused.
/// Brightness comes from the live controller in `task->spawnArg2.pointer`:
/// its low halfword is treated as signed Q12 (normally 0..4096), and scaled
/// colour channels narrow to bytes without saturation.
///
/// Queues six core quads and ten halo quads, each with a draw-mode packet,
/// without projection rejection. All sort at the quantized `sortingDepth`
/// (camera Z / 4) plus a carrier offset measured in OT tags. The selected OT
/// must contain that entry: actor_141000 uses -20, Dryfield uses +3. The core
/// is additive; Dryfield's halo is quarter-additive, the actor's additive.
///
/// The task's signed-halfword `killCountdown` is a geometry angle, in 4096
/// units per turn. Dryfield advances it by 64 per draw; both carriers reset
/// values >= 2048 to zero. Geometry is built before this update, so it affects
/// subsequent builds. Borrows the controller and consumes frame arena space
/// for sixteen `POLY_G4`/`DR_TPAGE` pairs without a capacity check.
static void _glowDrawCappedBeam(Task* task, const SVECTOR screenPoints[24], s32 sortingDepth)
{
    enum {
        GLOW_CAPPED_BEAM_CORE_QUAD_COUNT = 6,
        GLOW_CAPPED_BEAM_CORE_DRAW_MODE  = 0xE1000425, // Draw-to-display enabled, dithering off, additive blend
    };

    CVECTOR scaledColors[ARRAY_SIZE(gGlowRingBeamColors)];
    const s8(*quadIndices)[4];
    const CVECTOR* colors;
    POLY_G4*       quad;
    DR_TPAGE*      drawMode;
    s32            index;
    u16            brightnessQ12;
    s32            vertex0Index, vertex1Index, vertex2Index, vertex3Index;

    /// Allocates and fills one semitransparent beam quad from four vertex indices.
    ///
    /// `indexRow` supplies four s8 indices in 0..23. Reads RGB from `colors`
    /// and signed pixel X/Y from `screenPoints`; their other components are
    /// untouched. Arguments must have no side effects, and `quad` must be a
    /// writable pointer variable: every argument is evaluated repeatedly.
    /// Captures the four s32 `vertexNIndex` locals and `gGpuPrimCursor`, advancing
    /// the unchecked frame arena by one POLY_G4. The caller queues the packet
    /// with a draw-mode command. Defined only for this function, then undefined.
#define GLOW_CAPPED_BEAM_ALLOC_QUAD(quad, indexRow, colors, screenPoints) \
    {                                                                     \
        vertex0Index   = (indexRow)[0];                                   \
        vertex1Index   = (indexRow)[1];                                   \
        vertex2Index   = (indexRow)[2];                                   \
        vertex3Index   = (indexRow)[3];                                   \
        (quad)         = gGpuPrimCursor;                                  \
        gGpuPrimCursor = (quad) + 1;                                      \
        setPolyG4((quad));                                                \
        setSemiTrans((quad), true);                                       \
        (quad)->r0 = (colors)[vertex0Index].r;                            \
        (quad)->g0 = (colors)[vertex0Index].g;                            \
        (quad)->b0 = (colors)[vertex0Index].b;                            \
        (quad)->r1 = (colors)[vertex1Index].r;                            \
        (quad)->g1 = (colors)[vertex1Index].g;                            \
        (quad)->b1 = (colors)[vertex1Index].b;                            \
        (quad)->r2 = (colors)[vertex2Index].r;                            \
        (quad)->g2 = (colors)[vertex2Index].g;                            \
        (quad)->b2 = (colors)[vertex2Index].b;                            \
        (quad)->r3 = (colors)[vertex3Index].r;                            \
        (quad)->g3 = (colors)[vertex3Index].g;                            \
        (quad)->b3 = (colors)[vertex3Index].b;                            \
        (quad)->x0 = (screenPoints)[vertex0Index].vx;                     \
        (quad)->y0 = (screenPoints)[vertex0Index].vy;                     \
        (quad)->x1 = (screenPoints)[vertex1Index].vx;                     \
        (quad)->y1 = (screenPoints)[vertex1Index].vy;                     \
        (quad)->x2 = (screenPoints)[vertex2Index].vx;                     \
        (quad)->y2 = (screenPoints)[vertex2Index].vy;                     \
        (quad)->x3 = (screenPoints)[vertex3Index].vx;                     \
        (quad)->y3 = (screenPoints)[vertex3Index].vy;                     \
    }

    // Snapshot the parent brightness before advancing the next geometry phase.
    quadIndices   = gGlowRingBeamQuads;
    brightnessQ12 = GLOW_DRAW_RING_BEAM_BRIGHTNESS(task);
#ifdef GLOW_DRAW_RING_BEAM_PHASE_STEP
    task->killCountdown += GLOW_DRAW_RING_BEAM_PHASE_STEP;
#endif
    if (task->killCountdown >= GLOW_HALF_TURN) {
        task->killCountdown = 0;
    }
    // The binary also calls sine here and discards its result.
    rsin(task->killCountdown);
    for (index = 0; index < (s32)ARRAY_SIZE(scaledColors); index++) {
        scaledColors[index].r = (gGlowRingBeamColors[index][0] * (s16)brightnessQ12) >> GLOW_TRIG_SHIFT;
        scaledColors[index].g = (gGlowRingBeamColors[index][1] * (s16)brightnessQ12) >> GLOW_TRIG_SHIFT;
        scaledColors[index].b = (gGlowRingBeamColors[index][2] * (s16)brightnessQ12) >> GLOW_TRIG_SHIFT;
    }
    // Queue the lit core first, then the ten quads fading to the outer contour.
    colors = scaledColors;
    for (index = 0; index < GLOW_CAPPED_BEAM_CORE_QUAD_COUNT; index++) {
        GLOW_CAPPED_BEAM_ALLOC_QUAD(quad, *quadIndices, colors, screenPoints);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET((((u32)(sortingDepth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) + GLOW_DRAW_RING_BEAM_OT_OFFSET, quad);
        drawMode       = gGpuPrimCursor;
        gGpuPrimCursor = drawMode + 1;
        setlen(drawMode, ARRAY_SIZE(drawMode->code));
        drawMode->code[0] = GLOW_CAPPED_BEAM_CORE_DRAW_MODE;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET((((u32)(sortingDepth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) + GLOW_DRAW_RING_BEAM_OT_OFFSET, drawMode);
        quadIndices++;
    }
    for (index = GLOW_CAPPED_BEAM_CORE_QUAD_COUNT; index < (s32)ARRAY_SIZE(gGlowRingBeamQuads); index++) {
        GLOW_CAPPED_BEAM_ALLOC_QUAD(quad, *quadIndices, colors, screenPoints);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET((((u32)(sortingDepth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) + GLOW_DRAW_RING_BEAM_OT_OFFSET, quad);
        drawMode       = gGpuPrimCursor;
        gGpuPrimCursor = drawMode + 1;
        setlen(drawMode, ARRAY_SIZE(drawMode->code));
        drawMode->code[0] = GLOW_DRAW_RING_BEAM_HALO_TPAGE;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET((((u32)(sortingDepth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) + GLOW_DRAW_RING_BEAM_OT_OFFSET, drawMode);
        quadIndices++;
    }
#undef GLOW_CAPPED_BEAM_ALLOC_QUAD
}

#undef GLOW_DRAW_RING_BEAM_BRIGHTNESS
#undef GLOW_DRAW_RING_BEAM_PHASE_STEP
#undef GLOW_DRAW_RING_BEAM_OT_OFFSET
#undef GLOW_DRAW_RING_BEAM_HALO_TPAGE
