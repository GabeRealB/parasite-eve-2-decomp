/* Part of the glow drawing library; see glow.h. */

/// Draws a beam with rounded caps inside a halo fading to black, from 24 projected `screenPoints` and the carrier's
/// quad and color tables, scaled by the brightness `GLOW_CAPPED_BEAM_BRIGHTNESS` reads from `task` and sorted at
/// `sortingDepth`. Also advances the task's `killCountdown` phase by `GLOW_CAPPED_BEAM_PHASE_STEP`, if defined, back to
/// zero at a half turn.
static void _glowDrawCappedBeam(Task* task, const SVECTOR screenPoints[24], s32 sortingDepth)
{
    enum {
        GLOW_CAPPED_BEAM_CORE_QUAD_COUNT = 6,
        GLOW_CAPPED_BEAM_CORE_DRAW_MODE  = 0xE1000425, // Draw-to-display enabled, dithering off, additive blend
    };

    CVECTOR scaledColors[ARRAY_SIZE(_gGlowCappedBeamColors)];
    const s8(*quadIndices)[4];
    const CVECTOR* colors;
    POLY_G4*       quad;
    DR_TPAGE*      drawMode;
    s32            index;
    u16            brightnessQ12;
    s32            vertex0Index, vertex1Index, vertex2Index, vertex3Index;

    /// Takes a semitransparent quad from the packet arena with the colors and screen points `indexRow` names.
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
    quadIndices   = _gGlowCappedBeamQuadIndices;
    brightnessQ12 = GLOW_CAPPED_BEAM_BRIGHTNESS(task);
#ifdef GLOW_CAPPED_BEAM_PHASE_STEP
    task->killCountdown += GLOW_CAPPED_BEAM_PHASE_STEP;
#endif
    if (task->killCountdown >= GLOW_HALF_TURN) {
        task->killCountdown = 0;
    }
    // The binary also calls sine here and discards its result.
    rsin(task->killCountdown);
    for (index = 0; index < (s32)ARRAY_SIZE(scaledColors); index++) {
        scaledColors[index].r = (_gGlowCappedBeamColors[index].r * (s16)brightnessQ12) >> GLOW_TRIG_SHIFT;
        scaledColors[index].g = (_gGlowCappedBeamColors[index].g * (s16)brightnessQ12) >> GLOW_TRIG_SHIFT;
        scaledColors[index].b = (_gGlowCappedBeamColors[index].b * (s16)brightnessQ12) >> GLOW_TRIG_SHIFT;
    }
    // Queue the lit core first, then the ten quads fading to the outer contour.
    colors = scaledColors;
    for (index = 0; index < GLOW_CAPPED_BEAM_CORE_QUAD_COUNT; index++) {
        GLOW_CAPPED_BEAM_ALLOC_QUAD(quad, *quadIndices, colors, screenPoints);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(sortingDepth) + GLOW_CAPPED_BEAM_ORDERING_TABLE_TAG_BIAS, quad);
        drawMode       = gGpuPrimCursor;
        gGpuPrimCursor = drawMode + 1;
        setlen(drawMode, ARRAY_SIZE(drawMode->code));
        drawMode->code[0] = GLOW_CAPPED_BEAM_CORE_DRAW_MODE;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(sortingDepth) + GLOW_CAPPED_BEAM_ORDERING_TABLE_TAG_BIAS, drawMode);
        quadIndices++;
    }
    for (index = GLOW_CAPPED_BEAM_CORE_QUAD_COUNT; index < (s32)ARRAY_SIZE(_gGlowCappedBeamQuadIndices); index++) {
        GLOW_CAPPED_BEAM_ALLOC_QUAD(quad, *quadIndices, colors, screenPoints);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(sortingDepth) + GLOW_CAPPED_BEAM_ORDERING_TABLE_TAG_BIAS, quad);
        drawMode       = gGpuPrimCursor;
        gGpuPrimCursor = drawMode + 1;
        setlen(drawMode, ARRAY_SIZE(drawMode->code));
        drawMode->code[0] = GLOW_CAPPED_BEAM_HALO_DRAW_MODE;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(sortingDepth) + GLOW_CAPPED_BEAM_ORDERING_TABLE_TAG_BIAS, drawMode);
        quadIndices++;
    }
#undef GLOW_CAPPED_BEAM_ALLOC_QUAD
}

#undef GLOW_CAPPED_BEAM_BRIGHTNESS
#undef GLOW_CAPPED_BEAM_PHASE_STEP
#undef GLOW_CAPPED_BEAM_ORDERING_TABLE_TAG_BIAS
#undef GLOW_CAPPED_BEAM_HALO_DRAW_MODE
